#include "EmitterApp.h"

#include <Arduino.h>
#include <string.h>

namespace {
bool startsWithToken(const char* line, const char* token) {
    return strncmp(line, token, strlen(token)) == 0;
}

const char* findValue(const char* token) {
    const char* equals = strchr(token, '=');
    return equals != nullptr ? equals + 1 : nullptr;
}
}

EmitterApp::EmitterApp(int outputPin, int rxPin, int txPin, unsigned long baudRate)
    : _outputPin(outputPin),
      _rxPin(rxPin),
      _txPin(txPin),
      _baudRate(baudRate),
      _chirpOutput(outputPin) {}

void EmitterApp::begin() {
    Serial2.begin(_baudRate, SERIAL_8N1, _rxPin, _txPin);
    _chirpOutput.begin();
    _chirpOutput.setToneHz(2400);
    _chirpOutput.setTiming(100, 0);
    _lineLength = 0;
    _lineBuffer[0] = '\0';
}

void EmitterApp::update() {
    pollControlSerial();
    _chirpOutput.update();

    if (_chirpOutput.finished()) {
        Serial.println("EVT emitter_chirp_finished");
    }
}

void EmitterApp::pollControlSerial() {
    while (Serial2.available() > 0) {
        const char c = static_cast<char>(Serial2.read());
        if (c == '\r') {
            continue;
        }

        if (c == '\n') {
            _lineBuffer[_lineLength] = '\0';
            if (_lineLength > 0) {
                handleLine(_lineBuffer);
            }
            _lineLength = 0;
            continue;
        }

        if (_lineLength < sizeof(_lineBuffer) - 1) {
            _lineBuffer[_lineLength++] = c;
        }
    }
}

void EmitterApp::handleLine(const char* line) {
    if (!startsWithToken(line, "CHIRP")) {
        return;
    }

    char buffer[96];
    strncpy(buffer, line, sizeof(buffer));
    buffer[sizeof(buffer) - 1] = '\0';

    unsigned long toneHz = 2400;
    unsigned long durationMs = 100;
    char* savePtr = nullptr;
    char* token = strtok_r(buffer, " ", &savePtr);
    while (token != nullptr) {
        if (startsWithToken(token, "freq=")) {
            toneHz = strtoul(findValue(token), nullptr, 10);
        } else if (startsWithToken(token, "dur=")) {
            durationMs = strtoul(findValue(token), nullptr, 10);
        }

        token = strtok_r(nullptr, " ", &savePtr);
    }

    startChirp(toneHz, durationMs);
}

void EmitterApp::startChirp(unsigned long toneHz, unsigned long durationMs) {
    _chirpOutput.setToneHz(static_cast<uint32_t>(toneHz));
    _chirpOutput.setTiming(durationMs, 0);
    _chirpOutput.start();

    Serial.print("EVT emitter_chirp_started freq=");
    Serial.print(toneHz);
    Serial.print(" dur=");
    Serial.print(durationMs);
    Serial.println(" pattern=single");
}
