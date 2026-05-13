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

char* nextToken(char*& savePtr) {
    return strtok_r(nullptr, " ", &savePtr);
}

void sendControlAck(const char* line) {
    Serial2.println(line);
}
}

EmitterApp::EmitterApp(int outputPin, int rxPin, int txPin, unsigned long baudRate)
    : EmitterApp(outputPin, -1, rxPin, txPin, baudRate) {}

EmitterApp::EmitterApp(int outputPin, int outputBtlPin, int rxPin, int txPin, unsigned long baudRate)
    : _outputPin(outputPin),
      _outputBtlPin(outputBtlPin),
      _rxPin(rxPin),
      _txPin(txPin),
      _baudRate(baudRate),
      _toneOutput(outputPin),
      _toneOutputBTL(outputPin, outputBtlPin),
      _chirpOutput(outputBtlPin >= 0
                       ? static_cast<ToneOutput&>(_toneOutputBTL)
                       : static_cast<ToneOutput&>(_toneOutput)) {}

void EmitterApp::begin() {
    Serial2.begin(_baudRate, SERIAL_8N1, _rxPin, _txPin);
    _chirpOutput.begin();
    configureAuto(_autoIntervalMs, _autoToneHz, _autoDurationMs);
    setMode(EmitterMode::Auto);
    _nextAutoChirpAtMs = millis();
    _lineLength = 0;
    _lineBuffer[0] = '\0';

    Serial.print("EVT emitter_ready mode=");
    Serial.print(modeName());
    Serial.print(" interval_ms=");
    Serial.print(_autoIntervalMs);
    Serial.print(" freq_hz=");
    Serial.print(_autoToneHz);
    Serial.print(" dur_ms=");
    Serial.println(_autoDurationMs);

}

void EmitterApp::update() {
    pollControlSerial();
    _chirpOutput.update();

    if (_chirpOutput.finished()) {
        Serial.println("EVT emitter_chirp_finished");
    }

    if (_mode == EmitterMode::Auto && !_chirpOutput.isActive()) {
        const unsigned long now = millis();
        if (now >= _nextAutoChirpAtMs) {
            startChirp(_autoToneHz, _autoDurationMs);
            _nextAutoChirpAtMs = now + _autoIntervalMs;
        }
    }

    if (_mode == EmitterMode::Sweep) {
        advanceSweep(millis());
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
    char buffer[96];
    strncpy(buffer, line, sizeof(buffer));
    buffer[sizeof(buffer) - 1] = '\0';

    char* savePtr = nullptr;
    char* token = strtok_r(buffer, " ", &savePtr);
    if (token == nullptr) {
        return;
    }

    if (strcmp(token, "CHIRP") == 0) {
        claimExternalControl();
        handleChirpCommand(line);
        return;
    }

    if (strcmp(token, "AUTO") == 0 || strcmp(token, "REMOTE") == 0 || strcmp(token, "REMOTE_CONTROL") == 0 || strcmp(token, "MODE") == 0 || strcmp(token, "SWEEP") == 0) {
        claimExternalControl();
        handleModeCommand(line);
    }
}

void EmitterApp::handleChirpCommand(const char* line) {
    char buffer[96];
    strncpy(buffer, line, sizeof(buffer));
    buffer[sizeof(buffer) - 1] = '\0';

    unsigned long toneHz = 3200;
    unsigned long durationMs = 100;
    char* savePtr = nullptr;
    char* token = strtok_r(buffer, " ", &savePtr);
    token = nextToken(savePtr);
    while (token != nullptr) {
        if (startsWithToken(token, "freq=")) {
            toneHz = strtoul(findValue(token), nullptr, 10);
        } else if (startsWithToken(token, "dur=")) {
            durationMs = strtoul(findValue(token), nullptr, 10);
        }

        token = nextToken(savePtr);
    }

    startChirp(toneHz, durationMs);
}

void EmitterApp::handleModeCommand(const char* line) {
    char buffer[96];
    strncpy(buffer, line, sizeof(buffer));
    buffer[sizeof(buffer) - 1] = '\0';

    char* savePtr = nullptr;
    char* token = strtok_r(buffer, " ", &savePtr);
    if (token == nullptr) {
        return;
    }

    if (strcmp(token, "MODE") == 0) {
        token = nextToken(savePtr);
        if (token == nullptr) {
            return;
        }
    }

    if (strcmp(token, "SWEEP") == 0) {
        handleSweepCommand(line);
        return;
    }

    if (strcmp(token, "AUTO") == 0) {
        unsigned long intervalMs = _autoIntervalMs;
        unsigned long toneHz = _autoToneHz;
        unsigned long durationMs = _autoDurationMs;
        token = nextToken(savePtr);
        while (token != nullptr) {
            if (startsWithToken(token, "interval=")) {
                intervalMs = strtoul(findValue(token), nullptr, 10);
            } else if (startsWithToken(token, "freq=")) {
                toneHz = strtoul(findValue(token), nullptr, 10);
            } else if (startsWithToken(token, "dur=")) {
                durationMs = strtoul(findValue(token), nullptr, 10);
            }
            token = nextToken(savePtr);
        }

        _chirpOutput.stop();
        configureAuto(intervalMs, toneHz, durationMs);
        setMode(EmitterMode::Auto);
        _nextAutoChirpAtMs = millis();
        char modeLine[96];
        snprintf(modeLine,
                 sizeof(modeLine),
                 "OK MODE %s interval=%lu freq=%lu dur=%lu",
                 modeName(),
                 _autoIntervalMs,
                 _autoToneHz,
                 _autoDurationMs);
        sendControlAck(modeLine);
        return;
    }

    if (strcmp(token, "REMOTE") == 0 || strcmp(token, "REMOTE_CONTROL") == 0) {
        _chirpOutput.stop();
        setMode(EmitterMode::RemoteControl);
        char modeLine[96];
        snprintf(modeLine, sizeof(modeLine), "OK MODE %s", modeName());
        sendControlAck(modeLine);
    }
}

void EmitterApp::handleSweepCommand(const char* line) {
    char buffer[96];
    strncpy(buffer, line, sizeof(buffer));
    buffer[sizeof(buffer) - 1] = '\0';

    unsigned long startHz = _sweepStartHz;
    unsigned long stopHz = _sweepStopHz;
    unsigned long stepHz = _sweepStepHz;
    unsigned long durationMs = _sweepDurationMs;
    unsigned long pauseMs = _sweepPauseMs;

    char* savePtr = nullptr;
    char* token = strtok_r(buffer, " ", &savePtr);
    token = nextToken(savePtr);
    while (token != nullptr) {
        if (startsWithToken(token, "start=")) {
            startHz = strtoul(findValue(token), nullptr, 10);
        } else if (startsWithToken(token, "stop=")) {
            stopHz = strtoul(findValue(token), nullptr, 10);
        } else if (startsWithToken(token, "step=")) {
            stepHz = strtoul(findValue(token), nullptr, 10);
        } else if (startsWithToken(token, "dur=")) {
            durationMs = strtoul(findValue(token), nullptr, 10);
        } else if (startsWithToken(token, "pause=")) {
            pauseMs = strtoul(findValue(token), nullptr, 10);
        }
        token = nextToken(savePtr);
    }

    configureSweep(startHz, stopHz, stepHz, durationMs, pauseMs);
    _chirpOutput.stop();
    setMode(EmitterMode::Sweep);
    _nextSweepStepAtMs = millis();
    char modeLine[128];
    snprintf(modeLine,
             sizeof(modeLine),
             "OK MODE %s start=%lu stop=%lu step=%lu dur=%lu pause=%lu",
             modeName(),
             _sweepStartHz,
             _sweepStopHz,
             _sweepStepHz,
             _sweepDurationMs,
             _sweepPauseMs);
    sendControlAck(modeLine);
}

void EmitterApp::startChirp(unsigned long toneHz, unsigned long durationMs) {
    if (_chirpOutput.isActive()) {
        _chirpOutput.stop();
    }
    _chirpOutput.setToneHz(static_cast<uint32_t>(toneHz));
    _chirpOutput.setTiming(durationMs, 0);
    _chirpOutput.start();
    if (_mode == EmitterMode::Auto) {
        _nextAutoChirpAtMs = millis() + _autoIntervalMs;
    }

    char chirpLine[128];
    snprintf(chirpLine,
             sizeof(chirpLine),
             "OK CHIRP freq=%lu dur=%lu",
             toneHz,
             durationMs);
    sendControlAck(chirpLine);
}

void EmitterApp::setMode(EmitterMode mode) {
    _mode = mode;
}

void EmitterApp::claimExternalControl() {
    if (_mode == EmitterMode::RemoteControl) {
        return;
    }

    _chirpOutput.stop();
    _nextAutoChirpAtMs = millis();
    _nextSweepStepAtMs = millis();
    setMode(EmitterMode::RemoteControl);
}

void EmitterApp::configureAuto(unsigned long intervalMs, unsigned long toneHz, unsigned long durationMs) {
    _autoIntervalMs = intervalMs;
    _autoToneHz = toneHz;
    _autoDurationMs = durationMs;
    _chirpOutput.setToneHz(static_cast<uint32_t>(_autoToneHz));
    _chirpOutput.setTiming(_autoDurationMs, 0);
}

void EmitterApp::configureSweep(unsigned long startHz, unsigned long stopHz, unsigned long stepHz, unsigned long durationMs, unsigned long pauseMs) {
    if (stepHz == 0) {
        stepHz = 1;
    }
    if (stopHz < startHz) {
        unsigned long tmp = startHz;
        startHz = stopHz;
        stopHz = tmp;
    }

    _sweepStartHz = startHz;
    _sweepStopHz = stopHz;
    _sweepStepHz = stepHz;
    _sweepDurationMs = durationMs;
    _sweepPauseMs = pauseMs;
    _sweepCurrentHz = _sweepStartHz;
    _chirpOutput.setToneHz(static_cast<uint32_t>(_sweepCurrentHz));
    _chirpOutput.setTiming(_sweepDurationMs, 0);
}

void EmitterApp::advanceSweep(unsigned long now) {
    if (_chirpOutput.isActive()) {
        return;
    }

    if (now < _nextSweepStepAtMs) {
        return;
    }

    _chirpOutput.setToneHz(static_cast<uint32_t>(_sweepCurrentHz));
    _chirpOutput.setTiming(_sweepDurationMs, 0);
    _chirpOutput.start();
    Serial.print("EVT sweep_step freq=");
    Serial.println(_sweepCurrentHz);

    if (_sweepCurrentHz >= _sweepStopHz || _sweepCurrentHz > _sweepStopHz - _sweepStepHz) {
        _sweepCurrentHz = _sweepStartHz;
    } else {
        _sweepCurrentHz += _sweepStepHz;
    }

    _nextSweepStepAtMs = now + _sweepDurationMs + _sweepPauseMs;
}

const char* EmitterApp::modeName() const {
    switch (_mode) {
        case EmitterMode::Auto:
            return "AUTO";
        case EmitterMode::RemoteControl:
            return "REMOTE_CONTROL";
        case EmitterMode::Sweep:
            return "SWEEP";
    }
    return "AUTO";
}
