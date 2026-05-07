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
    bool readRawSample(int& sample, uint32_t& sampleTimeUs) override;
    bool readBlock(AudioBlock& block) override;
    unsigned long droppedSamples() const override;
    unsigned long bufferedSamplesMax() const override;
    uint32_t sampleRateHz() const override;
    const AudioSourceStats& stats() const override;
    void resetStats() override;
    uint32_t samplePeriodUs() const;

private:
    bool refillBlock();
    void recordReadAttempt(int requestedBytes, int bytesRead, bool readError);

    int _sckPin;
    int _fsPin;
    int _dataInPin;
    int _sampleRate;
    int _bitsPerSample;
    bool _started = false;
    static constexpr size_t kRefillBatchSize = 32;
    int32_t _blockSamples[kRefillBatchSize] = {};
    size_t _blockCount = 0;
    size_t _blockCursor = 0;
    uint64_t _blockStartSampleIndex = 0;
    uint32_t _blockApproxStartMicros = 0;
    bool _blockOverflowBeforeBlock = false;
    unsigned long _droppedSamples = 0;
    size_t _maxBufferedSamples = 0;
    uint32_t _samplePeriodUs = 0;
    AudioSourceStats _stats;
};
