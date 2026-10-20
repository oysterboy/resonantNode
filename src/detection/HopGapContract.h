#pragma once

#include "DetectionProfile.h"

/*
HopGapContract (step 5h item 2, DET-011)

The Node's network signal (a train of steps x pulse ms with gap ms between,
output.hop_*) and the scalar detector that has to read it as one occurrence
(detection.scalar_*) are independent params. This is the check between them,
used by the Node at boot and after a PARAM SET, and by the host tests on the
shipped defaults.

Hard (the train can never be one occurrence):
  - duration: the train (steps * pulse + (steps - 1) * gap) inside
    [min duration, max duration];
  - coverage: steps * pulse >= min coverage above release;
  - island: pulse >= min longest island;
  - max gap: gap <= max gap (the carrier-quality gap check).
Soft (the train is one occurrence only in some rooms):
  - debounce: gap < release debounce. A dip in the band feature that lasts the
    debounce closes the candidate; the gap is bridged only while the room tail
    keeps the feature above release. Replay of the 70 cm capture with the tail
    removed (test/fixtures/*_dry_g10.csv, 2026-10-20): at gap 10 = debounce 10
    the train loses its first step; at debounce 20 it is read whole.
  - tail: train + kHopRoomTailMarginMs <= max duration. The board read the
    165 ms train as up to 214 ms at 100 cm (lab note 2026-10-16 section 8).
*/
namespace detection {

constexpr unsigned long kHopRoomTailMarginMs = 50;

struct HopTrainShape {
    unsigned long steps = 0;
    unsigned long pulseMs = 0;
    unsigned long gapMs = 0;
};

struct HopGapCheck {
    unsigned long trainMs = 0;
    bool durationFits = true;
    bool coverageFits = true;
    bool islandFits = true;
    bool gapWithinMaxGap = true;
    bool gapBelowDebounce = true;
    bool tailFits = true;

    bool hardOk() const { return durationFits && coverageFits && islandFits && gapWithinMaxGap; }
    bool softOk() const { return gapBelowDebounce && tailFits; }
};

inline unsigned long hopTrainMs(const HopTrainShape& shape) {
    if (shape.steps == 0) {
        return 0;
    }
    return shape.steps * shape.pulseMs + (shape.steps - 1) * shape.gapMs;
}

inline HopGapCheck checkHopGap(const HopTrainShape& shape, const ScalarTransientConfig& scalar) {
    HopGapCheck c;
    c.trainMs = hopTrainMs(shape);
    c.durationFits = c.trainMs >= scalar.minTransientDurationMs && c.trainMs <= scalar.maxTransientDurationMs;
    c.tailFits = c.trainMs + kHopRoomTailMarginMs <= scalar.maxTransientDurationMs;
    if (scalar.requireCarrierQuality) {
        c.coverageFits = shape.steps * shape.pulseMs >= scalar.minCoverageAboveReleaseMs;
        c.islandFits = shape.pulseMs >= scalar.minLongestIslandMs;
        c.gapWithinMaxGap = shape.steps < 2 || shape.gapMs <= scalar.maxGapMs;
    }
    c.gapBelowDebounce = shape.steps < 2 || shape.gapMs < scalar.releaseDebounceMs;
    return c;
}

}  // namespace detection
