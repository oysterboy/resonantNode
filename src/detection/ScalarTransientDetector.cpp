#include "ScalarTransientDetector.h"

#include <Arduino.h>

ScalarTransientDetector::ScalarTransientDetector() = default;

void ScalarTransientDetector::begin() {
    resetState();
    _statsStartUs = micros();
    _lastStatsPrintUs = 0;
    _peakAcceptedCount = 0;
}

void ScalarTransientDetector::resetState() {
    _onsetDetected = false;
    _onsetStrength = 0.0f;
    _lastOnsetUs = 0;
    _lastOnsetRejectReason = OnsetRejectReason::None;

    _transientDetected = false;
    _transientStrength = 0.0f;
    _transientDurationMs = 0;
    _lastTransientRejectReason = TransientRejectReason::None;
    _lastTransientRejectedDurationMs = 0;
    _lastTransientRejectedStrength = 0.0f;
    _peakActive = false;
    _peakStartedUs = 0;
    _releaseCandidateStartedUs = 0;
    _peakStrength = 0.0f;
    _onsetRejectedCount = 0;
    _transientRejectedCount = 0;
}

void ScalarTransientDetector::update(float signalMagnitude, uint32_t sampleTimeUs) {
    const unsigned long nowUs = sampleTimeUs;
    _onsetDetected = false;
    _onsetStrength = 0.0f;

    _transientDetected = false;
    _transientStrength = 0.0f;

    const bool aboveAttackThreshold = signalMagnitude > _onsetDetectionThreshold;
    const bool aboveReleaseThreshold = signalMagnitude > _onsetReleaseThreshold;
    const unsigned long cooldownAfterOnsetUs = _cooldownAfterOnsetMs * 1000UL;
    const bool onsetCooldownElapsed = nowUs - _lastOnsetUs >= cooldownAfterOnsetUs;

    updateOnsetStage(nowUs, signalMagnitude, aboveAttackThreshold, onsetCooldownElapsed);
    updateTransientStage(nowUs, signalMagnitude, aboveReleaseThreshold);
    printTransientStatsIfDue(nowUs);
}

void ScalarTransientDetector::updateOnsetStage(unsigned long nowUs, float signalMagnitude, bool aboveAttackThreshold, bool onsetCooldownElapsed) {
    // Use raw magnitude for the edge so short bursts are not delayed by smoothing.
    // The separate release threshold keeps the peak stable when the signal wobbles near the edge.
    if (aboveAttackThreshold && !_peakActive && onsetCooldownElapsed) {
        _peakActive = true;
        _peakStartedUs = nowUs;
        _peakStrength = signalMagnitude;

        _onsetDetected = true;
        _onsetStrength = signalMagnitude;
        _lastOnsetUs = nowUs;
        _lastOnsetRejectReason = OnsetRejectReason::None;
    } else if (!aboveAttackThreshold) {
        _lastOnsetRejectReason = OnsetRejectReason::BelowThreshold;
    } else if (_peakActive) {
        _lastOnsetRejectReason = OnsetRejectReason::PeakActive;
        _onsetRejectedCount++;
    } else if (!onsetCooldownElapsed) {
        _lastOnsetRejectReason = OnsetRejectReason::CooldownActive;
        _onsetRejectedCount++;
    }
}

void ScalarTransientDetector::updateTransientStage(unsigned long nowUs, float signalMagnitude, bool aboveReleaseThreshold) {
    if (_peakActive && signalMagnitude > _peakStrength) {
        _peakStrength = signalMagnitude;
    }

    // Ignore brief dips below the release threshold so one burst does not get
    // chopped into multiple timing buckets by ADC/loop quantization.
    if (_peakActive) {
        if (!aboveReleaseThreshold) {
            if (_releaseCandidateStartedUs == 0) {
                _releaseCandidateStartedUs = nowUs;
            }
        } else {
            _releaseCandidateStartedUs = 0;
        }
    }

    // Close the peak only after the signal has stayed below the release
    // threshold for long enough to count as a real end of burst.
    const unsigned long releaseDebounceUs = _releaseDebounceMs * 1000UL;
    if (_peakActive && _releaseCandidateStartedUs != 0 && nowUs - _releaseCandidateStartedUs >= releaseDebounceUs) {
        const unsigned long peakDurationUs = nowUs - _peakStartedUs;
        const unsigned long minTransientDurationUs = _minTransientDurationMs * 1000UL;
        const unsigned long maxTransientDurationUs = _maxTransientDurationMs * 1000UL;
        const bool durationAccepted = peakDurationUs >= minTransientDurationUs && peakDurationUs <= maxTransientDurationUs;
        // Duration alone is not enough: weak ambient crossings can still last
        // long enough to look valid, so require a minimum peak strength too.
        const bool strengthAccepted = _peakStrength >= _minTransientPeakStrength;
        const bool accepted = durationAccepted && strengthAccepted;

        if (accepted) {
            _peakAcceptedCount++;
            _transientDetected = true;
            _transientStrength = _peakStrength;
            _transientDurationMs = peakDurationUs / 1000UL;
            _lastTransientRejectReason = TransientRejectReason::None;
            _lastTransientRejectedDurationMs = 0;
            _lastTransientRejectedStrength = 0.0f;
        } else {
            _lastTransientRejectedDurationMs = peakDurationUs / 1000UL;
            _lastTransientRejectedStrength = _peakStrength;
            _transientRejectedCount++;
            if (!durationAccepted) {
                _lastTransientRejectReason = peakDurationUs < minTransientDurationUs
                                                ? TransientRejectReason::DurationTooShort
                                                : TransientRejectReason::DurationTooLong;
                if (_lastTransientRejectReason == TransientRejectReason::DurationTooShort) {
                    _transientRejectedDurationTooShortCount++;
                } else {
                    _transientRejectedDurationTooLongCount++;
                }
            } else if (!strengthAccepted) {
                _lastTransientRejectReason = TransientRejectReason::StrengthTooLow;
                _transientRejectedStrengthTooLowCount++;
            } else {
                _lastTransientRejectReason = TransientRejectReason::None;
            }
        }

        _peakActive = false;
        _peakStartedUs = 0;
        _releaseCandidateStartedUs = 0;
        _peakStrength = 0.0f;
    }
}

void ScalarTransientDetector::printTransientStatsIfDue(unsigned long nowUs) {
    if (!_diagnosticsEnabled || !AUDIO_VERBOSE_DEBUG) {
        return;
    }

    if (_diagnosticsLabel == nullptr) {
        _diagnosticsLabel = "EVT";
    }

    if (_lastStatsPrintUs == 0 || nowUs - _lastStatsPrintUs >= _statsPrintIntervalMs * 1000UL) {
        const unsigned long elapsedMs = (nowUs - _statsStartUs) / 1000UL;
        const unsigned long expectedCount = (elapsedMs + (_expectedTransientPeriodMs / 2)) / _expectedTransientPeriodMs;
        const unsigned long successRate = expectedCount > 0 ? ((_peakAcceptedCount * 100UL) / expectedCount) : 0;

        Serial.print(_diagnosticsLabel);
        Serial.print(" transient success t=");
        Serial.print(nowUs / 1000UL);
        Serial.print(" accepted=");
        Serial.print(_peakAcceptedCount);
        Serial.print(" expected=");
        Serial.print(expectedCount);
        Serial.print(" success=");
        Serial.print(successRate);
        Serial.println("%");

        _lastStatsPrintUs = nowUs;
    }
}

bool ScalarTransientDetector::onsetDetected() const {
    return _onsetDetected;
}

float ScalarTransientDetector::onsetStrength() const {
    return _onsetStrength;
}

const char* ScalarTransientDetector::lastOnsetRejectReasonName() const {
    switch (_lastOnsetRejectReason) {
        case OnsetRejectReason::None:
            return "none";
        case OnsetRejectReason::BelowThreshold:
            return "below_threshold";
        case OnsetRejectReason::CooldownActive:
            return "cooldown_active";
        case OnsetRejectReason::PeakActive:
            return "peak_active";
    }

    return "none";
}

bool ScalarTransientDetector::transientDetected() const {
    return _transientDetected;
}

float ScalarTransientDetector::transientStrength() const {
    return _transientStrength;
}

unsigned long ScalarTransientDetector::transientDurationMs() const {
    return _transientDurationMs;
}

bool ScalarTransientDetector::peakActive() const {
    return _peakActive;
}

float ScalarTransientDetector::peakStrength() const {
    return _peakStrength;
}

const char* ScalarTransientDetector::lastTransientRejectReasonName() const {
    switch (_lastTransientRejectReason) {
        case TransientRejectReason::None:
            return "none";
        case TransientRejectReason::DurationTooShort:
            return "duration_too_short";
        case TransientRejectReason::DurationTooLong:
            return "duration_too_long";
        case TransientRejectReason::StrengthTooLow:
            return "strength_too_low";
        case TransientRejectReason::PeakStillActive:
            return "peak_still_active";
    }

    return "none";
}

unsigned long ScalarTransientDetector::lastTransientRejectedDurationMs() const {
    return _lastTransientRejectedDurationMs;
}

float ScalarTransientDetector::lastTransientRejectedStrength() const {
    return _lastTransientRejectedStrength;
}

unsigned long ScalarTransientDetector::onsetRejectedCount() const {
    return _onsetRejectedCount;
}

unsigned long ScalarTransientDetector::transientRejectedCount() const {
    return _transientRejectedCount;
}

unsigned long ScalarTransientDetector::transientRejectedDurationTooShortCount() const {
    return _transientRejectedDurationTooShortCount;
}

unsigned long ScalarTransientDetector::transientRejectedDurationTooLongCount() const {
    return _transientRejectedDurationTooLongCount;
}

unsigned long ScalarTransientDetector::transientRejectedStrengthTooLowCount() const {
    return _transientRejectedStrengthTooLowCount;
}

float ScalarTransientDetector::onsetDetectionThreshold() const {
    return _onsetDetectionThreshold;
}

float ScalarTransientDetector::onsetReleaseThreshold() const {
    return _onsetReleaseThreshold;
}

unsigned long ScalarTransientDetector::cooldownAfterOnsetMs() const {
    return _cooldownAfterOnsetMs;
}

unsigned long ScalarTransientDetector::minTransientDurationMs() const {
    return _minTransientDurationMs;
}

unsigned long ScalarTransientDetector::maxTransientDurationMs() const {
    return _maxTransientDurationMs;
}

float ScalarTransientDetector::minTransientPeakStrength() const {
    return _minTransientPeakStrength;
}

unsigned long ScalarTransientDetector::releaseDebounceMs() const {
    return _releaseDebounceMs;
}

void ScalarTransientDetector::setOnsetDetectionThreshold(float value) {
    _onsetDetectionThreshold = value;
}

void ScalarTransientDetector::setOnsetReleaseThreshold(float value) {
    // Keep the release threshold below the attack threshold, but close enough
    // that the peak closes promptly once the burst really starts to decay.
    _onsetReleaseThreshold = value;
}

void ScalarTransientDetector::setCooldownAfterOnsetMs(unsigned long value) {
    _cooldownAfterOnsetMs = value;
}

void ScalarTransientDetector::setReleaseDebounceMs(unsigned long value) {
    // A small debounce makes the release edge less sensitive to one-sample dips.
    _releaseDebounceMs = value;
}

void ScalarTransientDetector::setMinTransientDurationMs(unsigned long value) {
    _minTransientDurationMs = value;
}

void ScalarTransientDetector::setMaxTransientDurationMs(unsigned long value) {
    _maxTransientDurationMs = value;
}

void ScalarTransientDetector::setMinTransientPeakStrength(float value) {
    // Set a floor above the ambient noise peaks we want to ignore.
    _minTransientPeakStrength = value;
}

void ScalarTransientDetector::setDiagnosticsEnabled(bool enabled) {
    _diagnosticsEnabled = enabled;
}

void ScalarTransientDetector::setDiagnosticsLabel(const char* value) {
    _diagnosticsLabel = value == nullptr ? "EVT" : value;
}
