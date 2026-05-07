#pragma once

#include <stdint.h>

struct AudioSourceStats {
    uint32_t reads = 0;
    uint32_t readBytes = 0;
    uint32_t zeroReads = 0;
    uint32_t shortReads = 0;
    uint32_t maxReadBytes = 0;
    uint32_t noSampleLoops = 0;

    uint32_t readErrors = 0;
    uint32_t overflowCount = 0;
    uint32_t droppedBlockCount = 0;

    uint64_t totalSamplesRead = 0;
};

struct AudioBlock {
    const int32_t* samples = nullptr;
    uint16_t sampleCount = 0;

    uint64_t startSampleIndex = 0;
    uint32_t approxStartMicros = 0;

    bool overflowBeforeBlock = false;
};

class AudioSource {
public:
    virtual ~AudioSource() = default;
    virtual void begin() = 0;
    virtual bool available() = 0;
    virtual bool readSample(int& sample, uint32_t& sampleTimeUs) = 0;
    virtual bool readRawSample(int& sample, uint32_t& sampleTimeUs) { return readSample(sample, sampleTimeUs); }
    virtual bool readBlock(AudioBlock& block) { (void)block; return false; }
    virtual unsigned long droppedSamples() const { return 0; }
    virtual unsigned long bufferedSamplesMax() const { return 0; }
    virtual uint32_t sampleRateHz() const { return 0; }
    virtual const AudioSourceStats& stats() const {
        static const AudioSourceStats emptyStats{};
        return emptyStats;
    }
    virtual void resetStats() {}
};
