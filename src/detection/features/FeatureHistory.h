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
    static constexpr size_t kBinsPerStream = 256;
    static constexpr size_t kStreamCount = 4;
    static constexpr unsigned long kBinDurationMs = 1UL;
    static constexpr size_t debugFeatureBinSize();

    void reset();

    void record(const FeatureStream& sample, bool fresh = true);
    void record(FeatureStreamId id, unsigned long timeMs, float value, bool fresh = true);

    ScalarWindow getWindow(
        FeatureStreamId stream,
        unsigned long startMs,
        unsigned long endMs,
        unsigned long inspectionNowMs,
        float sustainedThreshold = 0.0f
    ) const;
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
    struct FeatureHistoryBin {
        unsigned long startMs = 0;
        uint16_t inputCount = 0;
        uint16_t freshCount = 0;
        float mean = 0.0f;
        float rms = 0.0f;
        float peak = 0.0f;
        float meanAbs = 0.0f;
        float last = 0.0f;
        bool valid = false;
    };

    struct FeatureBinAccumulator {
        unsigned long startMs = 0;
        uint16_t inputCount = 0;
        uint16_t freshCount = 0;
        double sum = 0.0;
        double sumSquares = 0.0;
        double sumAbs = 0.0;
        float peak = 0.0f;
        float last = 0.0f;
        bool valid = false;
    };

    struct StreamBuffer {
        FeatureHistoryBin bins[kBinsPerStream] = {};
        FeatureBinAccumulator current = {};
        size_t binCount = 0;
        size_t writeIndex = 0;
        bool hasCurrent = false;
        unsigned long latestTimeMs = 0;
        float latestValue = 0.0f;
    };

    static bool isSupportedStream(FeatureStreamId stream);
    static size_t streamIndex(FeatureStreamId stream);
    static bool streamRequiresFreshAggregation(FeatureStreamId stream);
    static float representativeValueForStream(FeatureStreamId stream, const FeatureHistoryBin& bin);
    static float representativeValueForAccumulator(FeatureStreamId stream, const FeatureBinAccumulator& bin);
    static void resetStream(StreamBuffer& buffer);
    static void startCurrentBin(StreamBuffer& buffer, unsigned long timeMs);
    static void accumulateIntoCurrentBin(StreamBuffer& buffer, float value, bool fresh);
    static void finalizeCurrentBin(StreamBuffer& buffer, FeatureStreamId stream);

    StreamBuffer _streams[kStreamCount] = {};
};

} // namespace detection

constexpr size_t detection::FeatureHistory::debugFeatureBinSize() {
    return sizeof(FeatureHistoryBin);
}
