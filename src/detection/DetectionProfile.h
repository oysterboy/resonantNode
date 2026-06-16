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
FeatureStreamId { AmpEnvelope, FrequencyTarget, FrequencyScore, FrequencyTargetBand, FrequencyContrast }
EvidenceTarget { None, SupportStrength, FrequencyScoreStrength, FrequencyContrastQuality, TargetBandStrength }
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
- set `patternMatcherConfig`
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
    float onsetDetectionThreshold = 100.0f;
    float onsetReleaseThreshold = 50.5f;
    unsigned long cooldownAfterOnsetMs = 50;
    unsigned long minTransientDurationMs = 0;
    unsigned long maxTransientDurationMs = 120;
    float minTransientPeakStrength = 0.0f;
    unsigned long releaseDebounceMs = 20;
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
    PatternMatcherConfig patternMatcherConfig = {};
    InspectionPlan inspectionPlan = {};
    FieldStateConfig fieldStateConfig = {};
};

// Actual profiles. These are the concrete profile definitions used at runtime.
//PARAM TUNING TEMPORARY

inline DetectionProfile makeTonalPulseScalarProfile() {
    DetectionProfile profile;

    // Identity and occurrence routing.
    profile.kind = DetectionProfileKind::TonalPulseScalar;
    profile.detectorSelection = DetectorSelection::ScalarTransient;
    profile.scalarTransient.observedStream = FeatureStreamId::FrequencyTargetBand;
    profile.scalarTransient.onsetDetectionThreshold = 20000.0f;
    profile.scalarTransient.onsetReleaseThreshold = 5000.0f;
    profile.scalarTransient.cooldownAfterOnsetMs = 50;
    profile.scalarTransient.minTransientDurationMs = 60;
    profile.scalarTransient.maxTransientDurationMs = 300;
    profile.scalarTransient.minTransientPeakStrength = 0.0f;
    profile.scalarTransient.releaseDebounceMs = 30;

    // This profile is intentionally experimental and compares frequency-derived
    // scalar evidence through the existing scalar transient lifecycle.
    profile.inspectionPlan = {};
    profile.inspectionPlan.modules[0].kind = InspectionModuleKind::ScalarFeatureStrength;
    profile.inspectionPlan.modules[0].target = EvidenceTarget::FrequencyScoreStrength;
    profile.inspectionPlan.modules[0].scalar.stream = FeatureStreamId::FrequencyTargetBand;
    profile.inspectionPlan.modules[0].scalar.mode = ScalarInspectionMode::PeakCentered;
    profile.inspectionPlan.modules[0].scalar.supportStrength.strongPeakThreshold = 25000.0f;
    profile.inspectionPlan.modules[0].scalar.supportStrength.mediumPeakThreshold = 15000.0f;
    profile.inspectionPlan.modules[0].scalar.supportStrength.weakPeakThreshold = 8000.0f;
    profile.inspectionPlan.modules[0].scalar.windowPreMs = 10;
    profile.inspectionPlan.modules[0].scalar.windowPostMs = 90;
    profile.inspectionPlan.modules[0].scalar.preFloorWindowPreMs = 250;
    profile.inspectionPlan.modules[0].scalar.preFloorWindowPostMs = 50;

    profile.inspectionPlan.modules[1].kind = InspectionModuleKind::ScalarFeatureStrength;
    profile.inspectionPlan.modules[1].target = EvidenceTarget::FrequencyContrastQuality;
    profile.inspectionPlan.modules[1].scalar.stream = FeatureStreamId::FrequencyContrast;
    profile.inspectionPlan.modules[1].scalar.mode = ScalarInspectionMode::PeakCentered;
    profile.inspectionPlan.modules[1].scalar.supportStrength.strongPeakThreshold = 80.0f;
    profile.inspectionPlan.modules[1].scalar.supportStrength.mediumPeakThreshold = 50.0f;
    profile.inspectionPlan.modules[1].scalar.supportStrength.weakPeakThreshold = 25.0f;
    profile.inspectionPlan.modules[1].scalar.windowPreMs = 10;
    profile.inspectionPlan.modules[1].scalar.windowPostMs = 90;
    profile.inspectionPlan.modules[1].scalar.preFloorWindowPreMs = 250;
    profile.inspectionPlan.modules[1].scalar.preFloorWindowPostMs = 50;
    profile.inspectionPlan.count = 2;

    // Pattern rules.
    profile.patternMatcherConfig.requireSupportForAcceptance = false;
    profile.patternMatcherConfig.requiredSupportTarget = EvidenceTarget::FrequencyScoreStrength;
    profile.patternMatcherConfig.minimumSupportStrength = StrengthClass::Medium;

    // Field-state windowing.
    profile.fieldStateConfig.occurrenceWindowMs = 4000;
    profile.fieldStateConfig.patternWindowMs = 4000;
    profile.fieldStateConfig.busyOccurrenceCountThreshold = 3;
    profile.fieldStateConfig.denseOccurrenceCountThreshold = 6;
    profile.fieldStateConfig.busyActivityThreshold = 0.45f;
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
    profile.inspectionPlan.modules[0].target = EvidenceTarget::SupportStrength;
    profile.inspectionPlan.modules[0].scalar.stream = FeatureStreamId::AmpEnvelope;
    profile.inspectionPlan.modules[0].scalar.mode = ScalarInspectionMode::PeakCentered;
    profile.inspectionPlan.modules[0].scalar.supportStrength.strongPeakThreshold = 70.0f;
    profile.inspectionPlan.modules[0].scalar.supportStrength.mediumPeakThreshold = 40.0f;
    profile.inspectionPlan.modules[0].scalar.supportStrength.weakPeakThreshold = 20.0f;
    profile.inspectionPlan.modules[0].scalar.windowPreMs = 10;
    profile.inspectionPlan.modules[0].scalar.windowPostMs = 90;
    profile.inspectionPlan.modules[0].scalar.preFloorWindowPreMs = 250;
    profile.inspectionPlan.modules[0].scalar.preFloorWindowPostMs = 50;

    profile.inspectionPlan.modules[1].kind = InspectionModuleKind::ScalarFeatureStrength;
    profile.inspectionPlan.modules[1].target = EvidenceTarget::FrequencyScoreStrength;
    profile.inspectionPlan.modules[1].scalar.stream = FeatureStreamId::FrequencyScore;
    profile.inspectionPlan.modules[1].scalar.mode = ScalarInspectionMode::PeakCentered;
    profile.inspectionPlan.modules[1].scalar.supportStrength.strongPeakThreshold = 25000.0f;
    profile.inspectionPlan.modules[1].scalar.supportStrength.mediumPeakThreshold = 15000.0f;
    profile.inspectionPlan.modules[1].scalar.supportStrength.weakPeakThreshold = 8000.0f;
    profile.inspectionPlan.modules[1].scalar.windowPreMs = 10;
    profile.inspectionPlan.modules[1].scalar.windowPostMs = 90;
    profile.inspectionPlan.modules[1].scalar.preFloorWindowPreMs = 250;
    profile.inspectionPlan.modules[1].scalar.preFloorWindowPostMs = 50;

    profile.inspectionPlan.modules[2].kind = InspectionModuleKind::ScalarFeatureStrength;
    profile.inspectionPlan.modules[2].target = EvidenceTarget::FrequencyContrastQuality;
    profile.inspectionPlan.modules[2].scalar.stream = FeatureStreamId::FrequencyContrast;
    profile.inspectionPlan.modules[2].scalar.mode = ScalarInspectionMode::PeakCentered;
    profile.inspectionPlan.modules[2].scalar.supportStrength.strongPeakThreshold = 80.0f;
    profile.inspectionPlan.modules[2].scalar.supportStrength.mediumPeakThreshold = 50.0f;
    profile.inspectionPlan.modules[2].scalar.supportStrength.weakPeakThreshold = 25.0f;
    profile.inspectionPlan.modules[2].scalar.windowPreMs = 10;
    profile.inspectionPlan.modules[2].scalar.windowPostMs = 90;
    profile.inspectionPlan.modules[2].scalar.preFloorWindowPreMs = 250;
    profile.inspectionPlan.modules[2].scalar.preFloorWindowPostMs = 50;
    profile.inspectionPlan.count = 3;

    // Pattern rules.
    profile.patternMatcherConfig.requireSupportForAcceptance = false;
    profile.patternMatcherConfig.requiredSupportTarget = EvidenceTarget::SupportStrength;
    profile.patternMatcherConfig.minimumSupportStrength = StrengthClass::Medium;

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
    profile.scalarTransient.onsetDetectionThreshold = 23.0f;
    profile.scalarTransient.onsetReleaseThreshold = 20.0f;
    profile.scalarTransient.cooldownAfterOnsetMs = 300;
    profile.scalarTransient.minTransientDurationMs = 60;
    profile.scalarTransient.maxTransientDurationMs = 240;
    profile.scalarTransient.minTransientPeakStrength = 40.0f;
    profile.scalarTransient.releaseDebounceMs = 30;
    // Analyzer retune: recent AMP trials produced stable 29..38 peak-strength
    // occurrences, so keep the duration gate and lower only the peak gate here.
    profile.scalarTransient.minTransientPeakStrength = 28.0f;

    // Inspector composition.


    profile.inspectionPlan.modules[0].kind = InspectionModuleKind::ScalarFeatureStrength;
    profile.inspectionPlan.modules[0].target = EvidenceTarget::SupportStrength;
    profile.inspectionPlan.modules[0].scalar.stream = FeatureStreamId::AmpEnvelope;
    profile.inspectionPlan.modules[0].scalar.mode = ScalarInspectionMode::PeakCentered;
    profile.inspectionPlan.modules[0].scalar.supportStrength.strongPeakThreshold = 70.0f;
    profile.inspectionPlan.modules[0].scalar.supportStrength.mediumPeakThreshold = 40.0f;
    profile.inspectionPlan.modules[0].scalar.supportStrength.weakPeakThreshold = 20.0f;
    profile.inspectionPlan.modules[0].scalar.windowPreMs = 10;
    profile.inspectionPlan.modules[0].scalar.windowPostMs = 90;
    profile.inspectionPlan.modules[0].scalar.preFloorWindowPreMs = 250;
    profile.inspectionPlan.modules[0].scalar.preFloorWindowPostMs = 50;

    profile.inspectionPlan.modules[1].kind = InspectionModuleKind::ScalarFeatureStrength;
    profile.inspectionPlan.modules[1].target = EvidenceTarget::FrequencyScoreStrength;
    profile.inspectionPlan.modules[1].scalar.stream = FeatureStreamId::FrequencyScore;
    profile.inspectionPlan.modules[1].scalar.mode = ScalarInspectionMode::PeakCentered;
    profile.inspectionPlan.modules[1].scalar.supportStrength.strongPeakThreshold = 25000.0f;
    profile.inspectionPlan.modules[1].scalar.supportStrength.mediumPeakThreshold = 15000.0f;
    profile.inspectionPlan.modules[1].scalar.supportStrength.weakPeakThreshold = 8000.0f;
    profile.inspectionPlan.modules[1].scalar.windowPreMs = 10;
    profile.inspectionPlan.modules[1].scalar.windowPostMs = 90;
    profile.inspectionPlan.modules[1].scalar.preFloorWindowPreMs = 250;
    profile.inspectionPlan.modules[1].scalar.preFloorWindowPostMs = 50;
    //profile.inspectionPlan.modules[1].scalar.minSustainedMs = 25;
    
    profile.inspectionPlan.modules[2].kind = InspectionModuleKind::ScalarFeatureStrength;
    profile.inspectionPlan.modules[2].target = EvidenceTarget::FrequencyContrastQuality;
    profile.inspectionPlan.modules[2].scalar.stream = FeatureStreamId::FrequencyContrast;
    profile.inspectionPlan.modules[2].scalar.mode = ScalarInspectionMode::PeakCentered;
    profile.inspectionPlan.modules[2].scalar.supportStrength.strongPeakThreshold = 80.0f;
    profile.inspectionPlan.modules[2].scalar.supportStrength.mediumPeakThreshold = 50.0f;
    profile.inspectionPlan.modules[2].scalar.supportStrength.weakPeakThreshold = 25.0f;
    profile.inspectionPlan.modules[2].scalar.windowPreMs = 10;
    profile.inspectionPlan.modules[2].scalar.windowPostMs = 90;
    profile.inspectionPlan.modules[2].scalar.preFloorWindowPreMs = 250;
    profile.inspectionPlan.modules[2].scalar.preFloorWindowPostMs = 50;
    //profile.inspectionPlan.modules[2].scalar.minSustainedMs = 25;  
    
    profile.inspectionPlan.count = 3;

    // Pattern rules.
    profile.patternMatcherConfig.requireSupportForAcceptance = false;
    profile.patternMatcherConfig.requiredSupportTarget = EvidenceTarget::FrequencyScoreStrength;
    profile.patternMatcherConfig.minimumSupportStrength = StrengthClass::Medium;

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
