#pragma once

#include <ParamRegistry.h>

#include "../../app/ParamIds.h"
#include "InspectionLimits.h"
#include "InspectorTypes.h"

/*
The inspection plan's params (step 11i N7, review A10): the BinShape module's settings (detection.shape_*) and the
Node's live inspection limits (detection.contrast_medium / amp_medium, then the shape settings). The host replay
binds the shape settings straight into the plan's module; the Node binds its InspectionLimits and writes them into
the plan with applyInspectionLimits().
*/
namespace param {

// detection.shape_*: the BinShape module's settings (DET-015; step 11g B2, review D2.7). The window must fit the
// BinTrack (preMs + max duration + tailMs <= BinTrack::kMs): RB plan_check prints shape_window_too_long if not.
inline void describeParams(ParamRegistry& registry, detection::ShapeInspectionConfig& shape) {
    registry.addUInt32(ParamId::Detection_ShapePreMs, ModuleId::Detection, "detection.shape_pre_ms", &shape.preMs, 0U,
                       200U);
    registry.addUInt32(ParamId::Detection_ShapeTailMs, ModuleId::Detection, "detection.shape_tail_ms", &shape.tailMs,
                       0U, 200U);
    registry.addUInt16(ParamId::Detection_ShapeMaxRisingStepBins, ModuleId::Detection,
                       "detection.shape_max_rising_step_bins", &shape.maxRisingStepBins, 1U, 4U);
    registry.addUInt32(ParamId::Detection_ShapeMinRunMs, ModuleId::Detection, "detection.shape_min_run_ms",
                       &shape.minRunMs, 1U, 50U);
    registry.addUInt16(ParamId::Detection_ShapeMediumMinBins, ModuleId::Detection, "detection.shape_medium_min_bins",
                       &shape.mediumMinBins, 1U, 16U);
    registry.addUInt16(ParamId::Detection_ShapeMediumMinRisingSteps, ModuleId::Detection,
                       "detection.shape_medium_min_rising_steps", &shape.mediumMinRisingSteps, 0U, 16U);
    registry.addUInt16(ParamId::Detection_ShapeStrongMinBins, ModuleId::Detection, "detection.shape_strong_min_bins",
                       &shape.strongMinBins, 1U, 16U);
    registry.addUInt16(ParamId::Detection_ShapeStrongMinRisingSteps, ModuleId::Detection,
                       "detection.shape_strong_min_rising_steps", &shape.strongMinRisingSteps, 0U, 16U);
}

// detection.contrast_medium, detection.amp_medium, then detection.shape_* (the Node's PARAM LIST order).
inline void describeParams(ParamRegistry& registry, detection::InspectionLimits& limits) {
    registry.addFloat(ParamId::Detection_ContrastMedium, ModuleId::Detection, "detection.contrast_medium",
                      &limits.contrastMedium, 0.0f, 32767.0f);
    registry.addFloat(ParamId::Detection_AmpMedium, ModuleId::Detection, "detection.amp_medium", &limits.ampMedium,
                      0.0f, 32767.0f);
    describeParams(registry, limits.shape);
}

}  // namespace param
