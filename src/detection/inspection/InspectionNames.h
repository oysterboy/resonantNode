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

inline const char* evidenceTargetName(EvidenceTarget value) {
    switch (value) {
        case EvidenceTarget::SupportStrength:
            return "SupportStrength";
        case EvidenceTarget::FrequencyScoreStrength:
            return "FrequencyScoreStrength";
        case EvidenceTarget::FrequencyContrastQuality:
            return "FrequencyContrastQuality";
        case EvidenceTarget::TargetBandStrength:
            return "TargetBandStrength";
        case EvidenceTarget::None:
        default:
            return "None";
    }
}

inline const char* inspectionPlanName(const InspectionPlan& plan) {
    if (plan.count == 1 &&
        plan.modules[0].kind == InspectionModuleKind::ScalarFeatureStrength) {
        switch (plan.modules[0].target) {
            case EvidenceTarget::FrequencyScoreStrength:
                return "frequency_score";
            case EvidenceTarget::TargetBandStrength:
                return "target_band";
            case EvidenceTarget::SupportStrength:
            default:
                return "support_strength";
        }
    }
    if (plan.count == 2 &&
        plan.modules[0].kind == InspectionModuleKind::ScalarFeatureStrength &&
        plan.modules[0].target == EvidenceTarget::FrequencyScoreStrength &&
        plan.modules[1].kind == InspectionModuleKind::ScalarFeatureStrength &&
        plan.modules[1].target == EvidenceTarget::FrequencyContrastQuality) {
        return "frequency_score_contrast";
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

inline const char* inspectionEvidenceTargetsName(const InspectionPlan& plan) {
    if (plan.count > 0 && plan.modules[0].kind == InspectionModuleKind::ScalarFeatureStrength) {
        return evidenceTargetName(plan.modules[0].target);
    }

    return "none";
}

} // namespace detection
