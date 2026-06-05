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
Wraps FrequencyMatchDetector to produce frequency candidates from AudioSamplePacket
and FrequencyBandMeasurementPacket input.
Does not decide pattern meaning or behavior.
Only fresh measurement packets advance the attack/release lifecycle; stale held
packets are treated as status/debug-only input.
*/
class FrequencyOccurrenceSource {
public:
    FrequencyOccurrenceSource();

    void reset();

    void setConfig(const FrequencyMatchConfig& config);
    void setDiagnosticsEnabled(bool enabled);

    void observeFrame(
        const AudioSamplePacket& audioSamplePacket,
        const detection::FrequencyBandMeasurementPacket& evidence
    );

    bool popOccurrence(Occurrence& out);
    FrequencyMatchDetector& detector();
    const FrequencyMatchDetector& detector() const;

private:
    bool _hasPending = false;
    detection::FrequencyBandMeasurementPacket _peakEvidence = {};
    FrequencyMatchDetector _detector = {};
    FrequencyMatchConfig _config = {};
    Occurrence _pending = {};
    unsigned long _lastEmittedCloseMs = 0;
};

} // namespace detection

