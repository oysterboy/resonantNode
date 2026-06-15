#include "../../modes/analyzer/AnalyzerModeApp.h"
#include "AnalyzerText.h"

#include <Arduino.h>
#include <stdlib.h>
#include <string.h>

namespace {

//PARAM TUNING TEMPORARY
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

void printParamFieldStart(bool& firstField) {
    if (!firstField) {
        Serial.print("PARAM ");
    } else {
        Serial.print(" ");
    }
    firstField = true;
}

void printParamField(const char* name, float value, bool& firstField) {
    printParamFieldStart(firstField);
    Serial.print(name);
    Serial.print("=");
    Serial.print(value, 1);
}

void printParamField(const char* name, unsigned long value, bool& firstField) {
    printParamFieldStart(firstField);
    Serial.print(name);
    Serial.print("=");
    Serial.print(value);
}

void printParamField(const char* name, detection::FeatureStreamId value, bool& firstField) {
    printParamFieldStart(firstField);
    Serial.print(name);
    Serial.print("=");
    Serial.print(scalarObservedStreamDisplayName(value));
}

bool parseFeatureStreamToken(const char* token, detection::FeatureStreamId& out) {
    if (token == nullptr) {
        return false;
    }
    if (equalsIgnoreCase(token, "scalar") || equalsIgnoreCase(token, "amp") || equalsIgnoreCase(token, "amplitude") || equalsIgnoreCase(token, "ampenvelope")) {
        out = detection::FeatureStreamId::AmpEnvelope;
        return true;
    }
    if (equalsIgnoreCase(token, "freq") || equalsIgnoreCase(token, "freqscore") || equalsIgnoreCase(token, "frequencyscore")) {
        out = detection::FeatureStreamId::FrequencyScore;
        return true;
    }
    if (equalsIgnoreCase(token, "contrast") || equalsIgnoreCase(token, "freqcontrast") || equalsIgnoreCase(token, "frequencycontrast")) {
        out = detection::FeatureStreamId::FrequencyContrast;
        return true;
    }
    return false;
}

bool parseScalarTransientToken(const char* token, detection::ScalarTransientConfig& config) {
    if (startsWithTokenIgnoreCase(token, "scalar_observed_stream=") || startsWithTokenIgnoreCase(token, "scalarObservedStream=") || startsWithTokenIgnoreCase(token, "scalarStream=")) {
        const char* value = token + (startsWithTokenIgnoreCase(token, "scalar_observed_stream=") ? 23 : startsWithTokenIgnoreCase(token, "scalarObservedStream=") ? 21 : 13);
        detection::FeatureStreamId stream = config.observedStream;
        if (parseFeatureStreamToken(value, stream)) {
            config.observedStream = stream;
            return true;
        }
        return false;
    }
    if (startsWithTokenIgnoreCase(token, "scalar_onset_threshold=") || startsWithTokenIgnoreCase(token, "scalarOnsetThreshold=")) {
        config.onsetDetectionThreshold = strtof(token + (startsWithTokenIgnoreCase(token, "scalar_onset_threshold=") ? 23 : 21), nullptr);
        return true;
    }
    if (startsWithTokenIgnoreCase(token, "scalar_release_threshold=") || startsWithTokenIgnoreCase(token, "scalarReleaseThreshold=")) {
        config.onsetReleaseThreshold = strtof(token + (startsWithTokenIgnoreCase(token, "scalar_release_threshold=") ? 25 : 23), nullptr);
        return true;
    }
    if (startsWithTokenIgnoreCase(token, "scalar_cooldown_ms=") || startsWithTokenIgnoreCase(token, "scalarCooldownMs=")) {
        config.cooldownAfterOnsetMs = strtoul(token + (startsWithTokenIgnoreCase(token, "scalar_cooldown_ms=") ? 19 : 17), nullptr, 10);
        return true;
    }
    if (startsWithTokenIgnoreCase(token, "scalar_min_duration_ms=") || startsWithTokenIgnoreCase(token, "scalarMinDurationMs=")) {
        config.minTransientDurationMs = strtoul(token + (startsWithTokenIgnoreCase(token, "scalar_min_duration_ms=") ? 23 : 20), nullptr, 10);
        return true;
    }
    if (startsWithTokenIgnoreCase(token, "scalar_max_duration_ms=") || startsWithTokenIgnoreCase(token, "scalarMaxDurationMs=")) {
        config.maxTransientDurationMs = strtoul(token + (startsWithTokenIgnoreCase(token, "scalar_max_duration_ms=") ? 23 : 20), nullptr, 10);
        return true;
    }
    if (startsWithTokenIgnoreCase(token, "scalar_min_peak_strength=") || startsWithTokenIgnoreCase(token, "scalarMinPeakStrength=")) {
        config.minTransientPeakStrength = strtof(token + (startsWithTokenIgnoreCase(token, "scalar_min_peak_strength=") ? 25 : 22), nullptr);
        return true;
    }
    if (startsWithTokenIgnoreCase(token, "scalar_release_debounce_ms=") || startsWithTokenIgnoreCase(token, "scalarReleaseDebounceMs=")) {
        config.releaseDebounceMs = strtoul(token + (startsWithTokenIgnoreCase(token, "scalar_release_debounce_ms=") ? 27 : 24), nullptr, 10);
        return true;
    }
    return false;
}

} // namespace

void AnalyzerApp::printSequenceHelp() {
    //PARAM TUNING TEMPORARY
    Serial.println("CMD: SEQ help");
    Serial.println("CMD: SEQ");
    Serial.println("CMD: SEQ stop");
    Serial.println("SEQ IN: start [N|tries=N] [period=MS] [window=MS] [freq=HZ] [dur=MS] [delay=MS] [report_settle=MS] [test=LABEL]");
    Serial.println("SEQ IN: OBS start [N|tries=N] [period=2000] [window=1800] [freq=HZ] [dur=MS] [delay=MS] [report_settle=MS] [test=LABEL]");
    Serial.println("SEQ IN: TRIES N");
    Serial.println("SEQ IN: [profile=TonalPulseFreq|TonalPulseScalar|AmpExperimental]");
    Serial.println("SEQ IN: MODE quiet|trial|inspect|source|system|explain");
    Serial.println("SEQ IN: MODE quiet = no sequence output");
    Serial.println("SEQ IN: MODE trial = trial verdict view");
    Serial.println("SEQ IN: MODE system = trial verdict + system health");
    Serial.println("SEQ IN: MODE source = canonical detector source view");
    Serial.println("SEQ IN: MODE inspect = canonical detector report inspect");
    Serial.println("SEQ IN: MODE explain = canonical detector report explain");
    Serial.println("SEQ IN: PROFILE TonalPulseFreq|TonalPulseScalar|AmpExperimental");
    Serial.println("SEQ IN: DIAG on|off");
    Serial.println("SEQ IN: FREQBAND on|off");
    Serial.println("SEQ IN: FREQUPDATEEVERYSAMPLES 1|4|8|16");
    Serial.println("SEQ IN: WHEN off|miss|all");
    Serial.println("SEQ IN: VERBOSE 0|1|2 (0=compact, 1=summary, 2=deep debug)");
    Serial.println("SEQ IN: SUMMARY");
    Serial.println("SEQ IN: REPORT");
    Serial.println("SEQ IN: TRIES N");
    Serial.println("SEQ IN: STATUS");
    Serial.println("SEQ IN: [dumpSamples=0|1] [curveFormat=off|samples]");
    Serial.println("SEQ IN: [sampleFirst=N] [sampleEvery=N] [sampleLead=MS] [sampleTail=MS] [sampleStep=MS] [sampleMax=N]");
    Serial.println("SEQ OUT: SEQ start / SEQ running / SEQ_TRIAL / SEQ_INSPECT / SEQ_EXPLAIN / SEQ_SOURCE / SEQ_SUMMARY / SEQ REPORT / AUDIO run");
    Serial.println("SEQ OUT: pending fields include onset_sample peak_sample release_sample peak_ms dur end_dt_ms freq_* scalar_*");
    Serial.println("SEQ OBS: passive observe mode for an already-running external emitter");
    Serial.println("SEQ IN: PROFILE TonalPulseFreq|TonalPulseScalar|AmpExperimental");
    Serial.println("SEQ PROFILE TonalPulseFreq");
    Serial.println("SEQ PROFILE TonalPulseScalar");
    Serial.println("SEQ PROFILE AmpExperimental");
    Serial.println("SEQ PARAM: freqScore=18000 freqContrast=50.0 freqReleaseScore=12000 freqReleaseContrast=50.0 scalar_observed_stream=Scalar scalar_onset_threshold=20000 scalar_release_threshold=5000 scalar_cooldown_ms=50 scalar_min_duration_ms=60 scalar_max_duration_ms=300 scalar_min_peak_strength=0 scalar_release_debounce_ms=30");
    Serial.println("SEQ PARAM STATUS");
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
        Serial.println("CMD: PARAM freqScore=18000 freqContrast=50.0 freqReleaseScore=12000 freqReleaseContrast=50.0 scalar_observed_stream=Scalar scalar_onset_threshold=20000 scalar_release_threshold=5000 scalar_cooldown_ms=50 scalar_min_duration_ms=60 scalar_max_duration_ms=300 scalar_min_peak_strength=0 scalar_release_debounce_ms=30");
        Serial.println("CMD: PARAM STATUS");
        Serial.println("CMD: EMIT CHIRP freq=3200 dur=100");
        Serial.println("CMD: EMIT MODE REMOTE");
        Serial.println("CMD: EMIT MODE AUTO interval=2000 freq=3200 dur=100");
        Serial.println("CMD: EMIT SWEEP start=3000 stop=3500 step=100 dur=80 pause=1000");
        Serial.println("CMD: RAWBAND contrast f=3200 dur=100 post=1000 decim=4");
        Serial.println("CMD: RAW trigger f=3200 dur=100 post=1000 dump=bin");
        Serial.println("CMD: SEQ");
        Serial.println("CMD: SEQ help");
        Serial.println("CMD: SEQ stop");
        return;
    }

    if (startsWithTokenIgnoreCase(line, "PARAM")) {
        //PARAM TUNING TEMPORARY
        strncpy(_commandScratch, line, sizeof(_commandScratch));
        _commandScratch[sizeof(_commandScratch) - 1] = '\0';

        char* savePtr = nullptr;
        char* token = strtok_r(_commandScratch, " ", &savePtr);
        token = strtok_r(nullptr, " ", &savePtr);
        if (token == nullptr || equalsIgnoreCase(token, "STATUS")) {
            printParamStatus();
            return;
        }

        FrequencyMatchCriteria::Values freqTuning = _analyzerTuning.frequencyMatch;
        detection::ScalarTransientConfig scalarTuning = _analyzerTuning.scalarTransient;
        const FrequencyMatchCriteria::Values oldFreqTuning = freqTuning;
        const detection::ScalarTransientConfig oldScalarTuning = scalarTuning;
        bool printedAny = false;

        do {
            FrequencyMatchCriteria::parseToken(token, freqTuning);
            parseScalarTransientToken(token, scalarTuning);
        } while ((token = strtok_r(nullptr, " ", &savePtr)) != nullptr);

        if (freqTuning.attackScoreMin != oldFreqTuning.attackScoreMin) {
            printParamField("freqScore", freqTuning.attackScoreMin, printedAny);
        }
        if (freqTuning.attackContrastMin != oldFreqTuning.attackContrastMin) {
            printParamField("freqContrast", freqTuning.attackContrastMin, printedAny);
        }
        if (freqTuning.releaseScoreMin != oldFreqTuning.releaseScoreMin) {
            printParamField("freqReleaseScore", freqTuning.releaseScoreMin, printedAny);
        }
        if (freqTuning.releaseContrastMin != oldFreqTuning.releaseContrastMin) {
            printParamField("freqReleaseContrast", freqTuning.releaseContrastMin, printedAny);
        }
        if (scalarTuning.observedStream != oldScalarTuning.observedStream) {
            printParamField("scalar_observed_stream", scalarTuning.observedStream, printedAny);
        }
        if (scalarTuning.onsetDetectionThreshold != oldScalarTuning.onsetDetectionThreshold) {
            printParamField("scalar_onset_threshold", scalarTuning.onsetDetectionThreshold, printedAny);
        }
        if (scalarTuning.onsetReleaseThreshold != oldScalarTuning.onsetReleaseThreshold) {
            printParamField("scalar_release_threshold", scalarTuning.onsetReleaseThreshold, printedAny);
        }
        if (scalarTuning.cooldownAfterOnsetMs != oldScalarTuning.cooldownAfterOnsetMs) {
            printParamField("scalar_cooldown_ms", scalarTuning.cooldownAfterOnsetMs, printedAny);
        }
        if (scalarTuning.minTransientDurationMs != oldScalarTuning.minTransientDurationMs) {
            printParamField("scalar_min_duration_ms", scalarTuning.minTransientDurationMs, printedAny);
        }
        if (scalarTuning.maxTransientDurationMs != oldScalarTuning.maxTransientDurationMs) {
            printParamField("scalar_max_duration_ms", scalarTuning.maxTransientDurationMs, printedAny);
        }
        if (scalarTuning.minTransientPeakStrength != oldScalarTuning.minTransientPeakStrength) {
            printParamField("scalar_min_peak_strength", scalarTuning.minTransientPeakStrength, printedAny);
        }
        if (scalarTuning.releaseDebounceMs != oldScalarTuning.releaseDebounceMs) {
            printParamField("scalar_release_debounce_ms", scalarTuning.releaseDebounceMs, printedAny);
        }

        _analyzerTuning.frequencyMatch = freqTuning;
        _analyzerTuning.scalarTransient = scalarTuning;
        if (printedAny) {
            Serial.println();
        } else {
            Serial.println("PARAM no_change");
        }
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
                Serial.println("ERR SEQ unknown mode use MODE quiet|trial|inspect|source|system|explain");
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

            detection::DetectionProfileKind profileKind = detection::DetectionProfileKind::TonalPulseFreq;
            const bool validProfile = detection::detectionProfileKindFromName(profileToken, profileKind);

            if (!validProfile) {
                Serial.print("ERR SEQ unknown profile=");
                Serial.print(profileToken);
                Serial.println(" use PROFILE TonalPulseFreq, PROFILE TonalPulseScalar or PROFILE AmpExperimental");
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

        if (equalsIgnoreCase(token, "SUMMARY")) {
            printSequenceSummaryClean();
            return;
        }

        if (equalsIgnoreCase(token, "REPORT")) {
            printSequenceReport();
            return;
        }

        if (equalsIgnoreCase(token, "STOP")) {
            printSequenceSummaryClean();
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
                    if (detection::detectionProfileKindFromName(profileValue, _pendingSequenceStart.profileKind)) {
                        profileSeen = true;
                    } else {
                        profileValid = false;
                        Serial.print("ERR SEQ unknown profile=");
                        Serial.print(profileValue);
                        Serial.println(" use profile=TonalPulseFreq, profile=TonalPulseScalar or profile=AmpExperimental");
                        return;
                    }
                } else if (startsWithTokenIgnoreCase(token, "mode=")) {
                    bool valid = false;
                    _seqOutputConfig.mode = AnalyzerApp::sequenceOutputModeFromToken(token + 5, &valid);
                    if (!valid) {
                        Serial.print("ERR SEQ unknown mode=");
                        Serial.print(token + 5);
                        Serial.println(" use mode=quiet, mode=trial, mode=inspect, mode=source, mode=system or mode=explain");
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

    Serial.print("EVT analyzer_unknown_cmd line=");
    Serial.println(line);
}
