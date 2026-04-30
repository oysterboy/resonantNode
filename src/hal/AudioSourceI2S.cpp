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
    _bufferStart = 0;
    _bufferCount = 0;
    _droppedSamples = 0;
    _maxBufferedSamples = 0;
    _samplePeriodUs = _sampleRate > 0 ? static_cast<uint32_t>(1000000UL / static_cast<uint32_t>(_sampleRate)) : 0;

    I2S.end();
    I2S.setAllPins(_sckPin, _fsPin, _dataInPin, I2S_PIN_NO_CHANGE, I2S_PIN_NO_CHANGE);
    const int beginResult = I2S.begin(I2S_PHILIPS_MODE, _sampleRate, _bitsPerSample);
    _started = beginResult != 0;
}

bool AudioSourceI2S::available() {
    refillBuffer();
    return _bufferCount > 0;
}

bool AudioSourceI2S::readSample(int& sample, uint32_t& sampleTimeUs) {
    refillBuffer();
    if (_bufferCount == 0) {
        return false;
    }

    const BufferedSample frame = _buffer[_bufferStart];
    _bufferStart = (_bufferStart + 1) % kBufferCapacity;
    --_bufferCount;

    sample = frame.sample;
    sampleTimeUs = frame.sampleTimeUs;
    return true;
}

unsigned long AudioSourceI2S::droppedSamples() const {
    return _droppedSamples;
}

unsigned long AudioSourceI2S::bufferedSamplesMax() const {
    return static_cast<unsigned long>(_maxBufferedSamples);
}

void AudioSourceI2S::refillBuffer() {
    if (!_started) {
        return;
    }

    const int bytesPerSample = _bitsPerSample / 8;
    if (bytesPerSample <= 0) {
        return;
    }

    int hardwareSamples = I2S.available() / bytesPerSample;
    if (hardwareSamples <= 0) {
        return;
    }

    int rawSamples[kRefillBatchSize];
    int chunkCount = 0;
    while (hardwareSamples > 0 && chunkCount < static_cast<int>(kRefillBatchSize)) {
        rawSamples[chunkCount++] = I2S.read();
        --hardwareSamples;
    }

    const uint32_t fillEndUs = micros();
    for (int i = 0; i < chunkCount; ++i) {
        const uint32_t sampleTimeUs = fillEndUs - static_cast<uint32_t>((chunkCount - 1 - i) * _samplePeriodUs);
        const int sample = normalizeToAdcScale(rawSamples[i]);
        pushSample(sample, sampleTimeUs);
    }
}

void AudioSourceI2S::pushSample(int sample, uint32_t sampleTimeUs) {
    if (_bufferCount == kBufferCapacity) {
        _bufferStart = (_bufferStart + 1) % kBufferCapacity;
        --_bufferCount;
        ++_droppedSamples;
    }

    const size_t writeIndex = (_bufferStart + _bufferCount) % kBufferCapacity;
    _buffer[writeIndex].sample = sample;
    _buffer[writeIndex].sampleTimeUs = sampleTimeUs;
    ++_bufferCount;
    if (_bufferCount > _maxBufferedSamples) {
        _maxBufferedSamples = _bufferCount;
    }
}
