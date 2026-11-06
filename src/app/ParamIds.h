#pragma once

#include <stdint.h>

/*
The Node's param vocabulary for lib/ParamRegistry (PAR-016): which module owns a
param (ModuleId, the registry's opaque group tag) and each param's stable id
(ParamId, the registry's opaque id). Paths are the public identity; ids are
internal, never reused once removed.
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
    Detection_ScalarMinStrength,  // retired 2026-11-06, not registered (same decision)
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
    // The idle call (decisions/2026-10-17-idle-call-wakes-the-network.md): an announcement tone outside the band,
    // then the node's own call (the hop train), so an idle call wakes the network.
    Output_IdleAnnounceHz,
    Output_IdleAnnounceMs,
    // Behavior's BehaviorGateConfig, one param per field (PAR-006,
    // decisions/2026-10-22-live-params-profile-reset-no-rb-behav.md).
    Behavior_IdleEnabled,
    Behavior_WaitAfterHeardMs,
    Behavior_RefractoryAfterEmitMs,
    Behavior_SuppressSelfChirpMs,
    Behavior_DetectionSuppressTailMs,
    Behavior_IdleTimeoutMs,
    Behavior_IdleTimeoutVariationMs,
    Behavior_IdleBlockedAfterHeardMs,
    Behavior_IdleBlockedAfterOwnEmitMs,
    // Retired 2026-11-06, not registered (decisions/2026-11-06-retire-the-old-rest-triggers.md); the ids stay taken.
    Behavior_RestAfterFieldPatterns,
    Behavior_OwnEmitBudget,
    Behavior_OwnEmitBudgetWindowMs,
    Behavior_RestMs,
    // The inspectors' medium limits, live. Applied to the plan's module with the
    // matching target.
    Detection_ContrastMedium,
    Detection_AmpMedium,
    // Silent (2026-10-23): the node listens and reports but never sounds;
    // PARAM SAVE keeps it across a reboot.
    Output_Silent,
    // The radio for remote params (PAR-013): on by default, read at
    // boot only (PARAM SET + SAVE, then a reboot, turns it off).
    Node_RadioEnabled,
    // The shape inspector's settings, live (DET-015). Applied to the plan's BinShape module. Its level is the
    // scalar release threshold, not a param of its own.
    Detection_ShapePreMs,
    Detection_ShapeTailMs,
    Detection_ShapeMaxRisingStepBins,
    Detection_ShapeMinRunMs,
    Detection_ShapeMediumMinBins,
    Detection_ShapeMediumMinRisingSteps,
    Detection_ShapeStrongMinBins,
    Detection_ShapeStrongMinRisingSteps,
    // The level automaton (BEH-008).
    Behavior_LevelMax,
    Behavior_LevelDecayMs,
    Behavior_AnswerJitterMs,
    // The base bin and a band per node (BEH-009).
    Behavior_BaseScatterBins,
    Behavior_BandPerNode,
    Behavior_BandRangeLowHz,
    Behavior_BandRangeHighHz,
    // The echo shell's modes (ECHO-003, 2026-11-04): the echo room's echo.* and Whispernet's whisper.*, each
    // registered only by its own build; stored in the same NVS namespace as the Node's, under their own ids.
    Echo_Onset,
    Echo_Release,
    Echo_Floor,
    Echo_MinPeak,
    Echo_AttackMs,
    Echo_PreMs,
    Echo_PostMs,
    Echo_Gain,
    Echo_Enabled,
    Echo_LevelMax,
    Echo_LevelOffset,  // retired 2026-11-06, not registered (echo.start_chance_pct is Echo_StartChancePct)
    Echo_LevelDecayMs,
    Echo_RestMs,
    Echo_WaitMs,
    Echo_JitterMs,
    Echo_TailMs,
    Echo_RefractoryMs,
    Whisper_Onset,
    Whisper_Release,
    Whisper_Floor,
    Whisper_Loud,
    Whisper_Modulation,
    Whisper_FlatMin,
    Whisper_FlatMax,
    Whisper_DelayMs,
    Whisper_Gain,
    Whisper_Enabled,
    Whisper_PreMs,
    Whisper_HoldMs,
    Whisper_TailMs,
    Whisper_RampMs,
    Whisper_FirstCheckMs,
    Whisper_CheckEveryMs,
    Whisper_MinPhraseMs,
    Whisper_GapMs,
    Whisper_LevelMax,
    Whisper_LevelOffset,  // retired 2026-11-06, not registered (whisper.start_chance_pct is Whisper_StartChancePct)
    Whisper_LevelDecayMs,
    Whisper_RestMs,
    Whisper_Adaptive,
    Whisper_RoomFactor,
    Whisper_RoomRiseMs,
    Whisper_RoomFallMs,
    Whisper_Aec,
    Whisper_AecTaps,
    Whisper_AecMu,
    Whisper_AecTrimMs,
    Whisper_ListenWhilePlaying,
    Whisper_ListenErleDb,
    Whisper_AecDelay,  // replaces Whisper_AecTrimMs (retired 2026-11-04 with the lockstep output: not registered)
    Whisper_MaxPhraseMs,
    Node_RadioLog,  // node.radio_log, shared by every mode (a board keeps it across the switch)
    Behavior_StartChancePct,
    Echo_DampingPct,
    Echo_FadeMs,
    Whisper_LevelBand,
    Whisper_TimbreCheck,
    Whisper_LowMaxDb,
    Behavior_PlateauPct,
    Behavior_SpreadShrinks,
    Echo_StartChancePct,
    Echo_PlateauPct,
    Whisper_StartChancePct,
    Whisper_PlateauPct,
};

}  // namespace param
