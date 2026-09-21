#pragma once

#include <stddef.h>
#include <stdint.h>

#include "FeatureStream.h"
#include "MagnitudeWindow.h"

namespace detection {

/*
FeatureHistory

Bounded history for measured feature streams used by retrospective inspection.
Stores feature values and window summaries by timestamp.
Does not decide occurrence validity or pattern meaning.

Streams are recorded on demand: only the streams the active inspection plan
names get a buffer (setActiveStreams()). Anything else passed to record() is
dropped at the door, costing neither RAM nor accumulation work. Producers
(FeatureExtractor) keep offering every stream they have; this class decides
which ones are worth keeping, based on what will actually be read back.
*/
class FeatureHistory {
public:
    // 256 bins x kBinDurationMs is not a round-number guess: the longest
    // occurrence any profile accepts is 240 ms (AmpExperimental
    // maxTransientDurationMs) and inspection may look back windowPreMs = 10
    // before its anchor, so 250 ms of history must still be resident when a
    // maximal occurrence closes and is inspected. Shrinking this silently
    // degrades long occurrences to HistoryWindowIncomplete.
    static constexpr size_t kBinsPerStream = 256;
    // One slot per inspection module: a plan can name at most
    // kMaxInspectionModules distinct streams, so this many slots is
    // sufficient by construction. DetectionRuntime static_asserts the two
    // stay in step (this header must not depend on InspectorTypes.h).
    static constexpr size_t kMaxActiveStreams = 3;
    static constexpr size_t kNoSlot = static_cast<size_t>(-1);
    static constexpr unsigned long kBinDurationMs = 1UL;
    static constexpr size_t debugFeatureBinSize();

    // Binds up to kMaxActiveStreams distinct, known streams and clears every
    // buffer. Unknown ids and duplicates are skipped. Returns how many were
    // bound, so a caller can notice a plan that asked for more than fit.
    size_t setActiveStreams(const FeatureStreamId* streams, size_t count);
    size_t activeStreamCount() const;
    FeatureStreamId activeStream(size_t slot) const;

    // Clears buffered samples; keeps the active-stream binding.
    void reset();

    void record(const FeatureStream& sample, bool fresh = true);
    void record(FeatureStreamId id, unsigned long timeMs, float value, bool fresh = true);

    MagnitudeWindow getWindow(
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
    // Only the fields representativeValueForStream() reads are stored per
    // bin. This struct is allocated kBinsPerStream * kMaxActiveStreams times
    // (768 today), so an unread float here costs 3 KB of RAM. Window-level
    // rms/peak in MagnitudeWindow are computed across bins from each bin's
    // representative value, not from per-bin aggregates, so per-bin rms and
    // peak were write-only and are not kept.
    struct FeatureHistoryBin {
        unsigned long startMs = 0;
        uint16_t inputCount = 0;
        uint16_t freshCount = 0;
        float mean = 0.0f;
        float meanAbs = 0.0f;
        float last = 0.0f;
        bool valid = false;
    };

    struct FeatureBinAccumulator {
        unsigned long startMs = 0;
        uint16_t inputCount = 0;
        uint16_t freshCount = 0;
        double sum = 0.0;
        double sumAbs = 0.0;
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

    static bool isKnownStream(FeatureStreamId stream);
    size_t slotFor(FeatureStreamId stream) const;
    static bool streamRequiresFreshAggregation(FeatureStreamId stream);
    static float representativeValueForStream(FeatureStreamId stream, const FeatureHistoryBin& bin);
    static float representativeValueForAccumulator(FeatureStreamId stream, const FeatureBinAccumulator& bin);
    static void resetStream(StreamBuffer& buffer);
    static void startCurrentBin(StreamBuffer& buffer, unsigned long timeMs);
    static void accumulateIntoCurrentBin(StreamBuffer& buffer, float value, bool fresh);
    static void finalizeCurrentBin(StreamBuffer& buffer, FeatureStreamId stream);

    FeatureStreamId _slotStream[kMaxActiveStreams] = {};
    size_t _activeStreamCount = 0;
    StreamBuffer _streams[kMaxActiveStreams] = {};
};

} // namespace detection

constexpr size_t detection::FeatureHistory::debugFeatureBinSize() {
    return sizeof(FeatureHistoryBin);
}
