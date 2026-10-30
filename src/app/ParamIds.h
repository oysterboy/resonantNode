#pragma once

#include <stdint.h>

/*
The Node's param vocabulary for lib/ParamRegistry (PAR-016): which module owns a
param (ModuleId, the registry's opaque group tag) and each param's stable id
(ParamId, the registry's opaque id). Paths are the public identity; ids are
internal, never reused once removed. Moved here from src/param/ParamTypes.h
when the registry became an in-repo library (step 10 item 3).
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

// The registry's group namer: PARAM LIST/DUMP print module=<name>.
inline const char* moduleNameForGroup(uint8_t group) {
    return moduleName(static_cast<ModuleId>(group));
}

// Stable internal identity for a registered param. Grows as modules register
// more fields; not reused across removed params.
enum class ParamId : uint16_t {
    // Retired 2026-10-22 with FrequencyMatch (decisions/2026-10-22-remove-
    // frequencymatch.md): no longer registered, kept so later ids keep
    // their values.
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
    // Behavior's BehaviorGateConfig, one param per field (PAR-006, step 10
    // item 4; they replace the RB BEHAV command, decisions/2026-10-22-live-
    // params-profile-reset-no-rb-behav.md).
    Behavior_IdleEnabled,
    Behavior_WaitAfterHeardMs,
    Behavior_RefractoryAfterEmitMs,
    Behavior_SuppressSelfChirpMs,
    Behavior_DetectionSuppressTailMs,
    Behavior_IdleTimeoutMs,
    Behavior_IdleTimeoutVariationMs,
    Behavior_IdleBlockedAfterHeardMs,
    Behavior_IdleBlockedAfterOwnEmitMs,
    Behavior_RestAfterFieldPatterns,
    Behavior_OwnEmitBudget,
    Behavior_OwnEmitBudgetWindowMs,
    Behavior_RestMs,
    // The inspectors' medium limits, live (step 10 item 7; build overrides
    // SCALAR_CONTRAST/AMP_MEDIUM_THRESHOLD before). Applied to the plan's
    // module with the matching target.
    Detection_ContrastMedium,
    Detection_AmpMedium,
    // Silent (2026-10-23): the node listens and reports but never sounds;
    // PARAM SAVE keeps it across a reboot.
    Output_Silent,
    // The radio for remote params (PAR-013, step 11a): on by default, read at
    // boot only (PARAM SET + SAVE, then a reboot, turns it off).
    Node_RadioEnabled,
    // The shape inspector's settings, live (step 11g B2, review D2.7; DET-015). Applied to the plan's BinShape
    // module. Its level is the scalar release threshold, not a param of its own (review D1.5).
    Detection_ShapePreMs,
    Detection_ShapeTailMs,
    Detection_ShapeMaxRisingStepBins,
    Detection_ShapeMinRunMs,
    Detection_ShapeMediumMinBins,
    Detection_ShapeMediumMinRisingSteps,
    Detection_ShapeStrongMinBins,
    Detection_ShapeStrongMinRisingSteps,
    // The level automaton (BEH-008, step 16).
    Behavior_LevelMax,
    Behavior_LevelDecayMs,
    Behavior_AnswerJitterMs,
};

}  // namespace param
