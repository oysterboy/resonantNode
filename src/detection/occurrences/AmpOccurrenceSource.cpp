#include "AmpOccurrenceSource.h"

namespace detection {

void AmpOccurrenceSource::fillAmpCandidate(Occurrence& candidate, const AudioSignalFrame& frame) {
    candidate.kind = OccurrenceKind::AmpTransient;
    candidate.source = OccurrenceSource::Amp;
    candidate.detectorKind = OccurrenceDetectorKind::Transient;
    candidate.valid = candidate.durationMs > 0 || candidate.strength > 0.0f || candidate.releaseMs != 0;
    candidate.endMs = candidate.releaseMs;
    candidate.confidence = candidate.valid ? 1.0f : 0.0f;
    candidate.signalConfidence = candidate.confidence;
    candidate.frequencyConfidence = 0.0f;
    candidate.ampEvidencePresent = true;
    candidate.ampLevel = static_cast<float>(frame.level);
    candidate.ampBaseline = frame.baseline;
    candidate.transient.present = true;
    candidate.transient.onsetSample = candidate.startSample;
    candidate.transient.peakSample = candidate.peakSample;
    candidate.transient.releaseSample = candidate.releaseSample;
    candidate.transient.startMs = candidate.startMs;
    candidate.transient.heardAtMs = candidate.startMs;
    candidate.transient.acceptedMs = candidate.startMs;
    candidate.transient.durationMs = candidate.durationMs;
    candidate.transient.onsetStrength = candidate.strength;
    candidate.transient.peakStrength = candidate.strength;
    candidate.transient.releaseStrength = static_cast<float>(frame.level);
    candidate.transient.ambientBaseline = frame.baseline;
    candidate.transient.audioOverflowDuringCandidate = frame.overflowDuringBlock;
}

AmpOccurrenceSource::AmpOccurrenceSource() = default;

void AmpOccurrenceSource::reset() {
    _hasPending = false;
    _pending = {};
    _scalarEmitter.reset();
}

void AmpOccurrenceSource::observeFrame(const AudioSignalFrame& frame) {
    _scalarEmitter.observe(frame, static_cast<float>(frame.level));

    if (_scalarEmitter.transientDetected()) {
        Occurrence candidate;
        if (_scalarEmitter.consumeCandidate(frame, OccurrenceKind::AmpTransient, OccurrenceSource::Amp, candidate)) {
            fillAmpCandidate(candidate, frame);
            _pending = candidate;
            _hasPending = true;
        }
    }
}

bool AmpOccurrenceSource::popOccurrence(Occurrence& out) {
    if (!_hasPending) {
        return false;
    }

    out = _pending;
    _hasPending = false;
    return true;
}

} // namespace detection

