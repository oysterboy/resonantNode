#pragma once

#include "SignalCandidate.h"
#include "../features/FrequencyMatchEvaluation.h"
#include "../../io/AudioSignal.h"
#include "../detectors/FrequencyMatchDetector.h"

namespace detection {

/*
FrequencySignalEmitter

Owns the frequency-match signal candidate path.
Wraps FrequencyMatchDetector to produce frequency candidates from AudioSignalFrame
and FrequencyEvidence input.
Does not decide pattern meaning or behavior.
*/
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
