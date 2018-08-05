#include "OSCManagement.hpp"

OSCManagement::OSCManagement(OSCRemote *_oscRemote, std::string _dispatcherIp, int _dispatcherPort) {
    oscRemote = _oscRemote;
    dispatcherIp   = _dispatcherIp.c_str();
    dispatcherPort = _dispatcherPort;
}

//OSC message processing
void OSCManagement::ProcessMessage(const osc::ReceivedMessage& m, const IpEndpointName& remoteEndpoint) {
    try {
        if(false) {
            info("OSC message in %s with %d arguments", m.AddressPattern(), m.ArgumentCount());
            osc::ReceivedMessage::const_iterator arg = m.ArgumentsBegin();
            for(int i = 0 ; i < m.ArgumentCount() ; i++)
                info("\t%d = %f", i, asNumber(arg++));
        }

        //Ping
        if(std::strcmp(m.AddressPattern(), "/pong") == 0) {
            oscRemote->pongReceived();
        }
        //Potentiometers change
        else if((((std::strcmp(m.AddressPattern(), "/potentiometer/set/absolute") == 0) || (std::strcmp(m.AddressPattern(), "/potentiometer/set/norm") == 0) || (std::strcmp(m.AddressPattern(), "/potentiometer/set/norm/10bits") == 0)) ||
                 ((std::strcmp(m.AddressPattern(), "/potentiometer/add/absolute") == 0) || (std::strcmp(m.AddressPattern(), "/potentiometer/add/norm") == 0) || (std::strcmp(m.AddressPattern(), "/potentiometer/set/norm/10bits") == 0)) || (std::strcmp(m.AddressPattern(), "/A") == 0)) && (m.ArgumentCount())) {
            osc::ReceivedMessage::const_iterator arg = m.ArgumentsBegin();
            int moduleId = -1, potentiometerId = -1;
            float potentiometerValue = -1;
            if(m.ArgumentCount() == 3) {
                moduleId = asNumber(arg++);
                potentiometerId = asNumber(arg++);
                potentiometerValue = asNumber(arg++);
            }
            else if(m.ArgumentCount() == 2) {
                oscRemote->mapFromPotentiometer(asNumber(arg++), &moduleId, &potentiometerId);
                potentiometerValue = asNumber(arg++);
            }
            if (std::strcmp(m.AddressPattern(), "/potentiometer/set/absolute") == 0)     oscRemote->setPotentiometer(moduleId, potentiometerId, potentiometerValue);
            if (std::strcmp(m.AddressPattern(), "/potentiometer/set/norm") == 0)         oscRemote->setPotentiometer(moduleId, potentiometerId, potentiometerValue, true);
            if((std::strcmp(m.AddressPattern(), "/potentiometer/set/norm/10bits") == 0) || (std::strcmp(m.AddressPattern(), "/A") == 0))  oscRemote->setPotentiometer(moduleId, potentiometerId, potentiometerValue/1023., true);
            if (std::strcmp(m.AddressPattern(), "/potentiometer/add/absolute") == 0)     oscRemote->setPotentiometer(moduleId, potentiometerId, potentiometerValue, false, true);
            if (std::strcmp(m.AddressPattern(), "/potentiometer/add/norm") == 0)         oscRemote->setPotentiometer(moduleId, potentiometerId, potentiometerValue, true, true);
            if (std::strcmp(m.AddressPattern(), "/potentiometer/add/norm/10bits") == 0)  oscRemote->setPotentiometer(moduleId, potentiometerId, potentiometerValue/1023., true, true);
        }
        else if((std::strcmp(m.AddressPattern(), "/potentiometer/reset") == 0) && (m.ArgumentCount())) {
            osc::ReceivedMessage::const_iterator arg = m.ArgumentsBegin();
            int moduleId = -1, potentiometerId = -1;
            if(m.ArgumentCount() == 2) {
                moduleId = asNumber(arg++);
                potentiometerId = asNumber(arg++);
            }
            else if(m.ArgumentCount() == 1)
                oscRemote->mapFromPotentiometer(asNumber(arg++), &moduleId, &potentiometerId);
            oscRemote->resetPotentiometer(moduleId, potentiometerId);
        }

        //Switches change
        else if(((std::strcmp(m.AddressPattern(), "/switch") == 0) || (std::strcmp(m.AddressPattern(), "/S") == 0)) && (m.ArgumentCount())) {
            osc::ReceivedMessage::const_iterator arg = m.ArgumentsBegin();
            int moduleId = -1, switchId = -1;
            float switchValue = -1;
            if(m.ArgumentCount() == 3) {
                moduleId    = asNumber(arg++);
                switchId    = asNumber(arg++);
                switchValue = asNumber(arg++);
            }
            else if(m.ArgumentCount() == 2) {
                oscRemote->mapFromSwitch(asNumber(arg++), &moduleId, &switchId);
                switchValue = asNumber(arg++);
            }
            oscRemote->setSwitch(moduleId, switchId, switchValue);
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
                bool firstIsInput = false, secondIsInput = false;
                oscRemote->mapFromJack(asNumber(arg++), &outputModuleId, &outputId, &firstIsInput);
                oscRemote->mapFromJack(asNumber(arg++), &inputModuleId,  &inputId,  &secondIsInput);
                if((firstIsInput) && (!secondIsInput)) {
                    info("Inversion des arguments : output —> input");
                    int outputModuleIdTmp = outputModuleId, outputIdTmp = outputId;
                    outputModuleId = inputModuleId;     outputId = inputId;     firstIsInput  = !firstIsInput;
                    inputModuleId  = outputModuleIdTmp; inputId  = outputIdTmp; secondIsInput = !secondIsInput;
                }
                active = asNumber(arg++);
            }
            oscRemote->setConnection(outputModuleId, outputId, inputModuleId, inputId, active);
        }
        else if(std::strcmp(m.AddressPattern(), "/link/clear") == 0)
            oscRemote->clearConnection();

        //Dump asked
        else if(std::strcmp(m.AddressPattern(), "/dump/modules") == 0)
            oscRemote->dumpModules(m.AddressPattern());
        else if(std::strcmp(m.AddressPattern(), "/dump/potentiometers") == 0)
            oscRemote->dumpPotentiometers(m.AddressPattern());
        else if(std::strcmp(m.AddressPattern(), "/dump/switches") == 0)
            oscRemote->dumpSwitches(m.AddressPattern());
        else if(std::strcmp(m.AddressPattern(), "/dump/jacks") == 0)
            oscRemote->dumpJacks(m.AddressPattern());
        else if(std::strcmp(m.AddressPattern(), "/dump/leds") == 0)
            oscRemote->dumpLEDs(m.AddressPattern());
        else if(std::strcmp(m.AddressPattern(), "/L") == 0)
            oscRemote->dumpLEDs("/serial", true);

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
    UdpTransmitSocket transmitSocket(IpEndpointName(dispatcherIp, dispatcherPort));
    osc::OutboundPacketStream packet(buffer, 1024);

    packet << osc::BeginMessage(address) << message.c_str() << osc::EndMessage;
    transmitSocket.Send(packet.Data(), packet.Size());
}
void OSCManagement::send(const char *address, int valueIndex, unsigned int moduleId, unsigned int valueId, const Vec &position, float valueAbsolute, float valueNormalized, int extraInfo) {
    UdpTransmitSocket transmitSocket(IpEndpointName(dispatcherIp, dispatcherPort));
    osc::OutboundPacketStream packet(buffer, 1024);

    packet << osc::BeginMessage(address) << valueIndex << (int)moduleId << (int)valueId << position.x << position.y << valueAbsolute << valueNormalized;
    if(extraInfo != -9999)
        packet << extraInfo;
    packet << osc::EndMessage;
    transmitSocket.Send(packet.Data(), packet.Size());
}
void OSCManagement::send(const char *address, unsigned int moduleId, std::string slug, std::string name, const Vec &position, const Vec &size, unsigned int nbInputs, unsigned int nbOutputs, unsigned int nbPotentiometers, unsigned int nbSwitches, unsigned int nbLEDs) {
    UdpTransmitSocket transmitSocket(IpEndpointName(dispatcherIp, dispatcherPort));
    osc::OutboundPacketStream packet(buffer, 1024);

    packet << osc::BeginMessage(address) << (int)moduleId << slug.c_str() << name.c_str() << position.x << position.y << size.x << size.y  << (int)nbInputs << (int)nbOutputs << (int)nbPotentiometers << (int)nbSwitches << (int)nbLEDs << osc::EndMessage;
    transmitSocket.Send(packet.Data(), packet.Size());
}
void OSCManagement::send(const char *address, float value) {
    UdpTransmitSocket transmitSocket(IpEndpointName(dispatcherIp, dispatcherPort));
    osc::OutboundPacketStream packet(buffer, 1024);

    packet << osc::BeginMessage(address) << value << osc::EndMessage;
    transmitSocket.Send(packet.Data(), packet.Size());
}
void OSCManagement::send(const char *address) {
    UdpTransmitSocket transmitSocket(IpEndpointName(dispatcherIp, dispatcherPort));
    osc::OutboundPacketStream packet(buffer, 1024);

    packet << osc::BeginMessage(address) << osc::EndMessage;
    transmitSocket.Send(packet.Data(), packet.Size());
}
