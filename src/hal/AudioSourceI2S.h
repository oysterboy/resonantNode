#pragma once

#include <cstddef>
#include <stdint.h>

#include "AudioSource.h"

class AudioSourceI2S : public AudioSource {
public:
    AudioSourceI2S(int sckPin, int fsPin, int dataInPin, int sampleRate = 16000, int bitsPerSample = 32);

    void begin() override;
    bool available() override;
    bool readSample(int& sample, uint32_t& sampleTimeUs) override;
    unsigned long droppedSamples() const override;
    unsigned long bufferedSamplesMax() const override;

private:
    struct BufferedSample {
        int sample = 0;
        uint32_t sampleTimeUs = 0;
    };

    void refillBuffer();
    void pushSample(int sample, uint32_t sampleTimeUs);

    int _sckPin;
    int _fsPin;
    int _dataInPin;
    int _sampleRate;
    int _bitsPerSample;
    bool _started = false;
    static constexpr size_t kBufferCapacity = 256;
    static constexpr size_t kRefillBatchSize = 32;
    BufferedSample _buffer[kBufferCapacity];
    size_t _bufferStart = 0;
    size_t _bufferCount = 0;
    unsigned long _droppedSamples = 0;
    size_t _maxBufferedSamples = 0;
    uint32_t _samplePeriodUs = 0;
};
