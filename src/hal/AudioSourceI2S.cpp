#include "AudioSourceI2S.h"
#include <I2S.h>

namespace {
int normalizeToAdcScale(int32_t rawSample) {
    // The analog path feeds 12-bit ADC-style values around 0..4095.
    // Map the signed I2S sample into the same range so the rest of the
    // pipeline can reuse the analog tuning unchanged.
    const int32_t signed12 = rawSample >> 20;
    const int32_t clamped = signed12 < -2048 ? -2048 : (signed12 > 2047 ? 2047 : signed12);
    return static_cast<int>(clamped + 2048);
}
}

AudioSourceI2S::AudioSourceI2S(int sckPin, int fsPin, int dataInPin, int sampleRate, int bitsPerSample)
    : _sckPin(sckPin),
      _fsPin(fsPin),
      _dataInPin(dataInPin),
      _sampleRate(sampleRate),
      _bitsPerSample(bitsPerSample) {}

void AudioSourceI2S::begin() {
    // Keep the public contract sample-based even though I2S may buffer internally.
    _started = false;

    I2S.end();
    I2S.setAllPins(_sckPin, _fsPin, _dataInPin, I2S_PIN_NO_CHANGE, I2S_PIN_NO_CHANGE);
    const int beginResult = I2S.begin(I2S_PHILIPS_MODE, _sampleRate, _bitsPerSample);
    _started = beginResult != 0;
}

int AudioSourceI2S::readSample() {
    if (!_started) {
        return 0;
    }

    // Many I2S MEMS mics publish signed audio in a wider slot.
    // Normalize into the same ADC-like scale used by the analog source.
    const int32_t rawSample = I2S.read();
    return normalizeToAdcScale(rawSample);
}
