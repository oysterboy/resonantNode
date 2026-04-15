#include "AudioSourceAnalog.h"

AudioSourceAnalog::AudioSourceAnalog(int pin) : _analogIn(pin) {}

void AudioSourceAnalog::begin() {
    _analogIn.begin();
}

int AudioSourceAnalog::readSample() {
    return _analogIn.readRaw();
}
