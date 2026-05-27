#pragma once

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
*/
enum class DetectionProfileKind {
    TonalPulse,
    Amp,
    ChirpExperimental,
};

enum class OccurrenceSourceKind {
    FrequencyMatch,
    ScalarTransient,
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
    InspectionConfig inspectionConfig = defaultInspectionConfig();
    FieldStateConfig fieldStateConfig = {};
};

// Actual profiles. These are the concrete profile definitions used at runtime.
inline DetectionProfile makeTonalPulseProfile() {
    DetectionProfile profile;

    // Identity and occurrence routing.
    profile.kind = DetectionProfileKind::TonalPulse;
    profile.occurrenceSource = OccurrenceSourceKind::FrequencyMatch;

    // Frequency path tuning.
    profile.frequencyMatch.releaseDebounceMs = 10;
    profile.frequencyMatch.cooldownAfterOnsetMs = 20;
    profile.frequencyMatch.minTransientDurationMs = 80;
    profile.frequencyMatch.scoreMin = 10000.0f;
    profile.frequencyMatch.contrastMin = 50.0f;

    // Inspector configuration.
    profile.inspectionConfig = defaultInspectionConfig();
    profile.inspectionConfig.frequencyScore.enabled = false;
    profile.inspectionConfig.ampStrength.windowPreMs = 10;
    profile.inspectionConfig.ampStrength.windowPostMs = 10;

    // Pattern rules.
    profile.patternRulesConfig.requireSupportForAcceptance = true;
    profile.patternRulesConfig.requiredSupportTarget = EvidenceTarget::AmpStrength;
    profile.patternRulesConfig.minimumSupportStrength = StrengthClass::Medium;

    // Field-state windowing.
    profile.fieldStateConfig.occurrenceWindowMs = 3500;
    profile.fieldStateConfig.patternWindowMs = 3500;
    profile.fieldStateConfig.busySignalCountThreshold = 3;
    profile.fieldStateConfig.denseSignalCountThreshold = 6;
    profile.fieldStateConfig.quietSignalCountThreshold = 0;
    profile.fieldStateConfig.quietActivityThreshold = 0.0f;
    profile.fieldStateConfig.busyActivityThreshold = 0.4f;
    return profile;
}

inline DetectionProfile makeAmpProfile() {
    DetectionProfile profile = makeTonalPulseProfile();

    // Identity and occurrence routing.
    profile.kind = DetectionProfileKind::Amp;
    profile.occurrenceSource = OccurrenceSourceKind::ScalarTransient;
    profile.scalarTransient = {};
    profile.scalarTransient.observedStream = FeatureStreamId::AmpEnvelope;

    // Amp detector path tuning.
    profile.frequencyMatch.releaseDebounceMs = 10;
    profile.frequencyMatch.cooldownAfterOnsetMs = 10;
    profile.frequencyMatch.minTransientDurationMs = 90;
    profile.frequencyMatch.scoreMin = 10000.0f;
    profile.frequencyMatch.contrastMin = 50.0f;


    // Inspector configuration.
    profile.inspectionConfig = defaultInspectionConfig();
    profile.inspectionConfig.ampStrength.enabled = false;
    profile.inspectionConfig.frequencyScore.enabled = true;
    profile.inspectionConfig.frequencyScore.stream = FeatureStreamId::FrequencyScore;
    profile.inspectionConfig.frequencyScore.target = EvidenceTarget::FrequencyScoreStrength;
    profile.inspectionConfig.frequencyScore.windowPreMs = 20;
    profile.inspectionConfig.frequencyScore.windowPostMs = 120;

        // Pattern rules.
    profile.patternRulesConfig.requireSupportForAcceptance = true;
    profile.patternRulesConfig.requiredSupportTarget = EvidenceTarget::FrequencyScoreStrength;
    profile.patternRulesConfig.minimumSupportStrength = StrengthClass::Medium;
    
    // Field-state windowing.
    profile.fieldStateConfig.occurrenceWindowMs = 4000;
    profile.fieldStateConfig.patternWindowMs = 4000;
    profile.fieldStateConfig.busySignalCountThreshold = 3;
    profile.fieldStateConfig.denseSignalCountThreshold = 6;
    profile.fieldStateConfig.busyActivityThreshold = 0.45f;
    return profile;
}

inline DetectionProfile makeChirpExperimentalProfile() {
    DetectionProfile profile = makeTonalPulseProfile();

    // Identity and occurrence routing.
    profile.kind = DetectionProfileKind::ChirpExperimental;
    profile.occurrenceSource = OccurrenceSourceKind::ScalarTransient;
    profile.scalarTransient.observedStream = FeatureStreamId::AmpEnvelope;

    // Amp detector path tuning.
    profile.frequencyMatch.releaseDebounceMs = 10;
    profile.frequencyMatch.cooldownAfterOnsetMs = 10;
    profile.frequencyMatch.minTransientDurationMs = 90;
    profile.frequencyMatch.scoreMin = 10000.0f;
    profile.frequencyMatch.contrastMin = 50.0f;

    // Pattern rules.
    profile.patternRulesConfig.requireSupportForAcceptance = false;
    profile.patternRulesConfig.requiredSupportTarget = EvidenceTarget::AmpStrength;
    profile.patternRulesConfig.minimumSupportStrength = StrengthClass::Medium;

    // Inspector configuration.
    profile.inspectionConfig = defaultInspectionConfig();
    profile.inspectionConfig.frequencyScore.enabled = false;
    profile.inspectionConfig.ampStrength.windowPreMs = 20;
    profile.inspectionConfig.ampStrength.windowPostMs = 120;

    // Field-state windowing.
    profile.fieldStateConfig.occurrenceWindowMs = 4000;
    profile.fieldStateConfig.patternWindowMs = 4000;
    profile.fieldStateConfig.busySignalCountThreshold = 3;
    profile.fieldStateConfig.denseSignalCountThreshold = 6;
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
