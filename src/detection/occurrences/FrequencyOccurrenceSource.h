#pragma once

#include "Occurrence.h"
#include "../DetectionProfile.h"
#include "../features/FrequencyMatchEvaluation.h"
#include "../../io/AudioSignal.h"
#include "../detectors/FrequencyMatchDetector.h"

namespace detection {

/*
FrequencyOccurrenceSource

Owns the frequency-match occurrence candidate path.
Wraps FrequencyMatchDetector to produce frequency candidates from AudioSignalFrame
and FrequencyFeatureFrame input.
Does not decide pattern meaning or behavior.
*/
class FrequencyOccurrenceSource {
public:
    FrequencyOccurrenceSource();

    void reset();

    void setConfig(const FrequencyMatchConfig& config);
    void setDiagnosticsEnabled(bool enabled);

    void observeFrame(
        const AudioSignalFrame& frame,
        const detection::FrequencyFeatureFrame& evidence
    );

    bool popOccurrence(Occurrence& out);
    FrequencyMatchDetector& detector();
    const FrequencyMatchDetector& detector() const;

private:
    bool _hasPending = false;
    detection::FrequencyFeatureFrame _peakEvidence = {};
    FrequencyMatchDetector _detector = {};
    FrequencyMatchConfig _config = {};
    Occurrence _pending = {};
    unsigned long _lastEmittedReleaseMs = 0;
};

} // namespace detection

