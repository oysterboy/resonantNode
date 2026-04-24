#pragma once

#include "../../hal/PiezoToneOutput.h"
#include "../../io/ChirpOutput.h"

class EmitterApp {
public:
    enum class ControlSerialKind {
        Serial2
    };

    EmitterApp(int outputPin = 25,
               int rxPin = 16,
               int txPin = 17,
               unsigned long baudRate = 115200);

    void begin();
    void update();

private:
    void pollControlSerial();
    void handleLine(const char* line);
    void startChirp(unsigned long toneHz, unsigned long durationMs);

    int _outputPin;
    int _rxPin;
    int _txPin;
    unsigned long _baudRate;
    PiezoToneOutput _toneOutput;
    ChirpOutput _chirpOutput;
    char _lineBuffer[96];
    size_t _lineLength = 0;
};
