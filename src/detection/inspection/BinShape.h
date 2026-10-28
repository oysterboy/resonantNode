#pragma once

#include <stddef.h>
#include <stdint.h>

#include "../../app/RuntimeDefaults.h"
#include "../occurrences/Occurrence.h"
#include "InspectorTypes.h"

namespace detection {

/*
BinShape (DET-015; decisions/2026-10-26-det015-bin-track-inspector.md)

The one shape rule: the bin runs over a window of a bin source, folded, counted
and classed. The ShapeInspector reads the runtime's BinTrack with it; the host
replay's --shape prints its runs from the same function over its whole-capture
log, so the printed facts are the ones the verdict used (review D2.3,
2026-10-26: the replay had its own copy and had drifted).

A BinSource has `bool at(uint32_t ms, uint32_t& binHz, uint16_t& level) const`
(false: no bin in that ms). A run is one bin held with gaps of at most
kRunGapMs; runs shorter than minRunMs are folded away (bin boundaries flicker
between neighbours for a few ms). A step up by a whole number of bins, up to
maxRisingStepBins, or from the top of the band back to the bottom (a rotation
that wraps) counts as rising. Times are 32-bit ms, wrap-safe: the window is a
start and a span, walked by count (review D1.1).
*/
struct BinRun {
    uint32_t hz = 0;
    uint32_t startMs = 0;
    uint32_t lengthMs = 0;
};

struct BinShapeMeasure {
    static constexpr size_t kMaxRuns = 16;
    OccurrenceShape shape = {};
    BinRun runs[kMaxRuns] = {};  // the kept runs, in order
    size_t runCount = 0;
    uint32_t activeMs = 0;       // ms with a bin at or above the level
};

constexpr uint32_t kBinShapeRunGapMs = 3;
// The band grid's spacing, from the one place it is defined (RuntimeDefaults.h: sample rate / FreqBandStream window).
constexpr uint32_t kBinShapeSpacingHz = static_cast<uint32_t>(runtime::kBandBinSpacingHz);

inline StrengthClass classifyBinShape(const OccurrenceShape& shape, const ShapeInspectionConfig& config) {
    if (!shape.applicable) {
        return StrengthClass::Unknown;
    }
    if (shape.distinctBins >= config.strongMinBins && shape.risingSteps >= config.strongMinRisingSteps) {
        return StrengthClass::Strong;
    }
    if (shape.distinctBins >= config.mediumMinBins && shape.risingSteps >= config.mediumMinRisingSteps) {
        return StrengthClass::Medium;
    }
    return shape.distinctBins >= 2 ? StrengthClass::Weak : StrengthClass::None;
}

// The bins over [startMs, startMs + spanMs] whose level reaches minLevel.
template <class BinSource>
BinShapeMeasure measureBinShape(const BinSource& source, uint32_t startMs, uint32_t spanMs, float minLevel,
                                const ShapeInspectionConfig& config) {
    BinShapeMeasure m;
    OccurrenceShape& shape = m.shape;
    BinRun raw[BinShapeMeasure::kMaxRuns * 2];
    size_t rawCount = 0;
    for (uint32_t k = 0; k <= spanMs; ++k) {
        const uint32_t ms = startMs + k;
        uint32_t hz = 0;
        uint16_t level = 0;
        if (!source.at(ms, hz, level) || static_cast<float>(level) < minLevel) {
            continue;
        }
        shape.applicable = true;
        ++m.activeMs;
        BinRun* last = rawCount > 0 ? &raw[rawCount - 1] : nullptr;
        if (last != nullptr && last->hz == hz &&
            static_cast<uint32_t>(ms - (last->startMs + last->lengthMs)) <= kBinShapeRunGapMs) {
            last->lengthMs = ms - last->startMs + 1;
        } else if (rawCount < BinShapeMeasure::kMaxRuns * 2) {
            BinRun& run = raw[rawCount++];
            run.hz = hz;
            run.startMs = ms;
            run.lengthMs = 1;
        }
    }
    BinRun* kept = m.runs;
    size_t& keptCount = m.runCount;
    for (size_t i = 0; i < rawCount; ++i) {
        if (raw[i].lengthMs < config.minRunMs) {
            continue;
        }
        if (keptCount > 0 && kept[keptCount - 1].hz == raw[i].hz) {
            kept[keptCount - 1].lengthMs =
                static_cast<uint32_t>(raw[i].startMs + raw[i].lengthMs - kept[keptCount - 1].startMs);
        } else if (keptCount < BinShapeMeasure::kMaxRuns) {
            kept[keptCount++] = raw[i];
        }
    }
    uint32_t seen[BinShapeMeasure::kMaxRuns];
    size_t seenCount = 0;
    for (size_t i = 0; i < keptCount; ++i) {
        bool known = false;
        for (size_t k = 0; k < seenCount; ++k) {
            known = known || seen[k] == kept[i].hz;
        }
        if (!known) {
            seen[seenCount++] = kept[i].hz;
        }
        if (i > 0) {
            const long d = static_cast<long>(kept[i].hz) - static_cast<long>(kept[i - 1].hz);
            const bool up = d > 0 && d <= static_cast<long>(config.maxRisingStepBins * kBinShapeSpacingHz) &&
                d % static_cast<long>(kBinShapeSpacingHz) == 0;
            if (up || d < -static_cast<long>(3 * kBinShapeSpacingHz)) {
                ++shape.risingSteps;
            } else {
                ++shape.otherSteps;
            }
        }
    }
    shape.distinctBins = static_cast<uint8_t>(seenCount);
    shape.runs = static_cast<uint8_t>(keptCount);
    if (keptCount > 0) {
        shape.firstBinHz = static_cast<uint16_t>(kept[0].hz);
        shape.lastBinHz = static_cast<uint16_t>(kept[keptCount - 1].hz);
    }
    shape.shapeClass = classifyBinShape(shape, config);
    return m;
}

}  // namespace detection
