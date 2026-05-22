#include "FreqBandStream.h"

#include <Arduino.h>
#include <math.h>

// FreqBandStream maintains the rolling narrow-band frequency evidence stream.
void FreqBandStream::resetState() {
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

void FreqBandStream::setTargetFrequencyHz(unsigned long value) {
    _targetFrequencyHz = value;
}

void FreqBandStream::setSampleRateHz(unsigned long value) {
    _sampleRateHz = value == 0 ? 1 : value;
}

void FreqBandStream::setWindowSizeSamples(unsigned long value) {
    if (value == 0) {
        value = 1;
    }
    if (value > kMaxWindowSizeSamples) {
        value = kMaxWindowSizeSamples;
    }
    _windowSizeSamples = value;
}

void FreqBandStream::observeCenteredSample(int centeredSample) {
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

void FreqBandStream::pushSample(int sample) {
    if (_windowSizeSamples == 0) {
        return;
    }

    _sampleBuffer[_sampleWriteIndex] = sample;
    _sampleWriteIndex = (_sampleWriteIndex + 1) % _windowSizeSamples;
    if (_sampleCount < _windowSizeSamples) {
        _sampleCount++;
    }
}

float FreqBandStream::computeGoertzelPowerAtFrequency(float frequencyHz) const {
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

float FreqBandStream::computeFrequencyScore() {
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

float FreqBandStream::lastFrequencyScore() const {
    return _lastFrequencyScore;
}

float FreqBandStream::lastTargetPower() const {
    return _lastTargetPower;
}

float FreqBandStream::lastNeighborPower() const {
    return _lastNeighborPower;
}

float FreqBandStream::lastTotalEnergy() const {
    return _lastTotalEnergy;
}

float FreqBandStream::lastSpectralContrast() const {
    return _lastSpectralContrast;
}

float FreqBandStream::frequencyBinSpacingHz() const {
    if (_windowSizeSamples == 0) {
        return 0.0f;
    }
    return static_cast<float>(_sampleRateHz) / static_cast<float>(_windowSizeSamples);
}

unsigned long FreqBandStream::targetFrequencyHz() const {
    return _targetFrequencyHz;
}

unsigned long FreqBandStream::sampleRateHz() const {
    return _sampleRateHz;
}

unsigned long FreqBandStream::windowSizeSamples() const {
    return _windowSizeSamples;
}

unsigned long FreqBandStream::sampleCount() const {
    return _sampleCount;
}

bool FreqBandStream::windowReady() const {
    return _sampleCount >= _windowSizeSamples && _windowSizeSamples > 0;
}
