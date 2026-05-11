#include "io/AudioFrequencyDetector.h"

#include <Arduino.h>
#include <math.h>

// -----------------------------------------------------------------------------
// Lifecycle
// -----------------------------------------------------------------------------

AudioFrequencyDetector::AudioFrequencyDetector(AudioSignal& audioSignal)
    : _audioSignal(audioSignal) {}

void AudioFrequencyDetector::begin() {
    resetState();
    _statsStartMs = millis();
    _lastStatsPrintMs = 0;
    _peakAcceptedCount = 0;
}

void AudioFrequencyDetector::resetState() {
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

    _sampleCount = 0;
    _sampleWriteIndex = 0;
    _lastFrequencyScore = 0.0f;
    _lastTargetPower = 0.0f;
    _lastNeighborPower = 0.0f;
    _lastTotalEnergy = 0.0f;
    _lastSpectralContrast = 0.0f;
    for (unsigned long i = 0; i < kMaxWindowSizeSamples; ++i) {
        _sampleBuffer[i] = 0;
    }
}

// -----------------------------------------------------------------------------
// Runtime update and sample window management
// -----------------------------------------------------------------------------

void AudioFrequencyDetector::update(unsigned long now) {
    _onsetDetected = false;
    _onsetStrength = 0.0f;

    _transientDetected = false;
    _transientStrength = 0.0f;

    observeCenteredSample(_audioSignal.centeredSignal());
    if (_sampleCount >= _windowSizeSamples) {
        const float score = _lastFrequencyScore;
        const bool aboveAttackThreshold = score > _onsetDetectionThreshold;
        const bool aboveReleaseThreshold = score > _onsetReleaseThreshold;
        const bool onsetCooldownElapsed = now - _lastOnsetMs >= _cooldownAfterOnsetMs;

        updateOnsetStage(now, score, aboveAttackThreshold, onsetCooldownElapsed);
        updateTransientStage(now, score, aboveReleaseThreshold);
        printTransientStatsIfDue(now);
    }
}

void AudioFrequencyDetector::observeCenteredSample(int centeredSample) {
    pushSample(centeredSample);
    if (_sampleCount < _windowSizeSamples) {
        _lastFrequencyScore = 0.0f;
        _lastTargetPower = 0.0f;
        _lastNeighborPower = 0.0f;
        _lastTotalEnergy = 0.0f;
        _lastSpectralContrast = 0.0f;
        return;
    }

    computeFrequencyScore();
}

// -----------------------------------------------------------------------------
// Frequency score helpers
// -----------------------------------------------------------------------------

void AudioFrequencyDetector::pushSample(int sample) {
    if (_windowSizeSamples == 0) {
        return;
    }

    _sampleBuffer[_sampleWriteIndex] = sample;
    _sampleWriteIndex = (_sampleWriteIndex + 1) % _windowSizeSamples;
    if (_sampleCount < _windowSizeSamples) {
        _sampleCount++;
    }
}

float AudioFrequencyDetector::computeGoertzelPowerAtFrequency(float frequencyHz) const {
    if (_windowSizeSamples == 0 || _sampleCount < _windowSizeSamples) {
        return 0.0f;
    }

    const float omega = 2.0f * PI * frequencyHz / static_cast<float>(_sampleRateHz);
    const float coeff = 2.0f * cosf(omega);

    float sPrev = 0.0f;
    float sPrev2 = 0.0f;

    const unsigned long startIndex = _sampleWriteIndex;
    for (unsigned long i = 0; i < _windowSizeSamples; ++i) {
        const unsigned long index = (startIndex + i) % _windowSizeSamples;
        const float sample = static_cast<float>(_sampleBuffer[index]);

        const float s = sample + coeff * sPrev - sPrev2;
        sPrev2 = sPrev;
        sPrev = s;
    }

    return sPrev2 * sPrev2 + sPrev * sPrev - coeff * sPrev * sPrev2;
}

float AudioFrequencyDetector::computeFrequencyScore() {
    if (_windowSizeSamples == 0 || _sampleCount < _windowSizeSamples) {
        _lastFrequencyScore = 0.0f;
        _lastTargetPower = 0.0f;
        _lastNeighborPower = 0.0f;
        _lastTotalEnergy = 0.0f;
        _lastSpectralContrast = 0.0f;
        return 0.0f;
    }

    float totalEnergy = 0.0f;
    const unsigned long startIndex = _sampleWriteIndex;
    for (unsigned long i = 0; i < _windowSizeSamples; ++i) {
        const unsigned long index = (startIndex + i) % _windowSizeSamples;
        const float sample = static_cast<float>(_sampleBuffer[index]);
        totalEnergy += sample * sample;
    }

    const float targetPower = computeGoertzelPowerAtFrequency(static_cast<float>(_targetFrequencyHz));
    const float binSpacingHz = frequencyBinSpacingHz();
    const float lowerFrequency = _targetFrequencyHz > static_cast<unsigned long>(binSpacingHz)
        ? static_cast<float>(_targetFrequencyHz) - binSpacingHz
        : static_cast<float>(_targetFrequencyHz) * 0.5f;
    const float upperFrequency = static_cast<float>(_targetFrequencyHz) + binSpacingHz;
    const float lowerPower = computeGoertzelPowerAtFrequency(lowerFrequency < 1.0f ? 1.0f : lowerFrequency);
    const float upperPower = computeGoertzelPowerAtFrequency(upperFrequency);
    const float neighborPower = (lowerPower + upperPower) * 0.5f;
    const float normalized = (targetPower * 1000.0f) / (totalEnergy + 1.0f);
    const float contrast = targetPower / (neighborPower + 1.0f);

    _lastFrequencyScore = normalized;
    _lastTargetPower = targetPower;
    _lastNeighborPower = neighborPower;
    _lastTotalEnergy = totalEnergy;
    _lastSpectralContrast = contrast;

    return normalized;
}

// -----------------------------------------------------------------------------
// Onset and transient stages
// -----------------------------------------------------------------------------

void AudioFrequencyDetector::updateOnsetStage(unsigned long now, float score, bool aboveAttackThreshold, bool onsetCooldownElapsed) {
    if (aboveAttackThreshold && !_peakActive && onsetCooldownElapsed) {
        _peakActive = true;
        _peakStartedMs = now;
        _peakStrength = score;

        _onsetDetected = true;
        _onsetStrength = score;
        _lastOnsetMs = now;
    }
}

void AudioFrequencyDetector::updateTransientStage(unsigned long now, float score, bool aboveReleaseThreshold) {
    if (_peakActive && score > _peakStrength) {
        _peakStrength = score;
    }

    if (_peakActive) {
        if (!aboveReleaseThreshold) {
            if (_releaseCandidateStartedMs == 0) {
                _releaseCandidateStartedMs = now;
            }
        } else {
            _releaseCandidateStartedMs = 0;
        }
    }

    if (_peakActive && _releaseCandidateStartedMs != 0 && now - _releaseCandidateStartedMs >= _releaseDebounceMs) {
        const unsigned long peakDurationMs = now - _peakStartedMs;
        const bool durationAccepted = peakDurationMs >= _minTransientDurationMs && peakDurationMs <= _maxTransientDurationMs;
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

// -----------------------------------------------------------------------------
// Diagnostics
// -----------------------------------------------------------------------------

void AudioFrequencyDetector::printTransientStatsIfDue(unsigned long now) {
    if (!_diagnosticsEnabled || !AUDIO_VERBOSE_DEBUG) {
        return;
    }

    if (_lastStatsPrintMs == 0 || now - _lastStatsPrintMs >= _statsPrintIntervalMs) {
        const unsigned long elapsedMs = now - _statsStartMs;
        const unsigned long expectedCount = (elapsedMs + (_expectedTransientPeriodMs / 2)) / _expectedTransientPeriodMs;
        const unsigned long successRate = expectedCount > 0 ? ((_peakAcceptedCount * 100UL) / expectedCount) : 0;

        Serial.print("EVT freq transient success t=");
        Serial.print(now);
        Serial.print(" accepted=");
        Serial.print(_peakAcceptedCount);
        Serial.print(" expected=");
        Serial.print(expectedCount);
        Serial.print(" success=");
        Serial.print(successRate);
        Serial.print("% score=");
        Serial.print(_lastFrequencyScore, 1);
        Serial.print(" target_power=");
        Serial.print(_lastTargetPower, 1);
        Serial.print(" neighbor_power=");
        Serial.print(_lastNeighborPower, 1);
        Serial.print(" contrast=");
        Serial.print(_lastSpectralContrast, 2);
        Serial.print(" energy=");
        Serial.println(_lastTotalEnergy, 1);

        _lastStatsPrintMs = now;
    }
}

// -----------------------------------------------------------------------------
// Inspection and tuning
// -----------------------------------------------------------------------------

bool AudioFrequencyDetector::onsetDetected() const {
    return _onsetDetected;
}

float AudioFrequencyDetector::onsetStrength() const {
    return _onsetStrength;
}

bool AudioFrequencyDetector::transientDetected() const {
    return _transientDetected;
}

float AudioFrequencyDetector::transientStrength() const {
    return _transientStrength;
}

unsigned long AudioFrequencyDetector::transientDurationMs() const {
    return _transientDurationMs;
}

float AudioFrequencyDetector::lastFrequencyScore() const {
    return _lastFrequencyScore;
}

float AudioFrequencyDetector::lastTargetPower() const {
    return _lastTargetPower;
}

float AudioFrequencyDetector::lastNeighborPower() const {
    return _lastNeighborPower;
}

float AudioFrequencyDetector::lastTotalEnergy() const {
    return _lastTotalEnergy;
}

float AudioFrequencyDetector::lastSpectralContrast() const {
    return _lastSpectralContrast;
}

float AudioFrequencyDetector::frequencyBinSpacingHz() const {
    if (_windowSizeSamples == 0) {
        return 0.0f;
    }
    return static_cast<float>(_sampleRateHz) / static_cast<float>(_windowSizeSamples);
}

float AudioFrequencyDetector::onsetDetectionThreshold() const {
    return _onsetDetectionThreshold;
}

float AudioFrequencyDetector::onsetReleaseThreshold() const {
    return _onsetReleaseThreshold;
}

unsigned long AudioFrequencyDetector::cooldownAfterOnsetMs() const {
    return _cooldownAfterOnsetMs;
}

unsigned long AudioFrequencyDetector::minTransientDurationMs() const {
    return _minTransientDurationMs;
}

unsigned long AudioFrequencyDetector::maxTransientDurationMs() const {
    return _maxTransientDurationMs;
}

float AudioFrequencyDetector::minTransientPeakStrength() const {
    return _minTransientPeakStrength;
}

unsigned long AudioFrequencyDetector::releaseDebounceMs() const {
    return _releaseDebounceMs;
}

unsigned long AudioFrequencyDetector::targetFrequencyHz() const {
    return _targetFrequencyHz;
}

unsigned long AudioFrequencyDetector::sampleRateHz() const {
    return _sampleRateHz;
}

unsigned long AudioFrequencyDetector::windowSizeSamples() const {
    return _windowSizeSamples;
}

void AudioFrequencyDetector::setOnsetDetectionThreshold(float value) {
    _onsetDetectionThreshold = value;
}

void AudioFrequencyDetector::setOnsetReleaseThreshold(float value) {
    _onsetReleaseThreshold = value;
}

void AudioFrequencyDetector::setCooldownAfterOnsetMs(unsigned long value) {
    _cooldownAfterOnsetMs = value;
}

void AudioFrequencyDetector::setReleaseDebounceMs(unsigned long value) {
    _releaseDebounceMs = value;
}

void AudioFrequencyDetector::setMinTransientDurationMs(unsigned long value) {
    _minTransientDurationMs = value;
}

void AudioFrequencyDetector::setMaxTransientDurationMs(unsigned long value) {
    _maxTransientDurationMs = value;
}

void AudioFrequencyDetector::setMinTransientPeakStrength(float value) {
    _minTransientPeakStrength = value;
}

void AudioFrequencyDetector::setTargetFrequencyHz(unsigned long value) {
    _targetFrequencyHz = value;
}

void AudioFrequencyDetector::setSampleRateHz(unsigned long value) {
    _sampleRateHz = value == 0 ? 1 : value;
}

void AudioFrequencyDetector::setWindowSizeSamples(unsigned long value) {
    if (value == 0) {
        value = 1;
    }
    if (value > kMaxWindowSizeSamples) {
        value = kMaxWindowSizeSamples;
    }
    _windowSizeSamples = value;
}

void AudioFrequencyDetector::setDiagnosticsEnabled(bool enabled) {
    _diagnosticsEnabled = enabled;
}
