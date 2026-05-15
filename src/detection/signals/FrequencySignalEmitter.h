#pragma once

#include "SignalCandidate.h"
#include "../FrequencyEvidenceEvaluation.h"
#include "../../io/AudioSignal.h"
#include "../ScalarTransientDetector.h"

namespace detection {

// Roadmap adapter for the target frequency stream.
// Long-term, frequency open/peak/release mechanics should reuse ScalarTransientDetector
// on the target frequency feature stream instead of duplicating scalar transient logic.
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
    bool _candidateOpen = false;
    uint64_t _onsetSample = 0;
    uint32_t _onsetTimeUs = 0;
    uint32_t _onsetTimeMs = 0;
    uint64_t _peakSample = 0;
    uint32_t _peakTimeUs = 0;
    uint32_t _peakTimeMs = 0;
    float _peakScore = 0.0f;
    DetectionPipeline::FrequencyEvidence _peakEvidence = {};
    ScalarTransientDetector _scalarDetector = {};
    SignalCandidate _pending = {};
};

} // namespace detection
