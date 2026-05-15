#include "ScalarSignalEmitter.h"

namespace detection {

ScalarSignalEmitter::ScalarSignalEmitter() = default;

void ScalarSignalEmitter::reset() {
    _detector.resetState();
    resetCandidateLifecycle();
}

void ScalarSignalEmitter::begin() {
    _detector.begin();
    resetCandidateLifecycle();
}

void ScalarSignalEmitter::setOnsetDetectionThreshold(float value) {
    _detector.setOnsetDetectionThreshold(value);
}

void ScalarSignalEmitter::setOnsetReleaseThreshold(float value) {
    _detector.setOnsetReleaseThreshold(value);
}

void ScalarSignalEmitter::setCooldownAfterOnsetMs(unsigned long value) {
    _detector.setCooldownAfterOnsetMs(value);
}

void ScalarSignalEmitter::setMinTransientDurationMs(unsigned long value) {
    _detector.setMinTransientDurationMs(value);
}

void ScalarSignalEmitter::setMaxTransientDurationMs(unsigned long value) {
    _detector.setMaxTransientDurationMs(value);
}

void ScalarSignalEmitter::setMinTransientPeakStrength(float value) {
    _detector.setMinTransientPeakStrength(value);
}

void ScalarSignalEmitter::setReleaseDebounceMs(unsigned long value) {
    _detector.setReleaseDebounceMs(value);
}

void ScalarSignalEmitter::setDiagnosticsEnabled(bool enabled) {
    _detector.setDiagnosticsEnabled(enabled);
}

void ScalarSignalEmitter::setDiagnosticsLabel(const char* value) {
    _detector.setDiagnosticsLabel(value);
}

void ScalarSignalEmitter::observe(const AudioSignalFrame& frame, float signalLevel) {
    if (!frame.valid) {
        return;
    }

    _detector.update(signalLevel, frame.sampleTimeUs);

    if (_detector.onsetDetected()) {
        _candidateActive = true;
        _releaseObserved = false;
        _candidateReady = false;
        _candidateFirstSeenSample = frame.sampleIndex;
        _candidatePeakSample = frame.sampleIndex;
        _candidateReleaseSample = frame.sampleIndex;
        _candidateFirstSeenUs = frame.sampleTimeUs;
        _candidatePeakUs = frame.sampleTimeUs;
        _candidateReleaseObservedUs = 0;
        _candidateFirstSeenMs = frame.sampleTimeMs;
        _candidatePeakMs = frame.sampleTimeMs;
        _candidateReleaseObservedMs = 0;
        _candidateHoldWindows = 1;
        _candidateOnsetStrength = signalLevel;
        _candidatePeakStrength = signalLevel;
        _candidateCurrentStrength = signalLevel;
    } else if (_candidateActive) {
        ++_candidateHoldWindows;

        if (signalLevel > _candidatePeakStrength) {
            _candidatePeakStrength = signalLevel;
            _candidatePeakSample = frame.sampleIndex;
            _candidatePeakUs = frame.sampleTimeUs;
            _candidatePeakMs = frame.sampleTimeMs;
        }

        _candidateCurrentStrength = signalLevel;
    }

    if (_candidateActive) {
        if (_detector.releaseObserved()) {
            _releaseObserved = true;
            _candidateReleaseObservedUs = _detector.releaseObservedUs();
            _candidateReleaseObservedMs = _candidateReleaseObservedUs / 1000UL;
        } else if (!_detector.transientDetected() && _releaseObserved) {
            // The signal recovered before the release debounce elapsed.
            _releaseObserved = false;
            _candidateReleaseObservedUs = 0;
            _candidateReleaseObservedMs = 0;
        }
    }

    if (_candidateActive && _detector.transientDetected()) {
        _candidateReady = true;
        _candidateReleaseSample = frame.sampleIndex;
    }
}

bool ScalarSignalEmitter::onsetDetected() const {
    return _detector.onsetDetected();
}

float ScalarSignalEmitter::onsetStrength() const {
    return _detector.onsetStrength();
}

bool ScalarSignalEmitter::transientDetected() const {
    return _candidateReady && _detector.transientDetected();
}

float ScalarSignalEmitter::transientStrength() const {
    return _detector.transientStrength();
}

unsigned long ScalarSignalEmitter::transientDurationMs() const {
    return _detector.transientDurationMs();
}

bool ScalarSignalEmitter::candidateActive() const {
    return _candidateActive;
}

bool ScalarSignalEmitter::releaseObserved() const {
    return _releaseObserved;
}

unsigned long ScalarSignalEmitter::candidateHoldWindows() const {
    return _candidateHoldWindows;
}

unsigned long ScalarSignalEmitter::candidateFirstSeenMs() const {
    return _candidateFirstSeenMs;
}

unsigned long ScalarSignalEmitter::candidatePeakMs() const {
    return _candidatePeakMs;
}

unsigned long ScalarSignalEmitter::candidateReleaseObservedMs() const {
    return _candidateReleaseObservedMs;
}

uint64_t ScalarSignalEmitter::candidateFirstSeenSample() const {
    return _candidateFirstSeenSample;
}

uint64_t ScalarSignalEmitter::candidatePeakSample() const {
    return _candidatePeakSample;
}

uint64_t ScalarSignalEmitter::candidateReleaseSample() const {
    return _candidateReleaseSample;
}

float ScalarSignalEmitter::candidatePeakStrength() const {
    return _candidatePeakStrength;
}

const char* ScalarSignalEmitter::lastOnsetRejectReasonName() const {
    return _detector.lastOnsetRejectReasonName();
}

const char* ScalarSignalEmitter::lastTransientRejectReasonName() const {
    return _detector.lastTransientRejectReasonName();
}

unsigned long ScalarSignalEmitter::lastTransientRejectedDurationMs() const {
    return _detector.lastTransientRejectedDurationMs();
}

float ScalarSignalEmitter::lastTransientRejectedStrength() const {
    return _detector.lastTransientRejectedStrength();
}

bool ScalarSignalEmitter::consumeCandidate(const AudioSignalFrame& frame,
                                           SignalKind kind,
                                           SignalSource source,
                                           SignalCandidate& out) {
    if (!_candidateReady || !_candidateActive) {
        return false;
    }

    out = {};
    out.kind = kind;
    out.source = source;
    out.present = true;
    out.startSample = _candidateFirstSeenSample;
    out.peakSample = _candidatePeakSample;
    out.releaseSample = _candidateReleaseSample;
    out.startMs = _candidateFirstSeenMs;
    out.peakMs = _candidatePeakMs;
    out.releaseMs = _releaseObserved ? _candidateReleaseObservedMs : frame.sampleTimeMs;
    out.durationMs = out.releaseMs >= out.startMs ? out.releaseMs - out.startMs : 0UL;
    out.strength = _candidatePeakStrength;
    out.score = _candidatePeakStrength;
    out.contrast = 0.0f;
    out.transient.present = true;
    out.transient.onsetSample = _candidateFirstSeenSample;
    out.transient.peakSample = _candidatePeakSample;
    out.transient.releaseSample = _candidateReleaseSample;
    out.transient.startMs = _candidateFirstSeenMs;
    out.transient.heardAtMs = out.releaseMs;
    out.transient.acceptedMs = out.releaseMs;
    out.transient.durationMs = out.durationMs;
    out.transient.onsetStrength = _candidateOnsetStrength;
    out.transient.peakStrength = _candidatePeakStrength;
    out.transient.releaseStrength = _candidateCurrentStrength;
    out.transient.ambientBaseline = frame.baseline;
    out.transient.audioOverflowDuringCandidate = frame.overflowDuringBlock;
    out.valid = true;

    resetCandidateLifecycle();
    return true;
}

void ScalarSignalEmitter::resetCandidateLifecycle() {
    _candidateActive = false;
    _releaseObserved = false;
    _candidateReady = false;
    _candidateFirstSeenSample = 0;
    _candidatePeakSample = 0;
    _candidateReleaseSample = 0;
    _candidateFirstSeenUs = 0;
    _candidatePeakUs = 0;
    _candidateReleaseObservedUs = 0;
    _candidateFirstSeenMs = 0;
    _candidatePeakMs = 0;
    _candidateReleaseObservedMs = 0;
    _candidateHoldWindows = 0;
    _candidateOnsetStrength = 0.0f;
    _candidatePeakStrength = 0.0f;
    _candidateCurrentStrength = 0.0f;
}

} // namespace detection
