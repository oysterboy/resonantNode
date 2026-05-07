#include "AudioSourceAnalog.h"
#include <Arduino.h>

AudioSourceAnalog::AudioSourceAnalog(int pin) : _analogIn(pin) {}

void AudioSourceAnalog::begin() {
    _analogIn.begin();
}

bool AudioSourceAnalog::available() {
    return true;
}

bool AudioSourceAnalog::readSample(int& sample, uint32_t& sampleTimeUs) {
    sample = _analogIn.readRaw();
    sampleTimeUs = micros();
    return true;
}

bool AudioSourceAnalog::readRawSample(int& sample, uint32_t& sampleTimeUs) {
    return readSample(sample, sampleTimeUs);
}
