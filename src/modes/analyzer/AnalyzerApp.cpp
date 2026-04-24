#include "AnalyzerApp.h"

#include <Arduino.h>

AnalyzerApp::AnalyzerApp(int inputPin, AudioSourceKind sourceKind)
    : _inputPin(inputPin),
      _analogSource(inputPin),
      _i2sSource(14, 27, 33, 16000, 32),
      _sourceKind(sourceKind),
      _audioSource(sourceKind == AudioSourceKind::I2S
                       ? static_cast<AudioSource&>(_i2sSource)
                       : static_cast<AudioSource&>(_analogSource)),
      _audioSignal(_audioSource),
      _audioOnsetDetector(_audioSignal) {}

void AnalyzerApp::begin() {
    configureParameters();
    _audioSource.begin();
    _audioSignal.begin();
    _audioOnsetDetector.begin();
    _lastPrintMs = 0;
}

void AnalyzerApp::configureParameters() {
    configureSharedParameters();

    if (_sourceKind == AudioSourceKind::I2S) {
        configureI2SParameters();
    } else {
        configureAnalogParameters();
    }
}

void AnalyzerApp::configureSharedParameters() {
    _audioSignal.setSmoothingFactor(0.5f);
    _audioSignal.setBaselineUpdateFactor(0.005f);
}

void AnalyzerApp::configureAnalogParameters() {
    _audioSignal.setBaselineTrackingQuietThreshold(40);
    _audioOnsetDetector.setOnsetDetectionThreshold(75.0f);
    _audioOnsetDetector.setOnsetReleaseThreshold(68.0f);
    _audioOnsetDetector.setCooldownAfterOnsetMs(50);
    _audioOnsetDetector.setReleaseDebounceMs(20);
    _audioOnsetDetector.setMinTransientDurationMs(50);
    _audioOnsetDetector.setMaxTransientDurationMs(190);
    _audioOnsetDetector.setMinTransientPeakStrength(180.0f);
}

void AnalyzerApp::configureI2SParameters() {
    _audioSignal.setBaselineTrackingQuietThreshold(20);
    _audioOnsetDetector.setOnsetDetectionThreshold(20.0f);
    _audioOnsetDetector.setOnsetReleaseThreshold(16.0f);
    _audioOnsetDetector.setCooldownAfterOnsetMs(50);
    _audioOnsetDetector.setReleaseDebounceMs(15);
    _audioOnsetDetector.setMinTransientDurationMs(40);
    _audioOnsetDetector.setMaxTransientDurationMs(190);
    _audioOnsetDetector.setMinTransientPeakStrength(35.0f);
}

void AnalyzerApp::update() {
    const unsigned long now = millis();

    _audioSignal.update();
    _audioOnsetDetector.update(now);

    printValues(now);
}

void AnalyzerApp::printValues(unsigned long now) {
    if (_lastPrintMs != 0 && now - _lastPrintMs < kPrintIntervalMs) {
        return;
    }

    _lastPrintMs = now;

    Serial.print("ANL t=");
    Serial.print(now);
    Serial.print(" raw=");
    Serial.print(_audioSignal.rawSignal());
    Serial.print(" centered=");
    Serial.print(_audioSignal.centeredSignal());
    Serial.print(" magnitude=");
    Serial.print(_audioSignal.signalMagnitude());
    Serial.print(" smooth=");
    Serial.print(_audioSignal.smoothedSignalMagnitude(), 3);
    Serial.print(" onset=");
    Serial.print(_audioOnsetDetector.onsetDetected() ? 1 : 0);
    Serial.print(" onsetStrength=");
    Serial.print(_audioOnsetDetector.onsetStrength(), 3);
    Serial.print(" transient=");
    Serial.print(_audioOnsetDetector.transientDetected() ? 1 : 0);
    Serial.print(" transientStrength=");
    Serial.print(_audioOnsetDetector.transientStrength(), 3);
    Serial.print(" transientMs=");
    Serial.println(_audioOnsetDetector.transientDurationMs());
}
