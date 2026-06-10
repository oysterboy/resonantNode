#pragma once
#ifndef DETECTION_PROFILE_H
#define DETECTION_PROFILE_H

#include <strings.h>

#include "field/FieldState.h"
#include "inspector/InspectorTypes.h"
#include "patterns/PatternRules.h"

namespace detection {

/*
DetectionProfile

Code-defined detection profile composition.
Profiles select the active occurrence emitter, inspection rules, pattern rules,
inspection config, field-state config, and frequency match tuning.

Profiles declare composition; DetectionRuntime applies the selected fields at fixed stages.

Common enum / selector types used in this file:

```text
DetectionProfileKind { TonalPulse, Amp, ChirpExperimental, ScalarFreqExperimental }
DetectorSelection { FrequencyMatch, ScalarTransient }
FeatureStreamId { AmpEnvelope, FrequencyScore, FrequencyContrast, AmbientFloor }
EvidenceTarget { None, AmpStrength, FrequencyScoreStrength, FrequencyContrastQuality, TargetBandStrength }
StrengthClass { Unknown, None, Weak, Medium, Strong }
InspectionModuleKind { None, ScalarFeatureStrength }
```

New profile checklist:
- add the kind here
- add a factory in this file
- register it in detectionProfileForKind(...)
- add its name in detectionProfileName(...)
- add parser support in detectionProfileKindFromName(...)
- update analyzer help/parser if SEQ should accept it
- update node help/parser and behavior mapping if RB should accept it
*/

enum class DetectorSelection {
    // Canonical profile-selected detector choice.
    FrequencyMatch,
    ScalarTransient,
};

// Legacy routing name retained as a compatibility alias only.
using OccurrenceSourceKind = DetectorSelection;

enum class DetectionProfileKind {
    TonalPulse,
    Amp,
    ChirpExperimental,
    ScalarFreqExperimental,
};

struct FrequencyMatchConfig {
    unsigned long releaseDebounceMs = 30;
    unsigned long cooldownAfterReleaseMs = 0;
    unsigned long minDurationMs = 60;
    float attackScoreMin = 18000.0f;
    float releaseScoreMin = 12000.0f;
    float attackContrastMin = 50.0f;
    float releaseContrastMin = 50.0f;
};

struct ScalarTransientConfig {
    FeatureStreamId observedStream = FeatureStreamId::AmpEnvelope;
    float onsetDetectionThreshold = 75.0f;
    float onsetReleaseThreshold = 67.5f;
    unsigned long cooldownAfterOnsetMs = 500;
    unsigned long minTransientDurationMs = 0;
    unsigned long maxTransientDurationMs = 120;
    float minTransientPeakStrength = 0.0f;
    unsigned long releaseDebounceMs = 20;
};


struct DetectionProfile {
    // Identity and composition.
    DetectionProfileKind kind = DetectionProfileKind::TonalPulse;
    DetectorSelection detectorSelection = DetectorSelection::FrequencyMatch;

    // Stage configuration.
    FrequencyMatchConfig frequencyMatch = {};
    ScalarTransientConfig scalarTransient = {};
    PatternRulesConfig patternRulesConfig = {};
    InspectionPlan inspectionPlan = {};
    FieldStateConfig fieldStateConfig = {};
};

inline void applyAmpEnvelopeScalarTransientTuning(ScalarTransientConfig& config) {
    // Lab-calibrated AMP thresholds: the default scalar detector thresholds are
    // too high for the current AmpEnvelope magnitude range on analyzer runs.
    config.onsetDetectionThreshold = 23.0f;
    config.onsetReleaseThreshold = 20.0f;
    config.cooldownAfterOnsetMs = 300;
    config.minTransientDurationMs = 60;
    config.maxTransientDurationMs = 240;
    config.minTransientPeakStrength = 40.0f;
    config.releaseDebounceMs = 30;
}

// Actual profiles. These are the concrete profile definitions used at runtime.
inline DetectionProfile makeTonalPulseProfile() {
    DetectionProfile profile;

    // Identity and occurrence routing.
    profile.kind = DetectionProfileKind::TonalPulse;
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
    profile.inspectionPlan.modules[0].target = EvidenceTarget::AmpStrength;
    profile.inspectionPlan.modules[0].scalar.stream = FeatureStreamId::AmpEnvelope;
    profile.inspectionPlan.modules[0].scalar.mode = ScalarInspectionMode::PeakCentered;
    profile.inspectionPlan.modules[0].scalar.strength.strongPeakThreshold = 70.0f;
    profile.inspectionPlan.modules[0].scalar.strength.mediumPeakThreshold = 40.0f;
    profile.inspectionPlan.modules[0].scalar.strength.weakPeakThreshold = 20.0f;
    profile.inspectionPlan.modules[0].scalar.windowPreMs = 10;
    profile.inspectionPlan.modules[0].scalar.windowPostMs = 90;
    profile.inspectionPlan.modules[0].scalar.preFloorWindowPreMs = 250;
    profile.inspectionPlan.modules[0].scalar.preFloorWindowPostMs = 50;

    profile.inspectionPlan.modules[1].kind = InspectionModuleKind::ScalarFeatureStrength;
    profile.inspectionPlan.modules[1].target = EvidenceTarget::FrequencyScoreStrength;
    profile.inspectionPlan.modules[1].scalar.stream = FeatureStreamId::FrequencyScore;
    profile.inspectionPlan.modules[1].scalar.mode = ScalarInspectionMode::PeakCentered;
    profile.inspectionPlan.modules[1].scalar.strength.strongPeakThreshold = 25000.0f;
    profile.inspectionPlan.modules[1].scalar.strength.mediumPeakThreshold = 15000.0f;
    profile.inspectionPlan.modules[1].scalar.strength.weakPeakThreshold = 8000.0f;
    profile.inspectionPlan.modules[1].scalar.windowPreMs = 10;
    profile.inspectionPlan.modules[1].scalar.windowPostMs = 90;
    profile.inspectionPlan.modules[1].scalar.preFloorWindowPreMs = 250;
    profile.inspectionPlan.modules[1].scalar.preFloorWindowPostMs = 50;

    profile.inspectionPlan.modules[2].kind = InspectionModuleKind::ScalarFeatureStrength;
    profile.inspectionPlan.modules[2].target = EvidenceTarget::FrequencyContrastQuality;
    profile.inspectionPlan.modules[2].scalar.stream = FeatureStreamId::FrequencyContrast;
    profile.inspectionPlan.modules[2].scalar.mode = ScalarInspectionMode::PeakCentered;
    profile.inspectionPlan.modules[2].scalar.strength.strongPeakThreshold = 80.0f;
    profile.inspectionPlan.modules[2].scalar.strength.mediumPeakThreshold = 50.0f;
    profile.inspectionPlan.modules[2].scalar.strength.weakPeakThreshold = 25.0f;
    profile.inspectionPlan.modules[2].scalar.windowPreMs = 10;
    profile.inspectionPlan.modules[2].scalar.windowPostMs = 90;
    profile.inspectionPlan.modules[2].scalar.preFloorWindowPreMs = 250;
    profile.inspectionPlan.modules[2].scalar.preFloorWindowPostMs = 50;
    profile.inspectionPlan.count = 3;

    // Pattern rules.
    profile.patternRulesConfig.requireSupportForAcceptance = false;
    profile.patternRulesConfig.requiredSupportTarget = EvidenceTarget::AmpStrength;
    profile.patternRulesConfig.minimumSupportStrength = StrengthClass::Medium;

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

inline DetectionProfile makeAmpProfile() {
    DetectionProfile profile;

    // Identity and occurrence routing.
    profile.kind = DetectionProfileKind::Amp;
    profile.detectorSelection = DetectorSelection::ScalarTransient;
    profile.scalarTransient.observedStream = FeatureStreamId::AmpEnvelope;
    applyAmpEnvelopeScalarTransientTuning(profile.scalarTransient);
    // Analyzer retune: recent AMP trials produced stable 29..38 peak-strength
    // candidates, so keep the duration gate and lower only the peak gate here.
    profile.scalarTransient.minTransientPeakStrength = 28.0f;

    // Inspector composition.


    profile.inspectionPlan.modules[0].kind = InspectionModuleKind::ScalarFeatureStrength;
    profile.inspectionPlan.modules[0].target = EvidenceTarget::AmpStrength;
    profile.inspectionPlan.modules[0].scalar.stream = FeatureStreamId::AmpEnvelope;
    profile.inspectionPlan.modules[0].scalar.mode = ScalarInspectionMode::PeakCentered;
    profile.inspectionPlan.modules[0].scalar.strength.strongPeakThreshold = 70.0f;
    profile.inspectionPlan.modules[0].scalar.strength.mediumPeakThreshold = 40.0f;
    profile.inspectionPlan.modules[0].scalar.strength.weakPeakThreshold = 20.0f;
    profile.inspectionPlan.modules[0].scalar.windowPreMs = 10;
    profile.inspectionPlan.modules[0].scalar.windowPostMs = 90;
    profile.inspectionPlan.modules[0].scalar.preFloorWindowPreMs = 250;
    profile.inspectionPlan.modules[0].scalar.preFloorWindowPostMs = 50;

    profile.inspectionPlan.modules[1].kind = InspectionModuleKind::ScalarFeatureStrength;
    profile.inspectionPlan.modules[1].target = EvidenceTarget::FrequencyScoreStrength;
    profile.inspectionPlan.modules[1].scalar.stream = FeatureStreamId::FrequencyScore;
    profile.inspectionPlan.modules[1].scalar.mode = ScalarInspectionMode::PeakCentered;
    profile.inspectionPlan.modules[1].scalar.strength.strongPeakThreshold = 25000.0f;
    profile.inspectionPlan.modules[1].scalar.strength.mediumPeakThreshold = 15000.0f;
    profile.inspectionPlan.modules[1].scalar.strength.weakPeakThreshold = 8000.0f;
    profile.inspectionPlan.modules[1].scalar.windowPreMs = 10;
    profile.inspectionPlan.modules[1].scalar.windowPostMs = 90;
    profile.inspectionPlan.modules[1].scalar.preFloorWindowPreMs = 250;
    profile.inspectionPlan.modules[1].scalar.preFloorWindowPostMs = 50;
    //profile.inspectionPlan.modules[1].scalar.minSustainedMs = 25;
    
    profile.inspectionPlan.modules[2].kind = InspectionModuleKind::ScalarFeatureStrength;
    profile.inspectionPlan.modules[2].target = EvidenceTarget::FrequencyContrastQuality;
    profile.inspectionPlan.modules[2].scalar.stream = FeatureStreamId::FrequencyContrast;
    profile.inspectionPlan.modules[2].scalar.mode = ScalarInspectionMode::PeakCentered;
    profile.inspectionPlan.modules[2].scalar.strength.strongPeakThreshold = 80.0f;
    profile.inspectionPlan.modules[2].scalar.strength.mediumPeakThreshold = 50.0f;
    profile.inspectionPlan.modules[2].scalar.strength.weakPeakThreshold = 25.0f;
    profile.inspectionPlan.modules[2].scalar.windowPreMs = 10;
    profile.inspectionPlan.modules[2].scalar.windowPostMs = 90;
    profile.inspectionPlan.modules[2].scalar.preFloorWindowPreMs = 250;
    profile.inspectionPlan.modules[2].scalar.preFloorWindowPostMs = 50;
    //profile.inspectionPlan.modules[2].scalar.minSustainedMs = 25;  
    
    profile.inspectionPlan.count = 3;

    // Pattern rules.
    profile.patternRulesConfig.requireSupportForAcceptance = false;
    profile.patternRulesConfig.requiredSupportTarget = EvidenceTarget::FrequencyScoreStrength;
    profile.patternRulesConfig.minimumSupportStrength = StrengthClass::Medium;

    // Field-state windowing.
    profile.fieldStateConfig.occurrenceWindowMs = 4000;
    profile.fieldStateConfig.patternWindowMs = 4000;
    profile.fieldStateConfig.busyOccurrenceCountThreshold = 3;
    profile.fieldStateConfig.denseOccurrenceCountThreshold = 6;
    profile.fieldStateConfig.busyActivityThreshold = 0.45f;
    return profile;
}

inline DetectionProfile makeChirpExperimentalProfile() {
    DetectionProfile profile;

    // Identity and occurrence routing.
    profile.kind = DetectionProfileKind::ChirpExperimental;
    profile.detectorSelection = DetectorSelection::ScalarTransient;
    profile.scalarTransient.observedStream = FeatureStreamId::AmpEnvelope;
    applyAmpEnvelopeScalarTransientTuning(profile.scalarTransient);

    // Pattern rules.
    profile.patternRulesConfig.requireSupportForAcceptance = false;
    profile.patternRulesConfig.requiredSupportTarget = EvidenceTarget::AmpStrength;
    profile.patternRulesConfig.minimumSupportStrength = StrengthClass::Medium;

    // Inspector composition.
    profile.inspectionPlan = {};
    profile.inspectionPlan.modules[0].kind = InspectionModuleKind::ScalarFeatureStrength;
    profile.inspectionPlan.modules[0].target = EvidenceTarget::AmpStrength;
    profile.inspectionPlan.modules[0].scalar.stream = FeatureStreamId::AmpEnvelope;
    profile.inspectionPlan.modules[0].scalar.mode = ScalarInspectionMode::PeakAbsolute;
    profile.inspectionPlan.modules[0].scalar.strength.strongPeakThreshold = 70.0f;
    profile.inspectionPlan.modules[0].scalar.strength.mediumPeakThreshold = 40.0f;
    profile.inspectionPlan.modules[0].scalar.strength.weakPeakThreshold = 20.0f;
    profile.inspectionPlan.modules[0].scalar.windowPreMs = 20;
    profile.inspectionPlan.modules[0].scalar.windowPostMs = 120;
    profile.inspectionPlan.modules[0].scalar.preFloorWindowPreMs = 250;
    profile.inspectionPlan.modules[0].scalar.preFloorWindowPostMs = 50;
    profile.inspectionPlan.count = 1;

    // Field-state windowing.
    profile.fieldStateConfig.occurrenceWindowMs = 4000;
    profile.fieldStateConfig.patternWindowMs = 4000;
    profile.fieldStateConfig.busyOccurrenceCountThreshold = 3;
    profile.fieldStateConfig.denseOccurrenceCountThreshold = 6;
    profile.fieldStateConfig.busyActivityThreshold = 0.45f;
    return profile;
}

inline DetectionProfile makeScalarFreqExperimentalProfile() {
    DetectionProfile profile;

    // Identity and occurrence routing.
    profile.kind = DetectionProfileKind::ScalarFreqExperimental;
    profile.detectorSelection = DetectorSelection::ScalarTransient;
    profile.scalarTransient.observedStream = FeatureStreamId::FrequencyScore;

    // This profile is intentionally experimental and compares frequency-derived
    // scalar evidence through the existing scalar transient lifecycle.
    profile.inspectionPlan = {};
    profile.inspectionPlan.modules[0].kind = InspectionModuleKind::ScalarFeatureStrength;
    profile.inspectionPlan.modules[0].target = EvidenceTarget::FrequencyScoreStrength;
    profile.inspectionPlan.modules[0].scalar.stream = FeatureStreamId::FrequencyScore;
    profile.inspectionPlan.modules[0].scalar.mode = ScalarInspectionMode::PeakCentered;
    profile.inspectionPlan.modules[0].scalar.strength.strongPeakThreshold = 25000.0f;
    profile.inspectionPlan.modules[0].scalar.strength.mediumPeakThreshold = 15000.0f;
    profile.inspectionPlan.modules[0].scalar.strength.weakPeakThreshold = 8000.0f;
    profile.inspectionPlan.modules[0].scalar.windowPreMs = 10;
    profile.inspectionPlan.modules[0].scalar.windowPostMs = 90;
    profile.inspectionPlan.modules[0].scalar.preFloorWindowPreMs = 250;
    profile.inspectionPlan.modules[0].scalar.preFloorWindowPostMs = 50;

    profile.inspectionPlan.modules[1].kind = InspectionModuleKind::ScalarFeatureStrength;
    profile.inspectionPlan.modules[1].target = EvidenceTarget::FrequencyContrastQuality;
    profile.inspectionPlan.modules[1].scalar.stream = FeatureStreamId::FrequencyContrast;
    profile.inspectionPlan.modules[1].scalar.mode = ScalarInspectionMode::PeakCentered;
    profile.inspectionPlan.modules[1].scalar.strength.strongPeakThreshold = 80.0f;
    profile.inspectionPlan.modules[1].scalar.strength.mediumPeakThreshold = 50.0f;
    profile.inspectionPlan.modules[1].scalar.strength.weakPeakThreshold = 25.0f;
    profile.inspectionPlan.modules[1].scalar.windowPreMs = 10;
    profile.inspectionPlan.modules[1].scalar.windowPostMs = 90;
    profile.inspectionPlan.modules[1].scalar.preFloorWindowPreMs = 250;
    profile.inspectionPlan.modules[1].scalar.preFloorWindowPostMs = 50;
    profile.inspectionPlan.count = 2;

    // Pattern rules.
    profile.patternRulesConfig.requireSupportForAcceptance = false;
    profile.patternRulesConfig.requiredSupportTarget = EvidenceTarget::FrequencyScoreStrength;
    profile.patternRulesConfig.minimumSupportStrength = StrengthClass::Medium;

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
    static const DetectionProfile kTonalPulse = makeTonalPulseProfile();
    static const DetectionProfile kAmp = makeAmpProfile();
    static const DetectionProfile kChirpExperimental = makeChirpExperimentalProfile();
    static const DetectionProfile kScalarFreqExperimental = makeScalarFreqExperimentalProfile();

    switch (kind) {
        case DetectionProfileKind::Amp:
            return kAmp;
        case DetectionProfileKind::ChirpExperimental:
            return kChirpExperimental;
        case DetectionProfileKind::ScalarFreqExperimental:
            return kScalarFreqExperimental;
        case DetectionProfileKind::TonalPulse:
        default:
            return kTonalPulse;
    }
}

// Human-readable names for profile kinds used in logs and help text.
inline const char* detectionProfileName(DetectionProfileKind kind) {
    switch (kind) {
        case DetectionProfileKind::TonalPulse:
            return "TonalPulse";
        case DetectionProfileKind::Amp:
            return "Amp";
        case DetectionProfileKind::ChirpExperimental:
            return "ChirpExperimental";
        case DetectionProfileKind::ScalarFreqExperimental:
            return "scalar_freq_experimental";
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

// Legacy compatibility helper for older source-routing naming.
inline const char* occurrenceSourceKindName(OccurrenceSourceKind kind) {
    return detectorSelectionName(kind);
}

// Parse profile names from user-facing text.
inline bool detectionProfileKindFromName(const char* name, DetectionProfileKind& outKind) {
    if (name == nullptr) {
        return false;
    }

    if (strcasecmp(name, "tonalpulse") == 0 || strcasecmp(name, "tonal_pulse") == 0) {
        outKind = DetectionProfileKind::TonalPulse;
        return true;
    }
    if (strcasecmp(name, "amp") == 0) {
        outKind = DetectionProfileKind::Amp;
        return true;
    }
    if (strcasecmp(name, "chirp_experimental") == 0) {
        outKind = DetectionProfileKind::ChirpExperimental;
        return true;
    }
    if (strcasecmp(name, "scalar_freq_experimental") == 0) {
        outKind = DetectionProfileKind::ScalarFreqExperimental;
        return true;
    }

    return false;
}

} // namespace detection

#endif // DETECTION_PROFILE_H
