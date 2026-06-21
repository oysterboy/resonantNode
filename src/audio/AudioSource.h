#pragma once

#include <stdint.h>

/*
AudioSource

Owns the hardware-facing audio input contract.
Provides raw and block sample access plus approximate sample timing.
Does not perform occurrence detection or classification.
*/
struct AudioSourceStats {
    uint32_t reads = 0;
    uint32_t readBytes = 0;
    uint32_t zeroReads = 0;
    uint32_t shortReads = 0;
    uint32_t maxReadBytes = 0;
    uint32_t noSampleLoops = 0;

    uint32_t readErrors = 0;
    uint32_t overflowCount = 0;

    uint64_t totalSamplesRead = 0;
};

struct AudioBlock {
    const int32_t* samples = nullptr;
    uint16_t sampleCount = 0;

    uint64_t startSampleIndex = 0;
    // Approximate wall-clock time of samples[0].
    uint32_t approxStartMicros = 0;

    bool overflowBeforeBlock = false;
};

class AudioSource {
public:
    virtual ~AudioSource() = default;
    virtual void begin() = 0;
    virtual bool available() = 0;
    virtual int availableBytes() const { return -1; }
    virtual bool readSample(int& sample, uint32_t& sampleTimeUs) = 0;
    virtual bool readRawSample(int& sample, uint32_t& sampleTimeUs) { return readSample(sample, sampleTimeUs); }
    virtual bool readBlock(AudioBlock& block) { (void)block; return false; }
    virtual uint32_t sampleRateHz() const { return 0; }
    virtual const AudioSourceStats& stats() const {
        static const AudioSourceStats emptyStats{};
        return emptyStats;
    }
    virtual void resetStats() {}
};
