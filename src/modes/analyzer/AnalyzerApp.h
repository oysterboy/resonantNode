#pragma once

#include "../../hal/AudioSourceAnalog.h"
#include "../../hal/AudioSourceI2S.h"
#include "../../io/AudioOnsetDetector.h"
#include "../../io/AudioSignal.h"
#include "../../hal/AudioSource.h"

class AnalyzerApp {
public:
    enum class AudioSourceKind {
        Analog,
        I2S
    };

    AnalyzerApp(int inputPin = 34, AudioSourceKind sourceKind = AudioSourceKind::I2S);

    void begin();
    void update();

private:
    void configureParameters();
    void configureSharedParameters();
    void configureAnalogParameters();
    void configureI2SParameters();
    void printValues(unsigned long now);

    int _inputPin;
    AudioSourceAnalog _analogSource;
    AudioSourceI2S _i2sSource;
    AudioSource& _audioSource;
    AudioSourceKind _sourceKind;
    AudioSignal _audioSignal;
    AudioOnsetDetector _audioOnsetDetector;
    unsigned long _lastPrintMs = 0;
    static constexpr unsigned long kPrintIntervalMs = 100;
};
