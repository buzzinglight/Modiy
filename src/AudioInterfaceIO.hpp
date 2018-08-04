#ifndef AUDIOINTERFACEIO_H
#define AUDIOINTERFACEIO_H

//Common
#include "rack.hpp"

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

//Namespaces
using namespace rack;

//Classes managing audio
struct AudioInterfaceIO : AudioIO {
public:
    std::mutex engineMutex;
    std::condition_variable engineCv;
    std::mutex audioMutex;
    std::condition_variable audioCv;
    // Audio thread produces, engine thread consumes
    DoubleRingBuffer<Frame<AUDIO_INPUTS>, (1<<15)> inputBuffer;
    // Audio thread consumes, engine thread produces
    DoubleRingBuffer<Frame<AUDIO_OUTPUTS>, (1<<15)> outputBuffer;
    bool active = false;

public:
    ~AudioInterfaceIO();
    void processStream(const float *input, float *output, int frames) override;
    void onCloseStream() override;
    void onChannelsChange() override;
};


#endif // AUDIOINTERFACEIO_H
