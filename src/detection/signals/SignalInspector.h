#pragma once

#include "InspectedSignal.h"
#include "../FrequencyEvidenceEvaluation.h"

namespace detection {

class SignalInspector {
public:
    InspectedSignal inspect(
        const SignalCandidate& candidate,
        const FrequencyEvidenceEvaluation::Values& frequencyTuning
    ) const;

private:
    InspectedSignal inspectFrequency(
        const SignalCandidate& candidate,
        const FrequencyEvidenceEvaluation::Values& frequencyTuning
    ) const;

    InspectedSignal inspectAmp(
        const SignalCandidate& candidate
    ) const;
};

} // namespace detection
