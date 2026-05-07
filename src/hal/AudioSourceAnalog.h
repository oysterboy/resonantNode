#pragma once

#include "AudioSource.h"
#include "AnalogInHal.h"

class AudioSourceAnalog : public AudioSource {
public:
    explicit AudioSourceAnalog(int pin);

    void begin() override;
    bool available() override;
    bool readSample(int& sample, uint32_t& sampleTimeUs) override;
    bool readRawSample(int& sample, uint32_t& sampleTimeUs) override;

private:
    AnalogInHal _analogIn;
};
