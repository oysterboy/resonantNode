#pragma once

#include "SignalCandidate.h"
#include "../FrequencyEvidenceEvaluation.h"
#include "../../io/AudioSignal.h"
#include "ScalarSignalEmitter.h"

namespace detection {

// Roadmap adapter for the target frequency stream.
// Long-term, frequency open/peak/release mechanics reuse ScalarSignalEmitter
// on the target frequency feature stream instead of duplicating lifecycle logic.
class FrequencySignalEmitter {
public:
    FrequencySignalEmitter();

    void reset();

    void observeFrame(
        const AudioSignalFrame& frame,
        const DetectionPipeline::FrequencyEvidence& evidence,
        const FrequencyEvidenceEvaluation::Values& frequencyTuning
    );

    bool popSignalCandidate(SignalCandidate& out);

private:
    void applyFrequencyScalarTuning(const FrequencyEvidenceEvaluation::Values& frequencyTuning);

    bool _hasPending = false;
    DetectionPipeline::FrequencyEvidence _peakEvidence = {};
    ScalarSignalEmitter _scalarEmitter = {};
    SignalCandidate _pending = {};
};

} // namespace detection
