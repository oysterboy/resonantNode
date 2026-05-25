#include "AmpDiagnosticProbe.h"

namespace detection {

AmpDiagnosticProbe::AmpDiagnosticProbe() = default;

void AmpDiagnosticProbe::begin() {
    _detector.begin();
}

void AmpDiagnosticProbe::resetState() {
    _detector.resetState();
    _hasPendingObservation = false;
    _pendingObservation = {};
}

void AmpDiagnosticProbe::observe(float signalLevel, uint32_t sampleTimeUs) {
    _lastSampleTimeUs = sampleTimeUs;
    _detector.update(signalLevel, sampleTimeUs);
    captureObservation(sampleTimeUs);
}

void AmpDiagnosticProbe::setOnsetDetectionThreshold(float value) {
    _detector.setOnsetDetectionThreshold(value);
}

void AmpDiagnosticProbe::setOnsetReleaseThreshold(float value) {
    _detector.setOnsetReleaseThreshold(value);
}

void AmpDiagnosticProbe::setCooldownAfterOnsetMs(unsigned long value) {
    _detector.setCooldownAfterOnsetMs(value);
}

void AmpDiagnosticProbe::setMinTransientDurationMs(unsigned long value) {
    _detector.setMinTransientDurationMs(value);
}

void AmpDiagnosticProbe::setMaxTransientDurationMs(unsigned long value) {
    _detector.setMaxTransientDurationMs(value);
}

void AmpDiagnosticProbe::setMinTransientPeakStrength(float value) {
    _detector.setMinTransientPeakStrength(value);
}

void AmpDiagnosticProbe::setReleaseDebounceMs(unsigned long value) {
    _detector.setReleaseDebounceMs(value);
}

void AmpDiagnosticProbe::setDiagnosticsEnabled(bool enabled) {
    _detector.setDiagnosticsEnabled(enabled);
}

bool AmpDiagnosticProbe::hasNewObservation() const {
    return _hasPendingObservation;
}

bool AmpDiagnosticProbe::popObservation(AmpDiagnosticObservation& out) {
    if (!_hasPendingObservation) {
        return false;
    }

    out = _pendingObservation;
    _hasPendingObservation = false;
    return true;
}

AmpDiagnosticSnapshot AmpDiagnosticProbe::snapshot() const {
    AmpDiagnosticSnapshot snapshot;
    snapshot.onsetVisible = _detector.onsetDetected();
    snapshot.transientVisible = _detector.transientDetected();
    snapshot.peakActive = _detector.peakActive();
    snapshot.onsetStrength = _detector.onsetStrength();
    snapshot.transientStrength = _detector.transientStrength();
    snapshot.transientDurationMs = _detector.transientDurationMs();
    snapshot.peakStrength = _detector.peakStrength();
    snapshot.onsetRejectReason = _detector.lastOnsetRejectReasonName();
    snapshot.transientRejectReason = ampDiagnosticRejectReasonFromName(_detector.lastTransientRejectReasonName());
    snapshot.rejectedDurationMs = _detector.lastTransientRejectedDurationMs();
    snapshot.rejectedStrength = _detector.lastTransientRejectedStrength();
    snapshot.onsetRejectedCount = _detector.onsetRejectedCount();
    snapshot.transientRejectedCount = _detector.transientRejectedCount();
    snapshot.transientRejectedDurationTooShortCount = _detector.transientRejectedDurationTooShortCount();
    snapshot.transientRejectedDurationTooLongCount = _detector.transientRejectedDurationTooLongCount();
    snapshot.transientRejectedStrengthTooLowCount = _detector.transientRejectedStrengthTooLowCount();
    snapshot.onsetDetectionThreshold = _detector.onsetDetectionThreshold();
    snapshot.onsetReleaseThreshold = _detector.onsetReleaseThreshold();
    snapshot.cooldownAfterOnsetMs = _detector.cooldownAfterOnsetMs();
    snapshot.minTransientDurationMs = _detector.minTransientDurationMs();
    snapshot.maxTransientDurationMs = _detector.maxTransientDurationMs();
    snapshot.minTransientPeakStrength = _detector.minTransientPeakStrength();
    snapshot.releaseDebounceMs = _detector.releaseDebounceMs();
    return snapshot;
}

void AmpDiagnosticProbe::captureObservation(uint32_t sampleTimeUs) {
    const AmpDiagnosticSnapshot snapshot = this->snapshot();

    _pendingObservation.transientObserved = snapshot.transientVisible;
    _pendingObservation.onsetMs = sampleTimeUs / 1000UL;
    _pendingObservation.acceptedMs = snapshot.transientVisible ? sampleTimeUs / 1000UL : 0UL;
    _pendingObservation.durationMs = snapshot.transientDurationMs;
    _pendingObservation.strength = snapshot.transientStrength;
    _pendingObservation.closeReason = snapshot.onsetRejectReason;
    _pendingObservation.rejectReason = snapshot.transientRejectReason;
    _hasPendingObservation = true;
}

} // namespace detection
