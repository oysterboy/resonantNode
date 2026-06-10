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
    // Analyzer SEQ uses this wrapper reset point as the per-trial scalar
    // report boundary, so clear both detector-owned canonical summaries here.
    _detector.resetAcceptedOccurrenceSummary();
    _detector.resetSelectedRejectSummary();
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

void ScalarOccurrenceSource::observeFrame(const AudioSamplePacket& audioSamplePacket, float signalLevel, OccurrenceKind kind, OccurrenceSource source) {
    observe(audioSamplePacket, signalLevel, kind, source);
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

void ScalarOccurrenceSource::observe(
    const AudioSamplePacket& audioSamplePacket,
    float signalLevel,
    OccurrenceKind kind,
    OccurrenceSource source
) {
    if (!audioSamplePacket.valid) {
        return;
    }

    _detector.update(audioSamplePacket, signalLevel, kind, source);

    if (_detector.onsetDetected()) {
        _candidateActive = true;
        _releaseObserved = false;
        _candidateFirstSeenSample = audioSamplePacket.sampleIndex;
        _candidatePeakSample = audioSamplePacket.sampleIndex;
        _candidateReleaseSample = audioSamplePacket.sampleIndex;
        _candidateFirstSeenUs = audioSamplePacket.timeUs;
        _candidatePeakUs = audioSamplePacket.timeUs;
        _candidateReleaseObservedUs = 0;
        _candidateFirstSeenMs = audioSamplePacket.timeMs;
        _candidatePeakMs = audioSamplePacket.timeMs;
        _candidateReleaseObservedMs = 0;
        _candidateHoldWindows = 1;
        _candidateOnsetStrength = signalLevel;
        _candidatePeakStrength = signalLevel;
        _candidateCurrentStrength = signalLevel;
    } else if (_candidateActive) {
        ++_candidateHoldWindows;

        if (signalLevel > _candidatePeakStrength) {
            _candidatePeakStrength = signalLevel;
            _candidatePeakSample = audioSamplePacket.sampleIndex;
            _candidatePeakUs = audioSamplePacket.timeUs;
            _candidatePeakMs = audioSamplePacket.timeMs;
        }

        _candidateCurrentStrength = signalLevel;
    }

    if (_candidateActive) {
        if (_detector.releaseObserved()) {
            _releaseObserved = true;
            _candidateReleaseSample = audioSamplePacket.sampleIndex;
            _candidateReleaseObservedUs = _detector.releaseObservedUs();
            _candidateReleaseObservedMs = _candidateReleaseObservedUs / 1000UL;
        } else if (!_detector.transientDetected() && _releaseObserved) {
            // The occurrence recovered before the release debounce elapsed.
            _releaseObserved = false;
            _candidateReleaseSample = 0;
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
        resetCandidateLifecycle();
    }
}

ScalarTransientDetector& ScalarOccurrenceSource::detector() {
    return _detector;
}

const ScalarTransientDetector& ScalarOccurrenceSource::detector() const {
    return _detector;
}

bool ScalarOccurrenceSource::onsetDetected() const {
    return _detector.onsetDetected();
}

float ScalarOccurrenceSource::onsetStrength() const {
    return _detector.onsetStrength();
}

bool ScalarOccurrenceSource::transientDetected() const {
    return _detector.transientDetected();
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
    // Legacy temporary shell: accepted scalar Occurrence construction now
    // lives in ScalarTransientDetector and this wrapper only delegates polling.
    return _detector.popOccurrence(out);
}

void ScalarOccurrenceSource::resetCandidateLifecycle() {
    _candidateActive = false;
    _releaseObserved = false;
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

