#pragma once

#include "../../features/FeatureStream.h"

namespace detection {

/*
ScalarTransientConfig

The scalar detector's thresholds and gates, in the feature domains of
myspec 5.2 (Strength16 / FrequencyScore16 values and milliseconds). The
detector holds one of these (ScalarTransientDetector::setConfig); the profiles
in DetectionProfile.h set the shipped values; the live detection.scalar_*
params bind to the Node's copy. The field initialisers here are the fallback
for a detector that was never configured, not a tuned set.
*/
struct ScalarTransientConfig {
    FeatureStreamId observedStream = FeatureStreamId::AmpEnvelope;
    float onsetDetectionThreshold = 18000.0f;
    float onsetReleaseThreshold = 12000.0f;
    unsigned long cooldownAfterOnsetMs = 50;
    unsigned long minTransientDurationMs = 0;
    unsigned long maxTransientDurationMs = 120;
    float minTransientPeakStrength = 8000.0f;
    unsigned long releaseDebounceMs = 20;
    bool requireCarrierQuality = false;
    bool requireMinStrength = false;
    float minMatchedMeanStrength = 0.0f;
    unsigned long minCoverageAboveReleaseMs = 0;
    unsigned long minLongestIslandMs = 0;
    unsigned long maxGapMs = 0;
};

}  // namespace detection
