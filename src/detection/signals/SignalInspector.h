#pragma once

#include "InspectedSignal.h"
#include "../FrequencyEvidenceEvaluation.h"

namespace detection {

class SignalInspector {
public:
    void reset();

    InspectedSignal inspect(
        const SignalCandidate& candidate,
        const FrequencyEvidenceEvaluation::Values& frequencyTuning
    ) const;

private:
    void annotateAcceptedSignal(InspectedSignal& out, const SignalCandidate& candidate) const;
    void annotateDuplicateRisk(InspectedSignal& out, const SignalCandidate& candidate) const;

    InspectedSignal inspectFrequency(
        const SignalCandidate& candidate,
        const FrequencyEvidenceEvaluation::Values& frequencyTuning
    ) const;

    InspectedSignal inspectAmp(
        const SignalCandidate& candidate
    ) const;

    mutable unsigned long _lastAcceptedAmpMs = 0;
    mutable unsigned long _lastAcceptedFrequencyMs = 0;
};

} // namespace detection
