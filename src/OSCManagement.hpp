#ifndef OSCMANAGEMENT_H
#define OSCMANAGEMENT_H

//Common
#include "rack.hpp"

//Includes for OSC server
#include <thread>
#include "osc/OscReceivedElements.h"
#include "osc/OscPacketListener.h"
#include "osc/OscOutboundPacketStream.h"
#include "ip/UdpSocket.h"

//Namespaces
using namespace rack;

//Virtual class for OSC remote control
class OSCRemote {
public:
    //Setters
    virtual void setParameter(unsigned int moduleId, unsigned int paramId, float paramValue, bool isNormalized = false, bool additive = false) = 0;
    virtual void setConnection(unsigned int outputModuleId, unsigned int outputPortId, unsigned int inputModuleId, unsigned int inputPortId, bool active) = 0;
    virtual void clearConnection() = 0;
    virtual void resetParameter(unsigned int moduleId, unsigned int paramId) = 0;

    //Absolute ID mapping to module (and reverse)
    virtual bool mapFromPotentiometer(unsigned int index, int *moduleId, int *paramId) = 0;
    virtual int  mapToPotentiometer(unsigned int moduleId, unsigned int paramId) = 0;
    virtual bool mapFromJack (unsigned int index, int *moduleId, int *inputOrOutputId, bool *isInput) = 0;
    virtual int  mapToJack(unsigned int moduleId, unsigned int inputOrOutputId, bool isInput) = 0;
    virtual int  mapToLED(unsigned int moduleId, unsigned int lightId) = 0;

    //Dump of data
    virtual void dumpLights    (const char *address) = 0;
    virtual void dumpModules   (const char *address) = 0;
    virtual void dumpParameters(const char *address) = 0;
    virtual void dumpJacks     (const char *address) = 0;

    //Communication
    virtual void pongReceived() = 0;
};


//OSC Management
class OSCManagement : public osc::OscPacketListener {
public:
    OSCManagement(OSCRemote *_oscRemote, std::string _dispatcherIp = "127.0.0.1", int _dispatcherPort = 4001);

private:
    char buffer[1024];
    OSCRemote *oscRemote;
    const char* dispatcherIp;
    int dispatcherPort;

public:
    void send(const char *address, std::string message);
    void send(const char *address, int valueIndex, unsigned int moduleId, unsigned int valueId, const Vec &position, float valueAbsolute, float valueNormalized, int extraInfo = -9999);
    void send(const char *address, unsigned int moduleId, std::string slug, std::string name, const Vec &position, const Vec &size, unsigned int nbInputs, unsigned int nbOutputs, unsigned int nbParameters, unsigned int nbLights);
    void send(const char *address, float value);
    void send(const char *address);

protected:
    virtual void ProcessMessage(const osc::ReceivedMessage& m, const IpEndpointName& remoteEndpoint);
    float asNumber(const osc::ReceivedMessage::const_iterator &arg) const;
};


#endif // OSCMANAGEMENT_H
