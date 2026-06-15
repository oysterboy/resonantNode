#pragma once

#include "inspection/InspectorTypes.h"

namespace detection {

inline float scalarInspectionLift(const ScalarInspectionObservation& observation) {
    switch (observation.mode) {
        case ScalarInspectionMode::PeakCenteredLift:
            return observation.classificationValue;
        case ScalarInspectionMode::PeakCentered:
            return observation.peak - observation.mean;
        case ScalarInspectionMode::PeakAbsolute:
        case ScalarInspectionMode::MeanAbsolute:
        case ScalarInspectionMode::SustainedAboveThreshold:
        default:
            return observation.classificationValue - observation.mean;
    }
}

inline float scalarInspectionLiftP75(const ScalarInspectionObservation& observation) {
    return observation.preFloorAvailable ? (observation.p75 - observation.preFloorMedian) : 0.0f;
}

inline float scalarInspectionLiftRms(const ScalarInspectionObservation& observation) {
    return observation.preFloorAvailable ? (observation.rms - observation.preFloorRms) : 0.0f;
}

inline float scalarInspectionLiftTrimmedMean(const ScalarInspectionObservation& observation) {
    return observation.preFloorAvailable ? (observation.trimmedMean - observation.preFloorMedian) : 0.0f;
}

} // namespace detection
