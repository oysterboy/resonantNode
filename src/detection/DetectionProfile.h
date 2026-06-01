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
DetectionProfileKind { TonalPulse, Amp, ChirpExperimental }
OccurrenceSourceKind { FrequencyMatch, ScalarTransient }
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

enum class OccurrenceSourceKind {
    FrequencyMatch,
    ScalarTransient,
};

enum class DetectionProfileKind {
    TonalPulse,
    Amp,
    ChirpExperimental,
};

struct FrequencyMatchConfig {
    unsigned long releaseDebounceMs = 20;
    unsigned long cooldownAfterOnsetMs = 300;
    unsigned long minTransientDurationMs = 80;
    float scoreMin = 10000.0f;
    float contrastMin = 50.0f;
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
    OccurrenceSourceKind occurrenceSource = OccurrenceSourceKind::FrequencyMatch;

    // Stage configuration.
    FrequencyMatchConfig frequencyMatch = {};
    ScalarTransientConfig scalarTransient = {};
    PatternRulesConfig patternRulesConfig = {};
    InspectionPlan inspectionPlan = {};
    FieldStateConfig fieldStateConfig = {};
};

// Actual profiles. These are the concrete profile definitions used at runtime.
inline DetectionProfile makeTonalPulseProfile() {
    DetectionProfile profile;

    // Identity and occurrence routing.
    profile.kind = DetectionProfileKind::TonalPulse;
    profile.occurrenceSource = OccurrenceSourceKind::FrequencyMatch;

    // Frequency path tuning.
    profile.frequencyMatch.releaseDebounceMs = 20;
    profile.frequencyMatch.cooldownAfterOnsetMs = 20;
    profile.frequencyMatch.minTransientDurationMs = 80;
    profile.frequencyMatch.scoreMin = 10000.0f;
    profile.frequencyMatch.contrastMin = 50.0f;

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
    profile.patternRulesConfig.requireSupportForAcceptance = true;
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
    profile.occurrenceSource = OccurrenceSourceKind::ScalarTransient;
    profile.scalarTransient.observedStream = FeatureStreamId::AmpEnvelope;

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
    profile.occurrenceSource = OccurrenceSourceKind::ScalarTransient;
    profile.scalarTransient.observedStream = FeatureStreamId::AmpEnvelope;

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

// Profile lookup by kind.
inline const DetectionProfile& detectionProfileForKind(DetectionProfileKind kind) {
    static const DetectionProfile kTonalPulse = makeTonalPulseProfile();
    static const DetectionProfile kAmp = makeAmpProfile();
    static const DetectionProfile kChirpExperimental = makeChirpExperimentalProfile();

    switch (kind) {
        case DetectionProfileKind::Amp:
            return kAmp;
        case DetectionProfileKind::ChirpExperimental:
            return kChirpExperimental;
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
    }
    return "unknown";
}

// Human-readable names for occurrence sources used in logs and help text.
inline const char* occurrenceSourceKindName(OccurrenceSourceKind kind) {
    switch (kind) {
        case OccurrenceSourceKind::FrequencyMatch:
            return "FrequencyMatchSource";
        case OccurrenceSourceKind::ScalarTransient:
            return "ScalarTransientSource";
    }
    return "unknown";
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

    return false;
}

} // namespace detection

#endif // DETECTION_PROFILE_H
