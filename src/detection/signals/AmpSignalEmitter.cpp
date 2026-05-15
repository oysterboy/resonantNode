#include "AmpSignalEmitter.h"

namespace detection {

namespace {

} // namespace

void AmpSignalEmitter::fillAmpCandidate(SignalCandidate& candidate, const AudioSignalFrame& frame) {
    candidate.kind = SignalKind::AmpTransient;
    candidate.source = SignalSource::Amp;
    candidate.detectorKind = SignalDetectorKind::Transient;
    candidate.valid = candidate.durationMs > 0 || candidate.strength > 0.0f || candidate.releaseMs != 0;
    // No explicit peak timestamp exists on the legacy AMP candidate, so peakMs stays at the default.
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

AmpSignalEmitter::AmpSignalEmitter() = default;

void AmpSignalEmitter::reset() {
    _hasPending = false;
    _pending = {};
    _scalarEmitter.reset();
}

void AmpSignalEmitter::observeFrame(const AudioSignalFrame& frame) {
    _scalarEmitter.observe(frame, static_cast<float>(frame.level));

    if (_scalarEmitter.transientDetected()) {
        SignalCandidate candidate;
        if (_scalarEmitter.consumeCandidate(frame, SignalKind::AmpTransient, SignalSource::Amp, candidate)) {
            fillAmpCandidate(candidate, frame);
            _pending = candidate;
            _hasPending = true;
        }
    }
}

bool AmpSignalEmitter::popSignalCandidate(SignalCandidate& out) {
    if (!_hasPending) {
        return false;
    }

    out = _pending;
    _hasPending = false;
    return true;
}

} // namespace detection
