#include "AnalyzerEmitterControl.h"
#include "../AnalyzerModeApp.h"
#include "../../../detection/analyzer/AnalyzerText.h"
#include "../../../app/SerialLine.h"

#include <Arduino.h>
#include <stdlib.h>
#include <string.h>
#include "../../../app/TimeBase.h"
#include "../../../app/TimingUtils.h"
#include "../../../app/SerialOut.h"

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
    const unsigned long startMs = timebase::nowMs();
    serial_line::LineReader<serial_line::kEmitterLineMax> line;

    // Wait synchronously for the emitter to acknowledge remote mode.
    while (timebase::nowMs() - startMs < timeoutMs) {
        while (Serial2.available() > 0) {
            if (line.feed(static_cast<char>(Serial2.read())) && line.length() > 0 &&
                strncmp(line.line(), expectedPrefix, strlen(expectedPrefix)) == 0) {
                return true;
            }
        }
        delay(1);
    }

    return false;
}

void AnalyzerApp::beginEmitterControl() {
    Serial2.begin(_controlBaudRate, SERIAL_8N1, _controlRxPin, _controlTxPin);
    serialOut().print("EVT analyzer_control rx=");
    serialOut().print(_controlRxPin);
    serialOut().print(" tx=");
    serialOut().println(_controlTxPin);
    serialOut().println("EVT analyzer_control_claim scheduled");
}

void AnalyzerApp::pollEmitterSerial() {
    while (Serial2.available() > 0) {
        if (!_emitterLine.feed(static_cast<char>(Serial2.read()))) {
            continue;
        }
        {
            const unsigned long now = timebase::nowMs();
            const char* emitterLine = _emitterLine.line();
            const bool emitStartLine = startsWithTokenIgnoreCase(emitterLine, "EMIT_START");
            const bool emitDriveOnLine = startsWithTokenIgnoreCase(emitterLine, "EMIT_DRIVE_ON");
            if (_controlClaimSent && !_controlClaimAcked && startsWithTokenIgnoreCase(emitterLine, "OK MODE REMOTE")) {
                _controlClaimAcked = true;
                serialOut().println("EVT analyzer_control_claim acked");
            }
            if (_sequenceTest.active && _sequenceTest.trial.number > 0 && (emitStartLine || emitDriveOnLine)) {
                const unsigned long markerTrial = parseUnsignedTokenValue(emitterLine, "trial=");
                if (markerTrial == _sequenceTest.trial.number) {
                    auto& trialDiag = _sequenceTest.trial.diagnostics;
                    trialDiag.emitStartSeen = true;
                    trialDiag.emitStartDtMs = timing::elapsedOrZero(now, _sequenceTest.trial.startMs);
                }
            }
        }
    }
}

void AnalyzerApp::sendEmitterCommand(const char* command) {
    // Leading newline: ends any partial junk line on the Emitter side (see
    // app/SerialLine.h) so this command always starts a fresh line.
    Serial2.print('\n');
    Serial2.println(command);
}
