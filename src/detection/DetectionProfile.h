#pragma once

#include "field/FieldState.h"

namespace detection {

enum class DetectionProfileKind {
    FreqAmp,
    AmpState,
    Chirp,
};

struct DetectionProfile {
    DetectionProfileKind kind = DetectionProfileKind::FreqAmp;
    bool useRoadmapDetection = true;
    bool useRoadmapFrequencyOnly = false;
    bool ampEnabled = true;
    bool detectionOnly = false;
    bool requireTonalForBehavior = true;
    bool idleEnabled = true;

    unsigned long waitAfterTransientMs = 100;
    unsigned long refractoryAfterEmitMs = 0;
    unsigned long idleTimeoutMs = 20000;
    unsigned long idleTimeVariationMs = 10000;
    unsigned long idleBlockedAfterHeardMs = 3000;
    unsigned long idleBlockedAfterOwnEmitMs = 5000;

    FieldStateConfig fieldStateConfig = {};
};

inline const char* detectionProfileName(DetectionProfileKind kind) {
    switch (kind) {
        case DetectionProfileKind::FreqAmp:
            return "FreqAmp";
        case DetectionProfileKind::AmpState:
            return "AmpState";
        case DetectionProfileKind::Chirp:
            return "Chirp";
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
    if (strcasecmp(name, "ampstate") == 0 || strcasecmp(name, "amp_state") == 0) {
        outKind = DetectionProfileKind::AmpState;
        return true;
    }
    if (strcasecmp(name, "chirp") == 0) {
        outKind = DetectionProfileKind::Chirp;
        return true;
    }

    return false;
}

inline DetectionProfile makeFreqAmpProfile() {
    DetectionProfile profile;
    profile.kind = DetectionProfileKind::FreqAmp;
    profile.useRoadmapDetection = true;
    profile.useRoadmapFrequencyOnly = false;
    profile.ampEnabled = true;
    profile.detectionOnly = false;
    profile.requireTonalForBehavior = true;
    profile.idleEnabled = true;
    profile.waitAfterTransientMs = 100;
    profile.refractoryAfterEmitMs = 0;
    profile.idleTimeoutMs = 20000;
    profile.idleTimeVariationMs = 10000;
    profile.idleBlockedAfterHeardMs = 3000;
    profile.idleBlockedAfterOwnEmitMs = 5000;
    profile.fieldStateConfig.signalWindowMs = 5000;
    profile.fieldStateConfig.patternWindowMs = 5000;
    profile.fieldStateConfig.busySignalCountThreshold = 4;
    profile.fieldStateConfig.denseSignalCountThreshold = 8;
    profile.fieldStateConfig.quietSignalCountThreshold = 0;
    profile.fieldStateConfig.quietActivityThreshold = 0.0f;
    profile.fieldStateConfig.busyActivityThreshold = 0.5f;
    return profile;
}

inline DetectionProfile makeAmpStateProfile() {
    DetectionProfile profile = makeFreqAmpProfile();
    profile.kind = DetectionProfileKind::AmpState;
    profile.useRoadmapDetection = false;
    profile.useRoadmapFrequencyOnly = false;
    profile.ampEnabled = true;
    profile.detectionOnly = false;
    profile.requireTonalForBehavior = false;
    profile.waitAfterTransientMs = 500;
    profile.idleTimeoutMs = 15000;
    profile.idleTimeVariationMs = 6000;
    profile.idleBlockedAfterHeardMs = 1500;
    profile.idleBlockedAfterOwnEmitMs = 3000;
    profile.fieldStateConfig.signalWindowMs = 2500;
    profile.fieldStateConfig.patternWindowMs = 2500;
    profile.fieldStateConfig.busySignalCountThreshold = 3;
    profile.fieldStateConfig.denseSignalCountThreshold = 6;
    profile.fieldStateConfig.quietSignalCountThreshold = 1;
    profile.fieldStateConfig.busyActivityThreshold = 0.35f;
    return profile;
}

inline DetectionProfile makeChirpProfile() {
    DetectionProfile profile = makeFreqAmpProfile();
    profile.kind = DetectionProfileKind::Chirp;
    profile.useRoadmapDetection = true;
    profile.useRoadmapFrequencyOnly = false;
    profile.ampEnabled = true;
    profile.detectionOnly = false;
    profile.requireTonalForBehavior = true;
    profile.waitAfterTransientMs = 100;
    profile.refractoryAfterEmitMs = 0;
    profile.idleTimeoutMs = 20000;
    profile.idleTimeVariationMs = 10000;
    profile.idleBlockedAfterHeardMs = 3000;
    profile.idleBlockedAfterOwnEmitMs = 5000;
    profile.fieldStateConfig.signalWindowMs = 4000;
    profile.fieldStateConfig.patternWindowMs = 4000;
    profile.fieldStateConfig.busySignalCountThreshold = 3;
    profile.fieldStateConfig.denseSignalCountThreshold = 6;
    profile.fieldStateConfig.busyActivityThreshold = 0.45f;
    return profile;
}

inline const DetectionProfile& detectionProfileForKind(DetectionProfileKind kind) {
    static const DetectionProfile kFreqAmp = makeFreqAmpProfile();
    static const DetectionProfile kAmpState = makeAmpStateProfile();
    static const DetectionProfile kChirp = makeChirpProfile();

    switch (kind) {
        case DetectionProfileKind::AmpState:
            return kAmpState;
        case DetectionProfileKind::Chirp:
            return kChirp;
        case DetectionProfileKind::FreqAmp:
        default:
            return kFreqAmp;
    }
}

} // namespace detection
