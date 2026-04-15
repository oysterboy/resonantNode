#include "io/AudioSignal.h"
#include <Arduino.h>

AudioSignal::AudioSignal(AudioSource& source)
    : _source(source) {}

void AudioSignal::begin() {
    long sum = 0;
    for (int i = 0; i < 200; i++) {
        sum += _source.readSample();
        delay(2);
    }
    _baseline = sum / 200.0f;
    _smoothedSignalMagnitude = 0.0f;
}

void AudioSignal::update() {
    _rawSignal = _source.readSample();
    _centeredSignal = _rawSignal - (int)_baseline;
    int magnitude = abs(_centeredSignal);

    // baseline follows only quiet state
    if (magnitude < _baselineTrackingQuietThreshold) {
        _baseline = _baseline * (1.0f - _baselineUpdateFactor) + _rawSignal * _baselineUpdateFactor;
    }

    // noise gate
    if (magnitude < _baselineTrackingQuietThreshold) {
        magnitude = 0;
    }

    _signalMagnitude = magnitude;

    // smoothing
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

int AudioSignal::centeredSignal() const {
    return _centeredSignal;
}

int AudioSignal::signalMagnitude() const {
    return _signalMagnitude;
}

int AudioSignal::smoothedSignalMagnitude() const {
    return (int)_smoothedSignalMagnitude;
}
