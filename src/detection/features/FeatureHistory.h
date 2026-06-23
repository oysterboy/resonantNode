#pragma once

#include <stddef.h>
#include <stdint.h>

#include "FeatureStream.h"
#include "ScalarWindow.h"

namespace detection {

/*
FeatureHistory

Bounded history for measured feature streams used by retrospective inspection.
Stores feature values and window summaries by timestamp.
Does not decide occurrence validity or pattern meaning.
*/
class FeatureHistory {
public:
    static constexpr size_t kMaxSamplesPerStream = 128;
    static constexpr size_t kStreamCount = 4;
    static constexpr size_t debugFeatureBinSize();

    void reset();

    void record(const FeatureStream& sample);
    void record(FeatureStreamId id, unsigned long timeMs, float value);

    ScalarWindow getWindow(FeatureStreamId stream, unsigned long startMs, unsigned long endMs, float sustainedThreshold = 0.0f) const;
    size_t copyWindowApproximateValues(
        FeatureStreamId stream,
        unsigned long startMs,
        unsigned long endMs,
        float* outValues,
        size_t capacity
    ) const;

    size_t sampleCount(FeatureStreamId stream) const;
    bool hasSamples(FeatureStreamId stream) const;
    unsigned long latestTimeMs(FeatureStreamId stream) const;
    float latestValue(FeatureStreamId stream) const;

private:
    struct FeatureBin {
        unsigned long timeMs = 0;
        float first = 0.0f;
        float last = 0.0f;
        float min = 0.0f;
        float max = 0.0f;
        float sum = 0.0f;
        float sumSquares = 0.0f;
        size_t count = 0;
    };

    struct RawSample {
        unsigned long timeMs = 0;
        float value = 0.0f;
    };

    struct StreamBuffer {
        FeatureBin bins[kMaxSamplesPerStream] = {};
        RawSample samples[kMaxSamplesPerStream] = {};
        size_t binCount = 0;
        size_t writeIndex = 0;
        size_t valueCount = 0;
        size_t sampleWriteIndex = 0;
    };

    static bool isSupportedStream(FeatureStreamId stream);
    static size_t streamIndex(FeatureStreamId stream);

    void pushRawSample(StreamBuffer& buffer, const FeatureStream& sample);
    void pushSample(StreamBuffer& buffer, const FeatureStream& sample);

    StreamBuffer _streams[kStreamCount] = {};
};

} // namespace detection

constexpr size_t detection::FeatureHistory::debugFeatureBinSize() {
    return sizeof(FeatureBin);
}
