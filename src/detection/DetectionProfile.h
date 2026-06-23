#pragma once
#ifndef DETECTION_PROFILE_H
#define DETECTION_PROFILE_H

#include <strings.h>

#include "field/FieldState.h"
#include "inspection/InspectorTypes.h"
#include "patterns/PatternMatcherTypes.h"

namespace detection {

/*
DetectionProfile

Code-defined detection profile composition.
Profiles select the active occurrence emitter, inspection rules, pattern rules,
inspection config, field-state config, and frequency match tuning.

Profiles declare composition; DetectionRuntime applies the selected fields at fixed stages.

Common enum / selector types used in this file:

```text
DetectionProfileKind { TonalPulseFreq, AmpExperimental, TonalPulseScalar }
DetectorSelection { FrequencyMatch, ScalarTransient }
FeatureStreamId { AmpEnvelope, FrequencyTarget, FrequencyContrast }
StrengthClass { Unknown, None, Weak, Medium, Strong }
InspectionModuleKind { None, ScalarFeatureStrength }
```

New profile checklist:
- add the kind here
- register it in `detectionProfileForKind(...)`
- add its display name in `detectionProfileName(...)`
- add parser support in `detectionProfileKindFromName(...)`
-------------------------------------------------------------
- add a factory in this file
--- inline DetectionProfile makePROFILENAMEProfile()
- set `detectorSelection` in the profile factory
- set the detector-specific config blocks in the profile factory
- set `inspectionPlan`
- set `fieldStateConfig`
- update SEQ help/parser if the analyzer should accept it
- update RB help/parser and behavior mapping if the resonant node should accept it
---------------------------
- if you add a new detector type, add it to `DetectorSelection` and `detectorSelectionName(...)`
*/

enum class DetectorSelection {
    // Available detectors selected by profiles.
    FrequencyMatch,
    ScalarTransient,
};

struct FrequencyMatchConfig {
    // Configuration and defaults.
    unsigned long releaseDebounceMs = 30;
    unsigned long cooldownAfterReleaseMs = 0;
    unsigned long minDurationMs = 60;
    float attackScoreMin = 18000.0f;
    float releaseScoreMin = 12000.0f;
    float attackContrastMin = 50.0f;
    float releaseContrastMin = 50.0f;
};

struct ScalarTransientConfig {
    // Configuration and defaults.
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

enum class DetectionProfileKind {
     // lists available Profiles
    TonalPulseFreq,
    AmpExperimental,
    TonalPulseScalar,
};

struct DetectionProfile {
    // Declaration and defaults
    
    // Identity and composition.
    DetectionProfileKind kind = DetectionProfileKind::TonalPulseFreq;
    DetectorSelection detectorSelection = DetectorSelection::FrequencyMatch;

    // Stage configuration.
    FrequencyMatchConfig frequencyMatch = {};
    ScalarTransientConfig scalarTransient = {};
    InspectionPlan inspectionPlan = {};
    FieldStateConfig fieldStateConfig = {};
};

inline const InspectionModuleConfig* patternMatcherFirstEnabledRequirement(const InspectionPlan& plan) {
    const size_t count = plan.count > kMaxInspectionModules ? kMaxInspectionModules : plan.count;
    for (size_t i = 0; i < count; ++i) {
        if (plan.modules[i].enabled) {
            return &plan.modules[i];
        }
    }
    return nullptr;
}

// Actual profiles. These are the concrete profile definitions used at runtime.
inline DetectionProfile makeTonalPulseScalarProfile() {
    DetectionProfile profile;

    // Identity and occurrence routing.
    profile.kind = DetectionProfileKind::TonalPulseScalar; // Profile ID for tonal pulse scalar analysis.
    profile.detectorSelection = DetectorSelection::ScalarTransient; // Use the scalar transient detector path.
    // This profile observes a frequency-derived scalar stream, not raw PCM.
    // "Offline" values here are deduced from RAW PCM captures and then mapped
    // into the normalized magnitude-like 0..32767 scale.
    profile.scalarTransient.observedStream = FeatureStreamId::FrequencyTarget; // Track the target-band scalar stream.
    profile.scalarTransient.onsetDetectionThreshold = 4500.0f; // Attack threshold for candidate start.
    profile.scalarTransient.onsetReleaseThreshold = 3000.0f; // Release threshold for candidate end.
    profile.scalarTransient.minTransientDurationMs = 85; // Reject pulses that are too short.
    profile.scalarTransient.maxTransientDurationMs = 130; // Reject pulses that are too long.
    profile.scalarTransient.releaseDebounceMs = 10; // Require a short stable release before ending.
    profile.scalarTransient.cooldownAfterOnsetMs = 50; // Avoid immediate re-triggering on the same pulse.
    // Minimum strength gate for the matched mean / peak utility switch.
    profile.scalarTransient.requireMinStrength = true;
    profile.scalarTransient.minMatchedMeanStrength = 0.0f; // Mean over samples above the release threshold.
    // Carrier quality gates to reject fragmented or weak target-band coverage.
    profile.scalarTransient.requireCarrierQuality = true;
    profile.scalarTransient.minCoverageAboveReleaseMs = 90; // Minimum time above release level.
    profile.scalarTransient.minLongestIslandMs = 80; // Longest continuous island above release level.
    profile.scalarTransient.maxGapMs = 10; // Largest allowed gap inside the candidate.


    // Inspection: Frequency Contrast
    profile.inspectionPlan = {};
    profile.inspectionPlan.modules[0].kind = InspectionModuleKind::ScalarFeatureStrength; // Scalar inspector module.
    profile.inspectionPlan.modules[0].target = InspectionTarget::Contrast; // Route this observation as contrast evidence.
    profile.inspectionPlan.modules[0].enabled = true; // Enable the module in the matcher.
    profile.inspectionPlan.modules[0].scalar.stream = FeatureStreamId::FrequencyContrast; // Measure frequency contrast.
    profile.inspectionPlan.modules[0].scalar.anchor = ScalarInspectionAnchor::Start; // Window from occurrence start.
    profile.inspectionPlan.modules[0].scalar.windowPreMs = 0; // No look-back before the anchor.
    profile.inspectionPlan.modules[0].scalar.windowPostMs = 100; // Inspect the first 100 ms after onset.
    profile.inspectionPlan.modules[0].minimumStrength = StrengthClass::Medium; // Require at least medium evidence.
    profile.inspectionPlan.modules[0].scalar.mode = ScalarInspectionMode::P75; // Use a robust percentile summary.
    profile.inspectionPlan.modules[0].scalar.supportStrength.strongPeakThreshold = 80.0f; // Strong contrast threshold.
    profile.inspectionPlan.modules[0].scalar.supportStrength.mediumPeakThreshold = 50.0f; // Medium contrast threshold.
    profile.inspectionPlan.modules[0].scalar.supportStrength.weakPeakThreshold = 25.0f; // Weak contrast threshold.


    // Secondary Inspection: Amplitude
    profile.inspectionPlan.modules[1].kind = InspectionModuleKind::ScalarFeatureStrength; // Scalar inspector module.
    profile.inspectionPlan.modules[1].target = InspectionTarget::Amp; // Route this observation as amplitude evidence.
    profile.inspectionPlan.modules[1].enabled = true; // Enable the module in the matcher.
    profile.inspectionPlan.modules[1].scalar.stream = FeatureStreamId::AmpEnvelope; // Measure the envelope stream.
    profile.inspectionPlan.modules[1].scalar.anchor = ScalarInspectionAnchor::Start; // Window from occurrence start.
    profile.inspectionPlan.modules[1].scalar.windowPreMs = 0; // No look-back before the anchor.
    profile.inspectionPlan.modules[1].scalar.windowPostMs = 100; // Inspect the first 100 ms after onset.
    profile.inspectionPlan.modules[1].minimumStrength = StrengthClass::Medium; // Require at least medium evidence.
    profile.inspectionPlan.modules[1].scalar.mode = ScalarInspectionMode::P75; // Use a robust percentile summary.
    profile.inspectionPlan.modules[1].scalar.supportStrength.strongPeakThreshold = 7500.0f; // Strong amplitude threshold.
    profile.inspectionPlan.modules[1].scalar.supportStrength.mediumPeakThreshold = 5000.0f; // Medium amplitude threshold.
    profile.inspectionPlan.modules[1].scalar.supportStrength.weakPeakThreshold = 3500.0f; // Weak amplitude threshold.
    profile.inspectionPlan.count = 2; // This profile uses two inspectors.

    profile.inspectionPlan.failedRequirementMeansUncertain = true; // Failed inspection requirements downgrade to uncertain.

    // Field-state windowing.
    profile.fieldStateConfig.occurrenceWindowMs = 4000; // Window for occurrence density tracking.
    profile.fieldStateConfig.patternWindowMs = 4000; // Window for pattern density tracking.
    profile.fieldStateConfig.busyOccurrenceCountThreshold = 3; // Occurrence count considered busy.
    profile.fieldStateConfig.denseOccurrenceCountThreshold = 6; // Occurrence count considered dense.
    profile.fieldStateConfig.busyActivityThreshold = 0.45f; // Activity threshold for busy field state.
    return profile;
}

inline DetectionProfile makeTonalPulseFreqProfile() {
    DetectionProfile profile;

    // Identity and occurrence routing.
    profile.kind = DetectionProfileKind::TonalPulseFreq;
    profile.detectorSelection = DetectorSelection::FrequencyMatch;

    // Frequency path tuning.
    profile.frequencyMatch.attackScoreMin = 18000.0f;
    profile.frequencyMatch.releaseScoreMin = 12000.0f;
    profile.frequencyMatch.attackContrastMin = 50.0f;
    profile.frequencyMatch.releaseContrastMin = 50.0f;
    profile.frequencyMatch.minDurationMs = 60;
    profile.frequencyMatch.releaseDebounceMs = 30;
    profile.frequencyMatch.cooldownAfterReleaseMs = 0;
  
    // Inspector composition.

    profile.inspectionPlan = {};
    profile.inspectionPlan.modules[0].kind = InspectionModuleKind::ScalarFeatureStrength;
    profile.inspectionPlan.modules[0].target = InspectionTarget::Amp;
    profile.inspectionPlan.modules[0].enabled = true;
    profile.inspectionPlan.modules[0].minimumStrength = StrengthClass::Medium;
    profile.inspectionPlan.modules[0].scalar.anchor = ScalarInspectionAnchor::Peak;
    profile.inspectionPlan.modules[0].scalar.stream = FeatureStreamId::AmpEnvelope;
    profile.inspectionPlan.modules[0].scalar.mode = ScalarInspectionMode::PeakCentered;
    profile.inspectionPlan.modules[0].scalar.windowPreMs = 10;
    profile.inspectionPlan.modules[0].scalar.windowPostMs = 90;
    profile.inspectionPlan.modules[0].scalar.supportStrength.strongPeakThreshold = 18000.0f;
    profile.inspectionPlan.modules[0].scalar.supportStrength.mediumPeakThreshold = 12000.0f;
    profile.inspectionPlan.modules[0].scalar.supportStrength.weakPeakThreshold = 8000.0f;

    profile.inspectionPlan.modules[1].kind = InspectionModuleKind::ScalarFeatureStrength;
    profile.inspectionPlan.modules[1].target = InspectionTarget::TargetScore;
    profile.inspectionPlan.modules[1].enabled = true;
    profile.inspectionPlan.modules[1].minimumStrength = StrengthClass::Medium;
    profile.inspectionPlan.modules[1].scalar.anchor = ScalarInspectionAnchor::Peak;
    profile.inspectionPlan.modules[1].scalar.stream = FeatureStreamId::FrequencyTarget;
    profile.inspectionPlan.modules[1].scalar.mode = ScalarInspectionMode::PeakCentered;
    profile.inspectionPlan.modules[1].scalar.windowPreMs = 10;
    profile.inspectionPlan.modules[1].scalar.windowPostMs = 90;
    profile.inspectionPlan.modules[1].scalar.supportStrength.strongPeakThreshold = 18000.0f;
    profile.inspectionPlan.modules[1].scalar.supportStrength.mediumPeakThreshold = 12000.0f;
    profile.inspectionPlan.modules[1].scalar.supportStrength.weakPeakThreshold = 8000.0f;

    profile.inspectionPlan.modules[2].kind = InspectionModuleKind::ScalarFeatureStrength;
    profile.inspectionPlan.modules[2].target = InspectionTarget::Contrast;
    profile.inspectionPlan.modules[2].enabled = true;
    profile.inspectionPlan.modules[2].minimumStrength = StrengthClass::Medium;
    profile.inspectionPlan.modules[2].scalar.anchor = ScalarInspectionAnchor::Peak;
    profile.inspectionPlan.modules[2].scalar.stream = FeatureStreamId::FrequencyContrast;
    profile.inspectionPlan.modules[2].scalar.mode = ScalarInspectionMode::PeakCentered;
    profile.inspectionPlan.modules[2].scalar.windowPreMs = 10;
    profile.inspectionPlan.modules[2].scalar.windowPostMs = 90;
    profile.inspectionPlan.modules[2].scalar.supportStrength.strongPeakThreshold = 80.0f;
    profile.inspectionPlan.modules[2].scalar.supportStrength.mediumPeakThreshold = 50.0f;
    profile.inspectionPlan.modules[2].scalar.supportStrength.weakPeakThreshold = 25.0f;
    profile.inspectionPlan.count = 3;

    // Pattern rules.
    profile.inspectionPlan.failedRequirementMeansUncertain = true;

    // Field-state windowing.
    profile.fieldStateConfig.occurrenceWindowMs = 3500;
    profile.fieldStateConfig.patternWindowMs = 3500;
    profile.fieldStateConfig.busyOccurrenceCountThreshold = 3;
    profile.fieldStateConfig.denseOccurrenceCountThreshold = 6;
    profile.fieldStateConfig.quietOccurrenceCountThreshold = 0;
    profile.fieldStateConfig.quietActivityThreshold = 0.0f;
    profile.fieldStateConfig.busyActivityThreshold = 0.4f;
    return profile;
}

inline DetectionProfile makeAmpExperimentalProfile() {
    DetectionProfile profile;

    // Identity and occurrence routing.
    profile.kind = DetectionProfileKind::AmpExperimental;
    profile.detectorSelection = DetectorSelection::ScalarTransient;
    profile.scalarTransient.observedStream = FeatureStreamId::AmpEnvelope;
    // Lab-calibrated AMP thresholds: the default scalar detector thresholds are
    // too high for the current scalar magnitude range on analyzer runs.
    profile.scalarTransient.onsetDetectionThreshold = 18000.0f;
    profile.scalarTransient.onsetReleaseThreshold = 12000.0f;
    profile.scalarTransient.cooldownAfterOnsetMs = 300;
    profile.scalarTransient.minTransientDurationMs = 60;
    profile.scalarTransient.maxTransientDurationMs = 240;
    profile.scalarTransient.minTransientPeakStrength = 15000.0f;
    profile.scalarTransient.releaseDebounceMs = 30;
    profile.scalarTransient.requireMinStrength = true;
    // Analyzer retune: keep the duration gate and use the normalized magnitude
    // scale so AMP-driven occurrences are compared on the same 0..32767 band.
    profile.scalarTransient.minTransientPeakStrength = 15000.0f;

    // Inspector composition.


    profile.inspectionPlan.modules[0].kind = InspectionModuleKind::ScalarFeatureStrength;
    profile.inspectionPlan.modules[0].target = InspectionTarget::Amp;
    profile.inspectionPlan.modules[0].enabled = true;
    profile.inspectionPlan.modules[0].minimumStrength = StrengthClass::Medium;
    profile.inspectionPlan.modules[0].scalar.anchor = ScalarInspectionAnchor::Peak;
    profile.inspectionPlan.modules[0].scalar.stream = FeatureStreamId::AmpEnvelope;
    profile.inspectionPlan.modules[0].scalar.mode = ScalarInspectionMode::PeakCentered;
    profile.inspectionPlan.modules[0].scalar.windowPreMs = 10;
    profile.inspectionPlan.modules[0].scalar.windowPostMs = 90;
    profile.inspectionPlan.modules[0].scalar.supportStrength.strongPeakThreshold = 18000.0f;
    profile.inspectionPlan.modules[0].scalar.supportStrength.mediumPeakThreshold = 12000.0f;
    profile.inspectionPlan.modules[0].scalar.supportStrength.weakPeakThreshold = 8000.0f;

    profile.inspectionPlan.modules[1].kind = InspectionModuleKind::ScalarFeatureStrength;
    profile.inspectionPlan.modules[1].target = InspectionTarget::TargetScore;
    profile.inspectionPlan.modules[1].enabled = true;
    profile.inspectionPlan.modules[1].minimumStrength = StrengthClass::Medium;
    profile.inspectionPlan.modules[1].scalar.anchor = ScalarInspectionAnchor::Peak;
    profile.inspectionPlan.modules[1].scalar.stream = FeatureStreamId::FrequencyTarget;
    profile.inspectionPlan.modules[1].scalar.mode = ScalarInspectionMode::PeakCentered;
    profile.inspectionPlan.modules[1].scalar.windowPreMs = 10;
    profile.inspectionPlan.modules[1].scalar.windowPostMs = 90;
    profile.inspectionPlan.modules[1].scalar.supportStrength.strongPeakThreshold = 18000.0f;
    profile.inspectionPlan.modules[1].scalar.supportStrength.mediumPeakThreshold = 12000.0f;
    profile.inspectionPlan.modules[1].scalar.supportStrength.weakPeakThreshold = 8000.0f;
    //profile.inspectionPlan.modules[1].scalar.minSustainedMs = 25;
    
    profile.inspectionPlan.modules[2].kind = InspectionModuleKind::ScalarFeatureStrength;
    profile.inspectionPlan.modules[2].target = InspectionTarget::Contrast;
    profile.inspectionPlan.modules[2].enabled = true;
    profile.inspectionPlan.modules[2].minimumStrength = StrengthClass::Medium;
    profile.inspectionPlan.modules[2].scalar.anchor = ScalarInspectionAnchor::Peak;
    profile.inspectionPlan.modules[2].scalar.stream = FeatureStreamId::FrequencyContrast;
    profile.inspectionPlan.modules[2].scalar.mode = ScalarInspectionMode::PeakCentered;
    profile.inspectionPlan.modules[2].scalar.windowPreMs = 10;
    profile.inspectionPlan.modules[2].scalar.windowPostMs = 90;
    profile.inspectionPlan.modules[2].scalar.supportStrength.strongPeakThreshold = 80.0f;
    profile.inspectionPlan.modules[2].scalar.supportStrength.mediumPeakThreshold = 50.0f;
    profile.inspectionPlan.modules[2].scalar.supportStrength.weakPeakThreshold = 25.0f;
    //profile.inspectionPlan.modules[2].scalar.minSustainedMs = 25;  
    
    profile.inspectionPlan.count = 3;

    // Pattern rules.
    profile.inspectionPlan.failedRequirementMeansUncertain = true;

    // Field-state windowing.
    profile.fieldStateConfig.occurrenceWindowMs = 4000;
    profile.fieldStateConfig.patternWindowMs = 4000;
    profile.fieldStateConfig.busyOccurrenceCountThreshold = 3;
    profile.fieldStateConfig.denseOccurrenceCountThreshold = 6;
    profile.fieldStateConfig.busyActivityThreshold = 0.45f;
    return profile;
}


// Profile lookup by kind.
inline const DetectionProfile& detectionProfileForKind(DetectionProfileKind kind) {
    static const DetectionProfile kTonalPulseFreq = makeTonalPulseFreqProfile();
    static const DetectionProfile kAmpExperimental = makeAmpExperimentalProfile();
    static const DetectionProfile kTonalPulseScalar = makeTonalPulseScalarProfile();

    switch (kind) {
        case DetectionProfileKind::AmpExperimental:
            return kAmpExperimental;
        case DetectionProfileKind::TonalPulseScalar:
            return kTonalPulseScalar;
        case DetectionProfileKind::TonalPulseFreq:
        default:
            return kTonalPulseFreq;
    }
}

// Human-readable names for profile kinds used in logs and help text.
inline const char* detectionProfileName(DetectionProfileKind kind) {
    switch (kind) {
        case DetectionProfileKind::TonalPulseFreq:
            return "TonalPulseFreq";
        case DetectionProfileKind::AmpExperimental:
            return "AmpExperimental";
        case DetectionProfileKind::TonalPulseScalar:
            return "TonalPulseScalar";
    }
    return "unknown";
}

// Human-readable names for profile-selected detector routing.
inline const char* detectorSelectionName(DetectorSelection kind) {
    switch (kind) {
        case DetectorSelection::FrequencyMatch:
            return "FrequencyMatch";
        case DetectorSelection::ScalarTransient:
            return "ScalarTransient";
    }
    return "unknown";
}

// Parse profile names from user-facing text.
inline bool detectionProfileKindFromName(const char* name, DetectionProfileKind& outKind) {
    if (name == nullptr) {
        return false;
    }

    if (strcasecmp(name, "tonalpulsefreq") == 0 || strcasecmp(name, "tonalpulse") == 0 || strcasecmp(name, "tonal_pulse") == 0) {
        outKind = DetectionProfileKind::TonalPulseFreq;
        return true;
    }
    if (strcasecmp(name, "ampexperimental") == 0 || strcasecmp(name, "amp") == 0) {
        outKind = DetectionProfileKind::AmpExperimental;
        return true;
    }
    if (strcasecmp(name, "tonalpulsescalar") == 0 || strcasecmp(name, "tonal_pulse_scalar") == 0 || strcasecmp(name, "scalar_freq_experimental") == 0) {
        outKind = DetectionProfileKind::TonalPulseScalar;
        return true;
    }

    return false;
}

} // namespace detection

#endif // DETECTION_PROFILE_H
