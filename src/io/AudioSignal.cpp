#include "io/AudioSignal.h"
#include <Arduino.h>

AudioSignal::AudioSignal(AudioSource& source)
    : _source(source) {}

void AudioSignal::begin() {
    rebase();
}

void AudioSignal::rebase() {
    long sum = 0;
    for (int i = 0; i < 200; ) {
        int sample = 0;
        uint32_t sampleTimeUs = 0;
        if (_source.readSample(sample, sampleTimeUs)) {
            sum += sample;
            ++i;
        } else {
            delay(1);
        }
    }
    // Seed the baseline from a short quiet window so the first update is stable.
    _baseline = sum / 200.0f;
    _smoothedSignalMagnitude = 0.0f;
    _rawSignal = 0;
    _centeredSignal = 0;
    _signalMagnitude = 0;
}

void AudioSignal::update(int sample, uint32_t sampleTimeUs) {
    _rawSignal = sample;
    _sampleTimeUs = sampleTimeUs;
    _centeredSignal = _rawSignal - (int)_baseline;
    int magnitude = abs(_centeredSignal);

    // Only let the baseline drift while the signal still looks quiet.
    if (magnitude < _baselineTrackingQuietThreshold) {
        _baseline = _baseline * (1.0f - _baselineUpdateFactor) + _rawSignal * _baselineUpdateFactor;
    }

    // Quiet samples are gated away so downstream detectors work on real activity.
    if (magnitude < _baselineTrackingQuietThreshold) {
        magnitude = 0;
    }

    _signalMagnitude = magnitude;

    // Smooth the gate output so display/debug code is easier to read.
    _smoothedSignalMagnitude = _smoothedSignalMagnitude * (1.0f - _smoothingFactor) + _signalMagnitude * _smoothingFactor;
}

void AudioSignal::setBaselineTrackingQuietThreshold(int value) {
    _baselineTrackingQuietThreshold = value;
}

void AudioSignal::setSmoothingFactor(float value) {
    _smoothingFactor = value;
}

void AudioSignal::setBaselineUpdateFactor(float value) {
    _baselineUpdateFactor = value;
}

int AudioSignal::rawSignal() const {
    return _rawSignal;
}

float AudioSignal::baseline() const {
    return _baseline;
}

int AudioSignal::centeredSignal() const {
    return _centeredSignal;
}

int AudioSignal::signalMagnitude() const {
    return _signalMagnitude;
}

int AudioSignal::smoothedSignalMagnitude() const {
    return (int)_smoothedSignalMagnitude;
}

uint32_t AudioSignal::sampleTimeUs() const {
    return _sampleTimeUs;
}
