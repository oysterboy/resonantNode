#pragma once

#include <stdint.h>

#include "../../features/FeatureStream.h"

namespace detection {

/*
ScalarTransientConfig

The scalar detector's thresholds and gates, in the feature domains of
myspec 5.2 (Strength16 / FrequencyScore16 values and milliseconds). The
detector holds one of these (ScalarTransientDetector::setConfig); the profiles
in DetectionProfile.h set the shipped values; the live detection.scalar_*
params bind to the Node's copy (and the Analyzer's since step 11e, through
app/ParamBindings.h), which is why the millisecond fields are uint32_t, the
registry's unsigned width on every build. The field initialisers here are the
fallback for a detector that was never configured, not a tuned set.
*/
struct ScalarTransientConfig {
    FeatureStreamId observedStream = FeatureStreamId::AmpEnvelope;
    float onsetDetectionThreshold = 18000.0f;
    float onsetReleaseThreshold = 12000.0f;
    uint32_t cooldownAfterOnsetMs = 50;
    uint32_t minTransientDurationMs = 0;
    uint32_t maxTransientDurationMs = 120;
    float minTransientPeakStrength = 8000.0f;
    uint32_t releaseDebounceMs = 20;
    bool requireCarrierQuality = false;
    bool requireMinStrength = false;
    float minMatchedMeanStrength = 0.0f;
    uint32_t minCoverageAboveReleaseMs = 0;
    uint32_t minLongestIslandMs = 0;
    uint32_t maxGapMs = 0;
};

}  // namespace detection
