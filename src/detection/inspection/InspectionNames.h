#pragma once

#include "InspectorTypes.h"

namespace detection {

inline const char* scalarInspectionBasisName(ScalarInspectionBasis value) {
    switch (value) {
        case ScalarInspectionBasis::CenteredMagnitudePeak:
            return "centered_magnitude_peak";
        case ScalarInspectionBasis::PeakAbsolute:
            return "peak_absolute";
        case ScalarInspectionBasis::MeanAbsolute:
            return "mean_absolute";
        case ScalarInspectionBasis::SustainedAboveThreshold:
            return "sustained_above_threshold";
        case ScalarInspectionBasis::PeakCenteredMean:
            return "peak_centered_mean";
        case ScalarInspectionBasis::PeakCenteredLift:
            return "peak_centered_lift";
        case ScalarInspectionBasis::None:
        default:
            return "none";
    }
}

inline const char* scalarInspectionNoteName(ScalarInspectionNote value) {
    switch (value) {
        case ScalarInspectionNote::ScalarObserved:
            return "scalar_observed";
        case ScalarInspectionNote::ScalarUnavailable:
            return "scalar_unavailable";
        case ScalarInspectionNote::WindowInvalid:
            return "window_invalid";
        case ScalarInspectionNote::InspectionDisabled:
            return "inspection_disabled";
        case ScalarInspectionNote::MissingFeatureHistory:
            return "missing_feature_history";
        case ScalarInspectionNote::PreFloorObserved:
            return "pre_floor_observed";
        case ScalarInspectionNote::PreFloorUnavailable:
            return "pre_floor_unavailable";
        case ScalarInspectionNote::None:
        default:
            return "none";
    }
}

inline const char* scalarInspectionAnchorName(ScalarInspectionAnchor value) {
    switch (value) {
        case ScalarInspectionAnchor::Peak:
            return "peak";
        case ScalarInspectionAnchor::Start:
            return "start";
        case ScalarInspectionAnchor::Release:
            return "release";
        case ScalarInspectionAnchor::Fallback:
            return "fallback";
        case ScalarInspectionAnchor::None:
        default:
            return "none";
    }
}

} // namespace detection
