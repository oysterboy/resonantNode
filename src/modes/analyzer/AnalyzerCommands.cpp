#include "AnalyzerApp.h"
#include "AnalyzerTextUtils.h"

#include <Arduino.h>
#include <stdlib.h>
#include <string.h>

void AnalyzerApp::printSequenceHelp() {
    Serial.println("CMD: SEQ help");
    Serial.println("CMD: SEQ");
    Serial.println("CMD: SEQ stop");
    Serial.println("SEQ IN: start [N|tries=N] [period=MS] [window=MS] [freq=HZ] [dur=MS] [test=LABEL]");
    Serial.println("SEQ IN: OBS start [N|tries=N] [period=2000] [window=1800] [freq=HZ] [dur=MS] [test=LABEL]");
    Serial.println("SEQ IN: TRIES N");
    Serial.println("SEQ IN: [profile=tonalpulse|tonalpulse2|amp]");
    Serial.println("SEQ IN: MODE trial|source|inspect|pattern|dump|quiet");
    Serial.println("SEQ IN: MODE quiet = no sequence output");
    Serial.println("SEQ IN: WHEN off|miss|all");
    Serial.println("SEQ IN: VERBOSE 0|1|2");
    Serial.println("SEQ IN: TRIES N");
    Serial.println("SEQ IN: STATUS");
    Serial.println("SEQ IN: [dumpSamples=0|1] [curveFormat=off|samples]");
    Serial.println("SEQ IN: [sampleFirst=N] [sampleEvery=N] [sampleLead=MS] [sampleTail=MS] [sampleStep=MS] [sampleMax=N]");
    Serial.println("SEQ OUT: SEQ start / SEQ running / SEQ_PATTERN / SEQ_TRIAL / SEQ_INSPECT / SEQ_SOURCE / SEQ_DUMP / SEQ_SUMMARY");
    Serial.println("SEQ OUT: candidate fields include onset_sample peak_sample release_sample peak_ms dur end_dt_ms freq_*");
    Serial.println("SEQ OBS: passive observe mode for an already-running external emitter");
    Serial.println("SEQ PROFILE: profile=tonalpulse");
    Serial.println("SEQ PROFILE(ALT): profile=tonalpulse2");
    Serial.println("SEQ PROFILE(AMP): profile=amp (scalar amp detector / freqscore inspector)");
    Serial.println("SEQ PROFILE(EXP): profile=chirp_experimental (experimental)");
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

        token = strtok_r(nullptr, " ", &savePtr);
        if (token == nullptr || equalsIgnoreCase(token, "HELP")) {
            printSequenceHelp();
            return;
        }

        if (equalsIgnoreCase(token, "STATUS")) {
            printSequenceStatus();
            return;
        }

        if (equalsIgnoreCase(token, "MODE")) {
            const char* modeToken = strtok_r(nullptr, " ", &savePtr);
            bool valid = false;
            const AnalyzerApp::SeqOutputMode mode = AnalyzerApp::sequenceOutputModeFromToken(modeToken, &valid);
            if (!valid) {
                Serial.println("ERR SEQ unknown mode use MODE trial|source|inspect|pattern|dump|quiet");
                return;
            }
            _seqOutputConfig.mode = mode;
            if (mode == AnalyzerApp::SeqOutputMode::Quiet) {
                _seqOutputConfig.when = AnalyzerApp::SeqOutputWhen::Off;
                _seqOutputConfig.verbosity = 0;
            }
            if (_sequenceTest.active) {
                _sequenceTest.outputConfig.mode = mode;
                if (mode == AnalyzerApp::SeqOutputMode::Quiet) {
                    _sequenceTest.outputConfig.when = AnalyzerApp::SeqOutputWhen::Off;
                    _sequenceTest.outputConfig.verbosity = 0;
                    _sequenceTest.diagMode = AnalyzerApp::SequenceDiagMode::Off;
                    if (_detection != nullptr) {
                        _detection->setDiagnosticsEnabled(false);
                    }
                }
            }
            Serial.print("OK SEQ MODE ");
            Serial.println(AnalyzerApp::sequenceOutputModeName(mode));
            return;
        }

        if (equalsIgnoreCase(token, "WHEN")) {
            const char* whenToken = strtok_r(nullptr, " ", &savePtr);
            bool valid = false;
            const AnalyzerApp::SeqOutputWhen when = AnalyzerApp::sequenceOutputWhenFromToken(whenToken, &valid);
            if (!valid) {
                Serial.println("ERR SEQ unknown when use WHEN off|miss|all");
                return;
            }
            _seqOutputConfig.when = when;
            _sequenceTest.diagMode = AnalyzerApp::sequenceDiagModeFromOutputWhen(when);
            if (_sequenceTest.active) {
                _sequenceTest.outputConfig.when = when;
                _sequenceTest.diagMode = AnalyzerApp::sequenceDiagModeFromOutputWhen(when);
                if (_detection != nullptr) {
                    _detection->setDiagnosticsEnabled(_sequenceTest.diagMode != AnalyzerApp::SequenceDiagMode::Off);
                }
            }
            Serial.print("OK SEQ WHEN ");
            Serial.println(AnalyzerApp::sequenceOutputWhenName(when));
            return;
        }

        if (equalsIgnoreCase(token, "VERBOSE")) {
            const char* verbosityToken = strtok_r(nullptr, " ", &savePtr);
            if (verbosityToken == nullptr || *verbosityToken == '\0') {
                Serial.println("ERR SEQ missing verbosity use VERBOSE 0|1|2");
                return;
            }
            const unsigned long verbosity = strtoul(verbosityToken, nullptr, 10);
            if (verbosity > 2UL) {
                Serial.println("ERR SEQ verbosity out of range use 0|1|2");
                return;
            }
            _seqOutputConfig.verbosity = static_cast<uint8_t>(verbosity);
            if (_sequenceTest.active) {
                _sequenceTest.outputConfig.verbosity = _seqOutputConfig.verbosity;
            }
            Serial.print("OK SEQ VERBOSE ");
            Serial.println(static_cast<unsigned long>(_seqOutputConfig.verbosity));
            return;
        }

        if (equalsIgnoreCase(token, "TRIES")) {
            const char* trialsToken = strtok_r(nullptr, " ", &savePtr);
            if (trialsToken == nullptr || *trialsToken == '\0') {
                Serial.println("ERR SEQ missing tries use TRIES N");
                return;
            }
            const unsigned long totalTrials = strtoul(trialsToken, nullptr, 10);
            if (totalTrials == 0UL) {
                Serial.println("ERR SEQ tries out of range use N>=1");
                return;
            }
            _seqOutputConfig.totalTrials = totalTrials;
            Serial.print("OK SEQ TRIES ");
            Serial.println(_seqOutputConfig.totalTrials);
            return;
        }

        if (equalsIgnoreCase(token, "STOP")) {
            stopSequenceTest();
            Serial.println("OK SEQ STOP");
            return;
        }

        if (equalsIgnoreCase(token, "START") || equalsIgnoreCase(token, "OBS")) {
            unsigned long totalTrials = _seqOutputConfig.totalTrials;
            bool totalTrialsSet = false;
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
            detection::DetectionProfileKind profileKind = detection::DetectionProfileKind::TonalPulse;
            bool profileSeen = false;
            bool profileValid = true;
            bool externalEmitter = false;
            char setupLabel[96] = TEST_SETUP_LABEL;
            AnalyzerApp::SeqOutputConfig outputConfig = _seqOutputConfig;
            externalEmitter = equalsIgnoreCase(token, "OBS");

            while ((token = strtok_r(nullptr, " ", &savePtr)) != nullptr) {
                if (!totalTrialsSet && strchr(token, '=') == nullptr && strspn(token, "0123456789") == strlen(token)) {
                    totalTrials = static_cast<unsigned long>(strtoul(token, nullptr, 10));
                    totalTrialsSet = true;
                } else if (startsWithTokenIgnoreCase(token, "tries=")) {
                    totalTrials = static_cast<unsigned long>(strtoul(token + 6, nullptr, 10));
                    totalTrialsSet = true;
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
                    if (equalsIgnoreCase(profileValue, "tonalpulse")) {
                        profileKind = detection::DetectionProfileKind::TonalPulse;
                        profileSeen = true;
                    } else if (equalsIgnoreCase(profileValue, "tonalpulse2") || equalsIgnoreCase(profileValue, "tonal_pulse_2") || equalsIgnoreCase(profileValue, "tonal_pulse2")) {
                        profileKind = detection::DetectionProfileKind::TonalPulse2;
                        profileSeen = true;
                    } else if (equalsIgnoreCase(profileValue, "amp")) {
                        profileKind = detection::DetectionProfileKind::Amp;
                        profileSeen = true;
                    } else if (equalsIgnoreCase(profileValue, "chirp_experimental")) {
                        profileKind = detection::DetectionProfileKind::ChirpExperimental;
                        profileSeen = true;
                    } else {
                        profileValid = false;
                        Serial.print("ERR SEQ unknown profile=");
                        Serial.print(profileValue);
                        Serial.println(" use profile=tonalpulse, profile=tonalpulse2, profile=amp or profile=chirp_experimental");
                        return;
                    }
                } else if (startsWithTokenIgnoreCase(token, "mode=")) {
                    bool valid = false;
                    outputConfig.mode = AnalyzerApp::sequenceOutputModeFromToken(token + 5, &valid);
                    if (!valid) {
                        Serial.print("ERR SEQ unknown mode=");
                        Serial.print(token + 5);
                        Serial.println(" use mode=trial, mode=source, mode=inspect, mode=pattern, mode=dump or mode=quiet");
                        return;
                    }
                    if (outputConfig.mode == AnalyzerApp::SeqOutputMode::Quiet) {
                        outputConfig.when = AnalyzerApp::SeqOutputWhen::Off;
                        outputConfig.verbosity = 0;
                    }
                } else if (startsWithTokenIgnoreCase(token, "when=")) {
                    bool valid = false;
                    outputConfig.when = AnalyzerApp::sequenceOutputWhenFromToken(token + 5, &valid);
                    if (!valid) {
                        Serial.print("ERR SEQ unknown when=");
                        Serial.print(token + 5);
                        Serial.println(" use when=off, when=miss or when=all");
                        return;
                    }
                } else if (startsWithTokenIgnoreCase(token, "verbose=")) {
                    const unsigned long verbosity = strtoul(token + 8, nullptr, 10);
                    if (verbosity > 2UL) {
                        Serial.print("ERR SEQ verbose=");
                        Serial.print(verbosity);
                        Serial.println(" use 0|1|2");
                        return;
                    }
                    outputConfig.verbosity = static_cast<uint8_t>(verbosity);
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
                    // Observing an already-running external emitter is an explicit mode.
                    // Keep the current mode if the caller already requested OBS.
                    if (!externalEmitter) {
                        externalEmitter = true;
                    }
                } else if (startsWithTokenIgnoreCase(token, "test=")) {
                    strncpy(setupLabel, token + 5, sizeof(setupLabel));
                    setupLabel[sizeof(setupLabel) - 1] = '\0';
                } else if (equalsIgnoreCase(token, "labels")) {
                    strncpy(setupLabel, "labels", sizeof(setupLabel));
                    setupLabel[sizeof(setupLabel) - 1] = '\0';
                }
            }

            if (!profileSeen) {
                profileKind = detection::DetectionProfileKind::TonalPulse;
            }
            if (!profileValid) {
                return;
            }
            _seqOutputConfig = outputConfig;
            _seqOutputConfig.totalTrials = totalTrials;
            startSequenceTest(totalTrials, periodMs, windowEndOffsetMs, toneHz, durationMs, quiet, showDetails, AnalyzerApp::SequenceDiagMode::Off, setupLabel, sampleDumpEnabled, sampleDumpFirstTrials, sampleDumpEveryNth, sampleDumpLeadMs, sampleDumpTailMs, sampleDumpStepMs, sampleDumpMaxRows, profileKind, externalEmitter);
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
