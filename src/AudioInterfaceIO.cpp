#include "AudioInterfaceIO.hpp"


AudioInterfaceIO::~AudioInterfaceIO() {
    // Close stream here before destructing AudioInterfaceIO, so the mutexes are still valid when waiting to close.
    setDevice(-1, 0);
}

void AudioInterfaceIO::processStream(const float *input, float *output, int frames) {
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

void AudioInterfaceIO::onCloseStream() {
    inputBuffer.clear();
    outputBuffer.clear();
}

void AudioInterfaceIO::onChannelsChange() {
}
