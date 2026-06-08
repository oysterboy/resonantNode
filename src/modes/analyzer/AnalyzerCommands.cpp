#include "AnalyzerApp.h"
#include "AnalyzerTextUtils.h"

#include <Arduino.h>
#include <stdlib.h>
#include <string.h>

namespace {

bool parseOnOffToken(const char* token, bool* valid) {
    if (valid != nullptr) {
        *valid = true;
    }
    if (token == nullptr || *token == '\0') {
        if (valid != nullptr) {
            *valid = false;
        }
        return true;
    }
    if (equalsIgnoreCase(token, "on") || equalsIgnoreCase(token, "1") || equalsIgnoreCase(token, "true")) {
        return true;
    }
    if (equalsIgnoreCase(token, "off") || equalsIgnoreCase(token, "0") || equalsIgnoreCase(token, "false")) {
        return false;
    }
    if (valid != nullptr) {
        *valid = false;
    }
    return true;
}

const char* onOffName(bool value) {
    return value ? "on" : "off";
}

} // namespace

void AnalyzerApp::printSequenceHelp() {
    Serial.println("CMD: SEQ help");
    Serial.println("CMD: SEQ");
    Serial.println("CMD: SEQ stop");
    Serial.println("SEQ IN: start [N|tries=N] [period=MS] [window=MS] [freq=HZ] [dur=MS] [delay=MS] [report_settle=MS] [test=LABEL]");
    Serial.println("SEQ IN: OBS start [N|tries=N] [period=2000] [window=1800] [freq=HZ] [dur=MS] [delay=MS] [report_settle=MS] [test=LABEL]");
    Serial.println("SEQ IN: TRIES N");
    Serial.println("SEQ IN: [profile=tonalpulse|amp|chirp_experimental]");
    Serial.println("SEQ IN: MODE quiet|trial|compact|signalcheck|streak|full|system|source|inspect|pattern|dump");
    Serial.println("SEQ IN: MODE quiet = no sequence output");
    Serial.println("SEQ IN: MODE trial|compact = compact trial view");
    Serial.println("SEQ IN: MODE signalcheck = compact trial view + audio health snapshot");
    Serial.println("SEQ IN: MODE streak = miss/duplicate streak diagnostics");
    Serial.println("SEQ IN: MODE full = trial + source + inspect + pattern");
    Serial.println("SEQ IN: MODE system = trial verdict + system health");
    Serial.println("SEQ IN: PROFILE tonalpulse|amp|chirp_experimental");
    Serial.println("SEQ IN: DIAG on|off");
    Serial.println("SEQ IN: FREQBAND on|off");
    Serial.println("SEQ IN: FREQUPDATEEVERYSAMPLES 1|4|8|16");
    Serial.println("SEQ IN: WHEN off|miss|all");
    Serial.println("SEQ IN: VERBOSE 0|1|2 (0=compact, 1=summary, 2=deep debug)");
    Serial.println("SEQ IN: TRIES N");
    Serial.println("SEQ IN: STATUS");
    Serial.println("SEQ IN: [dumpSamples=0|1] [curveFormat=off|samples]");
    Serial.println("SEQ IN: [sampleFirst=N] [sampleEvery=N] [sampleLead=MS] [sampleTail=MS] [sampleStep=MS] [sampleMax=N]");
    Serial.println("SEQ OUT: SEQ start / SEQ running / SEQ_PATTERN / SEQ_TRIAL / SEQ_STREAK / SEQ_INSPECT / SEQ_SOURCE / SEQ_DUMP / SEQ_SUMMARY / AUDIO run / SIGNALCHECK");
    Serial.println("SEQ OUT: candidate fields include onset_sample peak_sample release_sample peak_ms dur end_dt_ms freq_*");
    Serial.println("SEQ OBS: passive observe mode for an already-running external emitter");
    Serial.println("SEQ IN: PROFILE tonalpulse|amp|chirp_experimental");
    Serial.println("SEQ PROFILE tonalpulse");
    Serial.println("SEQ PROFILE amp");
    Serial.println("SEQ PROFILE chirp_experimental");
    Serial.println("SEQ PARAM: freqScore=18000 freqContrast=50.0 freqReleaseScore=12000 freqReleaseContrast=50.0");
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
        Serial.println("CMD: PARAM freqScore=18000 freqContrast=50.0 freqReleaseScore=12000 freqReleaseContrast=50.0");
        Serial.println("CMD: EMIT CHIRP freq=3200 dur=100");
        Serial.println("CMD: EMIT MODE REMOTE");
        Serial.println("CMD: EMIT MODE AUTO interval=2000 freq=3200 dur=100");
        Serial.println("CMD: EMIT SWEEP start=3000 stop=3500 step=100 dur=80 pause=1000");
        Serial.println("CMD: TEST");
        Serial.println("CMD: RAWBAND contrast f=3200 dur=100 post=1000 decim=4");
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
        strncpy(_commandScratch, line, sizeof(_commandScratch));
        _commandScratch[sizeof(_commandScratch) - 1] = '\0';

        char* savePtr = nullptr;
        char* token = strtok_r(_commandScratch, " ", &savePtr);
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
        strncpy(_commandScratch, line, sizeof(_commandScratch));
        _commandScratch[sizeof(_commandScratch) - 1] = '\0';

        bool quiet = false;
        unsigned long durationMs = 10000;

        char* savePtr = nullptr;
        char* token = strtok_r(_commandScratch, " ", &savePtr);
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

    if (startsWithTokenIgnoreCase(line, "RAWBAND")) {
        strncpy(_commandScratch, line, sizeof(_commandScratch));
        _commandScratch[sizeof(_commandScratch) - 1] = '\0';

        unsigned long toneHz = runtime::kDefaultChirpFrequencyHz;
        unsigned long durationMs = 100;
        unsigned long postMs = 1000;
        unsigned long preMs = 0;
        unsigned long decim = 4;
        bool contrastMode = false;

        char* savePtr = nullptr;
        char* token = strtok_r(_commandScratch, " ", &savePtr);
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
            } else if (equalsIgnoreCase(token, "contrast")) {
                contrastMode = true;
            }
        }

        if (!contrastMode) {
            Serial.println("ERR RAWBAND use RAWBAND contrast");
            return;
        }

        runRawBandTrigger(toneHz, durationMs, postMs, preMs, decim);
        Serial.println("OK RAWBAND");
        return;
    }

    if (startsWithTokenIgnoreCase(line, "RAW")) {
        strncpy(_commandScratch, line, sizeof(_commandScratch));
        _commandScratch[sizeof(_commandScratch) - 1] = '\0';

        unsigned long toneHz = runtime::kDefaultChirpFrequencyHz;
        unsigned long durationMs = 100;
        unsigned long postMs = 1000;
        unsigned long preMs = 0;
        unsigned long decim = 1;
        bool dumpChunks = false;
        bool dumpBinary = false;

        char* savePtr = nullptr;
        char* token = strtok_r(_commandScratch, " ", &savePtr);
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
        strncpy(_commandScratch, line, sizeof(_commandScratch));
        _commandScratch[sizeof(_commandScratch) - 1] = '\0';

        char* savePtr = nullptr;
        char* token = strtok_r(_commandScratch, " ", &savePtr);
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

    if (equalsIgnoreCase(token, "DIAG") || equalsIgnoreCase(token, "DIAGNOSTICS")) {
        const char* enabledToken = strtok_r(nullptr, " ", &savePtr);
        bool valid = false;
        const bool enabled = parseOnOffToken(enabledToken, &valid);
        if (!valid) {
            Serial.println("ERR SEQ unknown diagnostics use DIAG on|off");
            return;
        }
        _seqOutputConfig.diagnosticsEnabled = enabled;
        if (_sequenceTest.active) {
            _sequenceTest.outputConfig.diagnosticsEnabled = enabled;
            _detection.setDiagnosticsEnabled(enabled);
        }
        Serial.print("OK SEQ DIAG ");
        Serial.println(onOffName(enabled));
        return;
    }

    if (equalsIgnoreCase(token, "FREQBAND")) {
        const char* enabledToken = strtok_r(nullptr, " ", &savePtr);
        bool valid = false;
        const bool enabled = parseOnOffToken(enabledToken, &valid);
        if (!valid) {
            Serial.println("ERR SEQ unknown freqband use FREQBAND on|off");
            return;
        }
        _seqOutputConfig.frequencyBandEnabled = enabled;
        if (_sequenceTest.active) {
            _sequenceTest.outputConfig.frequencyBandEnabled = enabled;
        }
        Serial.print("OK SEQ FREQBAND ");
        Serial.println(onOffName(enabled));
        return;
    }

    if (equalsIgnoreCase(token, "FREQUPDATEEVERYSAMPLES")) {
        const char* decimateToken = strtok_r(nullptr, " ", &savePtr);
        if (decimateToken == nullptr || *decimateToken == '\0') {
            Serial.println("ERR SEQ missing freqUpdateEverySamples use FREQUPDATEEVERYSAMPLES 1|4|8|16");
            return;
        }
        const unsigned long decimation = strtoul(decimateToken, nullptr, 10);
        if (decimation == 0UL) {
            Serial.println("ERR SEQ freqUpdateEverySamples out of range use N>=1");
            return;
        }
        _seqOutputConfig.frequencyUpdateEverySamples = decimation;
        _freqBandStream.setFrequencyUpdateEverySamples(decimation);
        if (_sequenceTest.active) {
            _sequenceTest.outputConfig.frequencyUpdateEverySamples = decimation;
        }
        Serial.print("OK SEQ FREQUPDATEEVERYSAMPLES ");
        Serial.println(decimation);
        return;
    }

        if (equalsIgnoreCase(token, "MODE")) {
            const char* modeToken = strtok_r(nullptr, " ", &savePtr);
            bool valid = false;
            const AnalyzerApp::SeqOutputMode mode = AnalyzerApp::sequenceOutputModeFromToken(modeToken, &valid);
            if (!valid) {
                Serial.println("ERR SEQ unknown mode use MODE quiet|trial|compact|signalcheck|streak|full|system|source|inspect|pattern|dump");
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
                    _detection.setDiagnosticsEnabled(false);
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
                _detection.setDiagnosticsEnabled(_sequenceTest.outputConfig.diagnosticsEnabled);
            }
            Serial.print("OK SEQ WHEN ");
            Serial.println(AnalyzerApp::sequenceOutputWhenName(when));
            return;
        }

        if (equalsIgnoreCase(token, "PROFILE")) {
            const char* profileToken = strtok_r(nullptr, " ", &savePtr);
            if (profileToken == nullptr || *profileToken == '\0') {
                Serial.print("SEQ PROFILE ");
                Serial.println(detection::detectionProfileName(_seqOutputConfig.profileKind));
                return;
            }

            detection::DetectionProfileKind profileKind = detection::DetectionProfileKind::TonalPulse;
            bool validProfile = true;
            if (equalsIgnoreCase(profileToken, "tonalpulse")) {
                profileKind = detection::DetectionProfileKind::TonalPulse;
            } else if (equalsIgnoreCase(profileToken, "amp")) {
                profileKind = detection::DetectionProfileKind::Amp;
            } else if (equalsIgnoreCase(profileToken, "chirp_experimental")) {
                profileKind = detection::DetectionProfileKind::ChirpExperimental;
            } else {
                validProfile = false;
            }

            if (!validProfile) {
                Serial.print("ERR SEQ unknown profile=");
                Serial.print(profileToken);
                Serial.println(" use PROFILE tonalpulse, PROFILE amp or PROFILE chirp_experimental");
                return;
            }

            _seqOutputConfig.profileKind = profileKind;
            Serial.print("OK SEQ PROFILE ");
            Serial.println(detection::detectionProfileName(_seqOutputConfig.profileKind));
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
            printSequenceSummary();
            stopSequenceTest();
            Serial.println("OK SEQ STOP");
            return;
        }

        if (equalsIgnoreCase(token, "START") || equalsIgnoreCase(token, "OBS")) {
            _pendingSequenceStart = {};
            _pendingSequenceStart.active = true;
            _pendingSequenceStart.totalTrials = _seqOutputConfig.totalTrials;
            _pendingSequenceStart.periodMs = 2500;
            _pendingSequenceStart.windowEndOffsetMs = 2200;
            _pendingSequenceStart.toneHz = runtime::kDefaultChirpFrequencyHz;
            _pendingSequenceStart.durationMs = 100;
            _pendingSequenceStart.quiet = false;
            _pendingSequenceStart.showDetails = true;
            _pendingSequenceStart.diagMode = AnalyzerApp::SequenceDiagMode::Off;
            _pendingSequenceStart.setupLabel = _pendingSequenceStart.setupLabelStorage;
            _pendingSequenceStart.setupLabelStorage[0] = '\0';
            _pendingSequenceStart.sampleDumpEnabled = false;
            _pendingSequenceStart.sampleDumpFirstTrials = 2;
            _pendingSequenceStart.sampleDumpEveryNth = 0;
            _pendingSequenceStart.sampleDumpLeadMs = 50;
            _pendingSequenceStart.sampleDumpTailMs = 800;
            _pendingSequenceStart.sampleDumpStepMs = 1;
            _pendingSequenceStart.sampleDumpMaxRows = 5000;
            _pendingSequenceStart.startupDelayMs = 1000;
            _pendingSequenceStart.reportSettleMs = 500;
            _pendingSequenceStart.profileKind = _seqOutputConfig.profileKind;
            _pendingSequenceStart.externalEmitter = equalsIgnoreCase(token, "OBS");
            bool totalTrialsSet = false;
            bool profileSeen = false;
            bool profileValid = true;
            while ((token = strtok_r(nullptr, " ", &savePtr)) != nullptr) {
                if (!totalTrialsSet && strchr(token, '=') == nullptr && strspn(token, "0123456789") == strlen(token)) {
                    _pendingSequenceStart.totalTrials = static_cast<unsigned long>(strtoul(token, nullptr, 10));
                    totalTrialsSet = true;
                } else if (startsWithTokenIgnoreCase(token, "tries=")) {
                    _pendingSequenceStart.totalTrials = static_cast<unsigned long>(strtoul(token + 6, nullptr, 10));
                    totalTrialsSet = true;
                } else if (startsWithTokenIgnoreCase(token, "period=")) {
                    _pendingSequenceStart.periodMs = static_cast<unsigned long>(strtoul(token + 7, nullptr, 10));
                } else if (startsWithTokenIgnoreCase(token, "window=")) {
                    _pendingSequenceStart.windowEndOffsetMs = static_cast<unsigned long>(strtoul(token + 7, nullptr, 10));
                } else if (startsWithTokenIgnoreCase(token, "freq=")) {
                    _pendingSequenceStart.toneHz = static_cast<unsigned long>(strtoul(token + 5, nullptr, 10));
                } else if (startsWithTokenIgnoreCase(token, "dur=")) {
                    _pendingSequenceStart.durationMs = static_cast<unsigned long>(strtoul(token + 4, nullptr, 10));
                } else if (startsWithTokenIgnoreCase(token, "delay=") || startsWithTokenIgnoreCase(token, "warmup=")) {
                    _pendingSequenceStart.startupDelayMs = static_cast<unsigned long>(strtoul(strchr(token, '=') + 1, nullptr, 10));
                } else if (startsWithTokenIgnoreCase(token, "report_settle=") || startsWithTokenIgnoreCase(token, "settle=")) {
                    _pendingSequenceStart.reportSettleMs = static_cast<unsigned long>(strtoul(strchr(token, '=') + 1, nullptr, 10));
                } else if (equalsIgnoreCase(token, "quiet")) {
                    _pendingSequenceStart.quiet = true;
                } else if (equalsIgnoreCase(token, "show=0")) {
                    _pendingSequenceStart.showDetails = false;
                } else if (equalsIgnoreCase(token, "show=1")) {
                    _pendingSequenceStart.showDetails = true;
                } else if (startsWithTokenIgnoreCase(token, "profile=")) {
                    const char* profileValue = token + 8;
                    if (equalsIgnoreCase(profileValue, "tonalpulse")) {
                        _pendingSequenceStart.profileKind = detection::DetectionProfileKind::TonalPulse;
                        profileSeen = true;
                    } else if (equalsIgnoreCase(profileValue, "amp")) {
                        _pendingSequenceStart.profileKind = detection::DetectionProfileKind::Amp;
                        profileSeen = true;
                    } else if (equalsIgnoreCase(profileValue, "chirp_experimental")) {
                        _pendingSequenceStart.profileKind = detection::DetectionProfileKind::ChirpExperimental;
                        profileSeen = true;
                    } else {
                        profileValid = false;
                        Serial.print("ERR SEQ unknown profile=");
                        Serial.print(profileValue);
                        Serial.println(" use profile=tonalpulse, profile=amp or profile=chirp_experimental");
                        return;
                    }
                } else if (startsWithTokenIgnoreCase(token, "mode=")) {
                    bool valid = false;
                    _seqOutputConfig.mode = AnalyzerApp::sequenceOutputModeFromToken(token + 5, &valid);
                    if (!valid) {
                        Serial.print("ERR SEQ unknown mode=");
                        Serial.print(token + 5);
                        Serial.println(" use mode=quiet, mode=compact, mode=signalcheck, mode=full, mode=source, mode=inspect, mode=pattern or mode=dump");
                        return;
                    }
                    if (_seqOutputConfig.mode == AnalyzerApp::SeqOutputMode::Quiet) {
                        _seqOutputConfig.when = AnalyzerApp::SeqOutputWhen::Off;
                        _seqOutputConfig.verbosity = 0;
                    }
                } else if (startsWithTokenIgnoreCase(token, "when=")) {
                    bool valid = false;
                    _seqOutputConfig.when = AnalyzerApp::sequenceOutputWhenFromToken(token + 5, &valid);
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
                    _seqOutputConfig.verbosity = static_cast<uint8_t>(verbosity);
                } else if (startsWithTokenIgnoreCase(token, "diag=") || startsWithTokenIgnoreCase(token, "diagnostics=")) {
                    const char* valueToken = strchr(token, '=') != nullptr ? strchr(token, '=') + 1 : nullptr;
                    bool valid = false;
                    const bool enabled = parseOnOffToken(valueToken, &valid);
                    if (!valid) {
                        Serial.println("ERR SEQ diagnostics use diag=on|off");
                        return;
                    }
                    _seqOutputConfig.diagnosticsEnabled = enabled;
                } else if (startsWithTokenIgnoreCase(token, "sampleFirst=")) {
                    _pendingSequenceStart.sampleDumpEnabled = true;
                    _pendingSequenceStart.sampleDumpFirstTrials = static_cast<unsigned long>(strtoul(token + 12, nullptr, 10));
                } else if (startsWithTokenIgnoreCase(token, "sampleEvery=")) {
                    _pendingSequenceStart.sampleDumpEnabled = true;
                    _pendingSequenceStart.sampleDumpEveryNth = static_cast<unsigned long>(strtoul(token + 12, nullptr, 10));
                } else if (startsWithTokenIgnoreCase(token, "sampleLead=")) {
                    _pendingSequenceStart.sampleDumpEnabled = true;
                    _pendingSequenceStart.sampleDumpLeadMs = static_cast<unsigned long>(strtoul(token + 11, nullptr, 10));
                } else if (startsWithTokenIgnoreCase(token, "sampleTail=")) {
                    _pendingSequenceStart.sampleDumpEnabled = true;
                    _pendingSequenceStart.sampleDumpTailMs = static_cast<unsigned long>(strtoul(token + 11, nullptr, 10));
                } else if (startsWithTokenIgnoreCase(token, "sampleStep=")) {
                    _pendingSequenceStart.sampleDumpEnabled = true;
                    _pendingSequenceStart.sampleDumpStepMs = static_cast<unsigned long>(strtoul(token + 11, nullptr, 10));
                } else if (startsWithTokenIgnoreCase(token, "sampleMax=")) {
                    _pendingSequenceStart.sampleDumpEnabled = true;
                    _pendingSequenceStart.sampleDumpMaxRows = static_cast<unsigned long>(strtoul(token + 10, nullptr, 10));
                } else if (equalsIgnoreCase(token, "external")) {
                    // Observing an already-running external emitter is an explicit mode.
                    // Keep the current mode if the caller already requested OBS.
                    _pendingSequenceStart.externalEmitter = true;
                } else if (startsWithTokenIgnoreCase(token, "test=")) {
                    strncpy(_pendingSequenceStart.setupLabelStorage, token + 5, sizeof(_pendingSequenceStart.setupLabelStorage));
                    _pendingSequenceStart.setupLabelStorage[sizeof(_pendingSequenceStart.setupLabelStorage) - 1] = '\0';
                } else if (equalsIgnoreCase(token, "labels")) {
                    strncpy(_pendingSequenceStart.setupLabelStorage, "labels", sizeof(_pendingSequenceStart.setupLabelStorage));
                    _pendingSequenceStart.setupLabelStorage[sizeof(_pendingSequenceStart.setupLabelStorage) - 1] = '\0';
                }
            }

            if (!profileSeen) {
                _pendingSequenceStart.profileKind = _seqOutputConfig.profileKind;
            }
            if (!profileValid) {
                return;
            }
            _seqOutputConfig.totalTrials = _pendingSequenceStart.totalTrials;
            if (_pendingSequenceStart.setupLabelStorage[0] == '\0') {
                strncpy(_pendingSequenceStart.setupLabelStorage, TEST_SETUP_LABEL, sizeof(_pendingSequenceStart.setupLabelStorage));
                _pendingSequenceStart.setupLabelStorage[sizeof(_pendingSequenceStart.setupLabelStorage) - 1] = '\0';
            }
            _pendingSequenceStart.setupLabel = _pendingSequenceStart.setupLabelStorage;
            Serial.println("OK SEQ");
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
