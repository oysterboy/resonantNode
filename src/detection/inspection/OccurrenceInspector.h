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
        const FeatureHistory* featureHistory
    ) const;

private:
    void inspectAcceptedOccurrence(
        InspectedOccurrence& out,
        const Occurrence& occurrence,
        const FeatureHistory* featureHistory
    ) const;
    void annotateScalarFeatureStrength(
        InspectedOccurrence& out,
        const Occurrence& occurrence,
        const FeatureHistory* featureHistory,
        const ScalarFeatureInspectionConfig& config,
        InspectionTarget target
    ) const;
    void runInspectionModule(
        InspectedOccurrence& out,
        const Occurrence& occurrence,
        const FeatureHistory* featureHistory,
        const InspectionModuleConfig& module
    ) const;
    InspectedOccurrence inspectImpl(
        const Occurrence& occurrence,
        const FeatureHistory* featureHistory
    ) const;

    InspectedOccurrence inspectAcceptedOccurrenceResult(
        const Occurrence& occurrence,
        const FeatureHistory* featureHistory
    ) const;

    InspectionPlan _inspectionPlan = {};
};

} // namespace detection

