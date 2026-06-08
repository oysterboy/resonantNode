#include "AnalyzerApp.h"
#include "AnalyzerTextUtils.h"

#include <Arduino.h>
#include <stdlib.h>
#include <string.h>

namespace {

unsigned long parseUnsignedTokenValue(const char* line, const char* key, unsigned long fallback = 0) {
    const char* found = strstr(line, key);
    if (found == nullptr) {
        return fallback;
    }

    found += strlen(key);
    return strtoul(found, nullptr, 10);
}

} // namespace

bool waitForEmitterAck(const char* expectedPrefix, unsigned long timeoutMs) {
    const unsigned long startMs = millis();
    char line[96];
    size_t lineLength = 0;

    // Wait synchronously for the emitter to acknowledge remote mode.
    while (millis() - startMs < timeoutMs) {
        while (Serial2.available() > 0) {
            const char c = static_cast<char>(Serial2.read());
            if (c == '\r') {
                continue;
            }

            if (c == '\n') {
                line[lineLength] = '\0';
                if (lineLength > 0 && strncmp(line, expectedPrefix, strlen(expectedPrefix)) == 0) {
                    return true;
                }
                lineLength = 0;
                continue;
            }

            if (lineLength < sizeof(line) - 1) {
                line[lineLength++] = c;
            }
        }
        delay(1);
    }

    return false;
}

void AnalyzerApp::beginEmitterControl() {
    Serial2.begin(_controlBaudRate, SERIAL_8N1, _controlRxPin, _controlTxPin);
    Serial.print("EVT analyzer_control rx=");
    Serial.print(_controlRxPin);
    Serial.print(" tx=");
    Serial.println(_controlTxPin);
    Serial.println("EVT analyzer_control_claim scheduled");
}

void AnalyzerApp::pollEmitterSerial() {
    while (Serial2.available() > 0) {
        const char c = static_cast<char>(Serial2.read());
        if (c == '\r') {
            continue;
        }

        if (c == '\n') {
            const unsigned long now = millis();
            _emitterLineBuffer[_emitterLineLength] = '\0';
            const bool emitStartLine = startsWithTokenIgnoreCase(_emitterLineBuffer, "EMIT_START");
            const bool emitDoneLine = startsWithTokenIgnoreCase(_emitterLineBuffer, "EMIT_DONE");
            const bool emitDriveOnLine = startsWithTokenIgnoreCase(_emitterLineBuffer, "EMIT_DRIVE_ON");
            const bool emitDriveOffLine = startsWithTokenIgnoreCase(_emitterLineBuffer, "EMIT_DRIVE_OFF");
            if ((_sequenceTest.active && (emitStartLine || emitDoneLine || emitDriveOnLine || emitDriveOffLine))
                || (_emitterLineLength > 0
                    && !_valMode
                    && !startsWithTokenIgnoreCase(_emitterLineBuffer, "OK CHIRP")
                    && !startsWithTokenIgnoreCase(_emitterLineBuffer, "OK MODE REMOTE"))) {
                Serial.print("EMIT< ");
                Serial.println(_emitterLineBuffer);
            }
            if (_sequenceTest.active && _sequenceTest.currentTrial > 0 && (emitStartLine || emitDoneLine || emitDriveOnLine || emitDriveOffLine)) {
                const unsigned long markerTrial = parseUnsignedTokenValue(_emitterLineBuffer, "trial=");
                if (markerTrial == _sequenceTest.currentTrial) {
                    auto& trialDiag = _sequenceTest.currentTrialDiagnostics;
                    trialDiag.emitTrialId = markerTrial;
                    trialDiag.emitSeen = true;
                    if (emitStartLine || emitDriveOnLine) {
                        trialDiag.emitStartSeen = true;
                        trialDiag.emitDriveSeen = true;
                        trialDiag.emitStartTrialId = markerTrial;
                        trialDiag.emitStartDtMs = now >= _sequenceTest.currentTrialStartMs ? now - _sequenceTest.currentTrialStartMs : 0UL;
                    }
                    if (emitDoneLine || emitDriveOffLine) {
                        trialDiag.emitDoneSeen = true;
                        trialDiag.emitDoneTrialId = markerTrial;
                        trialDiag.emitDoneDtMs = now >= _sequenceTest.currentTrialStartMs ? now - _sequenceTest.currentTrialStartMs : 0UL;
                        trialDiag.emitDurationMs = trialDiag.emitDoneDtMs >= trialDiag.emitStartDtMs
                            ? trialDiag.emitDoneDtMs - trialDiag.emitStartDtMs
                            : 0UL;
                    }
                }
            }
            _emitterLineLength = 0;
            continue;
        }

        if (_emitterLineLength < sizeof(_emitterLineBuffer) - 1) {
            _emitterLineBuffer[_emitterLineLength++] = c;
        }
    }
}

void AnalyzerApp::sendEmitterCommand(const char* command) {
    Serial2.println(command);
}
