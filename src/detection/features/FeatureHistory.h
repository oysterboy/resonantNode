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
#ifndef FEATURE_HISTORY_BINS
// 576 ms: the inspection windows of the longest occurrence (max duration 450 + debounce 10 + the 100 ms windows =
// 560; decisions/2026-10-29-max-duration-450-for-chords.md) and 16 ms; RB plan_check flags a plan that outgrows it.
// Each ms costs 72 bytes (three streams), taken from the Analyzer's RAW block.
#define FEATURE_HISTORY_BINS 576
#endif

class FeatureHistory {
public:
    // The bin count follows the longest occurrence a profile accepts plus
    // the inspection look-back: 256 bins x kBinDurationMs covered the old
    // 240 ms maximum (AmpExperimental) plus windowPreMs = 10. Since
    // 2026-10-16 TonalPulseScalar accepts 300 ms (5-step hop trains), so
    // 512 is the default; it was the build override of the 5b gate
    // battery (Analyzer RAM +18.6 KB). Shrinking this silently degrades
    // long occurrences to HistoryWindowIncomplete. Still a build override
    // (-D FEATURE_HISTORY_BINS=...) for experiments.
    static constexpr size_t kBinsPerStream = FEATURE_HISTORY_BINS;
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

    void record(FeatureStreamId id, unsigned long timeMs, float value, bool fresh = true);

    MagnitudeWindow getWindow(FeatureStreamId stream, unsigned long startMs, unsigned long endMs,
                              unsigned long inspectionNowMs, float sustainedThreshold = 0.0f) const;

    size_t sampleCount(FeatureStreamId stream) const;
    // Diagnostics: span of the resident ring for a stream, and how often a
    // record() arrived with a time earlier than the bin being filled.
    unsigned long oldestBinTimeMs(FeatureStreamId stream) const;
    size_t binCount(FeatureStreamId stream) const;
    uint32_t outOfOrderRecordCount() const;
    unsigned long latestTimeMs(FeatureStreamId stream) const;

private:
    // getWindow()'s value scratch (step 8b item 5): one window at a time, the
    // detection loop is single-threaded.
    mutable float _windowValues[kBinsPerStream + 1U];

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
    };

    static bool isKnownStream(FeatureStreamId stream);
    size_t slotFor(FeatureStreamId stream) const;
    static bool streamRequiresFreshAggregation(FeatureStreamId stream);
    static float representativeValueForStream(FeatureStreamId stream, const FeatureHistoryBin& bin);
    static void resetStream(StreamBuffer& buffer);
    static void startCurrentBin(StreamBuffer& buffer, unsigned long timeMs);
    static void accumulateIntoCurrentBin(StreamBuffer& buffer, float value, bool fresh);
    static void finalizeCurrentBin(StreamBuffer& buffer);

    FeatureStreamId _slotStream[kMaxActiveStreams] = {};
    size_t _activeStreamCount = 0;
    uint32_t _outOfOrderRecords = 0;
    StreamBuffer _streams[kMaxActiveStreams] = {};
};

} // namespace detection

constexpr size_t detection::FeatureHistory::debugFeatureBinSize() {
    return sizeof(FeatureHistoryBin);
}
