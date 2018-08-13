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
#include <dirent.h>
#include <map>
#ifdef ARCH_WIN
    #define realpath(N,R) _fullpath((R),(N),_MAX_PATH)
#endif

//Classes needed for module caching
struct Potentiometer {
    ParamWidget *widget;
};
struct Switch {
    bool isToggle = false, hasLED = false;
    ParamWidget *widget;
};
struct LED {
    Light light;
    MultiLightWidget *widget = NULL;
};
struct Jack {
    Port *port = NULL;
};
struct JackInput : Jack {
    Input input;
};
struct JackOutput : Jack {
    Output output;
};
struct JackWire {
    WireWidget *widget = NULL;
    int inputModuleId  = -1;
    int outputModuleId = -1;
    static std::vector<JackWire> wires;
};
struct Modul {
public:
    static std::vector<std::string> modulesIgnoredStr;
    static std::vector<Modul> modules, modulesWithIgnored;
    static json_t* toJson();
    static void fromJson(json_t *rootJ);

public:
    inline bool isIgnored() const { return (std::find(Modul::modulesIgnoredStr.begin(), Modul::modulesIgnoredStr.end(), name) != Modul::modulesIgnoredStr.end()); }

public:
    ModuleWidget *widget = NULL;
    std::vector<LED> leds;
    std::vector<Potentiometer> potentiometers;
    std::vector<Switch> switches;
    std::vector<JackInput> inputs;
    std::vector<JackOutput> outputs;
    std::string name, panel;
    std::string tmpName;
    int moduleId = -1;
};

//LED Widget variable helper
struct LEDvars {
public:
    float value = 0, valueRounded = 0, valueRoundedOld = -1;
public:
    bool blink(float deltaTime, float threshold = 0.5);
};

//Menu item
struct PhysicalSync;
struct RTBrokerMenu : MenuItem {
    PhysicalSync *physicalSync;
    const char *oscAddress;
    void onAction(EventAction &e) override;
};
struct AudioPresetMenu : MenuItem {
    PhysicalSync *physicalSync;
    int nbChannels;
    void onAction(EventAction &e) override;
};
struct IgnoreModuleMenu : MenuItem {
    PhysicalSync *physicalSync;
    std::string moduleName;
    void onAction(EventAction &e) override;
};

//Main struct
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
    LEDvars ledStatusInt, pingLED, cacheCounter;
    std::string rtBrokerPath;
    bool rtBrokerFound = false, isRTBrokerTalking = false;
    int isRTBrokerTalkingCounter = 0;

    //OSC listener and sender
    bool oscServerJustStart = false;
    int oscPort = 57130;
    void startOSCServer();
    inline void logV(std::string message);

    //Getters
    JackInput     getInputPort    (unsigned int moduleId, unsigned int inputId);
    JackOutput    getOutputPort   (unsigned int moduleId, unsigned int outputId);
    Potentiometer getPotentiometer(unsigned int moduleId, unsigned int potentiometerId);
    Switch        getSwitch       (unsigned int moduleId, unsigned int switchId);
    LED           getLED          (unsigned int moduleId, unsigned int ledId);
    Modul         getModule       (unsigned int moduleId);
    JackWire      getWire         (unsigned int inputModuleId, unsigned int inputPortId, unsigned int outputModuleId, unsigned int outputPortId);

    //Cache management
    float updateCacheLastTime = 0;
    int updateCacheCounter = 0;
public:
    void updateCache(bool force = false);


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
    void dumpLEDs          (const char *address, bool inLine = false) override;
    void dumpModules       (const char *address) override;
    void dumpPotentiometers(const char *address) override;
    void dumpSwitches      (const char *address) override;
    void dumpJacks         (const char *address) override;

    //Communication
    void send(const char *address) override;
    void pongReceived() override;

public:
    //Sort modules by geometric position (y then x)
    struct modulesWidgetGraphicalSort {
        inline bool operator() (const Modul &module1, const Modul &module2) {
            if(module1.widget->box.pos.y == module2.widget->box.pos.y)
                return (module1.widget->box.pos.x < module2.widget->box.pos.x);
            else
                return (module1.widget->box.pos.y < module2.widget->box.pos.y);
        }
    };

private:
    std::map<std::string, std::string> hashCache, panelCache;
public:
    const std::string hash(const std::string &key, const std::string &filename, bool *inCache = 0);
    const std::string hash(const std::string &key, std::shared_ptr<SVG> svgPtr, bool *inCache = 0);
    static std::string findPath(Plugin *plugin, std::string filepath, std::string filename = "");
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
