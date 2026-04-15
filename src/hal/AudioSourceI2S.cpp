#include "AudioSourceI2S.h"
#include <I2S.h>

AudioSourceI2S::AudioSourceI2S(int sckPin, int fsPin, int dataInPin, int sampleRate, int bitsPerSample)
    : _sckPin(sckPin),
      _fsPin(fsPin),
      _dataInPin(dataInPin),
      _sampleRate(sampleRate),
      _bitsPerSample(bitsPerSample) {}

void AudioSourceI2S::begin() {
    // Keep the public contract sample-based even though I2S may buffer internally.
    I2S.end();
    I2S.setAllPins(_sckPin, _fsPin, I2S_PIN_NO_CHANGE, I2S_PIN_NO_CHANGE, _dataInPin);
    I2S.begin(I2S_PHILIPS_MODE, _sampleRate, _bitsPerSample);
    _started = true;
}

int AudioSourceI2S::readSample() {
    if (!_started) {
        return 0;
    }

    // The current pipeline consumes a single sample per call.
    return I2S.read();
}
