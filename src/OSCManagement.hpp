#ifndef OSCMANAGEMENT_H
#define OSCMANAGEMENT_H

#define OSC_BUFFER_SIZE 4096*4

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
    virtual void setPotentiometer(unsigned int moduleId, unsigned int potentiometerId, float potentiometerValue, bool isNormalized = false, bool additive = false) = 0;
    virtual void setConnection(unsigned int outputModuleId, unsigned int outputPortId, unsigned int inputModuleId, unsigned int inputPortId, bool active) = 0;
    virtual void setSwitch(unsigned int moduleId, unsigned int switchId, float switchValue) = 0;
    virtual void clearConnection() = 0;
    virtual void resetPotentiometer(unsigned int moduleId, unsigned int potentiometerId) = 0;

    //Absolute ID mapping to module (and reverse)
    virtual bool mapFromPotentiometer(unsigned int index, int *moduleId, int *potentiometerId) = 0;
    virtual int  mapToPotentiometer(unsigned int moduleId, unsigned int potentiometerId) = 0;
    virtual bool mapFromSwitch(unsigned int index, int *moduleId, int *switchId) = 0;
    virtual int  mapToSwitch(unsigned int moduleId, unsigned int switchId) = 0;
    virtual bool mapFromJack (unsigned int index, int *moduleId, int *inputOrOutputId, bool *isInput) = 0;
    virtual int  mapToJack(unsigned int moduleId, unsigned int inputOrOutputId, bool isInput) = 0;
    virtual int  mapToLED(unsigned int moduleId, unsigned int ledId) = 0;

    //Dump of data
    virtual void dumpLEDs          (const char *address, bool inLine = false) = 0;
    virtual void dumpModules       (const char *address) = 0;
    virtual void dumpPotentiometers(const char *address) = 0;
    virtual void dumpSwitches      (const char *address) = 0;
    virtual void dumpJacks         (const char *address) = 0;

    //Communication
    virtual void send(const char *address) = 0;
    virtual void pongReceived() = 0;
};


//OSC Management
class OSCManagement : public osc::OscPacketListener {
public:
    OSCManagement(OSCRemote *_oscRemote, const std::string &_rtBrokerIp = "127.0.0.1", int _rtBrokerPort = 4001);

private:
    OSCRemote *oscRemote;
public:
    char buffer[OSC_BUFFER_SIZE];
    const char* rtBrokerIp;
    int rtBrokerPort;

public:
    void send(const char *address, float value);
    void send(const char *address);

protected:
    virtual void ProcessMessage(const osc::ReceivedMessage& m, const IpEndpointName& remoteEndpoint);
    float asNumber(const osc::ReceivedMessage::const_iterator &arg) const;
};


#endif // OSCMANAGEMENT_H
