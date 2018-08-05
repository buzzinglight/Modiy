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
class LightWithWidget {
public:
    Light light;
    MultiLightWidget *widget = NULL;
};
class ModuleWithId {
public:
    ModuleWidget *widget = NULL;
    std::vector<LightWithWidget> lights;
    std::vector<ParamWidget*> potentiometers, switches;
    int moduleId = -1;
};
class WireWithId {
public:
    WireWidget *widget = NULL;
    int inputModuleId  = -1;
    int outputModuleId = -1;
};

//LED light
class LEDvars {
public:
    float value = 0, valueRounded = 0, valueRoundedOld = -1;
public:
    bool blink(float deltaTime, float threshold = 0.5);
};

//Menu item
struct PhysicalSyncMenu : MenuItem {
public:
    OSCRemote *oscRemote;
    const char *oscMessage;

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
        OSC_LIGHT_INT,
        OSC_LIGHT_EXT,
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
    MultiLightWidget *oscLightInt = NULL, *oscLightExt = NULL;
    //Audio interface
    AudioInterfaceIO audioIO;
    //OSC manager
    OSCManagement *osc = NULL;

private:
    //Audio
    SampleRateConverter<AUDIO_INPUTS>          inputSrc;
    SampleRateConverter<AUDIO_OUTPUTS>         outputSrc;
    DoubleRingBuffer<Frame<AUDIO_INPUTS>, 16>  inputBuffer;
    DoubleRingBuffer<Frame<AUDIO_OUTPUTS>, 16> outputBuffer;

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
    std::vector<WireWithId> wires;
    bool updateCacheNeeded = true;
    void updateCache(bool force = false);

    //Getters
    Port*           getInputPort (unsigned int moduleId, unsigned int inputId);
    Port*           getOutputPort(unsigned int moduleId, unsigned int outputId);
    ParamWidget*    getPotentiometer(unsigned int moduleId, unsigned int potentiometerId);
    ParamWidget*    getSwitch       (unsigned int moduleId, unsigned int switchId);
    LightWithWidget getLight     (unsigned int moduleId, unsigned int lightId);
    ModuleWithId    getModule    (unsigned int moduleId);
    WireWithId      getWire      (unsigned int inputModuleId, unsigned int inputPortId, unsigned int outputModuleId, unsigned int outputPortId);

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
    int  mapToLED(unsigned int moduleId, unsigned int lightId) override;

    //Dump of data
    void dumpLights        (const char *address, bool inLine = false) override;
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
