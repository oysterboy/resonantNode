#include "AmpSignalEmitter.h"

#include "../AmpCandidateBuilder.h"
#include "../AmpTransientDetector.h"

namespace detection {

namespace {

SignalCandidate toSignalCandidate(const AmpCandidate& candidate) {
    SignalCandidate out;
    out.kind = SignalKind::AmpTransient;
    out.source = SignalSource::Amp;
    out.present = true;
    out.valid = candidate.durationMs > 0 || candidate.peakStrength > 0.0f || candidate.releaseMillisApprox != 0;
    out.startSample = candidate.onsetSample;
    out.peakSample = candidate.peakSample;
    out.releaseSample = candidate.releaseSample;
    out.startMs = candidate.onsetMillisApprox;
    out.releaseMs = candidate.releaseMillisApprox;
    out.durationMs = candidate.durationMs;
    out.strength = candidate.peakStrength;
    out.score = 0.0f;
    out.contrast = 0.0f;
    // No explicit peak timestamp exists on the legacy AMP candidate, so peakMs stays at the default.
    out.transient.present = true;
    out.transient.onsetSample = candidate.onsetSample;
    out.transient.peakSample = candidate.peakSample;
    out.transient.releaseSample = candidate.releaseSample;
    out.transient.startMs = candidate.onsetMillisApprox;
    out.transient.heardAtMs = candidate.onsetMillisApprox;
    out.transient.acceptedMs = candidate.onsetMillisApprox;
    out.transient.durationMs = candidate.durationMs;
    out.transient.onsetStrength = candidate.onsetStrength;
    out.transient.peakStrength = candidate.peakStrength;
    out.transient.releaseStrength = candidate.releaseStrength;
    out.transient.ambientBaseline = candidate.ambientBaseline;
    out.transient.audioOverflowDuringCandidate = candidate.audioOverflowDuringCandidate;
    return out;
}

} // namespace

AmpSignalEmitter::AmpSignalEmitter() = default;

void AmpSignalEmitter::reset() {
    _hasPending = false;
    _pending = {};
}

void AmpSignalEmitter::observeFrame(
    const AudioSignalFrame& frame,
    AmpTransientDetector& detector,
    AmpCandidateBuilder& builder
) {
    builder.observeSample(frame, detector);

    AmpCandidate candidate;
    bool sawCandidate = false;
    while (builder.popCandidate(candidate)) {
        _pending = toSignalCandidate(candidate);
        sawCandidate = true;
    }

    _hasPending = sawCandidate;
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
