#include "PhysicalSync.hpp"

//Static variables
std::vector<std::string> Modul::modulesIgnoredStr;
std::vector<JackWire> JackWire::wires;
std::vector<Modul> Modul::modules;
std::vector<Modul> Modul::modulesWithIgnored;


//Module initialisation
PhysicalSync::PhysicalSync() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {
    info("Audio initialisation…");
    onSampleRateChange();

    info("OSC server thread creation…");
    std::thread oscThread(&PhysicalSync::startOSCServer, this);
    oscThread.detach();
}
PhysicalSync::~PhysicalSync() {
}

//(De)serialization
json_t *PhysicalSync::toJson() {
    json_t *rootJ = json_object();
    json_object_set_new(rootJ, "audio", audioIO.toJson());
    json_object_set_new(rootJ, "nbChannels", json_integer(getChannels()));
    if(osc)
        json_object_set_new(rootJ, "modules", Modul::toJson());
    return rootJ;
}

void PhysicalSync::fromJson(json_t *rootJ) {
    //Extract
    json_t *audioJ = json_object_get(rootJ, "audio");
    json_t *modulesJ = json_object_get(rootJ, "modules");
    json_t *nbChannels = json_object_get(rootJ, "nbChannels");

    //Set
    if (nbChannels)
        setChannels(json_integer_value(nbChannels));
    if(osc)
        Modul::fromJson(modulesJ);
    audioIO.fromJson(audioJ);

    //Cache update
    updateCache();
}


json_t* Modul::toJson() {
    json_t *rootJ = json_object();
    // ignored
    json_t *ignoresJ = json_array();
    for (unsigned int i = 0 ; i < modulesIgnoredStr.size() ; i++) {
        json_t *ignoreJ = json_string(modulesIgnoredStr.at(i).c_str());
        json_array_append_new(ignoresJ, ignoreJ);
    }
    json_object_set_new(rootJ, "ignore", ignoresJ);
    return rootJ;
}
void Modul::fromJson(json_t *rootJ) {
    // ignored
    json_t *ignoresJ = json_object_get(rootJ, "ignore");
    if (ignoresJ) {
        modulesIgnoredStr.clear();
        for (unsigned int i = 0 ; i < json_array_size(ignoresJ) ; i++) {
            json_t *ignoreJ = json_array_get(ignoresJ, i);
            if (ignoreJ)
                modulesIgnoredStr.push_back(json_string_value(ignoreJ));
        }
    }
}


//Plugin reset
void PhysicalSync::onReset() {
    audioIO.setDevice(-1, 0);
}





//Update modules, jacks, potentiometers, switches, LEDs… cache
void PhysicalSync::updateCache(bool force) {
    bool debug = false;
    if((updateCacheLastTime > 0.1) || (force)) {
        updateCacheCounter++;
        updateCacheLastTime = 0;

        //Order modules by graphical position and fill vectors
        Modul::modules.clear();
        Modul::modulesWithIgnored.clear();
        for (Widget *w : gRackWidget->moduleContainer->children) {
            Modul moduleWithId;
            moduleWithId.widget = dynamic_cast<ModuleWidget*>(w);

            //If the module has a widget and a module
            if((moduleWithId.widget) && (moduleWithId.widget->module != NULL)) {
                //Store potentiometers and switches
                for(unsigned int parameterId = 0 ; parameterId < moduleWithId.widget->params.size() ; parameterId++) {
                    ParamWidget *parameter = moduleWithId.widget->params.at(parameterId);
                    if(parameter->visible) {
                        ToggleSwitch    *isToggle = NULL;
                        MomentarySwitch *isMomentary = NULL;
                        LEDBezel        *hasLED1 = NULL;
                        LEDButton       *hasLED2 = NULL;
                        LEDBezelButton  *hasLED3 = NULL;

                        try { isToggle = dynamic_cast<ToggleSwitch*>(parameter); }
                        catch(const std::bad_cast& e) { isToggle = NULL; }
                        try { isMomentary = dynamic_cast<MomentarySwitch*>(parameter); }
                        catch(const std::bad_cast& e) { isMomentary = NULL; }
                        try { hasLED1 = dynamic_cast<LEDBezel*>(parameter); }
                        catch(const std::bad_cast& e) { hasLED1 = NULL; }
                        try { hasLED2 = dynamic_cast<LEDButton*>(parameter); }
                        catch(const std::bad_cast& e) { hasLED2 = NULL; }
                        try { hasLED3 = dynamic_cast<LEDBezelButton*>(parameter); }
                        catch(const std::bad_cast& e) { hasLED3 = NULL; }

                        if((isToggle) || (isMomentary)) {
                            Switch button;
                            button.isToggle = isToggle;
                            button.hasLED = hasLED1 || hasLED2 || hasLED3;
                            button.widget = parameter;
                            moduleWithId.switches.push_back(button);
                        }
                        else {
                            Potentiometer potentiometer;
                            potentiometer.widget = parameter;
                            moduleWithId.potentiometers.push_back(potentiometer);
                        }
                    }
                }

                //Store inputs and outputs
                for(unsigned int inputId = 0 ; inputId < moduleWithId.widget->inputs.size() ; inputId++) {
                    Port *input = moduleWithId.widget->inputs.at(inputId);
                    if(input->visible) {
                        JackInput inputWithId;
                        inputWithId.input  = moduleWithId.widget->module->inputs.at(inputId);
                        inputWithId.port = input;
                        moduleWithId.inputs.push_back(inputWithId);
                    }
                }
                for(unsigned int outputId = 0 ; outputId < moduleWithId.widget->outputs.size() ; outputId++) {
                    Port *output = moduleWithId.widget->outputs.at(outputId);
                    if(output->visible) {
                        JackOutput outputWithId;
                        outputWithId.output = moduleWithId.widget->module->outputs.at(outputId);
                        outputWithId.port = output;
                        moduleWithId.outputs.push_back(outputWithId);
                    }
                }

                //Store LEDs excepted ones from this module
                if(moduleWithId.widget->module != this) {
                    //Find LEDs widgets
                    std::vector<ModuleLightWidget*> ledsWidget;
                    for (Widget *w : moduleWithId.widget->children) {
                        try {
                            ModuleLightWidget *ledWidget = dynamic_cast<ModuleLightWidget*>(w);
                            if((ledWidget) && (ledWidget->visible))
                                ledsWidget.push_back(ledWidget);
                        } catch(const std::bad_cast& e) {
                        }
                    }
                    //Associate LED with widgets
                    for(unsigned int ledId = 0 ; ledId < moduleWithId.widget->module->lights.size() ; ledId++) {
                        if(ledId < ledsWidget.size()) {
                            LED led;
                            led.light  = moduleWithId.widget->module->lights.at(ledId);
                            led.widget = ledsWidget.at(ledId);
                            moduleWithId.leds.push_back(led);
                        }
                    }
                }

                //Name of module
                moduleWithId.tmpName = moduleWithId.widget->model->name;

                //Add to container
                Modul::modules.push_back(moduleWithId);
            }
        }

        //Sorts modules and set a name
        std::vector<int> modulesToRemove;
        std::sort(Modul::modules.begin(), Modul::modules.end(), modulesWidgetGraphicalSort());
        for(unsigned int moduleId = 0 ; moduleId < Modul::modules.size() ; moduleId++) {
            //Extended name
            int moduleIndex = -1;
            for(unsigned int moduleIdCheck = 0 ; moduleIdCheck < Modul::modules.size() ; moduleIdCheck++) {
                if((Modul::modules.at(moduleIdCheck).widget != Modul::modules.at(moduleId).widget) && (Modul::modules.at(moduleIdCheck).tmpName == Modul::modules.at(moduleId).tmpName)) {
                    if(moduleIndex < 0)
                        moduleIndex = 1;
                    else
                        moduleIndex++;
                }
            }
            if(moduleIndex < 0)
                Modul::modules[moduleId].name = Modul::modules.at(moduleId).tmpName;
            else
                Modul::modules[moduleId].name = Modul::modules.at(moduleId).tmpName + " #" + std::to_string(moduleIndex);
            /*
            if((module.slug != module.name) && (module.slug != "ModiySync"))
                introText = module.name + " (" + module.slug + ")";
            */

            //Check if not ignored
            if(Modul::modules.at(moduleId).isIgnored())
                modulesToRemove.push_back(moduleId);
        }
        //Last copy
        for(unsigned int moduleId = 0 ; moduleId < Modul::modules.size() ; moduleId++)
            Modul::modulesWithIgnored.push_back(Modul::modules.at(moduleId));
        //Removes ignored modules
        if(modulesToRemove.size())
            for(int i = modulesToRemove.size()-1 ; i >= 0 ; i--)
                Modul::modules.erase(Modul::modules.begin() + modulesToRemove.at(i));

        //Set an absolute ID and a name
        for(unsigned int moduleId = 0 ; moduleId < Modul::modules.size() ; moduleId++)
            Modul::modules[moduleId].moduleId = moduleId;


        //Store wires linked to modules (not in creation)
        JackWire::wires.clear();
        for (Widget *w : gRackWidget->wireContainer->children) {
            JackWire wireWithId;
            wireWithId.widget = dynamic_cast<WireWidget*>(w);
            if(wireWithId.widget->wire != NULL)
                JackWire::wires.push_back(wireWithId);
        }

        //Show infos on modules
        if(debug)
            info("MODULES");
        for(unsigned int moduleId = 0 ; moduleId < Modul::modules.size() ; moduleId++) {
            const Modul moduleWithId = Modul::modules.at(moduleId);
            ModuleWidget *widget = moduleWithId.widget;
            Module *module = widget->module;
            Model  *model  = widget->model;
            if((module) && (model)) {
                if(debug) {
                    info("Module %d : %s %s @ (%f %f) (I/O/P/S/L : %d %d %d %d %d)", moduleId, model->slug.c_str(), model->name.c_str(), widget->box.pos.x, widget->box.pos.y, moduleWithId.inputs.size(), moduleWithId.outputs.size(), moduleWithId.potentiometers.size(), moduleWithId.switches.size(), moduleWithId.leds.size());
                    for(unsigned int inputId = 0 ; inputId < moduleWithId.inputs.size() ; inputId++)
                        info("Input %d : %d", inputId, moduleWithId.inputs.at(inputId).input.active);
                    for(unsigned int outputId = 0 ; outputId < moduleWithId.outputs.size() ; outputId++)
                        info("Output %d : %d", outputId, moduleWithId.outputs.at(outputId).output.active);
                    for(unsigned int potentiometerId = 0 ; potentiometerId < moduleWithId.potentiometers.size() ; potentiometerId++)
                        info("Potentiometer %d : %f", potentiometerId, moduleWithId.potentiometers.at(potentiometerId).widget->value);
                    for(unsigned int switchId = 0 ; switchId < moduleWithId.switches.size() ; switchId++)
                        info("Switch %d : %f", switchId, moduleWithId.switches.at(switchId).widget->value);
                    for(unsigned int ledId = 0 ; ledId < moduleWithId.leds.size() ; ledId++)
                        info("LED %d : %f", ledId, moduleWithId.leds.at(ledId).light.value);
                }
            }
        }

        //Show infos on wires + cache modules references
        if(debug)
            info("WIRES");
        for(unsigned int wireId = 0 ; wireId < JackWire::wires.size() ; wireId++) {
            JackWire::wires[wireId].inputModuleId = JackWire::wires[wireId].outputModuleId = -1;
            Modul inputModule, outputModule;
            for(unsigned int moduleId = 0 ; moduleId < Modul::modules.size() ; moduleId++) {
                Modul moduleWidgetTest = Modul::modules.at(moduleId);
                if(moduleWidgetTest.widget->module == JackWire::wires.at(wireId).widget->wire->inputModule)
                    inputModule = moduleWidgetTest;
                if(moduleWidgetTest.widget->module == JackWire::wires.at(wireId).widget->wire->outputModule)
                    outputModule = moduleWidgetTest;
            }
            if((inputModule.widget) && (outputModule.widget)) {
                JackWire::wires[wireId].inputModuleId  = inputModule.moduleId;
                JackWire::wires[wireId].outputModuleId = outputModule.moduleId;
                if(debug)
                    info("Wire %d link %d - %s %s (%d) to %d - %s %s (%d)", wireId, JackWire::wires.at(wireId).outputModuleId, outputModule.widget->model->slug.c_str(), outputModule.widget->model->name.c_str(), JackWire::wires.at(wireId).widget->wire->outputId, JackWire::wires.at(wireId).inputModuleId, inputModule.widget->model->slug.c_str(), inputModule.widget->model->name.c_str(), JackWire::wires.at(wireId).widget->wire->inputId);
            }
        }
    }
}



//Start OSC server
void PhysicalSync::startOSCServer() {
    info("Beginning of OSC server on port %d", oscPort);
    try {
        osc = new OSCManagement(this);
        UdpListeningReceiveSocket socket(IpEndpointName(IpEndpointName::ANY_ADDRESS, oscPort), osc);
        oscServerJustStart = true;
        socket.RunUntilSigInt();
        info("End of OSC server on port ", oscPort);
    }
    catch(std::exception e) {
        osc = NULL;
        info("Impossible to bind OSC server on port %d", oscPort);
    }
}

//Log in console + through OSC
void PhysicalSync::logV(std::string message) {
    warn("%s", message.c_str());
}


//Number of visible channels
void PhysicalSync::setChannels(int nbChannels) {
    currentNbChannels = nbChannels;
    for(unsigned int i = 0 ; i < widget->inputs.size() ; i++)
        widget->inputs.at(i)->visible = (i < currentNbChannels);
    for(unsigned int i = 0 ; i < inputComponents.size() ; i++)
        inputComponents.at(i)->visible = (i < currentNbChannels);
    for(unsigned int i = 0 ; i < widget->outputs.size() ; i++)
        widget->outputs.at(i)->visible = (i < currentNbChannels);
    for(unsigned int i = 0 ; i < outputComponents.size() ; i++)
        outputComponents.at(i)->visible = (i < currentNbChannels);
}

//Audio step
void PhysicalSync::step() {
    float deltaTime = engineGetSampleTime();

    //Cache can be updated if sum of deltaTime > x
    updateCacheLastTime += deltaTime;

    //End of startup, after OSC creation, one time only
    if((osc) && (oscServerJustStart)) {
        oscServerJustStart = false;
        rtBrokerFound = false;

        //Current directory and realtime message broker path
#ifdef ARCH_MAC
        rtBrokerPath = assetPlugin(plugin, "res/Realtime Message Broker.app/Contents/MacOS/Realtime Message Broker");
#endif
#ifdef ARCH_WIN
        rtBrokerPath = assetPlugin(plugin, "res/Realtime Message Broker.exe");
#endif
#ifdef ARCH_LIN
        rtBrokerPath = assetPlugin(plugin, "res/RTBroker");
#endif
        info("Checks if realtime message broker is present at %s", rtBrokerPath.c_str());
        if(FILE *file = fopen(rtBrokerPath.c_str(), "r")) {
            //Realtime message broker is OK
            fclose(file);
            rtBrokerFound = true;
        }

        //LED visibility
        if((oscLEDInt) && (oscLEDInt->visible == false))
            oscLEDInt->visible = true;
        if((oscLEDExt) && (oscLEDExt->visible == false))
            oscLEDExt->visible = true;
    }

    //Only if Modiy as an OSC server
    if(osc) {
        //Ping fake LED
        if(pingLED.blink(deltaTime))
            osc->send("/ping", oscPort);

        //Stats of caching
        if(cacheCounter.blink(deltaTime, 1)) {
            //info("Cache updates: %d per second", updateCacheCounter);
            updateCacheCounter = 0;
        }

        //LED status
        float ledStatusIntDelta = deltaTime * 2;
        if(!rtBrokerFound)
            ledStatusIntDelta = -1;
        else if(!isRTBrokerTalking)
            ledStatusIntDelta = deltaTime / 4;
        if(ledStatusInt.blink(ledStatusIntDelta)) {
            osc->send("/pulse", ledStatusInt.valueRounded);

            //Opens realtime message broker if needed
            if(rtBrokerFound) {
                if(!isRTBrokerTalking)
                    isRTBrokerTalkingCounter++;
                isRTBrokerTalking = false;

                if(isRTBrokerTalkingCounter > 3) {
                    isRTBrokerTalkingCounter = -5;
                    info("Trying to start realtime message broker %s…", rtBrokerPath.c_str());

                    TinyProcessLib::Process process("\"" + rtBrokerPath + "\"");
                }
            }
        }
        lights[OSC_LED_INT].value = ledStatusInt.valueRounded;
    }

    //Audio
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
    if (!inputBuffer.empty())
        inputFrame = inputBuffer.shift();
    else
        memset(&inputFrame, 0, sizeof(inputFrame));
    for (int i = 0; i < audioIO.numInputs; i++)
        outputs[AUDIO_OUTPUT + i].value = 10.f * inputFrame.samples[i];
    for (int i = audioIO.numInputs; i < AUDIO_INPUTS; i++)
        outputs[AUDIO_OUTPUT + i].value = 0.f;

    // Outputs: rack engine -> audio engine
    if (audioIO.active && audioIO.numOutputs > 0) {
        // Get and push output SRC frame
        if (!outputBuffer.full()) {
            Frame<AUDIO_OUTPUTS> outputFrame;
            for (int i = 0; i < AUDIO_OUTPUTS; i++)
                outputFrame.samples[i] = inputs[AUDIO_INPUT + i].value / 10.f;
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
Modul PhysicalSync::getModule(unsigned int moduleId) {
    updateCache();
    if(moduleId < Modul::modules.size())
        return Modul::modules.at(moduleId);
    return Modul();
}
Potentiometer PhysicalSync::getPotentiometer(unsigned int moduleId, unsigned int potentiometerId) {
    Modul module = getModule(moduleId);
    if(module.widget) {
        if(potentiometerId < module.potentiometers.size())
            return module.potentiometers.at(potentiometerId);
    }
    return Potentiometer();
}
Switch PhysicalSync::getSwitch(unsigned int moduleId, unsigned int switchId) {
    Modul module = getModule(moduleId);
    if(module.widget) {
        if(switchId < module.switches.size())
            return module.switches.at(switchId);
    }
    return Switch();
}
JackInput PhysicalSync::getInputPort(unsigned int moduleId, unsigned int inputId) {
    Modul module = getModule(moduleId);
    if(module.widget) {
        if(inputId < module.inputs.size())
            return module.inputs.at(inputId);
    }
    return JackInput();
}
JackOutput PhysicalSync::getOutputPort(unsigned int moduleId, unsigned int outputId) {
    Modul module = getModule(moduleId);
    if(module.widget) {
        if(outputId < module.outputs.size())
            return module.outputs.at(outputId);
    }
    return JackOutput();
}
LED PhysicalSync::getLED(unsigned int moduleId, unsigned int ledId) {
    Modul module = getModule(moduleId);
    if(module.widget) {
        if(ledId < module.leds.size())
            return module.leds.at(ledId);
    }
    return LED();
}
JackWire PhysicalSync::getWire(unsigned int inputModuleId, unsigned int inputPortId, unsigned int outputModuleId, unsigned int outputPortId) {
    JackInput  input  = getInputPort (inputModuleId,  inputPortId);
    JackOutput output = getOutputPort(outputModuleId, outputPortId);
    if((input.port) && (output.port))
        for(unsigned int wireId = 0 ; wireId < JackWire::wires.size() ; wireId++)
            if((JackWire::wires.at(wireId).inputModuleId == (int)inputModuleId) && (JackWire::wires.at(wireId).outputModuleId == (int)outputModuleId))
                if((JackWire::wires.at(wireId).widget->inputPort->portId == (int)inputPortId) && (JackWire::wires.at(wireId).widget->outputPort->portId == (int)outputPortId))
                    return JackWire::wires.at(wireId);
    return JackWire();
}


//Absolute ID mapping to module (and reverse)
bool PhysicalSync::mapFromPotentiometer(unsigned int index, int *moduleId, int *potentiometerId) {
    Modul lastModule;
    *moduleId = 0;
    *potentiometerId  = 0;

    //Search for index overflow in modules
    for(unsigned int lastModuleId = 0 ; lastModuleId < Modul::modules.size() ; lastModuleId++) {
        lastModule = getModule(lastModuleId);
        if(lastModule.widget) {
            if(index >= lastModule.potentiometers.size()) {
                index -= lastModule.potentiometers.size();
                *moduleId = lastModuleId+1;
            }
            else
                break;
        }
    }

    //Add rest of index to potentiometerId
    if(lastModule.widget) {
        *potentiometerId = index;
        return true;
    }
    else {
        warn("Error with mapFromPotentiometer");
        return false;
    }
}
bool PhysicalSync::mapFromSwitch(unsigned int index, int *moduleId, int *switchId) {
    Modul lastModule;
    *moduleId = 0;
    *switchId = 0;

    //Search for index overflow in modules
    for(unsigned int lastModuleId = 0 ; lastModuleId < Modul::modules.size() ; lastModuleId++) {
        lastModule = getModule(lastModuleId);
        if(lastModule.widget) {
            if(index >= lastModule.switches.size()) {
                index -= lastModule.switches.size();
                *moduleId = lastModuleId+1;
            }
            else
                break;
        }
    }

    //Add rest of index to potentiometerId
    if(lastModule.widget) {
        *switchId = index;
        return true;
    }
    else {
        warn("Error with mapFromPotentiometer");
        return false;
    }
}
bool PhysicalSync::mapFromJack(unsigned int index, int *moduleId, int *inputOrOutputId, bool *isInput) {
    Modul lastModule;
    *isInput         = true;
    *moduleId        = 0;
    *inputOrOutputId = 0;

    //Search for index overflow in modules
    for(unsigned int lastModuleId = 0 ; lastModuleId < Modul::modules.size() ; lastModuleId++) {
        lastModule = getModule(lastModuleId);
        if(lastModule.widget) {
            if(index  >= (lastModule.inputs.size()+lastModule.outputs.size())) {
                index -= (lastModule.inputs.size()+lastModule.outputs.size());
                *moduleId = lastModuleId+1;
            }
            else
                break;
        }
    }

    //Add rest of index to inputOrOutputId
    if(lastModule.widget) {
        if(index >= lastModule.inputs.size()) {
            *isInput = false;
            *inputOrOutputId = index - lastModule.inputs.size();
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
int PhysicalSync::mapToLED(unsigned int moduleId, unsigned int ledId) {
    const LED led = getLED(moduleId, ledId);
    int index = -1;
    if(led.widget) {
        index = 0;

        //Look for all modules before
        for(unsigned int lastModuleId = 0 ; lastModuleId < moduleId ; lastModuleId++) {
            index += getModule(lastModuleId).leds.size();
        }
        index += ledId;
    }
    else
        warn("Error with mapToLED");
    return index;
}
int PhysicalSync::mapToPotentiometer(unsigned int moduleId, unsigned int potentiometerId) {
    const Potentiometer potentiometer = getPotentiometer(moduleId, potentiometerId);
    int index = -1;
    if(potentiometer.widget) {
        index = 0;

        //Look for all modules before
        for(unsigned int lastModuleId = 0 ; lastModuleId < moduleId ; lastModuleId++)
            index += getModule(lastModuleId).potentiometers.size();
        index += potentiometerId;
    }
    else
        warn("Error with mapToPotentiometer");
    return index;
}
int PhysicalSync::mapToSwitch(unsigned int moduleId, unsigned int switchId) {
    const Switch button = getSwitch(moduleId, switchId);
    int index = -1;
    if(button.widget) {
        index = 0;

        //Look for all modules before
        for(unsigned int lastModuleId = 0 ; lastModuleId < moduleId ; lastModuleId++)
            index += getModule(lastModuleId).switches.size();
        index += switchId;
    }
    else
        warn("Error with mapToSwitch");
    return index;
}
int PhysicalSync::mapToJack(unsigned int moduleId, unsigned int inputOrOutputId, bool isInput) {
    const Port *port;
    if(isInput) port = getInputPort (moduleId, inputOrOutputId).port;
    else        port = getOutputPort(moduleId, inputOrOutputId).port;
    int index = -1;
    if(port) {
        index = 0;

        //Look for all modules before
        for(unsigned int lastModuleId = 0 ; lastModuleId < moduleId ; lastModuleId++) {
            Modul module = getModule(lastModuleId);
            index += (module.inputs.size() + module.outputs.size());
        }

        //Add input size offset if it's an output
        if(!isInput)
            index += getModule(moduleId).inputs.size();

        index += inputOrOutputId;
    }
    else
        warn("Error with mapToJack");
    return index;
}


//Setters
void PhysicalSync::setPotentiometer(unsigned int moduleId, unsigned int potentiometerId, float potentiometerValue, bool isNormalized, bool additive) {
    Potentiometer potentiometer = getPotentiometer(moduleId, potentiometerId);
    if(potentiometer.widget) {
        if(isNormalized) {
            potentiometerValue = potentiometerValue * (potentiometer.widget->maxValue - potentiometer.widget->minValue);
            if(!additive)
                potentiometerValue += potentiometer.widget->minValue;
        }
        if(additive)
            potentiometerValue += potentiometer.widget->value;
        potentiometer.widget->setValue(potentiometerValue);
    }
    else {
        std::stringstream stream;
        stream << "Module ";
        stream << moduleId;
        stream << " potentiometer ";
        stream << potentiometerId;
        stream << " not found.";
        logV(stream.str());
    }
}
void PhysicalSync::setSwitch(unsigned int moduleId, unsigned int switchId, float switchValue) {
    Switch button = getSwitch(moduleId, switchId);
    if(button.widget)
        button.widget->setValue((switchValue * (button.widget->maxValue - button.widget->minValue)) + button.widget->minValue);
    else {
        std::stringstream stream;
        stream << "Module ";
        stream << moduleId;
        stream << " switch ";
        stream << switchId;
        stream << " not found.";
        logV(stream.str());
    }
}
void PhysicalSync::resetPotentiometer(unsigned int moduleId, unsigned int potentiometerId) {
    Potentiometer potentiometer = getPotentiometer(moduleId, potentiometerId);
    if(potentiometer.widget)
        potentiometer.widget->setValue(potentiometer.widget->defaultValue);
    else {
        std::stringstream stream;
        stream << "Module ";
        stream << moduleId;
        stream << "potentiometer ";
        stream << potentiometerId;
        stream << " not found.";
        logV(stream.str());
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

    const JackWire wire = getWire(inputModuleId, inputPortId, outputModuleId, outputPortId);
    if((!wire.widget) && (active)) {
        JackOutput output = getOutputPort(outputModuleId, outputPortId);
        JackInput  input  = getInputPort (inputModuleId,  inputPortId);
        if((input.port) && (output.port)) {
            //Test if input is inactive
            if(!input.input.active) {
                //Créé le lien
                logV(logTextBase + "creation");
                WireWidget *wireWidget = new WireWidget();
                wireWidget->inputPort  = input.port;
                wireWidget->outputPort = output.port;
                wireWidget->updateWire();
                gRackWidget->wireContainer->addChild(wireWidget);
                updateCache(true);
            }
            else
                logV(logTextBase + "input is already busy");
        }
        else
            logV(logTextBase + "input or output not found");
    }
    else if((wire.widget) && (!active)) {
        logV(logTextBase + "removed");
        wire.widget->inputPort = wire.widget->outputPort = NULL;
        wire.widget->updateWire();
        updateCache(true);
    }
    else
        logV(logTextBase + "no action done");
}
void PhysicalSync::clearConnection() {
    for(unsigned int moduleId = 0 ; moduleId < Modul::modules.size() ; moduleId++) {
        Modul module = getModule(moduleId);
        for(unsigned int inputId = 0 ; inputId < module.inputs.size() ; inputId++)
            gRackWidget->wireContainer->removeAllWires(module.inputs.at(inputId).port);
        for(unsigned int ouputId = 0 ; ouputId < module.outputs.size() ; ouputId++)
            gRackWidget->wireContainer->removeAllWires(module.outputs.at(ouputId).port);
    }
}


//Dump
void PhysicalSync::dumpLEDs(const char *address, bool inLine) {
    lights[OSC_LED_EXT].value = 1-lights[OSC_LED_EXT].value;
    if(osc) {
        std::string inLineString;

        //Prepare bundle
        UdpTransmitSocket transmitSocket(IpEndpointName(osc->rtBrokerIp, osc->rtBrokerPort));
        osc::OutboundPacketStream bundle(osc->buffer, OSC_BUFFER_SIZE);
        if(!inLine) {
            bundle << osc::BeginBundle();
            bundle << osc::BeginMessage(address) << "start" << osc::EndMessage;
        }

        //Browse each module
        for(unsigned int moduleId = 0 ; moduleId < Modul::modules.size() ; moduleId++) {
            Modul module = getModule(moduleId);
            if(module.widget) {
                for(unsigned int ledId = 0 ; ledId < module.leds.size() ; ledId++) {
                    LED led = getLED(moduleId, ledId);
                    if(led.widget) {
                        if(inLine)
                            inLineString += " " + std::to_string((int)(255 * led.light.value));
                        else
                            bundle << osc::BeginMessage(address)
                                   << mapToLED(moduleId, ledId)
                                   << (int)moduleId << (int)ledId
                                   << (int)led.widget->box.getCenter().x << (int)led.widget->box.getCenter().y
                                   << osc::EndMessage;
                    }
                }
            }
        }
        if(!inLine) {
            //Send bundle
            bundle << osc::BeginMessage(address) << "end" << osc::EndMessage;
            bundle << osc::EndBundle;
            transmitSocket.Send(bundle.Data(), bundle.Size());
        }
        else {
            inLineString = "L" + inLineString;

            //Send message
            bundle << osc::BeginMessage(address) << inLineString.c_str() << osc::EndMessage;
            transmitSocket.Send(bundle.Data(), bundle.Size());
        }
    }
}
void PhysicalSync::dumpPotentiometers(const char *address) {
    if(osc) {
        //Prepare bundle
        UdpTransmitSocket transmitSocket(IpEndpointName(osc->rtBrokerIp, osc->rtBrokerPort));
        osc::OutboundPacketStream bundle(osc->buffer, OSC_BUFFER_SIZE);
        bundle << osc::BeginBundle();
        bundle << osc::BeginMessage(address) << "start" << osc::EndMessage;

        //Browse each module
        for(unsigned int moduleId = 0 ; moduleId < Modul::modules.size() ; moduleId++) {
            Modul module = getModule(moduleId);
            if(module.widget) {
                for(unsigned int potentiometerId = 0 ; potentiometerId < module.potentiometers.size() ; potentiometerId++) {
                    Potentiometer potentiometer = getPotentiometer(moduleId, potentiometerId);
                    if(potentiometer.widget)
                        bundle << osc::BeginMessage(address)
                               << mapToPotentiometer(moduleId, potentiometerId)
                               << (int)moduleId << (int)potentiometerId
                               << (int)potentiometer.widget->box.getCenter().x << (int)potentiometer.widget->box.getCenter().y
                               << osc::EndMessage;
                }
            }
        }

        //Send bundle
        bundle << osc::BeginMessage(address) << "end" << osc::EndMessage;
        bundle << osc::EndBundle;
        transmitSocket.Send(bundle.Data(), bundle.Size());
    }
}
void PhysicalSync::dumpSwitches(const char *address) {
    if(osc) {
        //Prepare bundle
        UdpTransmitSocket transmitSocket(IpEndpointName(osc->rtBrokerIp, osc->rtBrokerPort));
        osc::OutboundPacketStream bundle(osc->buffer, OSC_BUFFER_SIZE);
        bundle << osc::BeginBundle();
        bundle << osc::BeginMessage(address) << "start" << osc::EndMessage;

        //Browse each module
        for(unsigned int moduleId = 0 ; moduleId < Modul::modules.size() ; moduleId++) {
            Modul module = getModule(moduleId);
            if(module.widget) {
                for(unsigned int switchId = 0 ; switchId < module.switches.size() ; switchId++) {
                    Switch button = getSwitch(moduleId, switchId);
                    if(button.widget)
                        bundle << osc::BeginMessage(address)
                               << mapToSwitch(moduleId, switchId)
                               << (int)moduleId << (int)switchId
                               << (int)button.widget->box.getCenter().x << (int)button.widget->box.getCenter().y
                               << (int)button.isToggle << (int)button.hasLED
                               << osc::EndMessage;
                }
            }
        }

        //Send bundle
        bundle << osc::BeginMessage(address) << "end" << osc::EndMessage;
        bundle << osc::EndBundle;
        info("%d", bundle.Size());
        transmitSocket.Send(bundle.Data(), bundle.Size());
    }
}
void PhysicalSync::dumpJacks(const char *address) {
    if(osc) {
        //Inputs
        if(true) {
            //Prepare bundle
            UdpTransmitSocket transmitSocket(IpEndpointName(osc->rtBrokerIp, osc->rtBrokerPort));
            osc::OutboundPacketStream bundle(osc->buffer, OSC_BUFFER_SIZE);
            bundle << osc::BeginBundle();
            bundle << osc::BeginMessage(address) << "start" << osc::EndMessage;

            //Browse each module
            for(unsigned int moduleId = 0 ; moduleId < Modul::modules.size() ; moduleId++) {
                Modul module = getModule(moduleId);
                if(module.widget) {
                    for(unsigned int inputId = 0 ; inputId < module.inputs.size() ; inputId++) {
                        JackInput input = getInputPort(moduleId, inputId);
                        if(input.port)
                            bundle << osc::BeginMessage(address)
                                   << mapToJack(moduleId, inputId, true)
                                   << (int)moduleId << (int)inputId
                                   << (int)input.port->box.getCenter().x << (int)input.port->box.getCenter().y
                                   << (int)input.input.active << 1
                                   << osc::EndMessage;
                    }
                }
            }

            //Send bundle
            bundle << osc::EndBundle;
            transmitSocket.Send(bundle.Data(), bundle.Size());
        }

        //Outputs
        if(true) {
            //Prepare bundle
            UdpTransmitSocket transmitSocket(IpEndpointName(osc->rtBrokerIp, osc->rtBrokerPort));
            osc::OutboundPacketStream bundle(osc->buffer, OSC_BUFFER_SIZE);
            bundle << osc::BeginBundle();

            //Browse each module
            for(unsigned int moduleId = 0 ; moduleId < Modul::modules.size() ; moduleId++) {
                Modul module = getModule(moduleId);
                if(module.widget) {
                    for(unsigned int ouputId = 0 ; ouputId < module.outputs.size() ; ouputId++) {
                        JackOutput output = getOutputPort(moduleId, ouputId);
                        if(output.port)
                            bundle << osc::BeginMessage(address)
                                   << mapToJack(moduleId, ouputId, false)
                                   << (int)moduleId << (int)ouputId
                                   << (int)output.port->box.getCenter().x << (int)output.port->box.getCenter().y
                                   << (int)output.output.active << 0
                                   << osc::EndMessage;
                    }
                }
            }

            //Send bundle
            bundle << osc::BeginMessage(address) << "end" << osc::EndMessage;
            bundle << osc::EndBundle;
            transmitSocket.Send(bundle.Data(), bundle.Size());
        }
    }
}
void PhysicalSync::dumpModules(const char *address) {
    if(osc) {
        //Ask for cache update for the first refresh
        updateCache();

        //Prepare bundle
        UdpTransmitSocket transmitSocket(IpEndpointName(osc->rtBrokerIp, osc->rtBrokerPort));
        osc::OutboundPacketStream bundle(osc->buffer, OSC_BUFFER_SIZE);
        bundle << osc::BeginBundle();
        bundle << osc::BeginMessage(address) << "start" << osc::EndMessage;

        for(unsigned int moduleId = 0 ; moduleId < Modul::modules.size() ; moduleId++) {
            Modul module = getModule(moduleId);
            if(module.widget)
                bundle << osc::BeginMessage(address)
                       << (int)moduleId << module.widget->model->slug.c_str() << module.widget->model->name.c_str() << module.widget->model->author.c_str() << module.name.c_str()
                       << (int)module.widget->box.pos.x  << (int)module.widget->box.pos.y
                       << (int)module.widget->box.size.x << (int)module.widget->box.size.y
                       << (int)module.inputs.size() << (int)module.outputs.size() << (int)module.potentiometers.size() << (int)module.switches.size() << (int)module.leds.size()
                       << osc::EndMessage;
        }

        //Send bundle
        bundle << osc::BeginMessage(address) << "end" << osc::EndMessage;
        bundle << osc::EndBundle;
        transmitSocket.Send(bundle.Data(), bundle.Size());
    }
}
void PhysicalSync::send(const char *address) {
    if(osc)
        osc->send(address);
}
void PhysicalSync::pongReceived() {
    isRTBrokerTalking = true;
    isRTBrokerTalkingCounter = 0;
}



//Module widget
PhysicalSyncWidget::PhysicalSyncWidget(PhysicalSync *physicalSync) : ModuleWidget(physicalSync) {
    setPanel(SVG::load(assetPlugin(plugin, "res/PhysicalSync.svg")));
    physicalSync->widget = this;
    SVGWidget *component;
    float y = 44;

    //Screws
    addChild(Widget::create<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
    addChild(Widget::create<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
    addChild(Widget::create<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
    addChild(Widget::create<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

    //Data sync
    physicalSync->oscLEDInt = ModuleLightWidget::create<SmallLight<RedLight>>   (Vec(RACK_GRID_WIDTH+4, 18.75)                 , physicalSync, PhysicalSync::OSC_LED_INT);
    physicalSync->oscLEDExt = ModuleLightWidget::create<SmallLight<YellowLight>>(Vec(box.size.x - 2 * RACK_GRID_WIDTH+4, 18.75), physicalSync, PhysicalSync::OSC_LED_EXT);
    physicalSync->oscLEDInt->visible = false;
    physicalSync->oscLEDExt->visible = false;
    addChild(physicalSync->oscLEDInt);
    addChild(physicalSync->oscLEDExt);

    //Audio sync
    //Input port (to go out on sound cart)
    addInput(Port::create<PJ301MPort>(mm2px(Vec(3.5, y+10*0)), Port::INPUT, physicalSync, PhysicalSync::AUDIO_INPUT + 0));
    addInput(Port::create<PJ301MPort>(mm2px(Vec(3.5, y+10*1)), Port::INPUT, physicalSync, PhysicalSync::AUDIO_INPUT + 1));
    addInput(Port::create<PJ301MPort>(mm2px(Vec(3.5, y+10*2)), Port::INPUT, physicalSync, PhysicalSync::AUDIO_INPUT + 2));
    addInput(Port::create<PJ301MPort>(mm2px(Vec(3.5, y+10*3)), Port::INPUT, physicalSync, PhysicalSync::AUDIO_INPUT + 3));
    addInput(Port::create<PJ301MPort>(mm2px(Vec(3.5, y+10*4)), Port::INPUT, physicalSync, PhysicalSync::AUDIO_INPUT + 4));
    addInput(Port::create<PJ301MPort>(mm2px(Vec(3.5, y+10*5)), Port::INPUT, physicalSync, PhysicalSync::AUDIO_INPUT + 5));
    addInput(Port::create<PJ301MPort>(mm2px(Vec(3.5, y+10*6)), Port::INPUT, physicalSync, PhysicalSync::AUDIO_INPUT + 6));
    addInput(Port::create<PJ301MPort>(mm2px(Vec(3.5, y+10*7)), Port::INPUT, physicalSync, PhysicalSync::AUDIO_INPUT + 7));

    //Fake output virtual port (but physically are real)
    component = new SVGWidget(); component->setSVG(SVG::load(assetGlobal("res/ComponentLibrary/CL1362.svg"))); component->box.pos = mm2px(Vec(12.5, (y-1)+10*0)); addChild(component); physicalSync->inputComponents.push_back(component);
    component = new SVGWidget(); component->setSVG(SVG::load(assetGlobal("res/ComponentLibrary/CL1362.svg"))); component->box.pos = mm2px(Vec(12.5, (y-1)+10*1)); addChild(component); physicalSync->inputComponents.push_back(component);
    component = new SVGWidget(); component->setSVG(SVG::load(assetGlobal("res/ComponentLibrary/CL1362.svg"))); component->box.pos = mm2px(Vec(12.5, (y-1)+10*2)); addChild(component); physicalSync->inputComponents.push_back(component);
    component = new SVGWidget(); component->setSVG(SVG::load(assetGlobal("res/ComponentLibrary/CL1362.svg"))); component->box.pos = mm2px(Vec(12.5, (y-1)+10*3)); addChild(component); physicalSync->inputComponents.push_back(component);
    component = new SVGWidget(); component->setSVG(SVG::load(assetGlobal("res/ComponentLibrary/CL1362.svg"))); component->box.pos = mm2px(Vec(12.5, (y-1)+10*4)); addChild(component); physicalSync->inputComponents.push_back(component);
    component = new SVGWidget(); component->setSVG(SVG::load(assetGlobal("res/ComponentLibrary/CL1362.svg"))); component->box.pos = mm2px(Vec(12.5, (y-1)+10*5)); addChild(component); physicalSync->inputComponents.push_back(component);
    component = new SVGWidget(); component->setSVG(SVG::load(assetGlobal("res/ComponentLibrary/CL1362.svg"))); component->box.pos = mm2px(Vec(12.5, (y-1)+10*6)); addChild(component); physicalSync->inputComponents.push_back(component);
    component = new SVGWidget(); component->setSVG(SVG::load(assetGlobal("res/ComponentLibrary/CL1362.svg"))); component->box.pos = mm2px(Vec(12.5, (y-1)+10*7)); addChild(component); physicalSync->inputComponents.push_back(component);

    //Fake input virtual port (but physically are real)
    component = new SVGWidget(); component->setSVG(SVG::load(assetGlobal("res/ComponentLibrary/CL1362.svg"))); component->box.pos = mm2px(Vec(27, (y-1)+10*0)); addChild(component); physicalSync->outputComponents.push_back(component);
    component = new SVGWidget(); component->setSVG(SVG::load(assetGlobal("res/ComponentLibrary/CL1362.svg"))); component->box.pos = mm2px(Vec(27, (y-1)+10*1)); addChild(component); physicalSync->outputComponents.push_back(component);
    component = new SVGWidget(); component->setSVG(SVG::load(assetGlobal("res/ComponentLibrary/CL1362.svg"))); component->box.pos = mm2px(Vec(27, (y-1)+10*2)); addChild(component); physicalSync->outputComponents.push_back(component);
    component = new SVGWidget(); component->setSVG(SVG::load(assetGlobal("res/ComponentLibrary/CL1362.svg"))); component->box.pos = mm2px(Vec(27, (y-1)+10*3)); addChild(component); physicalSync->outputComponents.push_back(component);
    component = new SVGWidget(); component->setSVG(SVG::load(assetGlobal("res/ComponentLibrary/CL1362.svg"))); component->box.pos = mm2px(Vec(27, (y-1)+10*4)); addChild(component); physicalSync->outputComponents.push_back(component);
    component = new SVGWidget(); component->setSVG(SVG::load(assetGlobal("res/ComponentLibrary/CL1362.svg"))); component->box.pos = mm2px(Vec(27, (y-1)+10*5)); addChild(component); physicalSync->outputComponents.push_back(component);
    component = new SVGWidget(); component->setSVG(SVG::load(assetGlobal("res/ComponentLibrary/CL1362.svg"))); component->box.pos = mm2px(Vec(27, (y-1)+10*6)); addChild(component); physicalSync->outputComponents.push_back(component);
    component = new SVGWidget(); component->setSVG(SVG::load(assetGlobal("res/ComponentLibrary/CL1362.svg"))); component->box.pos = mm2px(Vec(27, (y-1)+10*7)); addChild(component); physicalSync->outputComponents.push_back(component);

    //Output port (to go in on sound cart)
    addOutput(Port::create<PJ301MPort>(mm2px(Vec(39, y+10*0)), Port::OUTPUT, physicalSync, PhysicalSync::AUDIO_OUTPUT + 0));
    addOutput(Port::create<PJ301MPort>(mm2px(Vec(39, y+10*1)), Port::OUTPUT, physicalSync, PhysicalSync::AUDIO_OUTPUT + 1));
    addOutput(Port::create<PJ301MPort>(mm2px(Vec(39, y+10*2)), Port::OUTPUT, physicalSync, PhysicalSync::AUDIO_OUTPUT + 2));
    addOutput(Port::create<PJ301MPort>(mm2px(Vec(39, y+10*3)), Port::OUTPUT, physicalSync, PhysicalSync::AUDIO_OUTPUT + 3));
    addOutput(Port::create<PJ301MPort>(mm2px(Vec(39, y+10*4)), Port::OUTPUT, physicalSync, PhysicalSync::AUDIO_OUTPUT + 4));
    addOutput(Port::create<PJ301MPort>(mm2px(Vec(39, y+10*5)), Port::OUTPUT, physicalSync, PhysicalSync::AUDIO_OUTPUT + 5));
    addOutput(Port::create<PJ301MPort>(mm2px(Vec(39, y+10*6)), Port::OUTPUT, physicalSync, PhysicalSync::AUDIO_OUTPUT + 6));
    addOutput(Port::create<PJ301MPort>(mm2px(Vec(39, y+10*7)), Port::OUTPUT, physicalSync, PhysicalSync::AUDIO_OUTPUT + 7));

    //Audio control box
    AudioWidget *audioWidget = Widget::create<AudioWidget>(mm2px(Vec(3.2122073, 10)));
    audioWidget->box.size = mm2px(Vec(45.2, 28));
    audioWidget->audioIO = &physicalSync->audioIO;
    addChild(audioWidget);
}

//Additional menu
void PhysicalSyncWidget::appendContextMenu(Menu *menu) {
    PhysicalSync *physicalSync = dynamic_cast<PhysicalSync*>(module);
    assert(physicalSync);

    //Realtime Message Broker options
    if(physicalSync->osc) {
        menu->addChild(MenuEntry::create());

        //Settings page
        RTBrokerMenu *rtbroker_openSettings = MenuItem::create<RTBrokerMenu>("Serial port settings");
        rtbroker_openSettings->physicalSync = physicalSync;
        rtbroker_openSettings->oscAddress = "/rtbroker/opensettings";
        menu->addChild(rtbroker_openSettings);

        //Network page
        RTBrokerMenu *rtbroker_openNetwork = MenuItem::create<RTBrokerMenu>("Network system settings");
        rtbroker_openNetwork->physicalSync = physicalSync;
        rtbroker_openNetwork->oscAddress = "/rtbroker/opennetwork";
        menu->addChild(rtbroker_openNetwork);

        //Webpage page
        RTBrokerMenu *rtbroker_openWebpage = MenuItem::create<RTBrokerMenu>("Open webpage");
        rtbroker_openWebpage->physicalSync = physicalSync;
        rtbroker_openWebpage->oscAddress = "/rtbroker/openwebpage";
        menu->addChild(rtbroker_openWebpage);
    }

    //Audio presets
    if(true) {
        menu->addChild(MenuEntry::create());

        AudioPresetMenu *changeChannels2 = MenuItem::create<AudioPresetMenu>("Stereo audio I/O", CHECKMARK(physicalSync->getChannels() == 2));
        changeChannels2->physicalSync = physicalSync;
        changeChannels2->nbChannels = 2;
        menu->addChild(changeChannels2);
        AudioPresetMenu *changeChannels4 = MenuItem::create<AudioPresetMenu>("Quad audio I/O", CHECKMARK(physicalSync->getChannels() == 4));
        changeChannels4->physicalSync = physicalSync;
        changeChannels4->nbChannels = 4;
        menu->addChild(changeChannels4);
        AudioPresetMenu *changeChannels8 = MenuItem::create<AudioPresetMenu>("Octo audio I/O", CHECKMARK(physicalSync->getChannels() == 8));
        changeChannels8->physicalSync = physicalSync;
        changeChannels8->nbChannels = 8;
        menu->addChild(changeChannels8);
    }

    //Module ignore
    if(physicalSync->osc) {
        menu->addChild(MenuEntry::create());

        for(unsigned int moduleId = 0 ; moduleId < Modul::modulesWithIgnored.size() ; moduleId++) {
            const Modul module = Modul::modulesWithIgnored.at(moduleId);

            IgnoreModuleMenu *moduleIgnore = MenuItem::create<IgnoreModuleMenu>("Ignore " + module.name, CHECKMARK(module.isIgnored()));
            moduleIgnore->physicalSync = physicalSync;
            moduleIgnore->moduleName = module.name;
            menu->addChild(moduleIgnore);
        }
    }
}


//Menu item
void RTBrokerMenu::onAction(EventAction &) {
    if(physicalSync)
        physicalSync->send(oscAddress);
}
void AudioPresetMenu::onAction(EventAction &) {
    if(physicalSync)
        physicalSync->setChannels(nbChannels);
}
void IgnoreModuleMenu::onAction(EventAction &) {
    //Check if alread ignored or not
    if(std::find(Modul::modulesIgnoredStr.begin(), Modul::modulesIgnoredStr.end(), moduleName) != Modul::modulesIgnoredStr.end()) {
        info("No more ignore %s", moduleName.c_str());
        for(int i = Modul::modulesIgnoredStr.size()-1 ; i >= 0 ; i--)
            if(Modul::modulesIgnoredStr.at(i) == moduleName)
                Modul::modulesIgnoredStr.erase(Modul::modulesIgnoredStr.begin() + i);
    }
    else {
        info("Ignore %s", moduleName.c_str());
        Modul::modulesIgnoredStr.push_back(moduleName);
    }
}


//LED blinking variables
bool LEDvars::blink(float deltaTime, float threshold) {
    if(deltaTime >= 0)
        value += deltaTime;
    else
        value = -deltaTime;
    if (value > (2*threshold))
        value = 0;
    valueRounded = (value > threshold)?(1):(0);
    if(valueRoundedOld != valueRounded) {
        valueRoundedOld = valueRounded;
        return true;
    }
    return false;
}

// Specify the Module and ModuleWidget subclass, human-readable
// author name for categorization per plugin, module slug (should never
// change), human-readable module name, and any number of tags
// (found in `include/tags.hpp`) separated by commas.
Model *physicalSync = Model::create<PhysicalSync, PhysicalSyncWidget>("Modiy", "ModiySync", "Physical Sync", UTILITY_TAG,EXTERNAL_TAG);
