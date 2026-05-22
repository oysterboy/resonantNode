#pragma once

#include <strings.h>

#include "field/FieldState.h"
#include "features/FrequencyMatchEvaluation.h"
#include "inspector/InspectorTypes.h"
#include "patterns/PatternRules.h"

namespace detection {

enum class DetectionProfileKind {
    FreqAmp,
    Chirp,
};

enum class ProfileSignalEmitterKind {
    Frequency,
    Amp,
};

enum class ProfileInspectionRulesKind {
    FreqAmp,
    Chirp,
};

enum class ProfilePatternRulesKind {
    PatternRules,
    AmpActivity,
    ChirpSequence,
};

struct DetectionProfile {
    DetectionProfileKind kind = DetectionProfileKind::FreqAmp;
    ProfileSignalEmitterKind signalEmitter = ProfileSignalEmitterKind::Frequency;
    ProfileInspectionRulesKind inspectionRules = ProfileInspectionRulesKind::FreqAmp;
    ProfilePatternRulesKind patternRules = ProfilePatternRulesKind::PatternRules;
    FrequencyMatchEvaluation::Values frequencyTuning = {};
    PatternRulesConfig patternRulesConfig = {};

    InspectionConfig inspectionConfig = defaultInspectionConfig();

    FieldStateConfig fieldStateConfig = {};
};

// Actual profiles. These are the concrete profile definitions used at runtime.
inline DetectionProfile makeFreqAmpProfile() {
    DetectionProfile profile;

    // Identity and signal routing.
    profile.kind = DetectionProfileKind::FreqAmp;
    profile.signalEmitter = ProfileSignalEmitterKind::Frequency;
    profile.inspectionRules = ProfileInspectionRulesKind::FreqAmp;
    profile.patternRules = ProfilePatternRulesKind::PatternRules;

    profile.patternRulesConfig.requireSupportForAcceptance = true;
    profile.frequencyTuning.scoreMin = 10000.0f;
    profile.frequencyTuning.contrastMin = 50.0f;

    // Inspector configuration.
    profile.inspectionConfig = defaultInspectionConfig(); // shared inspector defaults
    profile.inspectionConfig.ampWindowPreMs = 10;
    profile.inspectionConfig.ampWindowPostMs = 80;

    // Field-state windowing.
    profile.fieldStateConfig.signalWindowMs = 3500;
    profile.fieldStateConfig.patternWindowMs = 3500;
    profile.fieldStateConfig.busySignalCountThreshold = 3;
    profile.fieldStateConfig.denseSignalCountThreshold = 6;
    profile.fieldStateConfig.quietSignalCountThreshold = 0;
    profile.fieldStateConfig.quietActivityThreshold = 0.0f;
    profile.fieldStateConfig.busyActivityThreshold = 0.4f;
    return profile;
}

inline DetectionProfile makeChirpProfile() {
    DetectionProfile profile = makeFreqAmpProfile();

    // Identity and signal routing.
    profile.kind = DetectionProfileKind::Chirp;
    profile.signalEmitter = ProfileSignalEmitterKind::Amp;
    profile.inspectionRules = ProfileInspectionRulesKind::Chirp;
    profile.patternRules = ProfilePatternRulesKind::ChirpSequence;

    profile.patternRulesConfig.requireSupportForAcceptance = true;
    profile.frequencyTuning.scoreMin = 10000.0f;
    profile.frequencyTuning.contrastMin = 50.0f;

    // Inspector configuration.
    profile.inspectionConfig = defaultInspectionConfig();
    profile.inspectionConfig.ampWindowPreMs = 20;
    profile.inspectionConfig.ampWindowPostMs = 120;

    // Field-state windowing.
    profile.fieldStateConfig.signalWindowMs = 4000;
    profile.fieldStateConfig.patternWindowMs = 4000;
    profile.fieldStateConfig.busySignalCountThreshold = 3;
    profile.fieldStateConfig.denseSignalCountThreshold = 6;
    profile.fieldStateConfig.busyActivityThreshold = 0.45f;
    return profile;
}

inline const DetectionProfile& detectionProfileForKind(DetectionProfileKind kind) {
    static const DetectionProfile kFreqAmp = makeFreqAmpProfile();
    static const DetectionProfile kChirp = makeChirpProfile();

    switch (kind) {
        case DetectionProfileKind::Chirp:
            return kChirp;
        case DetectionProfileKind::FreqAmp:
        default:
            return kFreqAmp;
    }
}

// Human-readable names for logs and help text.
inline const char* detectionProfileName(DetectionProfileKind kind) {
    switch (kind) {
        case DetectionProfileKind::FreqAmp:
            return "FreqAmp";
        case DetectionProfileKind::Chirp:
            return "Chirp";
    }
    return "unknown";
}

inline const char* profileSignalEmitterName(ProfileSignalEmitterKind kind) {
    switch (kind) {
        case ProfileSignalEmitterKind::Frequency:
            return "FrequencySignalEmitter";
        case ProfileSignalEmitterKind::Amp:
            return "AmpSignalEmitter";
    }
    return "unknown";
}

inline const char* profileInspectionRulesName(ProfileInspectionRulesKind kind) {
    switch (kind) {
        case ProfileInspectionRulesKind::FreqAmp:
            return "FreqAmpRules";
        case ProfileInspectionRulesKind::Chirp:
            return "ChirpRules";
    }
    return "unknown";
}

inline const char* profilePatternRulesName(ProfilePatternRulesKind kind) {
    switch (kind) {
        case ProfilePatternRulesKind::PatternRules:
            return "PatternRules";
        case ProfilePatternRulesKind::AmpActivity:
            return "AmpActivity";
        case ProfilePatternRulesKind::ChirpSequence:
            return "ChirpSequence";
    }
    return "unknown";
}

inline bool detectionProfileKindFromName(const char* name, DetectionProfileKind& outKind) {
    if (name == nullptr) {
        return false;
    }

    if (strcasecmp(name, "freqamp") == 0 || strcasecmp(name, "freq_amp") == 0) {
        outKind = DetectionProfileKind::FreqAmp;
        return true;
    }
    if (strcasecmp(name, "chirp") == 0) {
        outKind = DetectionProfileKind::Chirp;
        return true;
    }

    return false;
}

} // namespace detection
