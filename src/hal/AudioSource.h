#pragma once

#include <stdint.h>

class AudioSource {
public:
    virtual ~AudioSource() = default;
    virtual void begin() = 0;
    virtual bool available() = 0;
    virtual bool readSample(int& sample, uint32_t& sampleTimeUs) = 0;
    virtual unsigned long droppedSamples() const { return 0; }
    virtual unsigned long bufferedSamplesMax() const { return 0; }
};
