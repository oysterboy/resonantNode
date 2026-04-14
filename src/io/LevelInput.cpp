#include "io/LevelInput.h"
#include <Arduino.h>

LevelInput::LevelInput(AnalogInHal& input)
    : _input(input) {}

void LevelInput::begin() {
    long sum = 0;
    for (int i = 0; i < 200; i++) {
        sum += _input.readRaw();
        delay(2);
    }
    _baseline = sum / 200.0f;
    _smoothedSignalMagnitude = 0.0f;
    _activityLevel = 0.0f;
    _lastDetectionMs = 0;
}

void LevelInput::update() {
    const unsigned long now = millis();

    _rawSignal = _input.readRaw();
    _centeredSignal = _rawSignal - (int)_baseline;
    int magnitude = abs(_centeredSignal);

    // baseline follows only quiet state
    if (magnitude < _baselineTrackingQuietThreshold) {
        _baseline = _baseline * 0.995f + _rawSignal * 0.005f;
    }

    // noise gate
    if (magnitude < _baselineTrackingQuietThreshold) {
        magnitude = 0;
    }

    _signalMagnitude = magnitude;

    // smoothing
    _smoothedSignalMagnitude = _smoothedSignalMagnitude * 0.5f + _signalMagnitude * 0.5f;

    const bool detectionThresholdCrossed = _signalMagnitude > _signalMagnitudeDetectionThreshold;
    const bool detectionCooldownElapsed = now - _lastDetectionMs >= _cooldownAfterDetectMs;

    
    if (detectionThresholdCrossed && detectionCooldownElapsed) {
        _activityLevel += _activityRisePerDetection;
        _lastDetectionMs = now;
    }

    _activityLevel *= _activityDecayFactor;

    if (_activityLevel > 1.0f) _activityLevel = 1.0f;
    if (_activityLevel < 0.001f) _activityLevel = 0.0f;
}

int LevelInput::rawSignal() const {
    return _rawSignal;
}

int LevelInput::centeredSignal() const {
    return _centeredSignal;
}

int LevelInput::signalMagnitude() const {
    return _signalMagnitude;
}

int LevelInput::smoothedSignalMagnitude() const {
    return (int)_smoothedSignalMagnitude;
}

float LevelInput::activityLevel() const {
    return _activityLevel;
}

bool LevelInput::activityPresent() const {
    return _activityLevel > _activityPresentThreshold;
}
