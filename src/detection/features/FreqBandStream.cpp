#include "FreqBandStream.h"

#include <Arduino.h>
#include <math.h>

// FreqBandStream maintains the rolling narrow-band frequency evidence stream.
void FreqBandStream::resetState() {
    _sampleCount = 0;
    _sampleWriteIndex = 0;
    _samplesUntilNextFrequencyUpdate = 0;
    _producedFreshPacketOnLastObserve = false;
    _lastFrequencyScore = 0.0f;
    _lastTargetPower = 0.0f;
    _lastNeighborPower = 0.0f;
    _lastTotalEnergy = 0.0f;
    _lastSpectralContrast = 0.0f;
    for (unsigned long i = 0; i < kMaxWindowSizeSamples; ++i) {
        _sampleBuffer[i] = 0;
    }
    _profileObserveCalls = 0;
    _profileComputeCalls = 0;
    _profileObserveTotalUs = 0;
    _profileComputeTotalUs = 0;
    _profileEnergyTotalUs = 0;
    _profileGoertzelTotalUs = 0;
    updateCachedGoertzelCoefficients();
}

void FreqBandStream::setTargetFrequencyHz(unsigned long value) {
    _targetFrequencyHz = value;
    updateCachedGoertzelCoefficients();
}

void FreqBandStream::setSampleRateHz(unsigned long value) {
    _sampleRateHz = value == 0 ? 1 : value;
    updateCachedGoertzelCoefficients();
}

void FreqBandStream::setWindowSizeSamples(unsigned long value) {
    if (value == 0) {
        value = 1;
    }
    if (value > kMaxWindowSizeSamples) {
        value = kMaxWindowSizeSamples;
    }
    _windowSizeSamples = value;
    _samplesUntilNextFrequencyUpdate = 0;
    updateCachedGoertzelCoefficients();
}

void FreqBandStream::setFrequencyUpdateEverySamples(unsigned long value) {
    if (value == 0) {
        value = 1;
    }
    _frequencyUpdateEverySamples = value;
    _samplesUntilNextFrequencyUpdate = 0;
}

void FreqBandStream::observeCenteredSample(int centeredSample, unsigned long sampleTimeMs) {
    const unsigned long profileStartUs = micros();
    _producedFreshPacketOnLastObserve = false;
    pushSample(centeredSample);
    if (_sampleCount < _windowSizeSamples) {
        _lastFrequencyScore = 0.0f;
        _lastTargetPower = 0.0f;
        _lastNeighborPower = 0.0f;
        _lastTotalEnergy = 0.0f;
        _lastSpectralContrast = 0.0f;
        ++_profileObserveCalls;
        _profileObserveTotalUs += static_cast<unsigned long>(micros() - profileStartUs);
        return;
    }

    if (_frequencyUpdateEverySamples <= 1) {
        computeFrequencyScore();
        _producedFreshPacketOnLastObserve = true;
    } else if (_samplesUntilNextFrequencyUpdate == 0) {
        computeFrequencyScore();
        _samplesUntilNextFrequencyUpdate = _frequencyUpdateEverySamples - 1;
        _producedFreshPacketOnLastObserve = true;
    }
    if (!_producedFreshPacketOnLastObserve) {
        --_samplesUntilNextFrequencyUpdate;
    }
    ++_profileObserveCalls;
    _profileObserveTotalUs += static_cast<unsigned long>(micros() - profileStartUs);
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

void FreqBandStream::updateCachedGoertzelCoefficients() {
    const float sampleRateHz = static_cast<float>(_sampleRateHz == 0 ? 1 : _sampleRateHz);
    const float targetHz = static_cast<float>(_targetFrequencyHz);
    const float binSpacingHz = frequencyBinSpacingHz();
    const float lowerFrequency = _targetFrequencyHz > static_cast<unsigned long>(binSpacingHz)
        ? targetHz - binSpacingHz
        : targetHz * 0.5f;
    const float upperFrequency = targetHz + binSpacingHz;

    _cachedTargetFrequencyHz = targetHz;
    _cachedLowerFrequencyHz = lowerFrequency < 1.0f ? 1.0f : lowerFrequency;
    _cachedUpperFrequencyHz = upperFrequency;
    _cachedTargetCoeff = 2.0f * cosf(2.0f * PI * _cachedTargetFrequencyHz / sampleRateHz);
    _cachedLowerCoeff = 2.0f * cosf(2.0f * PI * _cachedLowerFrequencyHz / sampleRateHz);
    _cachedUpperCoeff = 2.0f * cosf(2.0f * PI * _cachedUpperFrequencyHz / sampleRateHz);
    _cachedGoertzelValid = true;
}

float FreqBandStream::computeGoertzelPowerFromCoeff(float coeff) const {
    if (_windowSizeSamples == 0 || _sampleCount < _windowSizeSamples) {
        return 0.0f;
    }

    const unsigned long profileStartUs = micros();
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

    const float power = sPrev2 * sPrev2 + sPrev * sPrev - coeff * sPrev * sPrev2;
    _profileGoertzelTotalUs += static_cast<unsigned long>(micros() - profileStartUs);
    return power;
}

float FreqBandStream::computeGoertzelPowerAtFrequency(float frequencyHz) const {
    if (!_cachedGoertzelValid) {
        return 0.0f;
    }

    if (frequencyHz == _cachedTargetFrequencyHz) {
        return computeGoertzelPowerFromCoeff(_cachedTargetCoeff);
    }
    if (frequencyHz == _cachedLowerFrequencyHz) {
        return computeGoertzelPowerFromCoeff(_cachedLowerCoeff);
    }
    if (frequencyHz == _cachedUpperFrequencyHz) {
        return computeGoertzelPowerFromCoeff(_cachedUpperCoeff);
    }

    const float sampleRateHz = static_cast<float>(_sampleRateHz == 0 ? 1 : _sampleRateHz);
    const float coeff = 2.0f * cosf(2.0f * PI * frequencyHz / sampleRateHz);
    return computeGoertzelPowerFromCoeff(coeff);
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

    const unsigned long profileStartUs = micros();
    const unsigned long energyStartUs = micros();
    float totalEnergy = 0.0f;
    const unsigned long startIndex = _sampleWriteIndex;
    for (unsigned long i = 0; i < _windowSizeSamples; ++i) {
        const unsigned long index = (startIndex + i) % _windowSizeSamples;
        const float sample = static_cast<float>(_sampleBuffer[index]);
        totalEnergy += sample * sample;
    }
    _profileEnergyTotalUs += static_cast<unsigned long>(micros() - energyStartUs);

    const float targetPower = computeGoertzelPowerAtFrequency(_cachedTargetFrequencyHz);
    const float lowerPower = computeGoertzelPowerAtFrequency(_cachedLowerFrequencyHz);
    const float upperPower = computeGoertzelPowerAtFrequency(_cachedUpperFrequencyHz);
    const float neighborPower = (lowerPower + upperPower) * 0.5f;
    const float normalized = (targetPower * 1000.0f) / (totalEnergy + 1.0f);
    const float contrast = targetPower / (neighborPower + 1.0f);

    _lastFrequencyScore = normalized;
    _lastTargetPower = targetPower;
    _lastNeighborPower = neighborPower;
    _lastTotalEnergy = totalEnergy;
    _lastSpectralContrast = contrast;
    ++_profileComputeCalls;
    _profileComputeTotalUs += static_cast<unsigned long>(micros() - profileStartUs);

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

unsigned long FreqBandStream::frequencyUpdateEverySamples() const {
    return _frequencyUpdateEverySamples;
}

unsigned long FreqBandStream::sampleCount() const {
    return _sampleCount;
}

bool FreqBandStream::windowReady() const {
    return _sampleCount >= _windowSizeSamples && _windowSizeSamples > 0;
}

unsigned long FreqBandStream::profileObserveCalls() const {
    return _profileObserveCalls;
}

unsigned long FreqBandStream::profileComputeCalls() const {
    return _profileComputeCalls;
}

unsigned long FreqBandStream::profileObserveTotalUs() const {
    return _profileObserveTotalUs;
}

unsigned long FreqBandStream::profileComputeTotalUs() const {
    return _profileComputeTotalUs;
}

unsigned long FreqBandStream::profileEnergyTotalUs() const {
    return _profileEnergyTotalUs;
}

unsigned long FreqBandStream::profileGoertzelTotalUs() const {
    return _profileGoertzelTotalUs;
}

bool FreqBandStream::producedFreshPacketOnLastObserve() const {
    return _producedFreshPacketOnLastObserve;
}

unsigned long FreqBandStream::lastPacketAgeSamples() const {
    if (!_producedFreshPacketOnLastObserve && _sampleCount >= _windowSizeSamples && _frequencyUpdateEverySamples > 1) {
        const unsigned long countdown = _samplesUntilNextFrequencyUpdate;
        const unsigned long maxAge = _frequencyUpdateEverySamples > 0 ? _frequencyUpdateEverySamples - 1U : 0U;
        return countdown <= maxAge ? (maxAge - countdown) : 0UL;
    }
    return 0UL;
}
