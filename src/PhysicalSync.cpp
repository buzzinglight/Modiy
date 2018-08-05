#include "PhysicalSync.hpp"

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
    return rootJ;
}

void PhysicalSync::fromJson(json_t *rootJ) {
    json_t *audioJ = json_object_get(rootJ, "audio");
    json_t *nbChannels = json_object_get(rootJ, "nbChannels");
    if (nbChannels)
        setChannels(json_integer_value(nbChannels));
    audioIO.fromJson(audioJ);
}

//Plugin reset
void PhysicalSync::onReset() {
    audioIO.setDevice(-1, 0);
}




//Update modules, jacks, potentiometers, switches, LEDs… cache
void PhysicalSync::updateCache(bool force) {
    bool debug = false;
    if((updateCacheNeeded) || (force)) {
        //Order modules by graphical position
        modules.clear();
        for (Widget *w : gRackWidget->moduleContainer->children) {
            ModuleWithId moduleWithId;
            moduleWithId.widget = dynamic_cast<ModuleWidget*>(w);
            if(moduleWithId.widget->module != NULL) {
                //Store potentiometers and switches
                for(unsigned int parameterId = 0 ; parameterId < moduleWithId.widget->params.size() ; parameterId++) {
                    ParamWidget *parameter = moduleWithId.widget->params.at(parameterId);
                    if(parameter->visible) {
                        ToggleSwitch *isToggle = NULL;
                        MomentarySwitch *isMomentary = NULL;
                        try {
                            isToggle = dynamic_cast<ToggleSwitch*>(parameter);
                        } catch(const std::bad_cast& e) {
                            isToggle = NULL;
                        }
                        try {
                            isMomentary = dynamic_cast<MomentarySwitch*>(parameter);
                        } catch(const std::bad_cast& e) {
                            isMomentary = NULL;
                        }

                        if((isToggle) || (isMomentary))
                            moduleWithId.switches.push_back(parameter);
                        else
                            moduleWithId.potentiometers.push_back(parameter);
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
            JackWire wireWithId;
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
                    info("Module %d : %s %s @ (%f %f) (I/O/P/S/L : %d %d %d %d %d)", moduleId, model->slug.c_str(), model->name.c_str(), widget->box.pos.x, widget->box.pos.y, moduleWithId.inputs.size(), moduleWithId.outputs.size(), moduleWithId.potentiometers.size(), moduleWithId.switches.size(), moduleWithId.leds.size());
                    for(unsigned int inputId = 0 ; inputId < moduleWithId.inputs.size() ; inputId++)
                        info("Input %d : %d", inputId, moduleWithId.inputs.at(inputId).input.active);
                    for(unsigned int outputId = 0 ; outputId < moduleWithId.outputs.size() ; outputId++)
                        info("Output %d : %d", outputId, moduleWithId.outputs.at(outputId).output.active);
                    for(unsigned int potentiometerId = 0 ; potentiometerId < moduleWithId.potentiometers.size() ; potentiometerId++)
                        info("Potentiometer %d : %f", potentiometerId, moduleWithId.potentiometers.at(potentiometerId)->value);
                    for(unsigned int switchId = 0 ; switchId < moduleWithId.switches.size() ; switchId++)
                        info("Switch %d : %f", switchId, moduleWithId.switches.at(switchId)->value);
                    for(unsigned int ledId = 0 ; ledId < moduleWithId.leds.size() ; ledId++)
                        info("LED %d : %f", ledId, moduleWithId.leds.at(ledId).light.value);
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
                ModuleWithId moduleWidgetTest = modules.at(moduleId);
                if(moduleWidgetTest.widget->module == wires.at(wireId).widget->wire->inputModule)
                    inputModule = moduleWidgetTest;
                if(moduleWidgetTest.widget->module == wires.at(wireId).widget->wire->outputModule)
                    outputModule = moduleWidgetTest;
            }
            if((inputModule.widget) && (outputModule.widget)) {
                wires[wireId].inputModuleId  = inputModule.moduleId;
                wires[wireId].outputModuleId = outputModule.moduleId;
                if(debug)
                    info("Wire %d link %d - %s %s (%d) to %d - %s %s (%d)", wireId, wires.at(wireId).outputModuleId, outputModule.widget->model->slug.c_str(), outputModule.widget->model->name.c_str(), wires.at(wireId).widget->wire->outputId, wires.at(wireId).inputModuleId, inputModule.widget->model->slug.c_str(), inputModule.widget->model->name.c_str(), wires.at(wireId).widget->wire->inputId);
            }
        }

        updateCacheNeeded = false;
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
void PhysicalSync::logToOsc(std::string message) {
    if(osc)
        osc->send("/log", message);
    info("%s", message.c_str());
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

    //Cache can be updated
    updateCacheNeeded = true;

    //End of startup, after OSC creation, one time only
    if((osc) && (oscServerJustStart)) {
        oscServerJustStart = false;
        isProtocolDispatcherFound = false;

        //Current directory and Protocol Dispatcher path
#ifdef ARCH_MAC
        protocolDispatcherPath = assetPlugin(plugin, "res/Protocol Dispatcher.app/Contents/MacOS/Protocol Dispatcher");
#endif
#ifdef ARCH_WIN
        protocolDispatcherPath = assetPlugin(plugin, "res/ProtocolDispatcher.exe");
#endif
#ifdef ARCH_LIN
        protocolDispatcherPath = assetPlugin(plugin, "res/ProtocolDispatcher");
#endif
        info("Checks if protocol dispatcher app is present at %s", protocolDispatcherPath.c_str());
        if(FILE *file = fopen(protocolDispatcherPath.c_str(), "r")) {
            //Protocol Dispatcher is OK
            fclose(file);
            isProtocolDispatcherFound = true;
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

        //LED status
        float ledStatusIntDelta = deltaTime * 2;
        if(!isProtocolDispatcherFound)
            ledStatusIntDelta = -1;
        else if(!isProtocolDispatcherTalking)
            ledStatusIntDelta = deltaTime / 4;
        if(ledStatusInt.blink(ledStatusIntDelta)) {
            osc->send("/pulse", ledStatusInt.valueRounded);

            //Opens Procotol Dispatcher if needed
            if(isProtocolDispatcherFound) {
                if(!isProtocolDispatcherTalking)
                    isProtocolDispatcherTalkingCounter++;
                isProtocolDispatcherTalking = false;

                if(isProtocolDispatcherTalkingCounter > 3) {
                    isProtocolDispatcherTalkingCounter = -5;
                    info("Trying to start protocol dispatcher app %s…", protocolDispatcherPath.c_str());

                    TinyProcessLib::Process process("\"" + protocolDispatcherPath + "\"");
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
ParamWidget* PhysicalSync::getPotentiometer(unsigned int moduleId, unsigned int potentiometerId) {
    ModuleWithId module = getModule(moduleId);
    if(module.widget) {
        if(potentiometerId < module.potentiometers.size())
            return module.potentiometers.at(potentiometerId);
    }
    return 0;
}
ParamWidget* PhysicalSync::getSwitch(unsigned int moduleId, unsigned int switchId) {
    ModuleWithId module = getModule(moduleId);
    if(module.widget) {
        if(switchId < module.switches.size())
            return module.switches.at(switchId);
    }
    return 0;
}
JackInput PhysicalSync::getInputPort(unsigned int moduleId, unsigned int inputId) {
    ModuleWithId module = getModule(moduleId);
    if(module.widget) {
        if(inputId < module.inputs.size())
            return module.inputs.at(inputId);
    }
    return JackInput();
}
JackOutput PhysicalSync::getOutputPort(unsigned int moduleId, unsigned int outputId) {
    ModuleWithId module = getModule(moduleId);
    if(module.widget) {
        if(outputId < module.outputs.size())
            return module.outputs.at(outputId);
    }
    return JackOutput();
}
ModuleWithId PhysicalSync::getModule(unsigned int moduleId) {
    updateCache();
    if(moduleId < modules.size())
        return modules.at(moduleId);
    return ModuleWithId();
}
LED PhysicalSync::getLED(unsigned int moduleId, unsigned int ledId) {
    ModuleWithId module = getModule(moduleId);
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
        for(unsigned int wireId = 0 ; wireId < wires.size() ; wireId++)
            if((wires.at(wireId).inputModuleId == (int)inputModuleId) && (wires.at(wireId).outputModuleId == (int)outputModuleId))
                if((wires.at(wireId).widget->inputPort->portId == (int)inputPortId) && (wires.at(wireId).widget->outputPort->portId == (int)outputPortId))
                    return wires.at(wireId);
    return JackWire();
}


//Absolute ID mapping to module (and reverse)
bool PhysicalSync::mapFromPotentiometer(unsigned int index, int *moduleId, int *potentiometerId) {
    updateCache();
    ModuleWithId lastModule;
    *moduleId = 0;
    *potentiometerId  = 0;

    //Search for index overflow in modules
    for(unsigned int lastModuleId = 0 ; lastModuleId < modules.size() ; lastModuleId++) {
        lastModule = modules.at(lastModuleId);
        if(index >= lastModule.potentiometers.size()) {
            index -= lastModule.potentiometers.size();
            *moduleId = lastModuleId+1;
        }
        else
            break;
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
    updateCache();
    ModuleWithId lastModule;
    *moduleId = 0;
    *switchId = 0;

    //Search for index overflow in modules
    for(unsigned int lastModuleId = 0 ; lastModuleId < modules.size() ; lastModuleId++) {
        lastModule = modules.at(lastModuleId);
        if(index >= lastModule.switches.size()) {
            index -= lastModule.switches.size();
            *moduleId = lastModuleId+1;
        }
        else
            break;
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
    updateCache();
    ModuleWithId lastModule;
    *isInput         = true;
    *moduleId        = 0;
    *inputOrOutputId = 0;

    //Search for index overflow in modules
    for(unsigned int lastModuleId = 0 ; lastModuleId < modules.size() ; lastModuleId++) {
        lastModule = modules.at(lastModuleId);
        if(index  >= (lastModule.inputs.size()+lastModule.outputs.size())) {
            index -= (lastModule.inputs.size()+lastModule.outputs.size());
            *moduleId = lastModuleId+1;
        }
        else
            break;
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
    updateCache();
    LED led = getLED(moduleId, ledId);
    int index = -1;
    if(led.widget) {
        index = 0;

        //Look for all modules before
        for(unsigned int lastModuleId = 0 ; lastModuleId < moduleId ; lastModuleId++)
            index += modules.at(lastModuleId).leds.size();
        index += ledId;
    }
    else
        warn("Error with mapToLED");
    return index;
}
int PhysicalSync::mapToPotentiometer(unsigned int moduleId, unsigned int potentiometerId) {
    updateCache();
    ParamWidget *potentiometer = getPotentiometer(moduleId, potentiometerId);
    int index = -1;
    if(potentiometer) {
        index = 0;

        //Look for all modules before
        for(unsigned int lastModuleId = 0 ; lastModuleId < moduleId ; lastModuleId++)
            index += modules.at(lastModuleId).potentiometers.size();
        index += potentiometerId;
    }
    else
        warn("Error with mapToPotentiometer");
    return index;
}
int PhysicalSync::mapToSwitch(unsigned int moduleId, unsigned int switchId) {
    updateCache();
    ParamWidget *button = getSwitch(moduleId, switchId);
    int index = -1;
    if(button) {
        index = 0;

        //Look for all modules before
        for(unsigned int lastModuleId = 0 ; lastModuleId < moduleId ; lastModuleId++)
            index += modules.at(lastModuleId).switches.size();
        index += switchId;
    }
    else
        warn("Error with mapToSwitch");
    return index;
}
int PhysicalSync::mapToJack(unsigned int moduleId, unsigned int inputOrOutputId, bool isInput) {
    updateCache();
    Port *port;
    if(isInput) port = getInputPort (moduleId, inputOrOutputId).port;
    else        port = getOutputPort(moduleId, inputOrOutputId).port;
    int index = -1;
    if(port) {
        index = 0;

        //Look for all modules before
        for(unsigned int lastModuleId = 0 ; lastModuleId < moduleId ; lastModuleId++)
            index += (modules.at(lastModuleId).inputs.size()+modules.at(lastModuleId).outputs.size());

        //Add input size offset if it's an output
        if(!isInput)
            index += modules.at(moduleId).inputs.size();

        index += inputOrOutputId;
    }
    else
        warn("Error with mapToJack");
    return index;
}


//Setters
void PhysicalSync::setPotentiometer(unsigned int moduleId, unsigned int potentiometerId, float potentiometerValue, bool isNormalized, bool additive) {
    ParamWidget *potentiometer = getPotentiometer(moduleId, potentiometerId);
    if(potentiometer) {
        if(isNormalized) {
            potentiometerValue = potentiometerValue * (potentiometer->maxValue - potentiometer->minValue);
            if(!additive)
                potentiometerValue += potentiometer->minValue;
        }
        if(additive)
            potentiometerValue += potentiometer->value;
        potentiometer->setValue(potentiometerValue);
    }
    else {
        std::stringstream stream;
        stream << "Module ";
        stream << moduleId;
        stream << " potentiometer ";
        stream << potentiometerId;
        stream << " not found.";
        logToOsc(stream.str());
    }
}
void PhysicalSync::setSwitch(unsigned int moduleId, unsigned int switchId, float switchValue) {
    ParamWidget *button = getSwitch(moduleId, switchId);
    if(button)
        button->setValue((switchValue * (button->maxValue - button->minValue)) + button->minValue);
    else {
        std::stringstream stream;
        stream << "Module ";
        stream << moduleId;
        stream << " switch ";
        stream << switchId;
        stream << " not found.";
        logToOsc(stream.str());
    }
}
void PhysicalSync::resetPotentiometer(unsigned int moduleId, unsigned int potentiometerId) {
    ParamWidget *potentiometer = getPotentiometer(moduleId, potentiometerId);
    if(potentiometer) {
        potentiometer->setValue(potentiometer->defaultValue);
    }
    else {
        std::stringstream stream;
        stream << "Module ";
        stream << moduleId;
        stream << "potentiometer ";
        stream << potentiometerId;
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

    JackWire wire = getWire(inputModuleId, inputPortId, outputModuleId, outputPortId);
    if((!wire.widget) && (active)) {
        JackOutput output = getOutputPort(outputModuleId, outputPortId);
        JackInput  input  = getInputPort (inputModuleId,  inputPortId);
        if((input.port) && (output.port)) {
            //Test if input is inactive
            if(!input.input.active) {
                //Créé le lien
                logToOsc(logTextBase + "creation");
                WireWidget *wireWidget = new WireWidget();
                wireWidget->inputPort  = input.port;
                wireWidget->outputPort = output.port;
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
        ModuleWithId module = getModule(moduleId);
        for(unsigned int inputId = 0 ; inputId < module.inputs.size() ; inputId++)
            gRackWidget->wireContainer->removeAllWires(module.inputs.at(inputId).port);
        for(unsigned int ouputId = 0 ; ouputId < module.outputs.size() ; ouputId++)
            gRackWidget->wireContainer->removeAllWires(module.outputs.at(ouputId).port);
    }
    updateCache();
}


//Dump
void PhysicalSync::dumpLEDs(const char *address, bool inLine) {
    lights[OSC_LED_EXT].value = 1-lights[OSC_LED_EXT].value;
    if(osc) {
        std::string inLineString;
        updateCache();
        if(!inLine)
            osc->send(address, "start");
        for(unsigned int moduleId = 0 ; moduleId < modules.size() ; moduleId++) {
            ModuleWithId module = getModule(moduleId);
            if(module.widget) {
                for(unsigned int ledId = 0 ; ledId < module.leds.size() ; ledId++) {
                    LED led = getLED(moduleId, ledId);
                    if(led.widget) {
                        if(inLine)
                            inLineString += " " + std::to_string((int)(255 * led.light.value));
                        else
                            osc->send(address, mapToLED(moduleId, ledId), moduleId, ledId, led.widget->box.getCenter(), led.light.getBrightness(), led.light.value);
                    }
                }
            }
        }
        if(!inLine)
            osc->send(address, "end");
        else {
            inLineString = "L" + inLineString;
            osc->send(address, inLineString);
        }
    }
}
void PhysicalSync::dumpPotentiometers(const char *address) {
    if(osc) {
        updateCache();
        osc->send(address, "start");
        for(unsigned int moduleId = 0 ; moduleId < modules.size() ; moduleId++) {
            ModuleWithId module = getModule(moduleId);
            if(module.widget) {
                for(unsigned int potentiometerId = 0 ; potentiometerId < module.potentiometers.size() ; potentiometerId++) {
                    ParamWidget *potentiometer = getPotentiometer(moduleId, potentiometerId);
                    if(potentiometer)
                        osc->send(address, mapToPotentiometer(moduleId, potentiometerId), moduleId, potentiometerId, potentiometer->box.getCenter(), potentiometer->value, (potentiometer->value - potentiometer->minValue) / (potentiometer->maxValue - potentiometer->minValue));
                }
            }
        }
        osc->send(address, "end");
    }
}
void PhysicalSync::dumpSwitches(const char *address) {
    if(osc) {
        updateCache();
        osc->send(address, "start");
        for(unsigned int moduleId = 0 ; moduleId < modules.size() ; moduleId++) {
            ModuleWithId module = getModule(moduleId);
            if(module.widget) {
                for(unsigned int switchId = 0 ; switchId < module.switches.size() ; switchId++) {
                    ParamWidget *button = getSwitch(moduleId, switchId);
                    if(button)
                        osc->send(address, mapToSwitch(moduleId, switchId), moduleId, switchId, button->box.getCenter(), button->value, (button->value - button->minValue) / (button->maxValue - button->minValue));
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
                for(unsigned int inputId = 0 ; inputId < module.inputs.size() ; inputId++) {
                    JackInput input = getInputPort(moduleId, inputId);
                    if(input.port)
                        osc->send(address, mapToJack(moduleId, inputId, true), moduleId, inputId, input.port->box.getCenter(), input.input.active, input.input.value, 1);
                }
                for(unsigned int ouputId = 0 ; ouputId < module.outputs.size() ; ouputId++) {
                    JackOutput output = getOutputPort(moduleId, ouputId);
                    if(output.port)
                        osc->send(address, mapToJack(moduleId, ouputId, false), moduleId, ouputId, output.port->box.getCenter(), output.output.active, output.output.value, 0);
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
                osc->send(address, moduleId, module.widget->model->slug, module.widget->model->name, module.widget->box.pos, module.widget->box.size, module.inputs.size(), module.outputs.size(), module.potentiometers.size(), module.switches.size(), module.leds.size());
        }
        osc->send(address, "end");
    }
}
void PhysicalSync::send(const char *message) {
    if(osc)
        osc->send(message);
}
void PhysicalSync::pongReceived() {
    isProtocolDispatcherTalking = true;
    isProtocolDispatcherTalkingCounter = 0;
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

    if(physicalSync->osc) {
        menu->addChild(MenuEntry::create());

        //Settings page
        PhysicalSyncOSCMenu *pd_openSettings = MenuItem::create<PhysicalSyncOSCMenu>("Serial port settings");
        pd_openSettings->physicalSync = physicalSync;
        pd_openSettings->oscMessage = "/protocoldispatcher/opensettings";
        menu->addChild(pd_openSettings);

        //Network page
        PhysicalSyncOSCMenu *pd_openNetwork = MenuItem::create<PhysicalSyncOSCMenu>("Network system settings");
        pd_openNetwork->physicalSync = physicalSync;
        pd_openNetwork->oscMessage = "/protocoldispatcher/opennetwork";
        menu->addChild(pd_openNetwork);

        //Webpage page
        PhysicalSyncOSCMenu *pd_openWebpage = MenuItem::create<PhysicalSyncOSCMenu>("Open webpage");
        pd_openWebpage->physicalSync = physicalSync;
        pd_openWebpage->oscMessage = "/protocoldispatcher/openwebpage";
        menu->addChild(pd_openWebpage);
    }

    if(true) {
        menu->addChild(MenuEntry::create());

        //Preset
        PhysicalSyncAudioMenu *changeChannels2 = MenuItem::create<PhysicalSyncAudioMenu>("Stereo audio I/O", CHECKMARK(physicalSync->getChannels() == 2));
        changeChannels2->physicalSync = physicalSync;
        changeChannels2->nbChannels = 2;
        menu->addChild(changeChannels2);
        PhysicalSyncAudioMenu *changeChannels4 = MenuItem::create<PhysicalSyncAudioMenu>("Quad audio I/O", CHECKMARK(physicalSync->getChannels() == 4));
        changeChannels4->physicalSync = physicalSync;
        changeChannels4->nbChannels = 4;
        menu->addChild(changeChannels4);
        PhysicalSyncAudioMenu *changeChannels8 = MenuItem::create<PhysicalSyncAudioMenu>("Octo audio I/O", CHECKMARK(physicalSync->getChannels() == 8));
        changeChannels8->physicalSync = physicalSync;
        changeChannels8->nbChannels = 8;
        menu->addChild(changeChannels8);
    }
}


//Menu item
void PhysicalSyncOSCMenu::onAction(EventAction &) {
    if(physicalSync)
        physicalSync->send(oscMessage);
}
void PhysicalSyncAudioMenu::onAction(EventAction &) {
    if(physicalSync)
        physicalSync->setChannels(nbChannels);
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
