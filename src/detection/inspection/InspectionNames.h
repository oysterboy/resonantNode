#pragma once

#include "InspectorTypes.h"

// Inspection-stage vocabulary for logs and profile/report text.
// Keep inspection naming here so Analyzer and mode shells do not duplicate it.
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
        case ScalarInspectionBasis::Rms:
            return "rms";
        case ScalarInspectionBasis::P75:
            return "p75";
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
        case ScalarInspectionNote::HistoryWindowIncomplete:
            return "history_window_incomplete";
        case ScalarInspectionNote::FutureWindowUnavailable:
            return "future_window_unavailable";
        case ScalarInspectionNote::WindowInvalid:
            return "window_invalid";
        case ScalarInspectionNote::InspectionDisabled:
            return "inspection_disabled";
        case ScalarInspectionNote::MissingFeatureHistory:
            return "missing_feature_history";
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

inline const char* strengthClassName(StrengthClass value) {
    switch (value) {
        case StrengthClass::None:
            return "none";
        case StrengthClass::Weak:
            return "weak";
        case StrengthClass::Medium:
            return "medium";
        case StrengthClass::Strong:
            return "strong";
        case StrengthClass::Unknown:
        default:
            return "unknown";
    }
}

inline const char* inspectionTargetName(InspectionTarget value) {
    switch (value) {
        case InspectionTarget::Amp:
            return "amp";
        case InspectionTarget::TargetScore:
            return "target";
        case InspectionTarget::Contrast:
            return "contrast";
        case InspectionTarget::TargetBand:
            return "band";
        case InspectionTarget::None:
        default:
            return "none";
    }
}

inline const char* inspectionPlanName(const InspectionPlan& plan) {
    if (plan.count == 1 && plan.modules[0].kind == InspectionModuleKind::ScalarFeatureStrength) {
        return inspectionTargetName(plan.modules[0].target);
    }

    return "custom";
}

inline const char* inspectionModulesName(const InspectionPlan& plan) {
    if (plan.count == 1 &&
        plan.modules[0].kind == InspectionModuleKind::ScalarFeatureStrength) {
        return "ScalarFeatureStrength";
    }

    return "custom";
}

} // namespace detection
