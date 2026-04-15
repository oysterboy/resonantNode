#pragma once

#include "AudioSource.h"
#include "AnalogInHal.h"

class AudioSourceAnalog : public AudioSource {
public:
    explicit AudioSourceAnalog(int pin);

    void begin() override;
    int readSample() override;

private:
    AnalogInHal _analogIn;
};
