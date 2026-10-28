#pragma once

#include <stddef.h>

#include "InspectorTypes.h"

namespace detection {

/*
InspectionLimits (step 11i N7, review A10; step 10 item 7, step 11g B2)

The inspection plan's live limits as one value: the Contrast and Amp modules' medium peak thresholds and the
BinShape module's settings. Read from a plan with inspectionLimitsFromPlan() (a plan without such a module gives
0 / the defaults), written back with applyInspectionLimits() into the module with the matching target (a plan
without it ignores the value), so the binding does not depend on the plan's module order. The Node's
detection.contrast_medium / amp_medium / shape_* params bind to its copy (InspectionParams.h).
*/
struct InspectionLimits {
    float contrastMedium = 0.0f;
    float ampMedium = 0.0f;
    ShapeInspectionConfig shape = {};
};

// The plan module that inspects `target`, or nullptr.
inline InspectionModuleConfig* inspectionModuleForTarget(InspectionPlan& plan, InspectionTarget target) {
    const size_t count = plan.count > kMaxInspectionModules ? kMaxInspectionModules : plan.count;
    for (size_t i = 0; i < count; ++i) {
        if (plan.modules[i].target == target) {
            return &plan.modules[i];
        }
    }
    return nullptr;
}

inline InspectionLimits inspectionLimitsFromPlan(InspectionPlan& plan) {
    InspectionLimits limits;
    const InspectionModuleConfig* contrast = inspectionModuleForTarget(plan, InspectionTarget::Contrast);
    const InspectionModuleConfig* amp = inspectionModuleForTarget(plan, InspectionTarget::Amp);
    limits.contrastMedium = contrast != nullptr ? contrast->magnitude.supportStrength.mediumPeakThreshold : 0.0f;
    limits.ampMedium = amp != nullptr ? amp->magnitude.supportStrength.mediumPeakThreshold : 0.0f;
    const InspectionModuleConfig* shape = inspectionModuleForTarget(plan, InspectionTarget::Shape);
    limits.shape = shape != nullptr ? shape->shape : ShapeInspectionConfig{};
    return limits;
}

inline void applyInspectionLimits(InspectionPlan& plan, const InspectionLimits& limits) {
    InspectionModuleConfig* contrast = inspectionModuleForTarget(plan, InspectionTarget::Contrast);
    InspectionModuleConfig* amp = inspectionModuleForTarget(plan, InspectionTarget::Amp);
    if (contrast != nullptr) {
        contrast->magnitude.supportStrength.mediumPeakThreshold = limits.contrastMedium;
    }
    if (amp != nullptr) {
        amp->magnitude.supportStrength.mediumPeakThreshold = limits.ampMedium;
    }
    InspectionModuleConfig* shape = inspectionModuleForTarget(plan, InspectionTarget::Shape);
    if (shape != nullptr) {
        shape->shape = limits.shape;
    }
}

}  // namespace detection
