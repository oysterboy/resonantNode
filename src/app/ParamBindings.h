#pragma once

#include <ParamRegistry.h>

#include "../behavior/BehaviorProfile.h"
#include "../detection/detectors/scalar/ScalarTransientConfig.h"
#include "ParamIds.h"

/*
ParamBindings (step 11e item 9, PAR-003)

The one place that spells a config struct's fields as param paths. The Node
binds its live copies here, the Analyzer binds its SEQ tuning, the host
replay binds the profile its --set flags override: same paths, same ranges,
same ids on every build. A field that gets a new path or range changes here
and nowhere else.

ScalarTransientConfig::observedStream (an enum) is not a param: the profile
sets it (SEQ PROFILE on the Analyzer; the Node ships TonalPulseScalar), owner
decision 2026-10-25 (decisions/2026-10-25-analyzer-params-on-registry-and-
line-policy.md).
*/
namespace param {

// detection.scalar_*: the 13 numeric and bool fields of ScalarTransientConfig
// (the Node's production profile is TonalPulseScalar, decisions/2026-10-14-
// node-ships-tonalpulsescalar.md). Strengths are Strength16 values, times ms.
inline void bindScalarTransientParams(ParamRegistry& registry, detection::ScalarTransientConfig& sc) {
    registry.addFloat(ParamId::Detection_ScalarOnsetThreshold, ModuleId::Detection,
                      "detection.scalar_onset_threshold", &sc.onsetDetectionThreshold, 0.0f, 200000.0f);
    registry.addFloat(ParamId::Detection_ScalarReleaseThreshold, ModuleId::Detection,
                      "detection.scalar_release_threshold", &sc.onsetReleaseThreshold, 0.0f, 200000.0f);
    registry.addUInt32(ParamId::Detection_ScalarCooldownMs, ModuleId::Detection,
                       "detection.scalar_cooldown_ms", &sc.cooldownAfterOnsetMs, 0U, 10000U);
    registry.addUInt32(ParamId::Detection_ScalarMinDurationMs, ModuleId::Detection,
                       "detection.scalar_min_duration_ms", &sc.minTransientDurationMs, 0U, 5000U);
    registry.addUInt32(ParamId::Detection_ScalarMaxDurationMs, ModuleId::Detection,
                       "detection.scalar_max_duration_ms", &sc.maxTransientDurationMs, 0U, 5000U);
    registry.addFloat(ParamId::Detection_ScalarMinPeakStrength, ModuleId::Detection,
                      "detection.scalar_min_peak_strength", &sc.minTransientPeakStrength, 0.0f, 200000.0f);
    registry.addUInt32(ParamId::Detection_ScalarReleaseDebounceMs, ModuleId::Detection,
                       "detection.scalar_release_debounce_ms", &sc.releaseDebounceMs, 0U, 1000U);
    registry.addBool(ParamId::Detection_ScalarRequireCarrierQuality, ModuleId::Detection,
                     "detection.scalar_require_carrier_quality", &sc.requireCarrierQuality);
    registry.addBool(ParamId::Detection_ScalarRequireMinStrength, ModuleId::Detection,
                     "detection.scalar_require_min_strength", &sc.requireMinStrength);
    registry.addFloat(ParamId::Detection_ScalarMinStrength, ModuleId::Detection,
                      "detection.scalar_min_strength", &sc.minMatchedMeanStrength, 0.0f, 200000.0f);
    registry.addUInt32(ParamId::Detection_ScalarMinReleaseCoverageMs, ModuleId::Detection,
                       "detection.scalar_min_release_coverage_ms", &sc.minCoverageAboveReleaseMs, 0U, 5000U);
    registry.addUInt32(ParamId::Detection_ScalarMinLongestIslandMs, ModuleId::Detection,
                       "detection.scalar_min_longest_island_ms", &sc.minLongestIslandMs, 0U, 5000U);
    registry.addUInt32(ParamId::Detection_ScalarMaxGapMs, ModuleId::Detection,
                       "detection.scalar_max_gap_ms", &sc.maxGapMs, 0U, 5000U);
}

// behavior.*: one param per BehaviorGateConfig field (PAR-006, step 10 item 4).
// The paths are the snake_case field names; the RB BEHAV keys they replaced
// are mapped in decisions/2026-10-22-live-params-profile-reset-no-rb-behav.md.
inline void bindBehaviorParams(ParamRegistry& registry, BehaviorGateConfig& b) {
    registry.addBool(ParamId::Behavior_IdleEnabled, ModuleId::Behavior, "behavior.idle_enabled", &b.idleEnabled);
    registry.addUInt32(ParamId::Behavior_WaitAfterHeardMs, ModuleId::Behavior,
                       "behavior.wait_after_heard_ms", &b.waitAfterHeardMs, 0U, 10000U);
    registry.addUInt32(ParamId::Behavior_RefractoryAfterEmitMs, ModuleId::Behavior,
                       "behavior.refractory_after_emit_ms", &b.refractoryAfterEmitMs, 0U, 60000U);
    registry.addUInt32(ParamId::Behavior_SuppressSelfChirpMs, ModuleId::Behavior,
                       "behavior.suppress_self_chirp_ms", &b.behaviorSuppressSelfChirpMs, 0U, 10000U);
    registry.addUInt32(ParamId::Behavior_DetectionSuppressTailMs, ModuleId::Behavior,
                       "behavior.detection_suppress_tail_ms", &b.detectionSuppressTailMsOwnEmit, 0U, 5000U);
    registry.addUInt32(ParamId::Behavior_IdleTimeoutMs, ModuleId::Behavior,
                       "behavior.idle_timeout_ms", &b.idleTimeoutMs, 0U, 3600000U);
    registry.addUInt32(ParamId::Behavior_IdleTimeoutVariationMs, ModuleId::Behavior,
                       "behavior.idle_timeout_variation_ms", &b.idleTimeVariationMs, 0U, 3600000U);
    registry.addUInt32(ParamId::Behavior_IdleBlockedAfterHeardMs, ModuleId::Behavior,
                       "behavior.idle_blocked_after_heard_ms", &b.idleBlockedAfterHeardMs, 0U, 600000U);
    registry.addUInt32(ParamId::Behavior_IdleBlockedAfterOwnEmitMs, ModuleId::Behavior,
                       "behavior.idle_blocked_after_own_emit_ms", &b.idleBlockedAfterOwnEmitMs, 0U, 600000U);
    registry.addUInt32(ParamId::Behavior_RestAfterFieldPatterns, ModuleId::Behavior,
                       "behavior.rest_after_field_patterns", &b.restAfterFieldPatterns, 0U, 100U);
    registry.addUInt32(ParamId::Behavior_OwnEmitBudget, ModuleId::Behavior,
                       "behavior.own_emit_budget", &b.ownEmitBudget, 0U, 100U);
    registry.addUInt32(ParamId::Behavior_OwnEmitBudgetWindowMs, ModuleId::Behavior,
                       "behavior.own_emit_budget_window_ms", &b.ownEmitBudgetWindowMs, 0U, 600000U);
    registry.addUInt32(ParamId::Behavior_RestMs, ModuleId::Behavior,
                       "behavior.rest_ms", &b.restMs, 0U, 600000U);
}

}  // namespace param
