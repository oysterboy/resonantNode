#include "AnalyzerApp.h"
#include "AnalyzerTextUtils.h"

#include <Arduino.h>
#include <stdlib.h>
#include <string.h>

void AnalyzerApp::printSequenceHelp() {
    Serial.println("CMD: SEQ help");
    Serial.println("CMD: SEQ");
    Serial.println("CMD: SEQ stop");
    Serial.println("SEQ IN: start [tries=N] [period=MS] [window=MS] [freq=HZ] [dur=MS] [test=LABEL]");
    Serial.println("SEQ IN: OBS start [tries=N] [period=2000] [window=1800] [freq=HZ] [dur=MS] [test=LABEL]");
    Serial.println("SEQ IN: [profile=freqamp|chirp]");
    Serial.println("SEQ IN: [log=default|none|quiet|summary|summary+trial|trial|candidate|explain|custom|full]");
    Serial.println("SEQ IN: stable summary=log=summary");
    Serial.println("SEQ IN: [debug=0|1|2] [dumpSamples=0|1] [curveFormat=off|samples]");
    Serial.println("SEQ IN: [sampleFirst=N] [sampleEvery=N] [sampleLead=MS] [sampleTail=MS] [sampleStep=MS] [sampleMax=N]");
    Serial.println("SEQ OUT: SEQ start / SEQ running / SEQ_CAND / SEQ_TRIAL / SEQ_EXPLAIN / SEQ_CUSTOM / SEQ_SUMMARY");
    Serial.println("SEQ OUT: candidate fields include onset_sample peak_sample release_sample peak_ms dur end_dt_ms freq_*");
    Serial.println("SEQ OBS: passive observe mode for an already-running external emitter");
    Serial.println("SEQ PROFILE: profile=freqamp|chirp");
    Serial.println("SEQ PARAM: freqScore=10000 freqContrast=50.0");
}

void AnalyzerApp::pollUsbConsole() {
    while (Serial.available() > 0) {
        const char c = static_cast<char>(Serial.read());
        if (c == '\r') {
            continue;
        }

        if (c == '\n') {
            _usbLineBuffer[_usbLineLength] = '\0';
            if (_usbLineLength > 0) {
                handleUsbLine(_usbLineBuffer);
            }
            _usbLineLength = 0;
            continue;
        }

        if (_usbLineLength < sizeof(_usbLineBuffer) - 1) {
            _usbLineBuffer[_usbLineLength++] = c;
        }
    }
}

void AnalyzerApp::handleUsbLine(const char* line) {
    if (equalsIgnoreCase(line, "HELP")) {
        if (_valMode) {
            return;
        }
        Serial.println("CMD: BASE dur=10000 quiet");
        Serial.println("CMD: BASE stop");
        Serial.println("CMD: PARAM freqScore=10000 freqContrast=50.0");
        Serial.println("CMD: EMIT CHIRP freq=3200 dur=100");
        Serial.println("CMD: EMIT MODE REMOTE");
        Serial.println("CMD: EMIT MODE AUTO interval=2000 freq=3200 dur=100");
        Serial.println("CMD: EMIT SWEEP start=3000 stop=3500 step=100 dur=80 pause=1000");
        Serial.println("CMD: TEST");
        Serial.println("CMD: RAW trigger f=3200 dur=100 post=1000 dump=bin");
        Serial.println("CMD: SEQ");
        Serial.println("CMD: SEQ help");
        Serial.println("CMD: SEQ stop");
        Serial.println("CMD: CAP");
        Serial.println("CMD: CAP stop");
        Serial.println("CMD: VAL");
        Serial.println("CMD: VAL OFF");
        return;
    }

    if (startsWithTokenIgnoreCase(line, "PARAM")) {
        char buffer[128];
        strncpy(buffer, line, sizeof(buffer));
        buffer[sizeof(buffer) - 1] = '\0';

        char* savePtr = nullptr;
        char* token = strtok_r(buffer, " ", &savePtr);
        FrequencyMatchEvaluation::Values freqTuning = _frequencyEvidenceTuning;

        while ((token = strtok_r(nullptr, " ", &savePtr)) != nullptr) {
            FrequencyMatchEvaluation::parseToken(token, freqTuning);
        }

        _frequencyEvidenceTuning = freqTuning;
        printDetectionParameters();
        Serial.println("OK PARAM");
        return;
    }

    if (startsWithTokenIgnoreCase(line, "BASE")) {
        char buffer[96];
        strncpy(buffer, line, sizeof(buffer));
        buffer[sizeof(buffer) - 1] = '\0';

        bool quiet = false;
        unsigned long durationMs = 10000;

        char* savePtr = nullptr;
        char* token = strtok_r(buffer, " ", &savePtr);
        while ((token = strtok_r(nullptr, " ", &savePtr)) != nullptr) {
            if (equalsIgnoreCase(token, "quiet")) {
                quiet = true;
            } else if (startsWithTokenIgnoreCase(token, "dur=")) {
                durationMs = static_cast<unsigned long>(strtoul(token + 4, nullptr, 10));
            }
        }

        startBaseSession(durationMs, quiet);
        Serial.println("OK BASE");
        return;
    }

    if (equalsIgnoreCase(line, "BASE STOP")) {
        stopBaseSession();
        Serial.println("OK BASE STOP");
        return;
    }

    if (startsWithTokenIgnoreCase(line, "TEST")) {
        startBaseSession(12000, false);
        Serial.println("OK TEST");
        return;
    }

    if (startsWithTokenIgnoreCase(line, "RAW")) {
        char buffer[160];
        strncpy(buffer, line, sizeof(buffer));
        buffer[sizeof(buffer) - 1] = '\0';

        unsigned long toneHz = runtime::kDefaultChirpFrequencyHz;
        unsigned long durationMs = 100;
        unsigned long postMs = 1000;
        unsigned long preMs = 0;
        unsigned long decim = 1;
        bool dumpChunks = false;
        bool dumpBinary = false;

        char* savePtr = nullptr;
        char* token = strtok_r(buffer, " ", &savePtr);
        while ((token = strtok_r(nullptr, " ", &savePtr)) != nullptr) {
            if (startsWithTokenIgnoreCase(token, "f=")) {
                toneHz = static_cast<unsigned long>(strtoul(token + 2, nullptr, 10));
            } else if (startsWithTokenIgnoreCase(token, "dur=")) {
                durationMs = static_cast<unsigned long>(strtoul(token + 4, nullptr, 10));
            } else if (startsWithTokenIgnoreCase(token, "post=")) {
                postMs = static_cast<unsigned long>(strtoul(token + 5, nullptr, 10));
            } else if (startsWithTokenIgnoreCase(token, "pre=")) {
                preMs = static_cast<unsigned long>(strtoul(token + 4, nullptr, 10));
            } else if (startsWithTokenIgnoreCase(token, "decim=")) {
                decim = static_cast<unsigned long>(strtoul(token + 6, nullptr, 10));
            } else if (equalsIgnoreCase(token, "dump=bin")) {
                dumpBinary = true;
            } else if (equalsIgnoreCase(token, "dump=chunks")) {
                dumpChunks = true;
            }
        }

        runRawTrigger(toneHz, durationMs, postMs, preMs, decim, dumpChunks, dumpBinary);
        Serial.println("OK RAW");
        return;
    }

    if (startsWithTokenIgnoreCase(line, "SEQ")) {
        char buffer[128];
        strncpy(buffer, line, sizeof(buffer));
        buffer[sizeof(buffer) - 1] = '\0';

        char* savePtr = nullptr;
        char* token = strtok_r(buffer, " ", &savePtr);
        if (token == nullptr) {
            printSequenceHelp();
            return;
        }

        if (equalsIgnoreCase(token, "SEQ") || equalsIgnoreCase(token, "HELP")) {
            printSequenceHelp();
            return;
        }

        if (equalsIgnoreCase(token, "STOP")) {
            stopSequenceTest();
            Serial.println("OK SEQ STOP");
            return;
        }

        if (equalsIgnoreCase(token, "OBS")) {
            _sequenceTest.externalEmitter = true;
        }

        if (equalsIgnoreCase(token, "START") || equalsIgnoreCase(token, "OBS") || token == nullptr) {
            unsigned long totalTrials = 100;
            unsigned long periodMs = 2500;
            unsigned long windowEndOffsetMs = 2200;
            unsigned long toneHz = runtime::kDefaultChirpFrequencyHz;
            unsigned long durationMs = 100;
            bool quiet = false;
            bool showDetails = true;
            bool sampleDumpEnabled = false;
            unsigned long sampleDumpFirstTrials = 2;
            unsigned long sampleDumpEveryNth = 0;
            unsigned long sampleDumpLeadMs = 50;
            unsigned long sampleDumpTailMs = 800;
            unsigned long sampleDumpStepMs = 1;
            unsigned long sampleDumpMaxRows = 5000;
            detection::DetectionProfileKind profileKind = detection::DetectionProfileKind::FreqAmp;
            bool externalEmitter = false;
            char setupLabel[96] = TEST_SETUP_LABEL;
            uint32_t logFlags = DEFAULT_ANALYZER_LOG_FLAGS;

            while ((token = strtok_r(nullptr, " ", &savePtr)) != nullptr) {
                if (startsWithTokenIgnoreCase(token, "tries=")) {
                    totalTrials = static_cast<unsigned long>(strtoul(token + 6, nullptr, 10));
                } else if (startsWithTokenIgnoreCase(token, "period=")) {
                    periodMs = static_cast<unsigned long>(strtoul(token + 7, nullptr, 10));
                } else if (startsWithTokenIgnoreCase(token, "window=")) {
                    windowEndOffsetMs = static_cast<unsigned long>(strtoul(token + 7, nullptr, 10));
                } else if (startsWithTokenIgnoreCase(token, "freq=")) {
                    toneHz = static_cast<unsigned long>(strtoul(token + 5, nullptr, 10));
                } else if (startsWithTokenIgnoreCase(token, "dur=")) {
                    durationMs = static_cast<unsigned long>(strtoul(token + 4, nullptr, 10));
                } else if (equalsIgnoreCase(token, "quiet")) {
                    quiet = true;
                } else if (equalsIgnoreCase(token, "show=0")) {
                    showDetails = false;
                } else if (equalsIgnoreCase(token, "show=1")) {
                    showDetails = true;
                } else if (startsWithTokenIgnoreCase(token, "profile=")) {
                    const char* profileValue = token + 8;
                    if (equalsIgnoreCase(profileValue, "freqamp")) {
                        profileKind = detection::DetectionProfileKind::FreqAmp;
                    } else if (equalsIgnoreCase(profileValue, "chirp")) {
                        profileKind = detection::DetectionProfileKind::Chirp;
                    }
                } else if (startsWithTokenIgnoreCase(token, "log=")) {
                    logFlags = analyzerLogFlagsFromToken(token + 4);
                } else if (startsWithTokenIgnoreCase(token, "sampleFirst=")) {
                    sampleDumpEnabled = true;
                    sampleDumpFirstTrials = static_cast<unsigned long>(strtoul(token + 12, nullptr, 10));
                } else if (startsWithTokenIgnoreCase(token, "sampleEvery=")) {
                    sampleDumpEnabled = true;
                    sampleDumpEveryNth = static_cast<unsigned long>(strtoul(token + 12, nullptr, 10));
                } else if (startsWithTokenIgnoreCase(token, "sampleLead=")) {
                    sampleDumpEnabled = true;
                    sampleDumpLeadMs = static_cast<unsigned long>(strtoul(token + 11, nullptr, 10));
                } else if (startsWithTokenIgnoreCase(token, "sampleTail=")) {
                    sampleDumpEnabled = true;
                    sampleDumpTailMs = static_cast<unsigned long>(strtoul(token + 11, nullptr, 10));
                } else if (startsWithTokenIgnoreCase(token, "sampleStep=")) {
                    sampleDumpEnabled = true;
                    sampleDumpStepMs = static_cast<unsigned long>(strtoul(token + 11, nullptr, 10));
                } else if (startsWithTokenIgnoreCase(token, "sampleMax=")) {
                    sampleDumpEnabled = true;
                    sampleDumpMaxRows = static_cast<unsigned long>(strtoul(token + 10, nullptr, 10));
                } else if (equalsIgnoreCase(token, "external")) {
                    externalEmitter = true;
                } else if (equalsIgnoreCase(token, "labels")) {
                    strncpy(setupLabel, "labels", sizeof(setupLabel));
                    setupLabel[sizeof(setupLabel) - 1] = '\0';
                }
            }

            startSequenceTest(totalTrials, periodMs, windowEndOffsetMs, toneHz, durationMs, quiet, showDetails, setupLabel, logFlags, sampleDumpEnabled, sampleDumpFirstTrials, sampleDumpEveryNth, sampleDumpLeadMs, sampleDumpTailMs, sampleDumpStepMs, sampleDumpMaxRows, profileKind, externalEmitter);
            Serial.println("OK SEQ");
            return;
        }

        if (equalsIgnoreCase(token, "PROFILE")) {
            printSequenceHelp();
            return;
        }
    }

    if (equalsIgnoreCase(line, "CAP")) {
        startCaptureSession(20, 2500, 500, runtime::kDefaultChirpFrequencyHz, 100, false);
        Serial.println("OK CAP");
        return;
    }

    if (equalsIgnoreCase(line, "CAP STOP")) {
        stopCaptureSession();
        Serial.println("OK CAP STOP");
        return;
    }

    if (equalsIgnoreCase(line, "VAL")) {
        _valMode = true;
        Serial.println("OK VAL");
        return;
    }

    if (equalsIgnoreCase(line, "VAL OFF")) {
        _valMode = false;
        Serial.println("OK VAL OFF");
        return;
    }

    if (startsWithTokenIgnoreCase(line, "EMIT ")) {
        if (startsWithTokenIgnoreCase(line, "EMIT CHIRP")) {
            sendEmitterCommand(line + 5);
            Serial.println("OK EMIT CHIRP");
            return;
        }
        if (startsWithTokenIgnoreCase(line, "EMIT MODE REMOTE")) {
            sendEmitterCommand("MODE REMOTE");
            Serial.println("OK EMIT MODE REMOTE");
            return;
        }
        if (startsWithTokenIgnoreCase(line, "EMIT MODE AUTO")) {
            sendEmitterCommand(line + 5);
            Serial.println("OK EMIT MODE AUTO");
            return;
        }
        if (startsWithTokenIgnoreCase(line, "EMIT SWEEP")) {
            sendEmitterCommand(line + 5);
            Serial.println("OK EMIT SWEEP");
            return;
        }
    }

    if (_valMode) {
        return;
    }

    Serial.print("EVT analyzer_unknown_cmd line=");
    Serial.println(line);
}
