#include "io/AudioOnsetDetector.h"
#include <Arduino.h>

AudioOnsetDetector::AudioOnsetDetector(AudioSignal& audioSignal)
    : _audioSignal(audioSignal) {}

void AudioOnsetDetector::begin() {
    resetState();
    _statsStartMs = millis();
    _lastStatsPrintMs = 0;
    _peakAcceptedCount = 0;
}

void AudioOnsetDetector::resetState() {
    _onsetDetected = false;
    _onsetStrength = 0.0f;
    _lastOnsetMs = 0;

    _transientDetected = false;
    _transientStrength = 0.0f;
    _transientDurationMs = 0;
    _peakActive = false;
    _peakStartedMs = 0;
    _releaseCandidateStartedMs = 0;
    _peakStrength = 0.0f;
}

void AudioOnsetDetector::update(unsigned long now) {
    _onsetDetected = false;
    _onsetStrength = 0.0f;

    _transientDetected = false;
    _transientStrength = 0.0f;

    const float signalMagnitude = _audioSignal.signalMagnitude();
    const bool aboveAttackThreshold = signalMagnitude > _onsetDetectionThreshold;
    const bool aboveReleaseThreshold = signalMagnitude > _onsetReleaseThreshold;
    const bool onsetCooldownElapsed = now - _lastOnsetMs >= _cooldownAfterOnsetMs;

    // ONSET STAGE
    updateOnsetStage(now, signalMagnitude, aboveAttackThreshold, onsetCooldownElapsed);

    // TRANSIENT STAGE
    updateTransientStage(now, signalMagnitude, aboveReleaseThreshold);

    printTransientStatsIfDue(now);
}

void AudioOnsetDetector::updateOnsetStage(unsigned long now, float signalMagnitude, bool aboveAttackThreshold, bool onsetCooldownElapsed) {
    // Use raw magnitude for the edge so short bursts are not delayed by smoothing.
    // The separate release threshold keeps the peak stable when the signal wobbles near the edge.
    if (aboveAttackThreshold && !_peakActive && onsetCooldownElapsed) {
        _peakActive = true;
        _peakStartedMs = now;
        _peakStrength = signalMagnitude;

        _onsetDetected = true;
        _onsetStrength = signalMagnitude;
        _lastOnsetMs = now;
    }
}

void AudioOnsetDetector::updateTransientStage(unsigned long now, float signalMagnitude, bool aboveReleaseThreshold) {
    if (_peakActive && signalMagnitude > _peakStrength) {
        _peakStrength = signalMagnitude;
    }

    // Ignore brief dips below the release threshold so one burst does not get
    // chopped into multiple timing buckets by ADC/loop quantization.
    if (_peakActive) {
        if (!aboveReleaseThreshold) {
            if (_releaseCandidateStartedMs == 0) {
                _releaseCandidateStartedMs = now;
            }
        } else {
            _releaseCandidateStartedMs = 0;
        }
    }

    // Close the peak only after the signal has stayed below the release
    // threshold for long enough to count as a real end of burst.
    if (_peakActive && _releaseCandidateStartedMs != 0 && now - _releaseCandidateStartedMs >= _releaseDebounceMs) {
        const unsigned long peakDurationMs = now - _peakStartedMs;
        const bool durationAccepted = peakDurationMs >= _minTransientDurationMs && peakDurationMs <= _maxTransientDurationMs;
        // Duration alone is not enough: weak ambient crossings can still last
        // long enough to look valid, so require a minimum peak strength too.
        const bool strengthAccepted = _peakStrength >= _minTransientPeakStrength;
        const bool accepted = durationAccepted && strengthAccepted;

        if (accepted) {
            _peakAcceptedCount++;
            _transientDetected = true;
            _transientStrength = _peakStrength;
            _transientDurationMs = peakDurationMs;
        }

        _peakActive = false;
        _peakStartedMs = 0;
        _releaseCandidateStartedMs = 0;
        _peakStrength = 0.0f;
    }
}

void AudioOnsetDetector::printTransientStatsIfDue(unsigned long now) {
    if (!_diagnosticsEnabled) {
        return;
    }

    if (_lastStatsPrintMs == 0 || now - _lastStatsPrintMs >= _statsPrintIntervalMs) {
        const unsigned long elapsedMs = now - _statsStartMs;
        const unsigned long expectedCount = (elapsedMs + (_expectedTransientPeriodMs / 2)) / _expectedTransientPeriodMs;
        const unsigned long successRate = expectedCount > 0 ? ((_peakAcceptedCount * 100UL) / expectedCount) : 0;

        Serial.print("EVT transient success t=");
        Serial.print(now);
        Serial.print(" accepted=");
        Serial.print(_peakAcceptedCount);
        Serial.print(" expected=");
        Serial.print(expectedCount);
        Serial.print(" success=");
        Serial.print(successRate);
        Serial.println("%");

        _lastStatsPrintMs = now;
    }
}

bool AudioOnsetDetector::onsetDetected() const {
    return _onsetDetected;
}

float AudioOnsetDetector::onsetStrength() const {
    return _onsetStrength;
}

bool AudioOnsetDetector::transientDetected() const {
    return _transientDetected;
}

float AudioOnsetDetector::transientStrength() const {
    return _transientStrength;
}

unsigned long AudioOnsetDetector::transientDurationMs() const {
    return _transientDurationMs;
}

float AudioOnsetDetector::onsetDetectionThreshold() const {
    return _onsetDetectionThreshold;
}

float AudioOnsetDetector::onsetReleaseThreshold() const {
    return _onsetReleaseThreshold;
}

unsigned long AudioOnsetDetector::cooldownAfterOnsetMs() const {
    return _cooldownAfterOnsetMs;
}

unsigned long AudioOnsetDetector::minTransientDurationMs() const {
    return _minTransientDurationMs;
}

unsigned long AudioOnsetDetector::maxTransientDurationMs() const {
    return _maxTransientDurationMs;
}

float AudioOnsetDetector::minTransientPeakStrength() const {
    return _minTransientPeakStrength;
}

unsigned long AudioOnsetDetector::releaseDebounceMs() const {
    return _releaseDebounceMs;
}

void AudioOnsetDetector::setOnsetDetectionThreshold(float value) {
    _onsetDetectionThreshold = value;
}

void AudioOnsetDetector::setOnsetReleaseThreshold(float value) {
    // Keep the release threshold below the attack threshold, but close enough
    // that the peak closes promptly once the burst really starts to decay.
    _onsetReleaseThreshold = value;
}

void AudioOnsetDetector::setCooldownAfterOnsetMs(unsigned long value) {
    _cooldownAfterOnsetMs = value;
}

void AudioOnsetDetector::setReleaseDebounceMs(unsigned long value) {
    // A small debounce makes the release edge less sensitive to one-sample dips.
    _releaseDebounceMs = value;
}

void AudioOnsetDetector::setMinTransientDurationMs(unsigned long value) {
    _minTransientDurationMs = value;
}

void AudioOnsetDetector::setMaxTransientDurationMs(unsigned long value) {
    _maxTransientDurationMs = value;
}

void AudioOnsetDetector::setMinTransientPeakStrength(float value) {
    // Set a floor above the ambient noise peaks we want to ignore.
    _minTransientPeakStrength = value;
}

void AudioOnsetDetector::setDiagnosticsEnabled(bool enabled) {
    _diagnosticsEnabled = enabled;
}
