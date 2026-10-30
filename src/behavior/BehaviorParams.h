#pragma once

#include <ParamRegistry.h>

#include "../app/ParamIds.h"
#include "BehaviorProfile.h"

/*
BehaviorGateConfig's params (step 11i N7, review A10): the config describes its own fields as behavior.* paths, ids
and ranges. The Node binds its live copy (a PARAM SET applies through ResonantBehavior::configure()).
*/
namespace param {

// behavior.*: one param per BehaviorGateConfig field (PAR-006, step 10 item 4).
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
    registry.addUInt32(ParamId::Behavior_RestAfterFieldPatterns, ModuleId::Behavior,
                       "behavior.rest_after_field_patterns", &b.restAfterFieldPatterns, 0U, 100U);
    registry.addUInt32(ParamId::Behavior_OwnEmitBudget, ModuleId::Behavior, "behavior.own_emit_budget",
                       &b.ownEmitBudget, 0U, 16U);  // the emission ring holds 16 (deep review L3)
    registry.addUInt32(ParamId::Behavior_OwnEmitBudgetWindowMs, ModuleId::Behavior,
                       "behavior.own_emit_budget_window_ms", &b.ownEmitBudgetWindowMs, 0U, 600000U);
    registry.addUInt32(ParamId::Behavior_RestMs, ModuleId::Behavior, "behavior.rest_ms", &b.restMs, 0U, 600000U);
    // The level automaton (BEH-008): L, the forgetting, the answer's random window. level_max 0 = off.
    registry.addUInt32(ParamId::Behavior_LevelMax, ModuleId::Behavior, "behavior.level_max", &b.levelMax, 0U, 32U);
    registry.addUInt32(ParamId::Behavior_LevelDecayMs, ModuleId::Behavior, "behavior.level_decay_ms", &b.levelDecayMs,
                       0U, 600000U);
    registry.addUInt32(ParamId::Behavior_AnswerJitterMs, ModuleId::Behavior, "behavior.answer_jitter_ms",
                       &b.answerJitterMs, 0U, 10000U);
}

}  // namespace param
