#pragma once

#include <strings.h>

#include "field/FieldState.h"
#include "features/FrequencyMatchEvaluation.h"
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
    ChirpExperimental,
};

enum class ProfileOccurrenceSourceKind {
    Frequency,
    Amp,
};

enum class ProfileInspectionRulesKind {
    TonalPulse,
    ChirpExperimental,
};

struct DetectionProfile {
    // Identity and composition.
    DetectionProfileKind kind = DetectionProfileKind::TonalPulse;
    ProfileOccurrenceSourceKind occurrenceSource = ProfileOccurrenceSourceKind::Frequency;
    ProfileInspectionRulesKind inspectionRules = ProfileInspectionRulesKind::TonalPulse;

    // Stage configuration.
    struct FrequencyOccurrenceTiming {
        unsigned long releaseDebounceMs = 20;
        unsigned long cooldownAfterOnsetMs = 300;
        unsigned long minTransientDurationMs = 80;
    } frequencyOccurrenceTiming = {};

    FrequencyMatchEvaluation::Values frequencyMatchTuning = {};
    PatternRulesConfig patternRulesConfig = {};
    InspectionConfig inspectionConfig = defaultInspectionConfig();
    FieldStateConfig fieldStateConfig = {};
};

// Actual profiles. These are the concrete profile definitions used at runtime.
inline DetectionProfile makeTonalPulseProfile() {
    DetectionProfile profile;

    // Identity and occurrence routing.
    profile.kind = DetectionProfileKind::TonalPulse;
    profile.occurrenceSource = ProfileOccurrenceSourceKind::Frequency;
    profile.inspectionRules = ProfileInspectionRulesKind::TonalPulse;

    // Frequency path tuning.
    profile.frequencyOccurrenceTiming.releaseDebounceMs = 10;
    profile.frequencyOccurrenceTiming.cooldownAfterOnsetMs = 20;
    profile.frequencyOccurrenceTiming.minTransientDurationMs = 80;
    profile.frequencyMatchTuning.scoreMin = 10000.0f;
    profile.frequencyMatchTuning.contrastMin = 50.0f;

    // Inspector configuration.
    profile.inspectionConfig = defaultInspectionConfig();
    profile.inspectionConfig.broadAmp.windowPreMs = 10;
    profile.inspectionConfig.broadAmp.windowPostMs = 10;

    // Pattern rules.
    profile.patternRulesConfig.requireSupportForAcceptance = true;
    profile.patternRulesConfig.supportSource = PatternSupportSource::BroadAmp;
    profile.patternRulesConfig.minimumSupport = StrengthClass::Medium;

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

inline DetectionProfile makeChirpExperimentalProfile() {
    DetectionProfile profile = makeTonalPulseProfile();

    // Identity and occurrence routing.
    profile.kind = DetectionProfileKind::ChirpExperimental;
    profile.occurrenceSource = ProfileOccurrenceSourceKind::Amp;
    profile.inspectionRules = ProfileInspectionRulesKind::ChirpExperimental;

    // Frequency path tuning.
    profile.frequencyOccurrenceTiming.releaseDebounceMs = 10;
    profile.frequencyOccurrenceTiming.cooldownAfterOnsetMs = 10;
    profile.frequencyOccurrenceTiming.minTransientDurationMs = 90;
    profile.frequencyMatchTuning.scoreMin = 10000.0f;
    profile.frequencyMatchTuning.contrastMin = 50.0f;

    // Pattern rules.
    profile.patternRulesConfig.requireSupportForAcceptance = false;
    profile.patternRulesConfig.supportSource = PatternSupportSource::BroadAmp;
    profile.patternRulesConfig.minimumSupport = StrengthClass::Medium;

    // Inspector configuration.
    profile.inspectionConfig = defaultInspectionConfig();
    profile.inspectionConfig.broadAmp.windowPreMs = 20;
    profile.inspectionConfig.broadAmp.windowPostMs = 120;

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
    static const DetectionProfile kChirpExperimental = makeChirpExperimentalProfile();

    switch (kind) {
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
        case DetectionProfileKind::ChirpExperimental:
            return "ChirpExperimental";
    }
    return "unknown";
}

// Human-readable names for occurrence sources used in logs and help text.
inline const char* profileOccurrenceSourceName(ProfileOccurrenceSourceKind kind) {
    switch (kind) {
        case ProfileOccurrenceSourceKind::Frequency:
            return "FrequencyOccurrenceSource";
        case ProfileOccurrenceSourceKind::Amp:
            return "AmpOccurrenceSource";
    }
    return "unknown";
}

// Human-readable names for inspection-rule sets used in logs and help text.
inline const char* profileInspectionRulesName(ProfileInspectionRulesKind kind) {
    switch (kind) {
        case ProfileInspectionRulesKind::TonalPulse:
            return "TonalPulseRules";
        case ProfileInspectionRulesKind::ChirpExperimental:
            return "ChirpExperimentalRules";
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
    if (strcasecmp(name, "chirp_experimental") == 0) {
        outKind = DetectionProfileKind::ChirpExperimental;
        return true;
    }

    return false;
}

} // namespace detection
