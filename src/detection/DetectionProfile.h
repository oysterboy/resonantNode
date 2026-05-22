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
inspection config, field-state config, and frequency tuning.

Profiles declare composition; DetectionRuntime applies the selected fields at fixed stages.
*/
enum class DetectionProfileKind {
    TonalPulse,
    Chirp,
};

enum class ProfileOccurrenceSourceKind {
    Frequency,
    Amp,
};

enum class ProfileInspectionRulesKind {
    TonalPulse,
    Chirp,
};

struct DetectionProfile {
    // Identity and composition.
    DetectionProfileKind kind = DetectionProfileKind::TonalPulse;
    ProfileOccurrenceSourceKind occurrenceSource = ProfileOccurrenceSourceKind::Frequency;
    ProfileInspectionRulesKind inspectionRules = ProfileInspectionRulesKind::TonalPulse;

    // Stage configuration.
    FrequencyMatchEvaluation::Values frequencyTuning = {};
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

    profile.patternRulesConfig.requireSupportForAcceptance = true;
    profile.frequencyTuning.scoreMin = 10000.0f;
    profile.frequencyTuning.contrastMin = 50.0f;

    // Inspector configuration.
    profile.inspectionConfig = defaultInspectionConfig(); // shared inspector defaults
    profile.inspectionConfig.ampWindowPreMs = 10;
    profile.inspectionConfig.ampWindowPostMs = 80;

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

inline DetectionProfile makeChirpProfile() {
    DetectionProfile profile = makeTonalPulseProfile();

    // Identity and occurrence routing.
    profile.kind = DetectionProfileKind::Chirp;
    profile.occurrenceSource = ProfileOccurrenceSourceKind::Amp;
    profile.inspectionRules = ProfileInspectionRulesKind::Chirp;

    profile.patternRulesConfig.requireSupportForAcceptance = true;
    profile.frequencyTuning.scoreMin = 10000.0f;
    profile.frequencyTuning.contrastMin = 50.0f;

    // Inspector configuration.
    profile.inspectionConfig = defaultInspectionConfig();
    profile.inspectionConfig.ampWindowPreMs = 20;
    profile.inspectionConfig.ampWindowPostMs = 120;

    // Field-state windowing.
    profile.fieldStateConfig.occurrenceWindowMs = 4000;
    profile.fieldStateConfig.patternWindowMs = 4000;
    profile.fieldStateConfig.busySignalCountThreshold = 3;
    profile.fieldStateConfig.denseSignalCountThreshold = 6;
    profile.fieldStateConfig.busyActivityThreshold = 0.45f;
    return profile;
}

inline const DetectionProfile& detectionProfileForKind(DetectionProfileKind kind) {
    static const DetectionProfile kTonalPulse = makeTonalPulseProfile();
    static const DetectionProfile kChirp = makeChirpProfile();

    switch (kind) {
        case DetectionProfileKind::Chirp:
            return kChirp;
        case DetectionProfileKind::TonalPulse:
        default:
            return kTonalPulse;
    }
}

// Human-readable names for logs and help text.
inline const char* detectionProfileName(DetectionProfileKind kind) {
    switch (kind) {
        case DetectionProfileKind::TonalPulse:
            return "TonalPulse";
        case DetectionProfileKind::Chirp:
            return "Chirp";
    }
    return "unknown";
}

inline const char* profileOccurrenceSourceName(ProfileOccurrenceSourceKind kind) {
    switch (kind) {
        case ProfileOccurrenceSourceKind::Frequency:
            return "FrequencyOccurrenceSource";
        case ProfileOccurrenceSourceKind::Amp:
            return "AmpOccurrenceSource";
    }
    return "unknown";
}

inline const char* profileInspectionRulesName(ProfileInspectionRulesKind kind) {
    switch (kind) {
        case ProfileInspectionRulesKind::TonalPulse:
            return "TonalPulseRules";
        case ProfileInspectionRulesKind::Chirp:
            return "ChirpRules";
    }
    return "unknown";
}

inline bool detectionProfileKindFromName(const char* name, DetectionProfileKind& outKind) {
    if (name == nullptr) {
        return false;
    }

    if (strcasecmp(name, "tonalpulse") == 0 || strcasecmp(name, "tonal_pulse") == 0) {
        outKind = DetectionProfileKind::TonalPulse;
        return true;
    }
    if (strcasecmp(name, "chirp") == 0) {
        outKind = DetectionProfileKind::Chirp;
        return true;
    }

    return false;
}

} // namespace detection


