#pragma once

#include "../DetectionProfile.h"
#include "../occurrences/InspectedOccurrence.h"
#include "../occurrences/RawWindow.h"
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
    void configure(const InspectionConfig& config);
    void setInspectionRules(ProfileInspectionRulesKind rules);
    void reset();

    InspectedOccurrence inspect(
        const Occurrence& candidate,
        const RawWindowStats* rawWindow = nullptr
    ) const;

    InspectedOccurrence inspectWithHistory(
        const Occurrence& candidate,
        const FeatureHistory* featureHistory,
        const RawWindowStats* rawWindow = nullptr
    ) const;

private:
    void annotateAcceptedSignal(
        InspectedOccurrence& out,
        const Occurrence& candidate,
        const FeatureHistory* featureHistory
    ) const;
    void annotateDuplicateRisk(InspectedOccurrence& out, const Occurrence& candidate) const;
    void annotateAmpSupport(
        InspectedOccurrence& out,
        const Occurrence& candidate,
        const FeatureHistory* featureHistory
    ) const;
    InspectedOccurrence inspectImpl(
        const Occurrence& candidate,
        const FeatureHistory* featureHistory
    ) const;

    InspectedOccurrence inspectAmp(
        const Occurrence& candidate,
        const FeatureHistory* featureHistory
    ) const;

    mutable unsigned long _lastAcceptedAmpMs = 0;
    mutable unsigned long _lastAcceptedFrequencyMs = 0;
    ProfileInspectionRulesKind _inspectionRules = ProfileInspectionRulesKind::TonalPulse;
    InspectionConfig _config = defaultInspectionConfig();
};

} // namespace detection

