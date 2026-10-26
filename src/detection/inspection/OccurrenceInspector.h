#pragma once

#include "../DetectionProfile.h"
#include "../occurrences/InspectedOccurrence.h"
#include "../features/FeatureHistory.h"
#include "../features/BinTrack.h"

namespace detection {

/*
OccurrenceInspector

Owns occurrence-stage inspection and evidence annotation.
Consumes Occurrence plus optional feature/raw history and produces InspectedOccurrence.
Does not decide pattern validity or behavior eligibility.
*/
class OccurrenceInspector {
public:
    void configure(const InspectionPlan& plan);
    // The bin track the BinShape module reads (DET-015); owned by DetectionRuntime. nullptr: no shape.
    void setBinTrack(const BinTrack* track) { _binTrack = track; }
    // The level a bin must reach to count for the shape: the detector's release threshold, set by
    // DetectionRuntime whenever the scalar config or the plan changes, so a live PARAM SET of
    // detection.scalar_release_threshold moves both stages together (review D1.5, 2026-10-26).
    void setShapeMinLevel(float level) { _shapeMinLevel = level; }

    // Annotates an accepted occurrence with the plan's evidence from the
    // history; an absent or invalid occurrence comes back Rejected.
    InspectedOccurrence inspectWithHistory(
        const Occurrence& occurrence,
        const FeatureHistory* featureHistory,
        unsigned long inspectionNowMs
    ) const;

    // When the latest window the plan inspects for this occurrence ends (ms,
    // the occurrence's time axis); 0 when the plan inspects no window. The
    // runtime holds an occurrence until then (step 8b item 1,
    // decisions/2026-10-22-inspect-when-window-complete.md).
    unsigned long windowCompleteAtMs(const Occurrence& occurrence) const;

private:
    void annotateMagnitudeFeatureStrength(
        InspectedOccurrence& out,
        const Occurrence& occurrence,
        const FeatureHistory* featureHistory,
        unsigned long inspectionNowMs,
        const MagnitudeFeatureInspectionConfig& config,
        InspectionTarget target
    ) const;
    void runInspectionModule(
        InspectedOccurrence& out,
        const Occurrence& occurrence,
        const FeatureHistory* featureHistory,
        unsigned long inspectionNowMs,
        const InspectionModuleConfig& module
    ) const;
    void annotateShape(InspectedOccurrence& out, const Occurrence& occurrence, const ShapeInspectionConfig& config) const;
    InspectionPlan _inspectionPlan = {};
    const BinTrack* _binTrack = nullptr;
    float _shapeMinLevel = 0.0f;
};

} // namespace detection

