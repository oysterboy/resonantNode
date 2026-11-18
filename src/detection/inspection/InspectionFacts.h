#pragma once

#include <stddef.h>
#include <stdint.h>

#include "../occurrences/InspectedOccurrence.h"
#include "InspectorTypes.h"

namespace detection {

/*
InspectionFacts

What one inspection measured, per enabled module of the plan: the class it found, the class the plan requires and
the value behind it (contrast and amp: the window's classification value; shape: the distinct bins; attack: the rise
in ms; loudness: p90; modulation: the index; timbre: the flatness). A copy of facts the inspector already wrote, for
a reader that prints them (the Node's RB inspect line, the bench's per-call report by radio); nothing here decides.
Does not own: the verdict (makeVerdict) or the classes' thresholds (the plan).

Pure: no Arduino, host-tested in test/test_inspection_facts.
*/
struct InspectionFacts {
    struct Module {
        InspectionTarget target = InspectionTarget::None;
        StrengthClass observed = StrengthClass::Unknown;
        StrengthClass required = StrengthClass::Unknown;
        bool skipped = false;  // shape without band bins: not judged (makeVerdict skips it too)
        float value = 0.0f;
    };
    bool present = false;
    unsigned long occurrenceId = 0;
    uint8_t count = 0;
    Module modules[kMaxInspectionModules] = {};
    uint8_t shapeRisingSteps = 0;
    uint16_t shapeLowestBinHz = 0;
    uint16_t shapeHighestBinHz = 0;
};

// The class an inspection wrote for `target` into the occurrence (the field makeVerdict reads).
inline StrengthClass observedStrength(const Occurrence& source, InspectionTarget target) {
    switch (target) {
        case InspectionTarget::Amp:
            return source.magnitude.strengthClass;
        case InspectionTarget::TargetScore:
            return source.band.scoreStrength;
        case InspectionTarget::Contrast:
            return source.band.contrastQuality;
        case InspectionTarget::TargetBand:
            return source.band.targetBandStrength;
        case InspectionTarget::Shape:
            return source.shape.shapeClass;
        case InspectionTarget::Attack:
            return source.attack.attackClass;
        case InspectionTarget::Loudness:
            return source.level.levelClass;
        case InspectionTarget::Modulation:
            return source.modulation.modulationClass;
        case InspectionTarget::Timbre:
            return source.timbre.timbreClass;
        case InspectionTarget::None:
        default:
            return StrengthClass::Unknown;
    }
}

// The facts of one inspected occurrence under `plan`. Magnitude-feature modules (contrast, amp, ...) each left one
// observation, in plan order; the others carry their value in the occurrence.
inline InspectionFacts makeInspectionFacts(const InspectedOccurrence& inspected, const InspectionPlan& plan) {
    InspectionFacts out;
    const Occurrence& source = inspected.occurrence;
    out.present = true;
    out.occurrenceId = source.occurrenceId;
    out.shapeRisingSteps = source.shape.risingSteps;
    out.shapeLowestBinHz = source.shape.lowestBinHz;
    out.shapeHighestBinHz = source.shape.highestBinHz;
    size_t observation = 0;
    const size_t count = plan.count > kMaxInspectionModules ? kMaxInspectionModules : plan.count;
    for (size_t i = 0; i < count; ++i) {
        const InspectionModuleConfig& module = plan.modules[i];
        float value = 0.0f;
        if (module.kind == InspectionModuleKind::MagnitudeFeatureStrength) {
            if (observation < inspected.magnitudeObservationCount) {
                value = inspected.magnitudeObservations[observation].classificationValue;
            }
            ++observation;
        } else if (module.target == InspectionTarget::Shape) {
            value = source.shape.distinctBins;
        } else if (module.target == InspectionTarget::Attack) {
            value = static_cast<float>(source.attack.riseMs);
        } else if (module.target == InspectionTarget::Loudness) {
            value = source.level.p90;
        } else if (module.target == InspectionTarget::Modulation) {
            value = source.modulation.index;
        } else if (module.target == InspectionTarget::Timbre) {
            value = source.timbre.flatness;
        }
        if (!module.enabled) {
            continue;
        }
        InspectionFacts::Module& m = out.modules[out.count++];
        m.target = module.target;
        m.observed = observedStrength(source, module.target);
        m.required = module.minimumStrength;
        m.skipped = module.target == InspectionTarget::Shape && !source.shape.applicable;
        m.value = value;
    }
    return out;
}

}  // namespace detection
