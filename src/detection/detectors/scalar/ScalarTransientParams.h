#pragma once

#include <ParamRegistry.h>

#include "../../../app/ParamIds.h"
#include "ScalarTransientConfig.h"

/*
ScalarTransientConfig's params (step 11i N7, review A10): the config describes its own fields as param paths, ids
and ranges; the Node, the Analyzer and the host replay register them through this one function (app/ParamBindings.h
gathers the shared ones). A field that gets a new path or range changes here and nowhere else.

ScalarTransientConfig::observedStream (an enum) is not a param: the profile sets it (SEQ PROFILE on the Analyzer;
the Node ships TonalPulseScalar), owner decision 2026-10-25 (decisions/2026-10-25-analyzer-params-on-registry-and-
line-policy.md).
*/
namespace param {

// detection.scalar_*: the 13 numeric and bool fields of ScalarTransientConfig
// (the Node's production profile is TonalPulseScalar, decisions/2026-10-14-
// node-ships-tonalpulsescalar.md). Strengths are Strength16 values, times ms.
inline void describeParams(ParamRegistry& registry, detection::ScalarTransientConfig& sc) {
    registry.addFloat(ParamId::Detection_ScalarOnsetThreshold, ModuleId::Detection, "detection.scalar_onset_threshold",
                      &sc.onsetDetectionThreshold, 0.0f, 200000.0f);
    registry.addFloat(ParamId::Detection_ScalarReleaseThreshold, ModuleId::Detection,
                      "detection.scalar_release_threshold", &sc.onsetReleaseThreshold, 0.0f, 200000.0f);
    registry.addUInt32(ParamId::Detection_ScalarCooldownMs, ModuleId::Detection, "detection.scalar_cooldown_ms",
                       &sc.cooldownAfterOnsetMs, 0U, 10000U);
    registry.addUInt32(ParamId::Detection_ScalarMinDurationMs, ModuleId::Detection, "detection.scalar_min_duration_ms",
                       &sc.minTransientDurationMs, 0U, 5000U);
    registry.addUInt32(ParamId::Detection_ScalarMaxDurationMs, ModuleId::Detection, "detection.scalar_max_duration_ms",
                       &sc.maxTransientDurationMs, 0U, 5000U);
    registry.addFloat(ParamId::Detection_ScalarMinPeakStrength, ModuleId::Detection,
                      "detection.scalar_min_peak_strength", &sc.minTransientPeakStrength, 0.0f, 200000.0f);
    registry.addUInt32(ParamId::Detection_ScalarReleaseDebounceMs, ModuleId::Detection,
                       "detection.scalar_release_debounce_ms", &sc.releaseDebounceMs, 0U, 1000U);
    registry.addBool(ParamId::Detection_ScalarRequireCarrierQuality, ModuleId::Detection,
                     "detection.scalar_require_carrier_quality", &sc.requireCarrierQuality);
    registry.addBool(ParamId::Detection_ScalarRequireMinStrength, ModuleId::Detection,
                     "detection.scalar_require_min_strength", &sc.requireMinStrength);
    registry.addFloat(ParamId::Detection_ScalarMinStrength, ModuleId::Detection, "detection.scalar_min_strength",
                      &sc.minMatchedMeanStrength, 0.0f, 200000.0f);
    registry.addUInt32(ParamId::Detection_ScalarMinReleaseCoverageMs, ModuleId::Detection,
                       "detection.scalar_min_release_coverage_ms", &sc.minCoverageAboveReleaseMs, 0U, 5000U);
    registry.addUInt32(ParamId::Detection_ScalarMinLongestIslandMs, ModuleId::Detection,
                       "detection.scalar_min_longest_island_ms", &sc.minLongestIslandMs, 0U, 5000U);
    registry.addUInt32(ParamId::Detection_ScalarMaxGapMs, ModuleId::Detection, "detection.scalar_max_gap_ms",
                       &sc.maxGapMs, 0U, 5000U);
}

}  // namespace param
