#include "ScalarOccurrenceSource.h"

namespace detection {

ScalarOccurrenceSource::ScalarOccurrenceSource() = default;

void ScalarOccurrenceSource::reset() {
    _detector.resetState();
    resetCandidateLifecycle();
    resetRejectSummary();
    _lastObservedTransientRejectedCount = 0;
}

void ScalarOccurrenceSource::resetRejectSummary() {
    _rejectedCandidateCount = 0;
    _rejectedBestDurationMs = 0;
    _rejectedSecondBestDurationMs = 0;
    _rejectedBestOpenMs = 0;
    _rejectedBestPeakMs = 0;
    _rejectedBestLastMatchMs = 0;
    _rejectedBestCloseMs = 0;
    _rejectedBestPeakStrength = 0.0f;
    _rejectedMaxPeakStrength = 0.0f;
    _rejectedMaxPeakStrengthMs = 0;
    _rejectedBestReason = "none";
    _rejectedBestGateReason = "none";
    _rejectedTotalMatchMs = 0;
    _rejectedTotalGapMs = 0;
    _rejectedMaxGapMs = 0;
    _lastRejectedCloseMs = 0;
    _rejectedIslandCount = 0;
}

void ScalarOccurrenceSource::begin() {
    _detector.begin();
    resetCandidateLifecycle();
    resetRejectSummary();
    _lastObservedTransientRejectedCount = 0;
}

void ScalarOccurrenceSource::setConfig(const ScalarTransientConfig& config) {
    setOnsetDetectionThreshold(config.onsetDetectionThreshold);
    setOnsetReleaseThreshold(config.onsetReleaseThreshold);
    setCooldownAfterOnsetMs(config.cooldownAfterOnsetMs);
    setMinTransientDurationMs(config.minTransientDurationMs);
    setMaxTransientDurationMs(config.maxTransientDurationMs);
    setMinTransientPeakStrength(config.minTransientPeakStrength);
    setReleaseDebounceMs(config.releaseDebounceMs);
}

void ScalarOccurrenceSource::observeFrame(const AudioSamplePacket& frame, float signalLevel, OccurrenceKind kind, OccurrenceSource source) {
    observe(frame, signalLevel);

    if (transientDetected()) {
        Occurrence candidate;
        if (consumeCandidate(frame, kind, source, candidate)) {
            _pending = candidate;
            _hasPending = true;
        }
    }
}

void ScalarOccurrenceSource::setOnsetDetectionThreshold(float value) {
    _detector.setOnsetDetectionThreshold(value);
}

void ScalarOccurrenceSource::setOnsetReleaseThreshold(float value) {
    _detector.setOnsetReleaseThreshold(value);
}

void ScalarOccurrenceSource::setCooldownAfterOnsetMs(unsigned long value) {
    _detector.setCooldownAfterOnsetMs(value);
}

void ScalarOccurrenceSource::setMinTransientDurationMs(unsigned long value) {
    _detector.setMinTransientDurationMs(value);
}

void ScalarOccurrenceSource::setMaxTransientDurationMs(unsigned long value) {
    _detector.setMaxTransientDurationMs(value);
}

void ScalarOccurrenceSource::setMinTransientPeakStrength(float value) {
    _detector.setMinTransientPeakStrength(value);
}

void ScalarOccurrenceSource::setReleaseDebounceMs(unsigned long value) {
    _detector.setReleaseDebounceMs(value);
}

void ScalarOccurrenceSource::setDiagnosticsEnabled(bool enabled) {
    _detector.setDiagnosticsEnabled(enabled);
}

void ScalarOccurrenceSource::setDiagnosticsLabel(const char* value) {
    _detector.setDiagnosticsLabel(value);
}

void ScalarOccurrenceSource::observe(const AudioSamplePacket& frame, float signalLevel) {
    if (!frame.valid) {
        return;
    }

    _detector.update(signalLevel, frame.timeUs);

    if (_detector.onsetDetected()) {
        _candidateActive = true;
        _releaseObserved = false;
        _candidateReady = false;
        _candidateFirstSeenSample = frame.sampleIndex;
        _candidatePeakSample = frame.sampleIndex;
        _candidateReleaseSample = frame.sampleIndex;
        _candidateFirstSeenUs = frame.timeUs;
        _candidatePeakUs = frame.timeUs;
        _candidateReleaseObservedUs = 0;
        _candidateFirstSeenMs = frame.timeMs;
        _candidatePeakMs = frame.timeMs;
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
            _candidatePeakUs = frame.timeUs;
            _candidatePeakMs = frame.timeMs;
        }

        _candidateCurrentStrength = signalLevel;
    }

    if (_candidateActive) {
        if (_detector.releaseObserved()) {
            _releaseObserved = true;
            _candidateReleaseObservedUs = _detector.releaseObservedUs();
            _candidateReleaseObservedMs = _candidateReleaseObservedUs / 1000UL;
        } else if (!_detector.transientDetected() && _releaseObserved) {
            // The occurrence recovered before the release debounce elapsed.
            _releaseObserved = false;
            _candidateReleaseObservedUs = 0;
            _candidateReleaseObservedMs = 0;
        }
    }

    const unsigned long transientRejectedCount = _detector.transientRejectedCount();
    if (_candidateActive && transientRejectedCount > _lastObservedTransientRejectedCount) {
        _lastObservedTransientRejectedCount = transientRejectedCount;

        const unsigned long rejectedDurationMs = _detector.lastTransientRejectedDurationMs();
        const float rejectedPeakStrength = _detector.lastTransientRejectedStrength();
        const unsigned long rejectedCloseMs = _candidateReleaseObservedMs != 0 ? _candidateReleaseObservedMs : _candidatePeakMs;
        if (_lastRejectedCloseMs > 0 && _candidateFirstSeenMs > _lastRejectedCloseMs) {
            const unsigned long gapMs = _candidateFirstSeenMs - _lastRejectedCloseMs;
            _rejectedTotalGapMs += gapMs;
            if (gapMs > _rejectedMaxGapMs) {
                _rejectedMaxGapMs = gapMs;
            }
        }
        _lastRejectedCloseMs = rejectedCloseMs;
        ++_rejectedCandidateCount;
        ++_rejectedIslandCount;
        _rejectedTotalMatchMs += rejectedDurationMs;
        if (rejectedPeakStrength >= _rejectedMaxPeakStrength) {
            _rejectedMaxPeakStrength = rejectedPeakStrength;
            _rejectedMaxPeakStrengthMs = _candidatePeakMs;
        }
        if (rejectedDurationMs >= _rejectedBestDurationMs) {
            _rejectedSecondBestDurationMs = _rejectedBestDurationMs;
            _rejectedBestDurationMs = rejectedDurationMs;
            _rejectedBestOpenMs = _candidateFirstSeenMs;
            _rejectedBestPeakMs = _candidatePeakMs;
            _rejectedBestLastMatchMs = _candidateReleaseObservedMs != 0 ? _candidateReleaseObservedMs : _candidatePeakMs;
            _rejectedBestCloseMs = _candidateReleaseObservedMs != 0 ? _candidateReleaseObservedMs : _candidatePeakMs;
            _rejectedBestPeakStrength = _candidatePeakStrength;
            _rejectedBestReason = _detector.lastTransientRejectReasonName();
            _rejectedBestGateReason = _detector.lastTransientRejectReasonName();
        } else if (rejectedDurationMs > _rejectedSecondBestDurationMs) {
            _rejectedSecondBestDurationMs = rejectedDurationMs;
        }

        resetCandidateLifecycle();
    }

    if (_candidateActive && _detector.transientDetected()) {
        _candidateReady = true;
        _candidateReleaseSample = frame.sampleIndex;
    }
}

bool ScalarOccurrenceSource::onsetDetected() const {
    return _detector.onsetDetected();
}

float ScalarOccurrenceSource::onsetStrength() const {
    return _detector.onsetStrength();
}

bool ScalarOccurrenceSource::transientDetected() const {
    return _candidateReady && _detector.transientDetected();
}

float ScalarOccurrenceSource::transientStrength() const {
    return _detector.transientStrength();
}

unsigned long ScalarOccurrenceSource::transientDurationMs() const {
    return _detector.transientDurationMs();
}

bool ScalarOccurrenceSource::candidateActive() const {
    return _candidateActive;
}

bool ScalarOccurrenceSource::releaseObserved() const {
    return _releaseObserved;
}

unsigned long ScalarOccurrenceSource::candidateHoldWindows() const {
    return _candidateHoldWindows;
}

unsigned long ScalarOccurrenceSource::candidateFirstSeenMs() const {
    return _candidateFirstSeenMs;
}

unsigned long ScalarOccurrenceSource::candidatePeakMs() const {
    return _candidatePeakMs;
}

unsigned long ScalarOccurrenceSource::candidateReleaseObservedMs() const {
    return _candidateReleaseObservedMs;
}

uint64_t ScalarOccurrenceSource::candidateFirstSeenSample() const {
    return _candidateFirstSeenSample;
}

uint64_t ScalarOccurrenceSource::candidatePeakSample() const {
    return _candidatePeakSample;
}

uint64_t ScalarOccurrenceSource::candidateReleaseSample() const {
    return _candidateReleaseSample;
}

float ScalarOccurrenceSource::candidatePeakStrength() const {
    return _candidatePeakStrength;
}

unsigned long ScalarOccurrenceSource::rejectedCandidateCount() const {
    return _rejectedCandidateCount;
}

unsigned long ScalarOccurrenceSource::bestRejectedDurationMs() const {
    return _rejectedBestDurationMs;
}

unsigned long ScalarOccurrenceSource::secondBestRejectedDurationMs() const {
    return _rejectedSecondBestDurationMs;
}

unsigned long ScalarOccurrenceSource::bestRejectedOpenMs() const {
    return _rejectedBestOpenMs;
}

unsigned long ScalarOccurrenceSource::bestRejectedPeakMs() const {
    return _rejectedBestPeakMs;
}

unsigned long ScalarOccurrenceSource::bestRejectedLastMatchMs() const {
    return _rejectedBestLastMatchMs;
}

unsigned long ScalarOccurrenceSource::bestRejectedCloseMs() const {
    return _rejectedBestCloseMs;
}

float ScalarOccurrenceSource::bestRejectedPeakStrength() const {
    return _rejectedBestPeakStrength;
}

float ScalarOccurrenceSource::maxRejectedPeakStrength() const {
    return _rejectedMaxPeakStrength;
}

unsigned long ScalarOccurrenceSource::maxRejectedPeakStrengthMs() const {
    return _rejectedMaxPeakStrengthMs;
}

const char* ScalarOccurrenceSource::bestRejectedReasonName() const {
    return _rejectedBestReason;
}

const char* ScalarOccurrenceSource::bestRejectedGateReasonName() const {
    return _rejectedBestGateReason;
}

unsigned long ScalarOccurrenceSource::totalRejectedMatchMs() const {
    return _rejectedTotalMatchMs;
}

unsigned long ScalarOccurrenceSource::totalRejectedGapMs() const {
    return _rejectedTotalGapMs;
}

unsigned long ScalarOccurrenceSource::maxRejectedGapMs() const {
    return _rejectedMaxGapMs;
}

unsigned long ScalarOccurrenceSource::rejectedIslandCount() const {
    return _rejectedIslandCount;
}

const char* ScalarOccurrenceSource::lastOnsetRejectReasonName() const {
    return _detector.lastOnsetRejectReasonName();
}

const char* ScalarOccurrenceSource::lastTransientRejectReasonName() const {
    return _detector.lastTransientRejectReasonName();
}

unsigned long ScalarOccurrenceSource::lastTransientRejectedDurationMs() const {
    return _detector.lastTransientRejectedDurationMs();
}

float ScalarOccurrenceSource::lastTransientRejectedStrength() const {
    return _detector.lastTransientRejectedStrength();
}

bool ScalarOccurrenceSource::popOccurrence(Occurrence& out) {
    if (!_hasPending) {
        return false;
    }

    out = _pending;
    _pending = {};
    _hasPending = false;
    return true;
}

bool ScalarOccurrenceSource::consumeCandidate(const AudioSamplePacket& frame,
                                           OccurrenceKind kind,
                                           OccurrenceSource source,
                                           Occurrence& out) {
    if (!_candidateReady || !_candidateActive) {
        return false;
    }

    out = {};
    out.kind = kind;
    out.source = source;
    out.detectorKind = kind == OccurrenceKind::FrequencyMatch
        ? OccurrenceDetectorKind::FrequencyMatch
        : OccurrenceDetectorKind::Transient;
    out.present = true;
    out.startSample = _candidateFirstSeenSample;
    out.peakSample = _candidatePeakSample;
    out.releaseSample = _candidateReleaseSample;
    out.startMs = _candidateFirstSeenMs;
    out.peakMs = _candidatePeakMs;
    out.releaseMs = _releaseObserved ? _candidateReleaseObservedMs : frame.timeMs;
    out.endMs = out.releaseMs;
    out.durationMs = out.releaseMs >= out.startMs ? out.releaseMs - out.startMs : 0UL;
    out.strength = _candidatePeakStrength;
    out.score = _candidatePeakStrength;
    out.contrast = 0.0f;
    out.confidence = 1.0f;
    out.ampEvidencePresent = true;
    out.ampLevel = frame.audioMagnitudeValue;
    out.ampBaseline = frame.baseline;
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

void ScalarOccurrenceSource::resetCandidateLifecycle() {
    _candidateActive = false;
    _releaseObserved = false;
    _candidateReady = false;
    _hasPending = false;
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
    _pending = {};
}

} // namespace detection

