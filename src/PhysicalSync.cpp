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
    return rootJ;
}

void PhysicalSync::fromJson(json_t *rootJ) {
    json_t *audioJ = json_object_get(rootJ, "audio");
    audioIO.fromJson(audioJ);
}

//Plugin reset
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
        std::string protocolDispatcher;
        #ifdef ARCH_MAC
        protocolDispatcher = assetPlugin(plugin, "res/Protocol Dispatcher.app/Contents/MacOS/Protocol Dispatcher");
        #endif
        #ifdef ARCH_WIN
        protocolDispatcher = assetPlugin(plugin, "res/ProtocolDispatcher.exe");
        #endif
        #ifdef ARCH_LIN
        protocolDispatcher = assetPlugin(plugin, "res/ProtocolDispatcher");
        #endif
        info("Checks if Protocol Dispatcher is present at %s", protocolDispatcher.c_str());
        if(FILE *file = fopen(protocolDispatcher.c_str(), "r")) {
            //Protocol Dispatcher is OK
            fclose(file);
            isProtocolDispatcherFound = true;
        }
    }

    //Only if Modiy as an OSC server
    if(osc) {
        //Ping status
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
            isProtocolDispatcherTalking = false;
        }
        lights[OSC_LIGHT_INT].value = ledStatusInt.valueRounded;


        //LED visibility
        if((oscLightInt) && (oscLightInt->visible == false))
            oscLightInt->visible = true;
        if((oscLightExt) && (oscLightExt->visible == false))
            oscLightExt->visible = true;
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
void PhysicalSync::pongReceived() {
    isProtocolDispatcherTalking = true;
}



//Module widget
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

//Additional menus
void PhysicalSyncWidget::appendContextMenu(Menu *menu) {
    menu->addChild(MenuEntry::create());

    PhysicalSync *physicalSync = dynamic_cast<PhysicalSync*>(module);
    assert(physicalSync);

    //Settings page
    PhysicalSyncMenu *pd_openSettings = MenuItem::create<PhysicalSyncMenu>("Serial port settings");
    pd_openSettings->module = physicalSync;
    pd_openSettings->oscMessage = "/protocoldispatcher/opensettings";
    menu->addChild(pd_openSettings);

    //Network page
    PhysicalSyncMenu *pd_openNetwork = MenuItem::create<PhysicalSyncMenu>("Network system settings");
    pd_openNetwork->module = physicalSync;
    pd_openNetwork->oscMessage = "/protocoldispatcher/opennetwork";
    menu->addChild(pd_openNetwork);

    //Webpage page
    PhysicalSyncMenu *pd_openWebpage = MenuItem::create<PhysicalSyncMenu>("Open webpage");
    pd_openWebpage->module = physicalSync;
    pd_openWebpage->oscMessage = "/protocoldispatcher/openwebpage";
    menu->addChild(pd_openWebpage);
}




//LED blinking variables
bool LEDvars::blink(float deltaTime) {
    if(deltaTime >= 0)
        value += deltaTime;
    else
        value = -deltaTime;
    if (value > 1)
        value = 0;
    valueRounded = (value > 0.5)?(1):(0);
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
Model *physicalSync = Model::create<PhysicalSync, PhysicalSyncWidget>("Modiy", "ModiySync", "Physical Sync", UTILITY_TAG);
