#pragma once

#include <ParamRegistry.h>

#include "../app/ParamIds.h"
#include "../app/RuntimeDefaults.h"
#include "BehaviorProfile.h"

/*
BehaviorGateConfig's params: the config describes its own fields as behavior.* paths, ids
and ranges. The Node binds its live copy (a PARAM SET applies through ResonantBehavior::configure()).
*/
namespace param {

// behavior.*: one param per BehaviorGateConfig field (PAR-006).
// The paths are the snake_case field names; the RB BEHAV keys they replaced
// are mapped in decisions/2026-10-22-live-params-profile-reset-no-rb-behav.md.
inline void describeParams(ParamRegistry& registry, BehaviorGateConfig& b) {
    registry.addBool(ParamId::Behavior_IdleEnabled, ModuleId::Behavior, "behavior.idle_enabled", &b.idleEnabled);
    registry.addUInt32(ParamId::Behavior_WaitAfterHeardMs, ModuleId::Behavior, "behavior.wait_after_heard_ms",
                       &b.waitAfterHeardMs, 0U, 10000U);
    registry.addUInt32(ParamId::Behavior_RefractoryAfterEmitMs, ModuleId::Behavior, "behavior.refractory_after_emit_ms",
                       &b.refractoryAfterEmitMs, 0U, 60000U);
    registry.addUInt32(ParamId::Behavior_SuppressSelfChirpMs, ModuleId::Behavior, "behavior.suppress_self_chirp_ms",
                       &b.behaviorSuppressSelfChirpMs, 0U, 10000U);
    registry.addUInt32(ParamId::Behavior_DetectionSuppressTailMs, ModuleId::Behavior,
                       "behavior.detection_suppress_tail_ms", &b.detectionSuppressTailMsOwnEmit, 0U, 5000U);
    registry.addUInt32(ParamId::Behavior_IdleTimeoutMs, ModuleId::Behavior, "behavior.idle_timeout_ms",
                       &b.idleTimeoutMs, 0U, 3600000U);
    registry.addUInt32(ParamId::Behavior_IdleTimeoutVariationMs, ModuleId::Behavior,
                       "behavior.idle_timeout_variation_ms", &b.idleTimeVariationMs, 0U, 3600000U);
    registry.addUInt32(ParamId::Behavior_IdleBlockedAfterHeardMs, ModuleId::Behavior,
                       "behavior.idle_blocked_after_heard_ms", &b.idleBlockedAfterHeardMs, 0U, 600000U);
    registry.addUInt32(ParamId::Behavior_IdleBlockedAfterOwnEmitMs, ModuleId::Behavior,
                       "behavior.idle_blocked_after_own_emit_ms", &b.idleBlockedAfterOwnEmitMs, 0U, 600000U);
    registry.addUInt32(ParamId::Behavior_RestMs, ModuleId::Behavior, "behavior.rest_ms", &b.restMs, 0U, 600000U);
    // The level automaton (BEH-008): L, the forgetting, the answer's random window. level_max 0 = off.
    // 500, not 32 (chosen 2026-11-05): the level counts every train heard, so four players pass 20 events in ~5 s.
    registry.addUInt32(ParamId::Behavior_LevelMax, ModuleId::Behavior, "behavior.level_max", &b.levelMax, 0U, 500U);
    registry.addUInt32(ParamId::Behavior_LevelDecayMs, ModuleId::Behavior, "behavior.level_decay_ms", &b.levelDecayMs,
                       0U, 600000U);
    registry.addUInt32(ParamId::Behavior_AnswerJitterMs, ModuleId::Behavior, "behavior.answer_jitter_ms",
                       &b.answerJitterMs, 0U, 10000U);
    // The start apart from the length (decisions/2026-11-05-start-chance-and-chain-length.md): the chance of
    // answering the first train in a quiet field, rising evenly to 100 % at the top; 0 = 1 / level_max.
    registry.addUInt32(ParamId::Behavior_StartChancePct, ModuleId::Behavior, "behavior.start_chance_pct",
                       &b.startChancePct, 0U, 100U);
    // The base bin (BEH-009) and a band per node (decisions/2026-10-31-bands-per-node.md): the scatter's range, the
    // mode, the range it moves on.
    registry.addUInt32(ParamId::Behavior_BaseScatterBins, ModuleId::Behavior, "behavior.base_scatter_bins",
                       &b.baseScatterBins, 0U, 7U);
    registry.addBool(ParamId::Behavior_BandPerNode, ModuleId::Behavior, "behavior.band_per_node", &b.bandPerNode);
    // Down to 250 Hz: measured 2026-11-04, a train on 300-1300 Hz was heard 40 / 40 on the desk, stronger than on
    // 950-1950 (bench/sessions/2026-11-04-lowband); 300-5000 is the range asked for. The room's low noise is untested.
    registry.addUInt32(ParamId::Behavior_BandRangeLowHz, ModuleId::Behavior, "behavior.band_range_low_hz",
                       &b.bandRangeLowHz, 250U, runtime::kMaxToneHz);
    registry.addUInt32(ParamId::Behavior_BandRangeHighHz, ModuleId::Behavior, "behavior.band_range_high_hz",
                       &b.bandRangeHighHz, 250U, runtime::kMaxToneHz);
}

}  // namespace param
