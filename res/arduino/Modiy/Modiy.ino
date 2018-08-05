//HardwareSerial.h -> #define SERIAL_RX_BUFFER_SIZE 256
#include <ResponsiveAnalogRead.h>

//Jack - Analog - LED - Switches (JALS)
struct JALS {
  enum Type { Jack, Analog, LED, Switch };
  uint16_t offset[4] = {0, 0, 0, 0};
  uint8_t  count[4]  = {0, 0, 0, 0};
};
JALS jals;


//Intercommunication betweend cards
const uint8_t comPinout[] = {18, 19}; // TX / RX
uint8_t cycle = 1, cycleCheck = 0;
bool firstLoop = true;


//All avalailable digital pins to be shared between Wires/Jack and Buttons/Switches
//{14, 15, 16, 17, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 47, 48, 49, 50, 51, 52, 53};

//Wires = Jacks (Digital)
const uint8_t jackPinout[] = {24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 47, 48, 49, 50, 51, 52, 53};
uint8_t ***jackValues;


//Buttons = Switches (Digital)
const uint8_t switchPinout[] = {14, 15, 16, 17, 20, 21, 22, 23};
uint16_t **switchValues;


//Potentiometers = Analog
const uint8_t analogPinout[] = {A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15};
ResponsiveAnalogRead *analogValues;

//PWM = LEDs
const uint8_t ledPinout[] = {2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 44, 45, 46};
//LED refresh counter
int lastLEDAsk = 0;


//Serial port
#define serialInputDataIndexMax  16*5
String serialInput[2][serialInputDataIndexMax];
int8_t serialInputDataIndex[2] = { -1, -1};
bool isDumping = false;


//Start of sketch
void setup() {
  //Serial port (serialInput.reserve(200); ?)
  Serial .begin(115200);
  Serial1.begin(115200);
  delay(1000);

  //Size of each pinouts categories
  jals.count[jals.Jack]   = sizeof(jackPinout);
  jals.count[jals.Analog] = sizeof(analogPinout);
  jals.count[jals.LED]    = sizeof(ledPinout);
  jals.count[jals.Switch] = sizeof(switchPinout);

  //Print some debug infos
  Serial.print("JALS_O ");
  Serial.print(jals.offset[jals.Jack]);
  Serial.print(" ");
  Serial.print(jals.offset[jals.Analog]);
  Serial.print(" ");
  Serial.print(jals.offset[jals.LED]);
  Serial.print(" ");
  Serial.println(jals.count[jals.Switch]);

  Serial.print("JALS_C ");
  Serial.print(jals.count[jals.Jack]);
  Serial.print(" ");
  Serial.print(jals.count[jals.Analog]);
  Serial.print(" ");
  Serial.print(jals.count[jals.LED]);
  Serial.print(" ");
  Serial.println(jals.count[jals.Switch]);

  Serial.print("Serial Buffer\t");
  Serial.println(SERIAL_RX_BUFFER_SIZE);

  //Allocate arrays/matrix
  alloc();

  //LED refresh counter
  lastLEDAsk = millis();
}

void loop() {
  //Comparaison n/n-1
  cycleCheck = 1 - cycle;

  //Traitement
  if (firstLoop)
    Serial.println("DUMP_START");
  if (!isDumping) {
    processJack();
    processSwitch();
    processAnalog();
  }
  if (firstLoop)
    Serial.println("DUMP_END");

  //LED refresh ask each 50ms
  if ((millis() - lastLEDAsk) > 50) {
    lastLEDAsk = millis();
    sendLED();
  }

  //Comparaison n/n-1
  cycle = cycleCheck;
  if (firstLoop)
    firstLoop = false;
}

//Variable allocation
void alloc() {
  //Initialize serial input buffers
  for (uint8_t sI = 0 ; sI < sizeof(serialInputDataIndex) ; sI++) {
    for (uint8_t dataIndex = 0 ; dataIndex < serialInputDataIndexMax ; dataIndex++) {
      serialInput[sI][dataIndex] = "";
      serialInput[sI][dataIndex].reserve(8);
    }
  }

  //Initialize the matrix (upper-right diag)
  jackValues = new uint8_t**[2];
  for (uint8_t cycle = 0 ; cycle < 2 ; cycle++) {
    jackValues[cycle] = new uint8_t*[jals.count[jals.Jack]];
    for (uint8_t jack = 0 ; jack < jals.count[jals.Jack] ; jack++) {
      jackValues[cycle][jack] = new uint8_t[jals.count[jals.Jack] - (jack + 1)];
      for (uint8_t jackTested = 0 ; jackTested < (jals.count[jals.Jack] - (jack + 1)) ; jackTested++)
        jackValues[cycle][jack][jackTested] = (cycle) ? (0) : (-1);
    }
  }

  //Initialize analog values storage
  analogValues = new ResponsiveAnalogRead[jals.count[jals.Analog]];
  for (uint8_t analog = 0 ; analog < jals.count[jals.Analog] ; analog++)
    analogValues[analog] = ResponsiveAnalogRead(analogPinout[analog], true);

  //Initialize switches values storage
  switchValues = new uint16_t*[2];
  for (uint8_t cycle = 0 ; cycle < 2 ; cycle++) {
    switchValues[cycle] = new uint16_t[jals.count[jals.Switch]];
    for (uint8_t button = 0 ; button < jals.count[jals.Switch] ; button++)
      switchValues[cycle][button] = 0;
  }

  //Configure digital pins (for jacks) with internal pull up
  for (uint8_t jack = 0 ; jack < jals.count[jals.Jack] ; jack++)
    pinMode(jackPinout[jack], INPUT_PULLUP);

  //Configure digital pins (for switches) with internal pull up
  for (uint8_t button = 0 ; button < jals.count[jals.Switch] ; button++)
    pinMode(switchPinout[button], INPUT_PULLUP);

  //Configure PWM pins to output @ 0Hz
  for (uint8_t led = 0 ; led < jals.count[jals.LED] ; led++) {
    pinMode(ledPinout[led], OUTPUT);
    analogWrite(ledPinout[led], 0);
  }

  //Configure analog pins
  for (uint8_t analog = 0 ; analog < jals.count[jals.Analog] ; analog++)
    digitalWrite(analogPinout[analog], INPUT_PULLUP);
}


//Read digital cord patching
void processJack() {
  //Browse all digital pins
  for (uint8_t jack = 0 ; jack < jals.count[jals.Jack] ; jack++) {
    //Switch current pin to output (others are inputs)
    pinMode(jackPinout[jack], OUTPUT);
    digitalWrite(jackPinout[jack], LOW);

    //Browse all the other digital pins
    for (uint8_t jackTested = 0 ; jackTested < (jals.count[jals.Jack] - (jack + 1)) ; jackTested++) {
      //Read value
      jackValues[cycle][jack][jackTested] = 1 - digitalRead(jackPinout[jackTested + (jack + 1)]);

      //Send on serial port on change
      if (jackValues[cycle][jack][jackTested] != jackValues[cycleCheck][jack][jackTested])
        sendJack(jack, jackTested + (jack + 1), jackValues[cycle][jack][jackTested]);
    }

    //Switch back to input
    pinMode(jackPinout[jack], INPUT_PULLUP);
  }
}

//Read all analog values
void processAnalog() {
  //Browse all analog inputs
  for (uint8_t analog = 0 ; analog < jals.count[jals.Analog] ; analog++) {
    //Read value
    analogValues[analog].update();

    //Send on serial port on change
    if (analogValues[analog].hasChanged())
      sendAnalog(analog, analogValues[analog].getValue());
  }
}

//Real all switches values
void processSwitch() {
  //Browse all digital pins
  for (uint8_t button = 0 ; button < jals.count[jals.Switch] ; button++) {
    switchValues[cycle][button] = 1 - digitalRead(switchPinout[button]);

    //Send on serial port on change
    if (switchValues[cycle][button] != switchValues[cycleCheck][button])
      sendSwitch(button, switchValues[cycle][button]);
  }
}

//Serial port reception
void serialEvent() {
  uint8_t sI = 0;

  //Master serial port
  while (Serial.available()) {
    char receivedChar = (char)Serial.read();

    //Clear buffer (values separated by space)
    if (serialInputDataIndex[sI] < 0) {
      for (uint8_t dataIndex = 0 ; dataIndex < serialInputDataIndexMax ; dataIndex++)
        serialInput[sI][dataIndex] = "";
      serialInputDataIndex[sI] = 0;
    }

    //Switch depending on received character
    if (receivedChar == '\n') {
      //Received message
      if (serialInput[sI][0] == "L") {
        bool hasRedirected = false;
        for (uint8_t led = 0 ; led < serialInputDataIndex[sI] ; led++) {
          if (led < jals.count[jals.LED]) {
            if (false) {
              Serial.print("L PIN #");
              Serial.print(ledPinout[led]);
              Serial.print(" = ");
              Serial.println(serialInput[sI][1 + led].toInt());
            }
            analogWrite(ledPinout[led], serialInput[sI][1 + led].toInt());
          }
          else {
            if (!hasRedirected)
              Serial1.print("L ");
            if (led == (serialInputDataIndex[sI] - 1))
              Serial1.println(serialInput[sI][1 + led].toInt());
            else {
              Serial1.print(serialInput[sI][1 + led].toInt());
              Serial1.print(" ");
            }
            hasRedirected = true;
          }
          //L 1 2 3 4 5 6 7 8 9 10 11 255 12 13 14 1 2 3 4 5 6 7 8 9 10 11 255 12 13 14
        }
        serialInputDataIndex[sI] = -1;
      }

      //Next please!
      serialInputDataIndex[sI] = -1;
    }
    else if (((receivedChar == ' ') || (receivedChar == '\t')) && (serialInputDataIndex[sI] < (serialInputDataIndexMax - 1))) {
      serialInputDataIndex[sI]++;
      serialInput[sI][serialInputDataIndex[sI]] = "";
    }
    else {
      //Add to current reception buffer
      serialInput[sI][serialInputDataIndex[sI]] += receivedChar;
    }
  }
}

void sendAnalog(uint16_t port, uint16_t value) {
  Serial.print("A ");
  Serial.print(jals.offset[jals.Analog] + port);
  Serial.print(" ");
  Serial.println(value);
  if (firstLoop)
    delay(1);
}
void sendJack(uint16_t jackA, uint16_t jackB, uint8_t onoff) {
  Serial.print("J ");
  Serial.print(jals.offset[jals.Jack] + jackA);
  Serial.print(" ");
  Serial.print(jals.offset[jals.Jack] + jackB);
  Serial.print(" ");
  Serial.println(onoff);
  if (firstLoop)
    delay(1);
}
void sendLED() {
  Serial.println("L");
}
void sendSwitch(uint16_t button, uint8_t onoff) {
  Serial.print("S ");
  Serial.print(jals.offset[jals.Switch] + button);
  Serial.print(" ");
  Serial.println(onoff);
  if (firstLoop)
    delay(1);
}


//Slave serial port reception
void serialEvent1() {
  uint8_t sI = 1;

  //Master serial port
  if (Serial1.available() > (SERIAL_RX_BUFFER_SIZE - 10)) {
    Serial.print("Serial buffer overflow (");
    Serial.print(Serial1.available());
    Serial.println(")");
  }
  while (Serial1.available()) {
    char receivedChar = (char)Serial1.read();

    //Clear buffer (values separated by space)
    if (serialInputDataIndex[sI] < 0) {
      for (uint8_t dataIndex = 0 ; dataIndex < serialInputDataIndexMax ; dataIndex++)
        serialInput[sI][dataIndex] = "";
      serialInputDataIndex[sI] = 0;
    }

    //Switch depending on received character
    if (receivedChar == '\n') {
      //Analog message reception
      if ((serialInputDataIndex[sI] == 2) && (serialInput[sI][0] == "A"))
        sendAnalog(jals.count[jals.Analog] + serialInput[sI][1].toInt(), serialInput[sI][2].toInt());
      //Switch message reception
      else if ((serialInputDataIndex[sI] == 2) && (serialInput[sI][0] == "S"))
        sendSwitch(jals.count[jals.Switch] + serialInput[sI][1].toInt(), serialInput[sI][2].toInt());
      //Digital message reception
      else if ((serialInputDataIndex[sI] == 3) && (serialInput[sI][0] == "J"))
        sendJack(jals.count[jals.Jack] + serialInput[sI][1].toInt(), jals.count[jals.Jack] + serialInput[sI][2].toInt(), serialInput[sI][3].toInt());
      else if (serialInput[sI][0] == "DUMP_START") {
        Serial.println("DUMP_START");
        isDumping = true;
      }
      else if (serialInput[sI][0] == "DUMP_END") {
        Serial.println("DUMP_END");
        isDumping = false;
      }
      else if (false) {
        Serial.print("Slave Reception (");
        Serial.print(serialInputDataIndex[sI]);
        Serial.print(") (RX @ ");
        Serial.print(Serial1.available());
        Serial.println(")");
        for (uint8_t arg = 0 ; arg <= serialInputDataIndex[sI] ; arg++)
          Serial.println(serialInput[sI][arg]);
      }

      //Next please!
      serialInputDataIndex[sI] = -1;
    }
    else if (((receivedChar == ' ') || (receivedChar == '\t')) && (serialInputDataIndex[sI] < (serialInputDataIndexMax - 1))) {
      serialInputDataIndex[sI]++;
      serialInput[sI][serialInputDataIndex[sI]] = "";
    }
    else {
      //Add to current reception buffer
      serialInput[sI][serialInputDataIndex[sI]] += receivedChar;
    }
  }
}
