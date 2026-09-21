#pragma once

#include "InspectorTypes.h"

// Inspection-stage vocabulary for logs and profile/report text.
// Keep inspection naming here so Analyzer and mode shells do not duplicate it.
namespace detection {

inline const char* magnitudeInspectionBasisName(MagnitudeInspectionBasis value) {
    switch (value) {
        case MagnitudeInspectionBasis::CenteredMagnitudePeak:
            return "centered_magnitude_peak";
        case MagnitudeInspectionBasis::PeakAbsolute:
            return "peak_absolute";
        case MagnitudeInspectionBasis::MeanAbsolute:
            return "mean_absolute";
        case MagnitudeInspectionBasis::SustainedAboveThreshold:
            return "sustained_above_threshold";
        case MagnitudeInspectionBasis::PeakCenteredMean:
            return "peak_centered_mean";
        case MagnitudeInspectionBasis::PeakCenteredLift:
            return "peak_centered_lift";
        case MagnitudeInspectionBasis::Rms:
            return "rms";
        case MagnitudeInspectionBasis::P75:
            return "p75";
        case MagnitudeInspectionBasis::None:
        default:
            return "none";
    }
}

inline const char* magnitudeInspectionNoteName(MagnitudeInspectionNote value) {
    switch (value) {
        case MagnitudeInspectionNote::MagnitudeObserved:
            return "magnitude_observed";
        case MagnitudeInspectionNote::MagnitudeUnavailable:
            return "magnitude_unavailable";
        case MagnitudeInspectionNote::HistoryWindowIncomplete:
            return "history_window_incomplete";
        case MagnitudeInspectionNote::FutureWindowUnavailable:
            return "future_window_unavailable";
        case MagnitudeInspectionNote::WindowInvalid:
            return "window_invalid";
        case MagnitudeInspectionNote::InspectionDisabled:
            return "inspection_disabled";
        case MagnitudeInspectionNote::MissingFeatureHistory:
            return "missing_feature_history";
        case MagnitudeInspectionNote::None:
        default:
            return "none";
    }
}

inline const char* magnitudeInspectionAnchorName(MagnitudeInspectionAnchor value) {
    switch (value) {
        case MagnitudeInspectionAnchor::Peak:
            return "peak";
        case MagnitudeInspectionAnchor::Start:
            return "start";
        case MagnitudeInspectionAnchor::Release:
            return "release";
        case MagnitudeInspectionAnchor::Fallback:
            return "fallback";
        case MagnitudeInspectionAnchor::None:
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
    if (plan.count == 1 && plan.modules[0].kind == InspectionModuleKind::MagnitudeFeatureStrength) {
        return inspectionTargetName(plan.modules[0].target);
    }

    return "custom";
}

inline const char* inspectionModulesName(const InspectionPlan& plan) {
    if (plan.count == 1 &&
        plan.modules[0].kind == InspectionModuleKind::MagnitudeFeatureStrength) {
        return "MagnitudeFeatureStrength";
    }

    return "custom";
}

} // namespace detection
