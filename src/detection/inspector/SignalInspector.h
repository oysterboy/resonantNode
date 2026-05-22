#pragma once

#include "../DetectionProfile.h"
#include "../signals/InspectedSignal.h"
#include "../signals/RawWindow.h"
#include "../features/FeatureHistory.h"

namespace detection {

/*
SignalInspector

Owns signal-stage inspection and evidence annotation.
Consumes SignalCandidate plus optional feature/raw history and produces InspectedSignal.
Does not decide pattern validity or behavior eligibility.
*/
class SignalInspector {
public:
    void configure(const InspectionConfig& config);
    void setInspectionRules(ProfileInspectionRulesKind rules);
    void reset();

    InspectedSignal inspect(
        const SignalCandidate& candidate,
        const RawWindowStats* rawWindow = nullptr
    ) const;

    InspectedSignal inspectWithHistory(
        const SignalCandidate& candidate,
        const FeatureHistory* featureHistory,
        const RawWindowStats* rawWindow = nullptr
    ) const;

private:
    void annotateAcceptedSignal(
        InspectedSignal& out,
        const SignalCandidate& candidate,
        const FeatureHistory* featureHistory
    ) const;
    void annotateDuplicateRisk(InspectedSignal& out, const SignalCandidate& candidate) const;
    void annotateAmpSupport(
        InspectedSignal& out,
        const SignalCandidate& candidate,
        const FeatureHistory* featureHistory
    ) const;
    InspectedSignal inspectImpl(
        const SignalCandidate& candidate,
        const FeatureHistory* featureHistory
    ) const;

    InspectedSignal inspectAmp(
        const SignalCandidate& candidate,
        const FeatureHistory* featureHistory
    ) const;

    mutable unsigned long _lastAcceptedAmpMs = 0;
    mutable unsigned long _lastAcceptedFrequencyMs = 0;
    ProfileInspectionRulesKind _inspectionRules = ProfileInspectionRulesKind::FreqAmp;
    InspectionConfig _config = defaultInspectionConfig();
};

} // namespace detection
