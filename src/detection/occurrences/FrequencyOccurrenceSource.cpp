#include "FrequencyOccurrenceSource.h"

namespace detection {

FrequencyOccurrenceSource::FrequencyOccurrenceSource() = default;

void FrequencyOccurrenceSource::reset() {
    _detector.resetState();
}

void FrequencyOccurrenceSource::setConfig(const FrequencyMatchConfig& config) {
    _config = config;
}

void FrequencyOccurrenceSource::setDiagnosticsEnabled(bool enabled) {
    _detector.setDiagnosticsEnabled(enabled);
}

void FrequencyOccurrenceSource::observeFrame(
    const AudioSamplePacket& audioSamplePacket,
    const detection::FrequencyBandMeasurementPacket& evidence
) {
    if (!audioSamplePacket.valid) {
        return;
    }

    if (!evidence.present || !evidence.fresh) {
        // Fresh-only lifecycle: stale or held measurements do not move the detector.
        return;
    }

    FrequencyMatchEvaluation::Values frequencyTuning = {};
    frequencyTuning.attackScoreMin = _config.attackScoreMin;
    frequencyTuning.releaseScoreMin = _config.releaseScoreMin;
    frequencyTuning.attackContrastMin = _config.attackContrastMin;
    frequencyTuning.releaseContrastMin = _config.releaseContrastMin;

    _detector.update(
        evidence,
        audioSamplePacket,
        audioSamplePacket.timeMs,
        audioSamplePacket.sampleIndex,
        frequencyTuning,
        _config.releaseDebounceMs,
        _config.cooldownAfterReleaseMs,
        _config.minDurationMs);
}

bool FrequencyOccurrenceSource::popOccurrence(Occurrence& out) {
    // Legacy shell-only compatibility: detector now owns the pending accepted
    // Occurrence payload and this wrapper simply forwards access.
    return _detector.popOccurrence(out);
}

FrequencyMatchDetector& FrequencyOccurrenceSource::detector() {
    return _detector;
}

const FrequencyMatchDetector& FrequencyOccurrenceSource::detector() const {
    return _detector;
}

} // namespace detection

