#include "Modiy.hpp"
#include <iostream>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <algorithm>

//Audio
#include <assert.h>
#include <mutex>
#include <chrono>
#include <thread>
#include <mutex>
#include <condition_variable>
#include "audio.hpp"
#include "dsp/resampler.hpp"
#include "dsp/ringbuffer.hpp"
#define AUDIO_OUTPUTS 8
#define AUDIO_INPUTS 8

//Includes for OSC server
#include <thread>
#include "osc/OscReceivedElements.h"
#include "osc/OscPacketListener.h"
#include "osc/OscOutboundPacketStream.h"
#include "ip/UdpSocket.h"

//Websockets
#ifdef WEBSOCKETS_INTERNAL
#include "websockets/uWS.h"
#endif

//Structs needed for module caching
struct LightWithWidget {
    Light light;
    MultiLightWidget *widget = NULL;
};
struct ModuleWithId {
    ModuleWidget *widget = NULL;
    std::vector<LightWithWidget> lights;
    int moduleId = -1;
};
struct WireWithId {
    WireWidget *widget = NULL;
    int inputModuleId  = -1;
    int outputModuleId = -1;
};
//Structs needed for audio
struct AudioInterfaceIO : AudioIO {
    std::mutex engineMutex;
    std::condition_variable engineCv;
    std::mutex audioMutex;
    std::condition_variable audioCv;
    // Audio thread produces, engine thread consumes
    DoubleRingBuffer<Frame<AUDIO_INPUTS>, (1<<15)> inputBuffer;
    // Audio thread consumes, engine thread produces
    DoubleRingBuffer<Frame<AUDIO_OUTPUTS>, (1<<15)> outputBuffer;
    bool active = false;

    ~AudioInterfaceIO() {
        // Close stream here before destructing AudioInterfaceIO, so the mutexes are still valid when waiting to close.
        setDevice(-1, 0);
    }

    void processStream(const float *input, float *output, int frames) override {
        // Reactivate idle stream
        if (!active) {
            active = true;
            inputBuffer.clear();
            outputBuffer.clear();
        }

        if (numInputs > 0) {
            // TODO Do we need to wait on the input to be consumed here? Experimentally, it works fine if we don't.
            for (int i = 0; i < frames; i++) {
                if (inputBuffer.full())
                    break;
                Frame<AUDIO_INPUTS> inputFrame;
                memset(&inputFrame, 0, sizeof(inputFrame));
                memcpy(&inputFrame, &input[numInputs * i], numInputs * sizeof(float));
                inputBuffer.push(inputFrame);
            }
        }

        if (numOutputs > 0) {
            std::unique_lock<std::mutex> lock(audioMutex);
            auto cond = [&] {
                return (outputBuffer.size() >= (size_t) frames);
            };
            auto timeout = std::chrono::milliseconds(100);
            if (audioCv.wait_for(lock, timeout, cond)) {
                // Consume audio block
                for (int i = 0; i < frames; i++) {
                    Frame<AUDIO_OUTPUTS> f = outputBuffer.shift();
                    for (int j = 0; j < numOutputs; j++) {
                        output[numOutputs*i + j] = clamp(f.samples[j], -1.f, 1.f);
                    }
                }
            }
            else {
                // Timed out, fill output with zeros
                memset(output, 0, frames * numOutputs * sizeof(float));
                debug("Audio Interface IO underflow");
            }
        }

        // Notify engine when finished processing
        engineCv.notify_one();
    }

    void onCloseStream() override {
        inputBuffer.clear();
        outputBuffer.clear();
    }

    void onChannelsChange() override {
    }
};


//Main class
class OSCManagement;
struct PhysicalSync : Module {
    //VCV-Rack base functions
    PhysicalSync();
    ~PhysicalSync();
    void step() override;
    json_t *toJson() override;
    void fromJson(json_t *rootJ) override;
    void onReset() override;

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

    //Audio stuff
    AudioInterfaceIO audioIO;
    int lastSampleRate = 0;
    int lastNumOutputs = -1;
    int lastNumInputs  = -1;
    SampleRateConverter<AUDIO_INPUTS>          inputSrc;
    SampleRateConverter<AUDIO_OUTPUTS>         outputSrc;
    DoubleRingBuffer<Frame<AUDIO_INPUTS>, 16>  inputBuffer;
    DoubleRingBuffer<Frame<AUDIO_OUTPUTS>, 16> outputBuffer;


    //Shared variables
    float ledStatusIntPhase = 0.0, ledStatusExtPhase = 0.0, ledStatusIntPhaseOld = -1;

    //OSC listener and sender
    OSCManagement *osc = NULL;
    void startOSCServer();
    inline void logToOsc(std::string message);
    MultiLightWidget *oscLightInt = NULL, *oscLightExt = NULL;

    //Websockets
    void startWebsocketsServer();

    //Cache management
    std::vector<ModuleWithId> modules;
    std::vector<WireWithId> wires;
    bool updateCacheNeeded = true;
    void updateCache(bool force = false);

    //Dump of data
    void dumpLights    (const char *address);
    void dumpModules   (const char *address);
    void dumpParameters(const char *address);
    void dumpJacks     (const char *address);

    //Getters
    Port*        getInputPort (unsigned int moduleId, unsigned int inputId);
    Port*        getOutputPort(unsigned int moduleId, unsigned int outputId);
    ParamWidget* getParameter (unsigned int moduleId, unsigned int paramId);
    LightWithWidget getLight  (unsigned int moduleId, unsigned int lightId);
    ModuleWithId    getModule (unsigned int moduleId);
    WireWithId      getWire   (unsigned int inputModuleId, unsigned int inputPortId, unsigned int outputModuleId, unsigned int outputPortId);

    //Setters
    void setParameter(unsigned int moduleId, unsigned int paramId, float paramValue, bool isNormalized = false, bool additive = false);
    void setConnection(unsigned int outputModuleId, unsigned int outputPortId, unsigned int inputModuleId, unsigned int inputPortId, bool active);
    void clearConnection();
    void resetParameter(unsigned int moduleId, unsigned int paramId);

    //Absolute ID mapping to module (and reverse)
    bool mapFromPotentiometer(unsigned int index, int *moduleId, int *paramId);
    int  mapToPotentiometer(unsigned int moduleId, unsigned int paramId);
    bool mapFromJack (unsigned int index, int *moduleId, int *inputOrOutputId, bool *isInput);
    int  mapToJack(unsigned int moduleId, unsigned int inputOrOutputId, bool isInput);
    int  mapToLED(unsigned int moduleId, unsigned int lightId);

    // For more advanced Module features, read Rack's engine.hpp header file
    // - toJson, fromJson: serialization of internal data
    // - onSampleRateChange: event triggered by a change of sample rate
    // - onReset, onRandomize, onCreate, onDelete: implements special behavior when user clicks these from the context menu
};

//Sort modules by geometric position (y then x)
struct modulesWidgetGraphicalSort {
    inline bool operator() (const ModuleWithId &module1, const ModuleWithId &module2) {
        if(module1.widget->box.pos.y == module2.widget->box.pos.y)
            return (module1.widget->box.pos.x < module2.widget->box.pos.x);
        else
            return (module1.widget->box.pos.y < module2.widget->box.pos.y);
    }
};

//OSC Management
class OSCManagement : public osc::OscPacketListener {
public:
    OSCManagement(PhysicalSync *_physicalSync);

private:
    char buffer[1024];
    PhysicalSync *physicalSync;

public:
    void send(const char *address, std::string message);
    void send(const char *address, int valueIndex, unsigned int moduleId, unsigned int valueId, const Vec &position, float valueAbsolute, float valueNormalized, int extraInfo = -9999);
    void send(const char *address, unsigned int moduleId, std::string slug, std::string name, const Vec &position, const Vec &size, unsigned int nbInputs, unsigned int nbOutputs, unsigned int nbParameters, unsigned int nbLights);
    void send(const char *address, float value);

protected:
    virtual void ProcessMessage(const osc::ReceivedMessage& m, const IpEndpointName& remoteEndpoint);
    float asNumber(const osc::ReceivedMessage::const_iterator &arg) const;
};



//Module initialisation
PhysicalSync::PhysicalSync() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {
    info("Audio initialisation…");
    onSampleRateChange();

    info("OSC server thread creation…");
    std::thread oscThread(&PhysicalSync::startOSCServer, this);
    oscThread.detach();

    info("Websockets server thread creation…");
    std::thread websocketsThread(&PhysicalSync::startWebsocketsServer, this);
    websocketsThread.detach();
}
PhysicalSync::~PhysicalSync() {
}
json_t *PhysicalSync::toJson() {
    json_t *rootJ = json_object();
    json_object_set_new(rootJ, "audio", audioIO.toJson());
    return rootJ;
}

void PhysicalSync::fromJson(json_t *rootJ) {
    json_t *audioJ = json_object_get(rootJ, "audio");
    audioIO.fromJson(audioJ);
}

void PhysicalSync::onReset() {
    audioIO.setDevice(-1, 0);
}




//Update modules, jacks, parameters, LEDs… cache
void PhysicalSync::updateCache(bool force) {
    bool debug = false;
    if((updateCacheNeeded) || (force)) {
        //Order modules by graphical position
        modules.clear();
        for (Widget *w : gRackWidget->moduleContainer->children) {
            ModuleWithId moduleWithId;
            moduleWithId.widget = dynamic_cast<ModuleWidget*>(w);
            if(moduleWithId.widget->module != NULL) {
                if(moduleWithId.widget->module != this) {
                    //Find light widgets
                    std::vector<ModuleLightWidget*> lightsWidget;
                    moduleWithId.lights.clear();
                    for (Widget *w : moduleWithId.widget->children) {
                        try {
                            ModuleLightWidget *lightWidget = dynamic_cast<ModuleLightWidget*>(w);
                            if((lightWidget) && (lightWidget->visible))
                                lightsWidget.push_back(lightWidget);
                        } catch(const std::bad_cast& e) {
                        }
                    }

                    //Associate light with widgets
                    for(unsigned int lightId = 0 ; lightId < moduleWithId.widget->module->lights.size() ; lightId++) {
                        if(lightId < lightsWidget.size()) {
                            LightWithWidget light;
                            light.light  = moduleWithId.widget->module->lights.at(lightId);
                            light.widget = lightsWidget.at(lightId);
                            moduleWithId.lights.push_back(light);
                        }
                    }
                }

                //Add to container
                modules.push_back(moduleWithId);
            }
        }
        //Set an absolute ID
        std::sort(modules.begin(), modules.end(), modulesWidgetGraphicalSort());
        for(unsigned int moduleId = 0 ; moduleId < modules.size() ; moduleId++)
            modules[moduleId].moduleId = moduleId;

        //Store wires linked to modules (not in creation)
        wires.clear();
        for (Widget *w : gRackWidget->wireContainer->children) {
            WireWithId wireWithId;
            wireWithId.widget = dynamic_cast<WireWidget*>(w);
            if(wireWithId.widget->wire != NULL)
                wires.push_back(wireWithId);
        }

        //Show infos on modules
        if(debug)
            info("MODULES");
        for(unsigned int moduleId = 0 ; moduleId < modules.size() ; moduleId++) {
            const ModuleWithId moduleWithId = modules.at(moduleId);
            ModuleWidget *widget = moduleWithId.widget;
            Module *module = widget->module;
            Model  *model  = widget->model;
            if((module) && (model)) {
                if(debug) {
                    info("Module %d : %s %s @ (%f %f) (I/O/P/L : %d %d %d %d)", moduleId, model->slug.c_str(), model->name.c_str(), widget->box.pos.x, widget->box.pos.y, module->inputs.size(), module->outputs.size(), module->params.size(), moduleWithId.lights.size());
                    for(unsigned int inputId = 0 ; inputId < module->inputs.size() ; inputId++)
                        info("Input %d : %d", inputId, module->inputs[inputId].active);
                    for(unsigned int outputId = 0 ; outputId < module->outputs.size() ; outputId++)
                        info("Output %d : %d", outputId, module->outputs[outputId].active);
                    for(unsigned int lightId = 0 ; lightId < moduleWithId.lights.size() ; lightId++)
                        info("Light %d : %f", lightId, moduleWithId.lights[lightId].light.value);
                    for(unsigned int paramId = 0 ; paramId < module->params.size() ; paramId++)
                        info("Param %d : %f", paramId, module->params[paramId].value);
                }
            }
        }

        //Show infos on wires + cache modules references
        if(debug)
            info("WIRES");
        for(unsigned int wireId = 0 ; wireId < wires.size() ; wireId++) {
            wires[wireId].inputModuleId = wires[wireId].outputModuleId = -1;
            ModuleWithId inputModule, outputModule;
            for(unsigned int moduleId = 0 ; moduleId < modules.size() ; moduleId++) {
                ModuleWithId moduleWidgetTest = modules[moduleId];
                if(moduleWidgetTest.widget->module == wires[wireId].widget->wire->inputModule)
                    inputModule = moduleWidgetTest;
                if(moduleWidgetTest.widget->module == wires[wireId].widget->wire->outputModule)
                    outputModule = moduleWidgetTest;
            }
            if((inputModule.widget) && (outputModule.widget)) {
                wires[wireId].inputModuleId  = inputModule.moduleId;
                wires[wireId].outputModuleId = outputModule.moduleId;
                if(debug)
                    info("Wire %d link %d - %s %s (%d) to %d - %s %s (%d)", wireId, wires[wireId].outputModuleId, outputModule.widget->model->slug.c_str(), outputModule.widget->model->name.c_str(), wires[wireId].widget->wire->outputId, wires[wireId].inputModuleId, inputModule.widget->model->slug.c_str(), inputModule.widget->model->name.c_str(), wires[wireId].widget->wire->inputId);
            }
        }

        updateCacheNeeded = false;
    }
}



//Start OSC server
void PhysicalSync::startOSCServer() {
    int port = 57130;
    info("Beginning of OSC server on port %d", port);
    try {
        osc = new OSCManagement(this);
        UdpListeningReceiveSocket socket(IpEndpointName(IpEndpointName::ANY_ADDRESS, port), osc);
        socket.RunUntilSigInt();
        info("End of OSC server on port ", port);
    }
    catch(std::exception e) {
        osc = NULL;
        info("Impossible to bind OSC server on port %d", port);
    }
}
OSCManagement::OSCManagement(PhysicalSync *_physicalSync) {
    physicalSync = _physicalSync;
}
//OSC message processing
void OSCManagement::ProcessMessage(const osc::ReceivedMessage& m, const IpEndpointName& remoteEndpoint) {
    try {
        if(false) {
            info("Reception OSC sur %s avec %d arguments", m.AddressPattern(), m.ArgumentCount());
            osc::ReceivedMessage::const_iterator arg = m.ArgumentsBegin();
            for(int i = 0 ; i < m.ArgumentCount() ; i++)
                info("\t%d = %f", i, asNumber(arg++));
        }
        //Parameters change
        if( (((std::strcmp(m.AddressPattern(), "/parameter/set/absolute") == 0) || (std::strcmp(m.AddressPattern(), "/parameter/set/norm") == 0) || (std::strcmp(m.AddressPattern(), "/parameter/set/norm/10bits") == 0)) ||
             ((std::strcmp(m.AddressPattern(), "/parameter/add/absolute") == 0) || (std::strcmp(m.AddressPattern(), "/parameter/add/norm") == 0) || (std::strcmp(m.AddressPattern(), "/parameter/set/norm/10bits") == 0)) || (std::strcmp(m.AddressPattern(), "/A") == 0)) && (m.ArgumentCount())) {
            osc::ReceivedMessage::const_iterator arg = m.ArgumentsBegin();
            int moduleId = -1, paramId = -1;
            float paramValue = -1;
            if(m.ArgumentCount() == 3) {
                moduleId   = asNumber(arg++);
                paramId    = asNumber(arg++);
                paramValue = asNumber(arg++);
            }
            else if(m.ArgumentCount() == 2) {
                physicalSync->mapFromPotentiometer(asNumber(arg++), &moduleId, &paramId);
                paramValue = asNumber(arg++);
            }
            if (std::strcmp(m.AddressPattern(), "/parameter/set/absolute") == 0)     physicalSync->setParameter(moduleId, paramId, paramValue);
            if (std::strcmp(m.AddressPattern(), "/parameter/set/norm") == 0)         physicalSync->setParameter(moduleId, paramId, paramValue, true);
            if((std::strcmp(m.AddressPattern(), "/parameter/set/norm/10bits") == 0) || (std::strcmp(m.AddressPattern(), "/A") == 0))  physicalSync->setParameter(moduleId, paramId, paramValue/1023., true);
            if (std::strcmp(m.AddressPattern(), "/parameter/add/absolute") == 0)     physicalSync->setParameter(moduleId, paramId, paramValue, false, true);
            if (std::strcmp(m.AddressPattern(), "/parameter/add/norm") == 0)         physicalSync->setParameter(moduleId, paramId, paramValue, true, true);
            if (std::strcmp(m.AddressPattern(), "/parameter/add/norm/10bits") == 0)  physicalSync->setParameter(moduleId, paramId, paramValue/1023., true, true);
        }
        else if((std::strcmp(m.AddressPattern(), "/parameter/reset") == 0) && (m.ArgumentCount())) {
            osc::ReceivedMessage::const_iterator arg = m.ArgumentsBegin();
            int moduleId = -1, paramId = -1;
            if(m.ArgumentCount() == 2) {
                moduleId   = asNumber(arg++);
                paramId    = asNumber(arg++);
            }
            else if(m.ArgumentCount() == 1)
                physicalSync->mapFromPotentiometer(asNumber(arg++), &moduleId, &paramId);
            physicalSync->resetParameter(moduleId, paramId);
        }

        //Wires change
        else if(((std::strcmp(m.AddressPattern(), "/link") == 0) || (std::strcmp(m.AddressPattern(), "/J") == 0)) && (m.ArgumentCount())) {
            osc::ReceivedMessage::const_iterator arg = m.ArgumentsBegin();
            int outputModuleId = -1, outputId = -1, inputModuleId = -1, inputId = -1;
            bool active = false;
            if(m.ArgumentCount() == 5) {
                outputModuleId = asNumber(arg++);
                outputId       = asNumber(arg++);
                inputModuleId  = asNumber(arg++);
                inputId        = asNumber(arg++);
                active         = asNumber(arg++);
            }
            else if(m.ArgumentCount() == 3) {
                bool firstIsInput = false, secondIsInput = false;;
                physicalSync->mapFromJack(asNumber(arg++), &outputModuleId, &outputId, &firstIsInput);
                physicalSync->mapFromJack(asNumber(arg++), &inputModuleId,  &inputId,  &secondIsInput);
                if((firstIsInput) && (!secondIsInput)) {
                    info("Inversion des paramètres : output —> input");
                    int outputModuleIdTmp = outputModuleId, outputIdTmp = outputId;
                    outputModuleId = inputModuleId;     outputId = inputId;     firstIsInput  = !firstIsInput;
                    inputModuleId  = outputModuleIdTmp; inputId  = outputIdTmp; secondIsInput = !secondIsInput;
                }
                active = asNumber(arg++);
            }
            physicalSync->setConnection(outputModuleId, outputId, inputModuleId, inputId, active);
        }
        else if(std::strcmp(m.AddressPattern(), "/link/clear") == 0)
            physicalSync->clearConnection();

        //Dump asked
        else if(std::strcmp(m.AddressPattern(), "/dump/modules") == 0)
            physicalSync->dumpModules(m.AddressPattern());
        else if(std::strcmp(m.AddressPattern(), "/dump/leds") == 0)
            physicalSync->dumpLights(m.AddressPattern());
        else if(std::strcmp(m.AddressPattern(), "/dump/potentiometers") == 0)
            physicalSync->dumpParameters(m.AddressPattern());
        else if(std::strcmp(m.AddressPattern(), "/dump/jacks") == 0)
            physicalSync->dumpJacks(m.AddressPattern());

    } catch(osc::Exception &e) {
        warn("Erreur %s / %s", m.AddressPattern(), e.what());
    }
}
//Convert any number (int, float…) into float of convenience
float OSCManagement::asNumber(const osc::ReceivedMessage::const_iterator &arg) const {
    if     (arg->IsFloat())     return arg->AsFloat();
    else if(arg->IsInt32())     return arg->AsInt32();
    else if(arg->IsInt64())     return arg->AsInt64();
    return -1;
}

//OSC send
void OSCManagement::send(const char *address, std::string message) {
    UdpTransmitSocket transmitSocket(IpEndpointName("127.0.0.1", 4001));
    osc::OutboundPacketStream packet(buffer, 1024);

    packet << osc::BeginMessage(address) << message.c_str() << osc::EndMessage;
    transmitSocket.Send(packet.Data(), packet.Size());
}
void OSCManagement::send(const char *address, int valueIndex, unsigned int moduleId, unsigned int valueId, const Vec &position, float valueAbsolute, float valueNormalized, int extraInfo) {
    UdpTransmitSocket transmitSocket(IpEndpointName("127.0.0.1", 4001));
    osc::OutboundPacketStream packet(buffer, 1024);

    packet << osc::BeginMessage(address) << valueIndex << (int)moduleId << (int)valueId << position.x << position.y << valueAbsolute << valueNormalized;
    if(extraInfo != -9999)
        packet << extraInfo;
    packet << osc::EndMessage;
    transmitSocket.Send(packet.Data(), packet.Size());
}
void OSCManagement::send(const char *address, unsigned int moduleId, std::string slug, std::string name, const Vec &position, const Vec &size, unsigned int nbInputs, unsigned int nbOutputs, unsigned int nbParameters, unsigned int nbLights) {
    UdpTransmitSocket transmitSocket(IpEndpointName("127.0.0.1", 4001));
    osc::OutboundPacketStream packet(buffer, 1024);

    packet << osc::BeginMessage(address) << (int)moduleId << slug.c_str() << name.c_str() << position.x << position.y << size.x << size.y  << (int)nbInputs << (int)nbOutputs << (int)nbParameters << (int)nbLights << osc::EndMessage;
    transmitSocket.Send(packet.Data(), packet.Size());
}
void OSCManagement::send(const char *address, float value) {
    UdpTransmitSocket transmitSocket(IpEndpointName("127.0.0.1", 4001));
    osc::OutboundPacketStream packet(buffer, 1024);

    packet << osc::BeginMessage(address) << value << osc::EndMessage;
    transmitSocket.Send(packet.Data(), packet.Size());
}
//Log in console + through OSC
void PhysicalSync::logToOsc(std::string message) {
    if(osc)
        osc->send("/log", message);
    info("%s", message.c_str());
}


//Start Websocket server
void PhysicalSync::startWebsocketsServer() {
#ifdef WEBSOCKETS_INTERNAL
    int port = 9002;
    info("Starting of Websockets server on port %d", port);

    uWS::Hub h;
    h.onMessage([](uWS::WebSocket<uWS::SERVER> *ws, char *message_char, size_t length, uWS::OpCode opCode) {
        std::string message = std::string(message_char, length);
        info("Réception de %s (%d / %d)", message.c_str(), length, opCode);
        //ws->send(message, length, opCode);
    });

    if (h.listen(port)) {
        h.run();
        info("End of Websockets server on port ", port);
    }
    else
        info("Impossible to bind Websockets server on port %d", port);
#endif
}




//Time is running in VCV Rack…
void PhysicalSync::step() {
    float deltaTime = engineGetSampleTime();

    //Data sync
    updateCacheNeeded = true;
    if(osc) {
        ledStatusIntPhase += deltaTime;
        if (ledStatusIntPhase >= 1)
            ledStatusIntPhase = 0;
        lights[OSC_LIGHT_INT].value = (ledStatusIntPhase > 0.5)?(1):(0);
        if(ledStatusIntPhaseOld != lights[OSC_LIGHT_INT].value) {
            ledStatusIntPhaseOld = lights[OSC_LIGHT_INT].value;
                osc->send("/pulse", ledStatusIntPhaseOld);
        }
        if((oscLightInt) && (oscLightInt->visible == false))
            oscLightInt->visible = true;
        if((oscLightExt) && (oscLightExt->visible == false))
            oscLightExt->visible = true;

    }

    // Update SRC states
    int sampleRate = (int)engineGetSampleRate();
    inputSrc .setRates(audioIO.sampleRate, sampleRate);
    outputSrc.setRates(sampleRate, audioIO.sampleRate);

    inputSrc .setChannels(audioIO.numInputs);
    outputSrc.setChannels(audioIO.numOutputs);

    // Inputs: audio engine -> rack engine
    if (audioIO.active && audioIO.numInputs > 0) {
        // Wait until inputs are present
        // Give up after a timeout in case the audio device is being unresponsive.
        std::unique_lock<std::mutex> lock(audioIO.engineMutex);
        auto cond = [&] {
            return (!audioIO.inputBuffer.empty());
        };
        auto timeout = std::chrono::milliseconds(200);
        if (audioIO.engineCv.wait_for(lock, timeout, cond)) {
            // Convert inputs
            int inLen = audioIO.inputBuffer.size();
            int outLen = inputBuffer.capacity();
            inputSrc.process(audioIO.inputBuffer.startData(), &inLen, inputBuffer.endData(), &outLen);
            audioIO.inputBuffer.startIncr(inLen);
            inputBuffer.endIncr(outLen);
        }
        else {
            // Give up on pulling input
            audioIO.active = false;
            debug("Audio Interface underflow");
        }
    }

    // Take input from buffer
    Frame<AUDIO_INPUTS> inputFrame;
    if (!inputBuffer.empty()) {
        inputFrame = inputBuffer.shift();
    }
    else {
        memset(&inputFrame, 0, sizeof(inputFrame));
    }
    for (int i = 0; i < audioIO.numInputs; i++) {
        outputs[AUDIO_OUTPUT + i].value = 10.f * inputFrame.samples[i];
    }
    for (int i = audioIO.numInputs; i < AUDIO_INPUTS; i++) {
        outputs[AUDIO_OUTPUT + i].value = 0.f;
    }

    // Outputs: rack engine -> audio engine
    if (audioIO.active && audioIO.numOutputs > 0) {
        // Get and push output SRC frame
        if (!outputBuffer.full()) {
            Frame<AUDIO_OUTPUTS> outputFrame;
            for (int i = 0; i < AUDIO_OUTPUTS; i++) {
                outputFrame.samples[i] = inputs[AUDIO_INPUT + i].value / 10.f;
            }
            outputBuffer.push(outputFrame);
        }

        if (outputBuffer.full()) {
            // Wait until enough outputs are consumed
            // Give up after a timeout in case the audio device is being unresponsive.
            std::unique_lock<std::mutex> lock(audioIO.engineMutex);
            auto cond = [&] {
                return (audioIO.outputBuffer.size() < (size_t) audioIO.blockSize);
            };
            auto timeout = std::chrono::milliseconds(200);
            if (audioIO.engineCv.wait_for(lock, timeout, cond)) {
                // Push converted output
                int inLen = outputBuffer.size();
                int outLen = audioIO.outputBuffer.capacity();
                outputSrc.process(outputBuffer.startData(), &inLen, audioIO.outputBuffer.endData(), &outLen);
                outputBuffer.startIncr(inLen);
                audioIO.outputBuffer.endIncr(outLen);
            }
            else {
                // Give up on pushing output
                audioIO.active = false;
                outputBuffer.clear();
                debug("Audio Interface underflow");
            }
        }

        // Notify audio thread that an output is potentially ready
        audioIO.audioCv.notify_one();
    }
}

//Getters
ParamWidget* PhysicalSync::getParameter(unsigned int moduleId, unsigned int paramId) {
    ModuleWithId module = getModule(moduleId);
    if(module.widget) {
        if(paramId < module.widget->params.size())
            return module.widget->params.at(paramId);
    }
    return 0;
}
Port* PhysicalSync::getInputPort(unsigned int moduleId, unsigned int inputId) {
    ModuleWithId module = getModule(moduleId);
    if(module.widget) {
        if(inputId < module.widget->inputs.size())
            return module.widget->inputs.at(inputId);
    }
    return 0;
}
Port* PhysicalSync::getOutputPort(unsigned int moduleId, unsigned int outputId) {
    ModuleWithId module = getModule(moduleId);
    if(module.widget) {
        if(outputId < module.widget->outputs.size())
            return module.widget->outputs.at(outputId);
    }
    return 0;
}
ModuleWithId PhysicalSync::getModule(unsigned int moduleId) {
    updateCache();
    if(moduleId < modules.size())
        return modules.at(moduleId);
    return ModuleWithId();
}
LightWithWidget PhysicalSync::getLight(unsigned int moduleId, unsigned int lightId) {
    ModuleWithId module = getModule(moduleId);
    if(module.widget) {
        if(lightId < module.lights.size())
            return module.lights.at(lightId);
    }
    return LightWithWidget();
}
WireWithId PhysicalSync::getWire(unsigned int inputModuleId, unsigned int inputPortId, unsigned int outputModuleId, unsigned int outputPortId) {
    Port *input  = getInputPort (inputModuleId,  inputPortId);
    Port *output = getOutputPort(outputModuleId, outputPortId);
    if((input) && (output))
        for(unsigned int wireId = 0 ; wireId < wires.size() ; wireId++)
            if((wires.at(wireId).inputModuleId == (int)inputModuleId) && (wires.at(wireId).outputModuleId == (int)outputModuleId))
                if((wires.at(wireId).widget->inputPort->portId == (int)inputPortId) && (wires.at(wireId).widget->outputPort->portId == (int)outputPortId))
                    return wires.at(wireId);
    return WireWithId();
}


//Absolute ID mapping to module (and reverse)
bool PhysicalSync::mapFromPotentiometer(unsigned int index, int *moduleId, int *paramId) {
    updateCache();
    Module *lastModule = 0;
    *moduleId = 0;
    *paramId  = 0;

    //Search for index overflow in modules
    for(unsigned int lastModuleId = 0 ; lastModuleId < modules.size() ; lastModuleId++) {
        lastModule = modules.at(lastModuleId).widget->module;
        if(index >= lastModule->params.size()) {
            index -= lastModule->params.size();
            *moduleId = lastModuleId+1;
        }
        else
            break;
    }

    //Add rest of index to paramId
    if(lastModule) {
        *paramId = index;
        return true;
    }
    else {
        warn("Error with mapFromPotentiometer");
        return false;
    }
}
bool PhysicalSync::mapFromJack(unsigned int index, int *moduleId, int *inputOrOutputId, bool *isInput) {
    updateCache();
    Module *lastModule = 0;
    *isInput         = true;
    *moduleId        = 0;
    *inputOrOutputId = 0;

    //Search for index overflow in modules
    for(unsigned int lastModuleId = 0 ; lastModuleId < modules.size() ; lastModuleId++) {
        lastModule = modules.at(lastModuleId).widget->module;
        if(index  >= (lastModule->inputs.size()+lastModule->outputs.size())) {
            index -= (lastModule->inputs.size()+lastModule->outputs.size());
            *moduleId = lastModuleId+1;
        }
        else
            break;
    }

    //Add rest of index to inputOrOutputId
    if(lastModule) {
        if(index >= lastModule->inputs.size()) {
            *isInput = false;
            *inputOrOutputId = index - lastModule->inputs.size();
        }
        else
            *inputOrOutputId = index;
        return true;
    }
    else {
        warn("Error with mapFromJack");
        return false;
    }
}
int PhysicalSync::mapToLED(unsigned int moduleId, unsigned int lightId) {
    updateCache();
    LightWithWidget lightWithWidget = getLight(moduleId, lightId);
    int index = -1;
    if(lightWithWidget.widget) {
        index = 0;

        //Look for all modules before
        for(unsigned int lastModuleId = 0 ; lastModuleId < moduleId ; lastModuleId++)
            index += modules.at(lastModuleId).lights.size();
        index += lightId;
    }
    else
        warn("Error with mapToLED");
    return index;
}
int PhysicalSync::mapToPotentiometer(unsigned int moduleId, unsigned int paramId) {
    updateCache();
    ParamWidget *param = getParameter(moduleId, paramId);
    int index = -1;
    if(param) {
        index = 0;

        //Look for all modules before
        for(unsigned int lastModuleId = 0 ; lastModuleId < moduleId ; lastModuleId++)
            index += modules.at(lastModuleId).widget->module->params.size();
        index += paramId;
    }
    else
        warn("Error with mapToPotentiometer");
    return index;
}
int PhysicalSync::mapToJack(unsigned int moduleId, unsigned int inputOrOutputId, bool isInput) {
    updateCache();
    Port *port;
    if(isInput) port = getInputPort (moduleId, inputOrOutputId);
    else        port = getOutputPort(moduleId, inputOrOutputId);
    int index = -1;
    if(port) {
        index = 0;

        //Look for all modules before
        for(unsigned int lastModuleId = 0 ; lastModuleId < moduleId ; lastModuleId++)
            index += (modules.at(lastModuleId).widget->module->inputs.size()+modules.at(lastModuleId).widget->module->outputs.size());

        //Add input size offset if it's an output
        if(!isInput)
            index += modules.at(moduleId).widget->module->inputs.size();

        index += inputOrOutputId;
    }
    else
        warn("Error with mapToJack");
    return index;
}


//Setters
void PhysicalSync::setParameter(unsigned int moduleId, unsigned int paramId, float paramValue, bool isNormalized, bool additive) {
    ParamWidget *param = getParameter(moduleId, paramId);
    if(param) {
        if(isNormalized) {
            paramValue = paramValue * (param->maxValue - param->minValue);
            if(!additive)
                paramValue += param->minValue;
        }
        if(additive)
            paramValue += param->value;
        param->setValue(paramValue);
    }
    else {
        std::stringstream stream;
        stream << "Module ";
        stream << moduleId;
        stream << " parameter ";
        stream << paramId;
        stream << " not found.";
        logToOsc(stream.str());
    }
}
void PhysicalSync::resetParameter(unsigned int moduleId, unsigned int paramId) {
    ParamWidget *param = getParameter(moduleId, paramId);
    if(param) {
        param->setValue(param->defaultValue);
    }
    else {
        std::stringstream stream;
        stream << "Module ";
        stream << moduleId;
        stream << "parameter ";
        stream << paramId;
        stream << " not found.";
        logToOsc(stream.str());
    }
}
void PhysicalSync::setConnection(unsigned int outputModuleId, unsigned int outputPortId, unsigned int inputModuleId, unsigned int inputPortId, bool active) {
    std::stringstream stream;
    stream << "Wiring ";
    stream << outputModuleId;
    stream << " ";
    stream << outputPortId;
    stream << " -> ";
    stream << inputModuleId;
    stream << " ";
    stream << inputPortId;
    stream << " (";
    stream << active;
    stream << ") : ";
    std::string logTextBase = stream.str();

    WireWithId wire = getWire(inputModuleId, inputPortId, outputModuleId, outputPortId);
    if((!wire.widget) && (active)) {
        Port *output = getOutputPort(outputModuleId, outputPortId);
        Port *input  = getInputPort (inputModuleId,  inputPortId);
        if((input) && (output)) {
            Input in = modules.at(inputModuleId).widget->module->inputs.at(inputPortId);

            //Test if input is inactive
            if(!in.active) {
                //Créé le lien
                logToOsc(logTextBase + "creation");
                WireWidget *wireWidget = new WireWidget();
                wireWidget->inputPort  = input;
                wireWidget->outputPort = output;
                wireWidget->updateWire();
                gRackWidget->wireContainer->addChild(wireWidget);
                updateCache(true);
            }
            else
                logToOsc(logTextBase + "input is already busy");
        }
        else
            logToOsc(logTextBase + "input or output not found");
    }
    else if((wire.widget) && (!active)) {
        logToOsc(logTextBase + "removed");
        wire.widget->inputPort = wire.widget->outputPort = NULL;
        wire.widget->updateWire();
        updateCache(true);
    }
    else
        logToOsc(logTextBase + "no action done");
}
void PhysicalSync::clearConnection() {
    updateCache();
    for(unsigned int moduleId = 0 ; moduleId < modules.size() ; moduleId++) {
        for(unsigned int inputId = 0 ; inputId < modules[moduleId].widget->inputs.size() ; inputId++)
            gRackWidget->wireContainer->removeAllWires(modules[moduleId].widget->inputs.at(inputId));
        for(unsigned int ouputId = 0 ; ouputId < modules[moduleId].widget->outputs.size() ; ouputId++)
            gRackWidget->wireContainer->removeAllWires(modules[moduleId].widget->outputs.at(ouputId));
    }
    updateCache();
}


//Dump
void PhysicalSync::dumpLights(const char *address) {
    ledStatusExtPhase = 1 - ledStatusExtPhase;
    lights[OSC_LIGHT_EXT].value = ledStatusExtPhase;
    if(osc) {
        updateCache();
        osc->send(address, "start");
        for(unsigned int moduleId = 0 ; moduleId < modules.size() ; moduleId++) {
            ModuleWithId module = getModule(moduleId);
            if(module.widget) {
                for(unsigned int lightId = 0 ; lightId < module.lights.size() ; lightId++) {
                    LightWithWidget lightWithWidget = getLight(moduleId, lightId);
                    if(lightWithWidget.widget)
                        osc->send(address, mapToLED(moduleId, lightId), moduleId, lightId, lightWithWidget.widget->box.getCenter(), lightWithWidget.light.getBrightness(), lightWithWidget.light.value);
                }
            }
        }
        osc->send(address, "end");
    }
}
void PhysicalSync::dumpParameters(const char *address) {
    if(osc) {
        updateCache();
        osc->send(address, "start");
        for(unsigned int moduleId = 0 ; moduleId < modules.size() ; moduleId++) {
            ModuleWithId module = getModule(moduleId);
            if(module.widget) {
                for(unsigned int paramId = 0 ; paramId < modules[moduleId].widget->module->params.size() ; paramId++) {
                    ParamWidget *param = getParameter(moduleId, paramId);
                    if(param)
                        osc->send(address, mapToPotentiometer(moduleId, paramId), moduleId, paramId, param->box.getCenter(), param->value, (param->value - param->minValue) / (param->maxValue - param->minValue));
                }
            }
        }
        osc->send(address, "end");
    }
}
void PhysicalSync::dumpJacks(const char *address) {
    if(osc) {
        updateCache();
        osc->send(address, "start");
        for(unsigned int moduleId = 0 ; moduleId < modules.size() ; moduleId++) {
            ModuleWithId module = getModule(moduleId);
            if(module.widget) {
                for(unsigned int inputId = 0 ; inputId < modules[moduleId].widget->module->inputs.size() ; inputId++) {
                    Port *inputPort = getInputPort(moduleId, inputId);
                    if(inputPort) {
                        Input input = modules[moduleId].widget->module->inputs.at(inputId);
                        osc->send(address, mapToJack(moduleId, inputId, true), moduleId, inputId, inputPort->box.getCenter(), input.active, input.value, 1);
                    }
                }
                for(unsigned int ouputId = 0 ; ouputId < modules[moduleId].widget->module->outputs.size() ; ouputId++) {
                    Port *ouputPort = getOutputPort(moduleId, ouputId);
                    if(ouputPort) {
                        Output output = modules[moduleId].widget->module->outputs.at(ouputId);
                        osc->send(address, mapToJack(moduleId, ouputId, false), moduleId, ouputId, ouputPort->box.getCenter(), output.active, output.value, 0);
                    }
                }
            }
        }
        osc->send(address, "end");
    }
}
void PhysicalSync::dumpModules(const char *address) {
    if(osc) {
        updateCache();
        osc->send(address, "start");
        for(unsigned int moduleId = 0 ; moduleId < modules.size() ; moduleId++) {
            ModuleWithId module = getModule(moduleId);
            if(module.widget)
                osc->send(address, moduleId, module.widget->model->slug, module.widget->model->name, module.widget->box.pos, module.widget->box.size, module.widget->module->inputs.size(), module.widget->module->outputs.size(), module.widget->module->params.size(), module.lights.size());
        }
        osc->send(address, "end");
    }
}




//Module widget
struct PhysicalSyncWidget : ModuleWidget {
    PhysicalSyncWidget(PhysicalSync *module);
};
PhysicalSyncWidget::PhysicalSyncWidget(PhysicalSync *module) : ModuleWidget(module) {
    setPanel(SVG::load(assetPlugin(plugin, "res/PhysicalSync.svg")));
    SVGWidget *component;
    float y = 44;

    //Screws
    addChild(Widget::create<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
    addChild(Widget::create<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
    addChild(Widget::create<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
    addChild(Widget::create<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

    //Data sync
    module->oscLightInt = ModuleLightWidget::create<SmallLight<RedLight>>   (Vec(RACK_GRID_WIDTH+4, 18.75)                 , module, PhysicalSync::OSC_LIGHT_INT);
    module->oscLightExt = ModuleLightWidget::create<SmallLight<YellowLight>>(Vec(box.size.x - 2 * RACK_GRID_WIDTH+4, 18.75), module, PhysicalSync::OSC_LIGHT_EXT);
    module->oscLightInt->visible = false;
    module->oscLightExt->visible = false;
    addChild(module->oscLightInt);
    addChild(module->oscLightExt);

    //Audio sync
    //Input port (to go out on sound cart)
    addInput(Port::create<PJ301MPort>(mm2px(Vec(3.5, y+10*0)), Port::INPUT, module, PhysicalSync::AUDIO_INPUT + 0));
    addInput(Port::create<PJ301MPort>(mm2px(Vec(3.5, y+10*1)), Port::INPUT, module, PhysicalSync::AUDIO_INPUT + 1));
    addInput(Port::create<PJ301MPort>(mm2px(Vec(3.5, y+10*2)), Port::INPUT, module, PhysicalSync::AUDIO_INPUT + 2));
    addInput(Port::create<PJ301MPort>(mm2px(Vec(3.5, y+10*3)), Port::INPUT, module, PhysicalSync::AUDIO_INPUT + 3));
    addInput(Port::create<PJ301MPort>(mm2px(Vec(3.5, y+10*4)), Port::INPUT, module, PhysicalSync::AUDIO_INPUT + 4));
    addInput(Port::create<PJ301MPort>(mm2px(Vec(3.5, y+10*5)), Port::INPUT, module, PhysicalSync::AUDIO_INPUT + 5));
    addInput(Port::create<PJ301MPort>(mm2px(Vec(3.5, y+10*6)), Port::INPUT, module, PhysicalSync::AUDIO_INPUT + 6));
    addInput(Port::create<PJ301MPort>(mm2px(Vec(3.5, y+10*7)), Port::INPUT, module, PhysicalSync::AUDIO_INPUT + 7));

    //Fake output virtual port (but physically are real)
    component = new SVGWidget(); component->setSVG(SVG::load(assetGlobal("res/ComponentLibrary/CL1362.svg"))); component->box.pos = mm2px(Vec(12.5, (y-1)+10*0)); addChild(component);
    component = new SVGWidget(); component->setSVG(SVG::load(assetGlobal("res/ComponentLibrary/CL1362.svg"))); component->box.pos = mm2px(Vec(12.5, (y-1)+10*1)); addChild(component);
    component = new SVGWidget(); component->setSVG(SVG::load(assetGlobal("res/ComponentLibrary/CL1362.svg"))); component->box.pos = mm2px(Vec(12.5, (y-1)+10*2)); addChild(component);
    component = new SVGWidget(); component->setSVG(SVG::load(assetGlobal("res/ComponentLibrary/CL1362.svg"))); component->box.pos = mm2px(Vec(12.5, (y-1)+10*3)); addChild(component);
    component = new SVGWidget(); component->setSVG(SVG::load(assetGlobal("res/ComponentLibrary/CL1362.svg"))); component->box.pos = mm2px(Vec(12.5, (y-1)+10*4)); addChild(component);
    component = new SVGWidget(); component->setSVG(SVG::load(assetGlobal("res/ComponentLibrary/CL1362.svg"))); component->box.pos = mm2px(Vec(12.5, (y-1)+10*5)); addChild(component);
    component = new SVGWidget(); component->setSVG(SVG::load(assetGlobal("res/ComponentLibrary/CL1362.svg"))); component->box.pos = mm2px(Vec(12.5, (y-1)+10*6)); addChild(component);
    component = new SVGWidget(); component->setSVG(SVG::load(assetGlobal("res/ComponentLibrary/CL1362.svg"))); component->box.pos = mm2px(Vec(12.5, (y-1)+10*7)); addChild(component);

    //Fake input virtual port (but physically are real)
    component = new SVGWidget(); component->setSVG(SVG::load(assetGlobal("res/ComponentLibrary/CL1362.svg"))); component->box.pos = mm2px(Vec(27, (y-1)+10*0)); addChild(component);
    component = new SVGWidget(); component->setSVG(SVG::load(assetGlobal("res/ComponentLibrary/CL1362.svg"))); component->box.pos = mm2px(Vec(27, (y-1)+10*1)); addChild(component);
    component = new SVGWidget(); component->setSVG(SVG::load(assetGlobal("res/ComponentLibrary/CL1362.svg"))); component->box.pos = mm2px(Vec(27, (y-1)+10*2)); addChild(component);
    component = new SVGWidget(); component->setSVG(SVG::load(assetGlobal("res/ComponentLibrary/CL1362.svg"))); component->box.pos = mm2px(Vec(27, (y-1)+10*3)); addChild(component);
    component = new SVGWidget(); component->setSVG(SVG::load(assetGlobal("res/ComponentLibrary/CL1362.svg"))); component->box.pos = mm2px(Vec(27, (y-1)+10*4)); addChild(component);
    component = new SVGWidget(); component->setSVG(SVG::load(assetGlobal("res/ComponentLibrary/CL1362.svg"))); component->box.pos = mm2px(Vec(27, (y-1)+10*5)); addChild(component);
    component = new SVGWidget(); component->setSVG(SVG::load(assetGlobal("res/ComponentLibrary/CL1362.svg"))); component->box.pos = mm2px(Vec(27, (y-1)+10*6)); addChild(component);
    component = new SVGWidget(); component->setSVG(SVG::load(assetGlobal("res/ComponentLibrary/CL1362.svg"))); component->box.pos = mm2px(Vec(27, (y-1)+10*7)); addChild(component);

    //Output port (to go in on sound cart)
    addOutput(Port::create<PJ301MPort>(mm2px(Vec(39, y+10*0)), Port::OUTPUT, module, PhysicalSync::AUDIO_OUTPUT + 0));
    addOutput(Port::create<PJ301MPort>(mm2px(Vec(39, y+10*1)), Port::OUTPUT, module, PhysicalSync::AUDIO_OUTPUT + 1));
    addOutput(Port::create<PJ301MPort>(mm2px(Vec(39, y+10*2)), Port::OUTPUT, module, PhysicalSync::AUDIO_OUTPUT + 2));
    addOutput(Port::create<PJ301MPort>(mm2px(Vec(39, y+10*3)), Port::OUTPUT, module, PhysicalSync::AUDIO_OUTPUT + 3));
    addOutput(Port::create<PJ301MPort>(mm2px(Vec(39, y+10*4)), Port::OUTPUT, module, PhysicalSync::AUDIO_OUTPUT + 4));
    addOutput(Port::create<PJ301MPort>(mm2px(Vec(39, y+10*5)), Port::OUTPUT, module, PhysicalSync::AUDIO_OUTPUT + 5));
    addOutput(Port::create<PJ301MPort>(mm2px(Vec(39, y+10*6)), Port::OUTPUT, module, PhysicalSync::AUDIO_OUTPUT + 6));
    addOutput(Port::create<PJ301MPort>(mm2px(Vec(39, y+10*7)), Port::OUTPUT, module, PhysicalSync::AUDIO_OUTPUT + 7));

    //Audio control box
    AudioWidget *audioWidget = Widget::create<AudioWidget>(mm2px(Vec(3.2122073, 10)));
    audioWidget->box.size = mm2px(Vec(45.2, 28));
    audioWidget->audioIO = &module->audioIO;
    addChild(audioWidget);
}


// Specify the Module and ModuleWidget subclass, human-readable
// author name for categorization per plugin, module slug (should never
// change), human-readable module name, and any number of tags
// (found in `include/tags.hpp`) separated by commas.
Model *physicalSync = Model::create<PhysicalSync, PhysicalSyncWidget>("Modiy", "ModiySync", "Physical Sync", UTILITY_TAG);

