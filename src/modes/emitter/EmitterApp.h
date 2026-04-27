#pragma once

#include "../../hal/PiezoToneOutputBTL.h"
#include "../../hal/PiezoToneOutput.h"
#include "../../io/ChirpOutput.h"

class EmitterApp {
public:
    enum class EmitterMode {
        Auto,
        RemoteControl,
        Sweep
    };

    enum class ControlSerialKind {
        Serial2
    };

    EmitterApp(int outputPin = 25,
               int rxPin = 16,
               int txPin = 17,
               unsigned long baudRate = 115200);
    EmitterApp(int outputPin,
               int outputBtlPin,
               int rxPin,
               int txPin,
               unsigned long baudRate);

    void begin();
    void update();

private:
    void pollControlSerial();
    void handleLine(const char* line);
    void handleChirpCommand(const char* line);
    void handleModeCommand(const char* line);
    void handleSweepCommand(const char* line);
    void startChirp(unsigned long toneHz, unsigned long durationMs);
    void setMode(EmitterMode mode);
    void configureAuto(unsigned long intervalMs, unsigned long toneHz, unsigned long durationMs);
    void configureSweep(unsigned long startHz, unsigned long stopHz, unsigned long stepHz, unsigned long durationMs, unsigned long pauseMs);
    void advanceSweep(unsigned long now);
    const char* modeName() const;

    int _outputPin;
    int _outputBtlPin;
    int _rxPin;
    int _txPin;
    unsigned long _baudRate;
    EmitterMode _mode = EmitterMode::Auto;
    unsigned long _autoIntervalMs = 2000;
    unsigned long _autoToneHz = 3200;
    unsigned long _autoDurationMs = 100;
    unsigned long _nextAutoChirpAtMs = 0;
    unsigned long _sweepStartHz = 3000;
    unsigned long _sweepStopHz = 3500;
    unsigned long _sweepStepHz = 100;
    unsigned long _sweepDurationMs = 80;
    unsigned long _sweepPauseMs = 1000;
    unsigned long _sweepCurrentHz = 1800;
    unsigned long _nextSweepStepAtMs = 0;
    PiezoToneOutput _toneOutput;
    PiezoToneOutputBTL _toneOutputBTL;
    ChirpOutput _chirpOutput;
    char _lineBuffer[96];
    size_t _lineLength = 0;
};
