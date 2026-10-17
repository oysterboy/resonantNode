#pragma once

#include <stdint.h>

/*
Param core vocabulary.

Minimal runtime-only bound-field param system. See
docs/roadmaps/roadmap-param-config.md (PAR-002) for the intended shape:
flat ParamBindings, module-owned meaning, no persistence yet.

Paths are the stable public identity. ParamId is internal identity, kept
separate so a future transport (Serial today, VEKTOR later) never needs to
know about field pointers or storage details.
*/
namespace param {

enum class ModuleId : uint8_t {
    Node,
    Detection,
    Behavior,
    Output,
};

inline const char* moduleName(ModuleId module) {
    switch (module) {
        case ModuleId::Node:
            return "node";
        case ModuleId::Detection:
            return "detection";
        case ModuleId::Behavior:
            return "behavior";
        case ModuleId::Output:
            return "output";
    }
    return "unknown";
}

enum class ParamType : uint8_t {
    UInt16,
    UInt32,
    Float,
    Bool,
};

// Stable internal identity for a registered param. Grows as modules register
// more fields; not reused across removed params.
enum class ParamId : uint16_t {
    Detection_FreqAttackScoreMin,
    Detection_FreqReleaseScoreMin,
    Detection_FreqAttackContrastMin,
    Detection_FreqReleaseContrastMin,
    Detection_ScalarOnsetThreshold,
    Detection_ScalarReleaseThreshold,
    Detection_ScalarCooldownMs,
    Detection_ScalarMinDurationMs,
    Detection_ScalarMaxDurationMs,
    Detection_ScalarMinPeakStrength,
    Detection_ScalarReleaseDebounceMs,
    Detection_ScalarRequireCarrierQuality,
    Detection_ScalarRequireMinStrength,
    Detection_ScalarMinStrength,
    Detection_ScalarMinReleaseCoverageMs,
    Detection_ScalarMinLongestIslandMs,
    Detection_ScalarMaxGapMs,
    // Network band the Node listens to (FreqBandStream bank; 0/0 = single
    // target at the chirp tone). DET-013, decisions/2026-10-15-signal-
    // variation-inside-a-shared-band.md.
    Detection_BandLowHz,
    Detection_BandHighHz,
    // The Node's network signal: a bin-aligned hop train inside the band
    // (BEH-005 / OUT-005, decisions/2026-10-16-hop-acceptance-contrast-
    // gate-not-amp.md). rotation = which bin the train starts on (per node,
    // from the MAC at boot unless set).
    Output_HopEnabled,
    Output_HopRotation,
    Output_HopPulseMs,
    Output_HopGapMs,
    Output_HopSteps,
    // The idle call (owner 2026-10-17): an announcement tone outside the band,
    // then the node's own call (the hop train), so an idle call wakes the network.
    Output_IdleAnnounceHz,
    Output_IdleAnnounceMs,
};

enum class ParamSetStatus : uint8_t {
    Ok,
    UnknownParam,
    ParseError,
    OutOfRange,
};

inline const char* paramSetStatusName(ParamSetStatus status) {
    switch (status) {
        case ParamSetStatus::Ok:
            return "ok";
        case ParamSetStatus::UnknownParam:
            return "unknown_param";
        case ParamSetStatus::ParseError:
            return "parse_error";
        case ParamSetStatus::OutOfRange:
            return "out_of_range";
    }
    return "unknown";
}

} // namespace param
