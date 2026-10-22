#pragma once

#include "../DetectionProfile.h"
#include "../occurrences/InspectedOccurrence.h"
#include "../features/FeatureHistory.h"

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
    void reset();

    InspectedOccurrence inspect(
        const Occurrence& occurrence
    ) const;

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
    void inspectAcceptedOccurrence(
        InspectedOccurrence& out,
        const Occurrence& occurrence,
        const FeatureHistory* featureHistory,
        unsigned long inspectionNowMs
    ) const;
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
    InspectedOccurrence inspectImpl(
        const Occurrence& occurrence,
        const FeatureHistory* featureHistory,
        unsigned long inspectionNowMs
    ) const;

    InspectedOccurrence inspectAcceptedOccurrenceResult(
        const Occurrence& occurrence,
        const FeatureHistory* featureHistory,
        unsigned long inspectionNowMs
    ) const;

    InspectionPlan _inspectionPlan = {};
};

} // namespace detection

