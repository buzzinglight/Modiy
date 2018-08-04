//HardwareSerial.h -> #define SERIAL_RX_BUFFER_SIZE 256

//Jack - Analog - LED (JAL)
struct JAL {
  enum Type { Jack, Analog, LED };
  uint16_t offset[3] = {0, 0, 0};
  uint8_t  count[3]  = {0, 0, 0};
};
JAL jal;


//Intercommunication betweend cards
const uint8_t comPinout[] = {18, 19}; // TX / RX
uint8_t cycle = 1, cycleCheck = 0;
bool firstLoop = true;


//Wires = Jacks (digital)
const uint8_t jackPinout[] = {14, 15, 16, 17, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 47, 48, 49, 50, 51, 52, 53};
uint8_t ***jackValues;


//Potentiometers = Analog
const uint8_t analogPinout[] = {A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15};
uint16_t **analogValues;
struct RunningAverage {
  uint8_t powerOfTwo = 4; //Only edit this line —> 2^4 = 16 values

  uint16_t **values;
  uint8_t number = 0;
  uint8_t index = 0;
};
RunningAverage ra;


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
  jal.count[jal.Jack]   = sizeof(jackPinout);
  jal.count[jal.Analog] = sizeof(analogPinout);
  jal.count[jal.LED]    = sizeof(ledPinout);
  ra.number = 1 << ra.powerOfTwo;
  Serial.print("JAL_O ");
  Serial.print(jal.offset[jal.Jack]);
  Serial.print(" ");
  Serial.print(jal.offset[jal.Analog]);
  Serial.print(" ");
  Serial.println(jal.offset[jal.LED]);
  Serial.print("JAL_C ");
  Serial.print(jal.count[jal.Jack]);
  Serial.print(" ");
  Serial.print(jal.count[jal.Analog]);
  Serial.print(" ");
  Serial.println(jal.count[jal.LED]);
  Serial.print("Serial Buffer\t");
  Serial.println(SERIAL_RX_BUFFER_SIZE);
  Serial.print("Running average\t");
  Serial.println(ra.number);

  alloc();

  //LED refresh counter
  lastLEDAsk = millis();

  //Analog running average fill
  for (uint8_t raIndex = 0 ; raIndex < ra.number ; raIndex++)
    processAnalog(false);
}

void loop() {
  //Comparaison n/n-1
  cycleCheck = 1 - cycle;

  //Traitement
  if (firstLoop)
    Serial.println("DUMP_START");
  if (!isDumping) {
    processJack();
    processAnalog(true);
  }
  if (firstLoop)
    Serial.println("DUMP_END");

  //LED refresh ask each 50ms
  if ((millis() - lastLEDAsk) > 50) {
    lastLEDAsk = millis();
    Serial.println("L");
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
    jackValues[cycle] = new uint8_t*[jal.count[jal.Jack]];
    for (uint8_t jack = 0 ; jack < jal.count[jal.Jack] ; jack++) {
      jackValues[cycle][jack] = new uint8_t[jal.count[jal.Jack] - (jack + 1)];
      for (uint8_t jackTested = 0 ; jackTested < (jal.count[jal.Jack] - (jack + 1)) ; jackTested++)
        jackValues[cycle][jack][jackTested] = (cycle) ? (0) : (-1);
    }
  }

  //Initialize analog values storage
  analogValues = new uint16_t*[2];
  for (uint8_t cycle = 0 ; cycle < 2 ; cycle++) {
    analogValues[cycle] = new uint16_t[jal.count[jal.Analog]];
    for (uint8_t analog = 0 ; analog < jal.count[jal.Analog] ; analog++)
      analogValues[cycle][analog] = 0;
  }

  //Initialize analog running average
  ra.values = new uint16_t*[jal.count[jal.Analog]];
  for (uint8_t analog = 0 ; analog < jal.count[jal.Analog] ; analog++) {
    ra.values[analog] = new uint16_t[ra.number];
    for (uint8_t raIndex = 0 ; raIndex < ra.number ; raIndex++)
      ra.values[analog][raIndex] = 0;
  }

  //Configure digital pins with internal pull up
  for (uint8_t jack = 0 ; jack < jal.count[jal.Jack] ; jack++)
    pinMode(jackPinout[jack], INPUT_PULLUP);

  //Configure PWM pins to output @ 0Hz
  for (uint8_t led = 0 ; led < jal.count[jal.LED] ; led++) {
    pinMode(ledPinout[led], OUTPUT);
    analogWrite(ledPinout[led], 0);
  }

  //Configure analog pins
  for (uint8_t analog = 0 ; analog < jal.count[jal.Analog] ; analog++)
    digitalWrite(analogPinout[analog], INPUT_PULLUP);
}


//Read digital cord patching
void processJack() {
  //Browse all digital pins
  for (uint8_t jack = 0 ; jack < jal.count[jal.Jack] ; jack++) {
    //Switch current pin to output (others are inputs)
    pinMode(jackPinout[jack], OUTPUT);
    digitalWrite(jackPinout[jack], LOW);

    //Browse all the other digital pins
    for (uint8_t jackTested = 0 ; jackTested < (jal.count[jal.Jack] - (jack + 1)) ; jackTested++) {
      //Read value
      jackValues[cycle][jack][jackTested] = 1 - digitalRead(jackPinout[jackTested + (jack + 1)]);

      //Send on serial port on change
      if (jackValues[cycle][jack][jackTested] != jackValues[cycleCheck][jack][jackTested])
        sendDigital(jack, jackTested + (jack + 1), jackValues[cycle][jack][jackTested]);
    }

    //Switch back to input
    pinMode(jackPinout[jack], INPUT_PULLUP);
  }
}

//Read all analog values
void processAnalog(bool doSend) {
  //Shift running average value
  ra.index = (ra.index + 1) % ra.number;
  uint16_t total = 0;

  //Browse all analog inputs
  for (uint8_t analog = 0 ; analog < jal.count[jal.Analog] ; analog++) {
    //Read value
    ra.values[analog][ra.index] = analogRead(analogPinout[analog]);

    //Total of values
    total = 0;
    for (uint8_t raIndex = 0 ; raIndex < ra.number ; raIndex++)
      total += ra.values[analog][raIndex];

    if (doSend) {
      //Running average value
      analogValues[cycle][analog] = total >> ra.powerOfTwo;

      //Send on serial port on change
      if (analogValues[cycleCheck][analog] != analogValues[cycle][analog])
        sendAnalog(analog, analogValues[cycle][analog]);
    }
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
          if (led < jal.count[jal.LED]) {
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
  Serial.print(jal.offset[jal.Analog] + port);
  Serial.print(" ");
  Serial.println(value);
  if (firstLoop)
    delay(1);
}
void sendDigital(uint16_t jackA, uint16_t jackB, uint8_t onoff) {
  Serial.print("J ");
  Serial.print(jal.offset[jal.Jack] + jackA);
  Serial.print(" ");
  Serial.print(jal.offset[jal.Jack] + jackB);
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
        sendAnalog(jal.count[jal.Analog] + serialInput[sI][1].toInt(), serialInput[sI][2].toInt());
      //Digital message reception
      else if ((serialInputDataIndex[sI] == 3) && (serialInput[sI][0] == "J"))
        sendDigital(jal.count[jal.Jack] + serialInput[sI][1].toInt(), jal.count[jal.Jack] + serialInput[sI][2].toInt(), serialInput[sI][3].toInt());
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
