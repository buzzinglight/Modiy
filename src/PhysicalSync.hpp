#ifndef PHYSICALSYNC_H
#define PHYSICALSYNC_H

//Internal
#include "AudioInterfaceIO.hpp"
#include "OSCManagement.hpp"
#include "Modiy.hpp"

//Process
#include "process/process.hpp"

//Divers
#include <iostream>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <algorithm>

//Classes needed for module caching
class LED {
public:
    Light light;
    MultiLightWidget *widget = NULL;
};
class JackInput {
public:
    Input input;
    Port *port = NULL;
};
class JackOutput {
public:
    Output output;
    Port *port = NULL;
};

class JackWire {
public:
    WireWidget *widget = NULL;
    int inputModuleId  = -1;
    int outputModuleId = -1;
};
class ModuleWithId {
public:
    ModuleWidget *widget = NULL;
    std::vector<LED> leds;
    std::vector<ParamWidget*> potentiometers, switches;
    std::vector<JackInput> inputs;
    std::vector<JackOutput> outputs;
    int moduleId = -1;
};

//LED Widget variable helper
class LEDvars {
public:
    float value = 0, valueRounded = 0, valueRoundedOld = -1;
public:
    bool blink(float deltaTime, float threshold = 0.5);
};

//Menu item
struct PhysicalSync;
struct PhysicalSyncOSCMenu : MenuItem {
public:
    PhysicalSync *physicalSync;
    const char *oscMessage;

public:
    void onAction(EventAction &e) override;
};
struct PhysicalSyncAudioMenu : MenuItem {
public:
    PhysicalSync *physicalSync;
    int nbChannels;

public:
    void onAction(EventAction &e) override;
};

//Main class
struct PhysicalSync : Module, OSCRemote {
public:
    //Interfaces
    enum ParamIds {
        NUM_PARAMS
    };
    enum InputIds {
        ENUMS(AUDIO_INPUT, AUDIO_INPUTS),
        NUM_INPUTS
    };
    enum OutputIds {
        ENUMS(AUDIO_OUTPUT, AUDIO_OUTPUTS),
        NUM_OUTPUTS
    };
    enum LightIds {
        OSC_LED_INT,
        OSC_LED_EXT,
        NUM_LIGHTS
    };

//VCV-Rack base functions
public:
    PhysicalSync();
    ~PhysicalSync();

    //(De)serialization
    json_t *toJson() override;
    void fromJson(json_t *rootJ) override;
    //Reset
    void onReset() override;
    //Audio step
    void step() override;
    //Other functions
    //void onSampleRateChange() override;
    //void onRandomize() override;
    //void onCreate() override;
    //void onDelete() override;

public:
    //Widgets
    MultiLightWidget *oscLEDInt = NULL, *oscLEDExt = NULL;
    //Audio interface
    AudioInterfaceIO audioIO;
    //OSC manager
    OSCManagement *osc = NULL;
    //Audio channels + fake plugs
    std::vector<SVGWidget*> inputComponents, outputComponents;
    ModuleWidget *widget;
    void setChannels(int nbChannels);
    inline unsigned int getChannels() { return currentNbChannels; }

private:
    //Audio
    SampleRateConverter<AUDIO_INPUTS>          inputSrc;
    SampleRateConverter<AUDIO_OUTPUTS>         outputSrc;
    DoubleRingBuffer<Frame<AUDIO_INPUTS>, 16>  inputBuffer;
    DoubleRingBuffer<Frame<AUDIO_OUTPUTS>, 16> outputBuffer;
    unsigned int currentNbChannels = min(AUDIO_INPUTS, AUDIO_OUTPUTS);

    //LED variables
    LEDvars ledStatusInt, pingLED;
    std::string protocolDispatcherPath;
    bool isProtocolDispatcherFound = false, isProtocolDispatcherTalking = false;
    int isProtocolDispatcherTalkingCounter = 0;

    //OSC listener and sender
    bool oscServerJustStart = false;
    int oscPort = 57130;
    void startOSCServer();
    inline void logToOsc(std::string message);

    //Cache management
    std::vector<ModuleWithId> modules;
    std::vector<JackWire> wires;
    bool updateCacheNeeded = true;
    void updateCache(bool force = false);

    //Getters
    JackInput    getInputPort    (unsigned int moduleId, unsigned int inputId);
    JackOutput   getOutputPort   (unsigned int moduleId, unsigned int outputId);
    ParamWidget* getPotentiometer(unsigned int moduleId, unsigned int potentiometerId);
    ParamWidget* getSwitch       (unsigned int moduleId, unsigned int switchId);
    LED          getLED          (unsigned int moduleId, unsigned int ledId);
    ModuleWithId getModule       (unsigned int moduleId);
    JackWire     getWire         (unsigned int inputModuleId, unsigned int inputPortId, unsigned int outputModuleId, unsigned int outputPortId);

//OSC Remote methods
public:
    //Setters
    void setPotentiometer(unsigned int moduleId, unsigned int potentiometerId, float potentiometerValue, bool isNormalized = false, bool additive = false) override;
    void setConnection(unsigned int outputModuleId, unsigned int outputPortId, unsigned int inputModuleId, unsigned int inputPortId, bool active) override;
    void setSwitch(unsigned int moduleId, unsigned int switchId, float switchValue) override;
    void clearConnection() override;
    void resetPotentiometer(unsigned int moduleId, unsigned int potentiometerId) override;

    //Absolute ID mapping to module (and reverse)
    bool mapFromPotentiometer(unsigned int index, int *moduleId, int *potentiometerId) override;
    int  mapToPotentiometer(unsigned int moduleId, unsigned int potentiometerId) override;
    bool mapFromSwitch(unsigned int index, int *moduleId, int *switchId) override;
    int  mapToSwitch(unsigned int moduleId, unsigned int switchId) override;
    bool mapFromJack (unsigned int index, int *moduleId, int *inputOrOutputId, bool *isInput) override;
    int  mapToJack(unsigned int moduleId, unsigned int inputOrOutputId, bool isInput) override;
    int  mapToLED(unsigned int moduleId, unsigned int ledId) override;

    //Dump of data
    void dumpLEDs        (const char *address, bool inLine = false) override;
    void dumpModules       (const char *address) override;
    void dumpPotentiometers(const char *address) override;
    void dumpSwitches      (const char *address) override;
    void dumpJacks         (const char *address) override;

    //Communication
    void send(const char *message) override;
    void pongReceived() override;

public:
    //Sort modules by geometric position (y then x)
    struct modulesWidgetGraphicalSort {
        inline bool operator() (const ModuleWithId &module1, const ModuleWithId &module2) {
            if(module1.widget->box.pos.y == module2.widget->box.pos.y)
                return (module1.widget->box.pos.x < module2.widget->box.pos.x);
            else
                return (module1.widget->box.pos.y < module2.widget->box.pos.y);
        }
    };
};


//Widget
struct PhysicalSyncWidget : ModuleWidget {
public:
    PhysicalSyncWidget(PhysicalSync *module);

public:
    //Additional menus
    void appendContextMenu(Menu *menu);
};



#endif // PHYSICALSYNC_H
