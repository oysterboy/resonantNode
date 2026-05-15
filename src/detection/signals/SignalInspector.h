#pragma once

#include "InspectedSignal.h"
#include "RawWindow.h"
#include "../features/FeatureHistory.h"
#include "../FrequencyEvidenceEvaluation.h"

namespace detection {

class SignalInspector {
public:
    void reset();

    InspectedSignal inspect(
        const SignalCandidate& candidate,
        const FrequencyEvidenceEvaluation::Values& frequencyTuning,
        const RawWindowStats* rawWindow = nullptr
    ) const;

    InspectedSignal inspectWithHistory(
        const SignalCandidate& candidate,
        const FrequencyEvidenceEvaluation::Values& frequencyTuning,
        const FeatureHistory* featureHistory,
        const RawWindowStats* rawWindow = nullptr
    ) const;

private:
    void annotateAcceptedSignal(
        InspectedSignal& out,
        const SignalCandidate& candidate,
        const FeatureHistory* featureHistory,
        const RawWindowStats* rawWindow
    ) const;
    void annotateDuplicateRisk(InspectedSignal& out, const SignalCandidate& candidate) const;
    void annotateAmpSupportAndLocality(
        InspectedSignal& out,
        const SignalCandidate& candidate,
        const FeatureHistory* featureHistory,
        const RawWindowStats* rawWindow
    ) const;
    InspectedSignal inspectImpl(
        const SignalCandidate& candidate,
        const FrequencyEvidenceEvaluation::Values& frequencyTuning,
        const FeatureHistory* featureHistory,
        const RawWindowStats* rawWindow
    ) const;

    InspectedSignal inspectFrequency(
        const SignalCandidate& candidate,
        const FrequencyEvidenceEvaluation::Values& frequencyTuning,
        const FeatureHistory* featureHistory,
        const RawWindowStats* rawWindow
    ) const;

    InspectedSignal inspectAmp(
        const SignalCandidate& candidate,
        const FeatureHistory* featureHistory,
        const RawWindowStats* rawWindow
    ) const;

    mutable unsigned long _lastAcceptedAmpMs = 0;
    mutable unsigned long _lastAcceptedFrequencyMs = 0;
};

} // namespace detection
