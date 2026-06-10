#pragma once

#include "Occurrence.h"
#include "../DetectionProfile.h"
#include "../features/FrequencyMatchEvaluation.h"
#include "../../io/AudioSignal.h"
#include "../detectors/FrequencyMatchDetector.h"

namespace detection {

/*
FrequencyOccurrenceSource

Temporary migration wrapper around the canonical FrequencyMatchDetector core.
Now forwards specialized frequency input into the detector core while the
remaining wrapper/routing cleanup is deferred.
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
    FrequencyMatchDetector _detector = {};
    FrequencyMatchConfig _config = {};
};

} // namespace detection

