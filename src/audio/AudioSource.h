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

    // Blocks whose estimated start time lies before the previous block's
    // estimated end, i.e. sample time went backwards between two blocks.
    uint32_t timestampBacksteps = 0;
    uint32_t maxTimestampBackstepUs = 0;

    // Sample-clock re-anchors (about once a second) and the largest single
    // correction applied, in either direction.
    uint32_t sampleClockCorrections = 0;
    uint32_t maxSampleClockCorrectionUs = 0;
    // DMA buffers the driver dropped because the reader fell behind (each
    // advances the sample index by one buffer; also counted in
    // overflowCount), and the sample rate the clock has measured, in mHz.
    uint32_t droppedDmaBuffers = 0;
    uint32_t sampleClockRateMilliHz = 0;
};

struct AudioBlock {
    const int32_t* samples = nullptr;
    uint16_t sampleCount = 0;

    uint64_t startSampleIndex = 0;
    // Approximate time of samples[0] on the 64-bit time base (app/TimeBase.h).
    // Absolute ms: timebase::msFromUs64(); never (us32 / 1000) (issue #30).
    uint64_t approxStartUs64 = 0;

    bool overflowBeforeBlock = false;
};

class AudioSource {
public:
    virtual ~AudioSource() = default;
    virtual void begin() = 0;
    virtual bool available() = 0;
    virtual int availableBytes() const { return -1; }
    // sampleTimeUs64: the sample's time on the 64-bit time base (app/TimeBase.h).
    virtual bool readSample(int& sample, uint64_t& sampleTimeUs64) = 0;
    virtual bool readRawSample(int& sample, uint64_t& sampleTimeUs64) { return readSample(sample, sampleTimeUs64); }
    virtual bool readBlock(AudioBlock& block) {
        (void)block;
        return false;
    }
    virtual uint32_t sampleRateHz() const { return 0; }
    virtual const AudioSourceStats& stats() const {
        static const AudioSourceStats emptyStats{};
        return emptyStats;
    }
    virtual void resetStats() {}
};
