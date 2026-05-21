#pragma once

#include "SignalCandidate.h"
#include "../features/FrequencyMatchEvaluation.h"
#include "../../io/AudioSignal.h"
#include "../detectors/FrequencyMatchDetector.h"

namespace detection {

// Roadmap adapter for the target frequency stream.
// Frequency lifecycle now lives in FrequencyMatchDetector and uses matched
// frequency windows rather than forcing the path through scalar timing.
class FrequencySignalEmitter {
public:
    FrequencySignalEmitter();

    void reset();

    void observeFrame(
        const AudioSignalFrame& frame,
        const detection::FrequencyEvidence& evidence,
        const FrequencyMatchEvaluation::Values& frequencyTuning
    );

    bool popSignalCandidate(SignalCandidate& out);
    const FrequencyMatchDetector& detector() const;

private:
    void applyFrequencyTuning(const FrequencyMatchEvaluation::Values& frequencyTuning);

    bool _hasPending = false;
    detection::FrequencyEvidence _peakEvidence = {};
    FrequencyMatchDetector _detector = {};
    SignalCandidate _pending = {};
    unsigned long _lastEmittedReleaseMs = 0;
};

} // namespace detection
