#include "../../modes/analyzer/AnalyzerModeApp.h"
#include "AnalyzerText.h"

#include <Arduino.h>
#include <stdlib.h>
#include <string.h>

bool waitForEmitterAck(const char* expectedPrefix, unsigned long timeoutMs);

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

void printParamField(const char* name, bool value, bool& firstField) {
    printParamFieldStart(firstField);
    Serial.print(name);
    Serial.print("=");
    Serial.print(value ? "1" : "0");
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
    if (equalsIgnoreCase(token, "ampmagnitude") || equalsIgnoreCase(token, "amp_magnitude")) {
        out = detection::FeatureStreamId::AmpMagnitude;
        return true;
    }
    if (equalsIgnoreCase(token, "freqtarget") || equalsIgnoreCase(token, "frequencytarget")) {
        out = detection::FeatureStreamId::FrequencyTarget;
        return true;
    }
    if (equalsIgnoreCase(token, "contrast") || equalsIgnoreCase(token, "freqcontrast") || equalsIgnoreCase(token, "frequencycontrast")) {
        out = detection::FeatureStreamId::FrequencyContrast;
        return true;
    }
    return false;
}

// A plain decimal with nothing after it; on anything else the value is left
// unchanged (issue #31 R2-4: "x" used to become 0).
template <typename T>
bool parseUnsignedValue(const char* text, T& out) {
    if (text == nullptr || *text < '0' || *text > '9') {
        return false;
    }
    char* end = nullptr;
    const unsigned long parsed = strtoul(text, &end, 10);
    if (*end != '\0') {
        return false;
    }
    out = static_cast<T>(parsed);
    return true;
}

// Every key below is matched with its trailing '=', so the value starts
// right after the first '='; counting key lengths by hand got four wrong.
const char* tokenValue(const char* token) {
    const char* equals = strchr(token, '=');
    return equals != nullptr ? equals + 1 : token + strlen(token);
}

bool parseScalarTransientToken(const char* token, detection::ScalarTransientConfig& config) {
    if (startsWithTokenIgnoreCase(token, "scalar_observed_stream=") || startsWithTokenIgnoreCase(token, "scalarObservedStream=") || startsWithTokenIgnoreCase(token, "scalarStream=")) {
        const char* value = tokenValue(token);
        detection::FeatureStreamId stream = config.observedStream;
        if (parseFeatureStreamToken(value, stream)) {
            config.observedStream = stream;
            return true;
        }
        return false;
    }
    if (startsWithTokenIgnoreCase(token, "scalar_onset_threshold=") || startsWithTokenIgnoreCase(token, "scalarOnsetThreshold=")) {
        return FrequencyMatchCriteria::parseFiniteFloat(tokenValue(token), config.onsetDetectionThreshold);
    }
    if (startsWithTokenIgnoreCase(token, "scalar_release_threshold=") || startsWithTokenIgnoreCase(token, "scalarReleaseThreshold=")) {
        return FrequencyMatchCriteria::parseFiniteFloat(tokenValue(token), config.onsetReleaseThreshold);
    }
    if (startsWithTokenIgnoreCase(token, "scalar_cooldown_ms=") || startsWithTokenIgnoreCase(token, "scalarCooldownMs=")) {
        return parseUnsignedValue(tokenValue(token), config.cooldownAfterOnsetMs);
    }
    if (startsWithTokenIgnoreCase(token, "scalar_min_duration_ms=") || startsWithTokenIgnoreCase(token, "scalarMinDurationMs=")) {
        return parseUnsignedValue(tokenValue(token), config.minTransientDurationMs);
    }
    if (startsWithTokenIgnoreCase(token, "scalar_max_duration_ms=") || startsWithTokenIgnoreCase(token, "scalarMaxDurationMs=")) {
        return parseUnsignedValue(tokenValue(token), config.maxTransientDurationMs);
    }
    if (startsWithTokenIgnoreCase(token, "scalar_min_peak_strength=") || startsWithTokenIgnoreCase(token, "scalarMinPeakStrength=")) {
        return FrequencyMatchCriteria::parseFiniteFloat(tokenValue(token), config.minTransientPeakStrength);
    }
    if (startsWithTokenIgnoreCase(token, "scalar_release_debounce_ms=") || startsWithTokenIgnoreCase(token, "scalarReleaseDebounceMs=")) {
        return parseUnsignedValue(tokenValue(token), config.releaseDebounceMs);
    }
    if (startsWithTokenIgnoreCase(token, "scalar_require_carrier_quality=") || startsWithTokenIgnoreCase(token, "scalarRequireCarrierQuality=")) {
        const char* value = tokenValue(token);
        bool valid = false;
        config.requireCarrierQuality = parseOnOffToken(value, &valid);
        return valid;
    }
    if (startsWithTokenIgnoreCase(token, "scalar_require_min_strength=") || startsWithTokenIgnoreCase(token, "scalarRequireMinStrength=")) {
        const char* value = tokenValue(token);
        bool valid = false;
        config.requireMinStrength = parseOnOffToken(value, &valid);
        return valid;
    }
    if (startsWithTokenIgnoreCase(token, "scalar_min_strength=") || startsWithTokenIgnoreCase(token, "scalarMinStrength=")) {
        return FrequencyMatchCriteria::parseFiniteFloat(tokenValue(token), config.minMatchedMeanStrength);
    }
    if (startsWithTokenIgnoreCase(token, "scalar_min_release_coverage_ms=") || startsWithTokenIgnoreCase(token, "scalarMinReleaseCoverageMs=")) {
        return parseUnsignedValue(tokenValue(token), config.minCoverageAboveReleaseMs);
    }
    if (startsWithTokenIgnoreCase(token, "scalar_min_longest_island_ms=") || startsWithTokenIgnoreCase(token, "scalarMinLongestIslandMs=")) {
        return parseUnsignedValue(tokenValue(token), config.minLongestIslandMs);
    }
    if (startsWithTokenIgnoreCase(token, "scalar_max_gap_ms=") || startsWithTokenIgnoreCase(token, "scalarMaxGapMs=")) {
        return parseUnsignedValue(tokenValue(token), config.maxGapMs);
    }
    return false;
}

} // namespace

void AnalyzerApp::printSequenceHelp() {
    Serial.println("CMD: SEQ help");
    Serial.println("CMD: SEQ");
    Serial.println("CMD: SEQ stop");
    Serial.println("SEQ IN: start [N|tries=N] [period=MS] [window=MS] [freq=HZ] [dur=MS] [delay=MS] [report_settle=MS] [test=LABEL]");
    Serial.println("SEQ IN: OBS start [N|tries=N] [period=2000] [window=1800] [freq=HZ] [dur=MS] [delay=MS] [report_settle=MS] [test=LABEL]");
    Serial.println("SEQ IN: pulse train per trial (DET-011): start ... dur=PULSE_MS pulses=N gap=MS [ramp=MS] [hop=F1,F2,...]");
    Serial.println("SEQ IN: sweep inside the chirp (OUT-005): start ... freq=START_HZ dur=MS sweep=END_HZ");
    Serial.println("SEQ IN: TRIES N");
    Serial.println("SEQ IN: [profile=TonalPulseFreq|TonalPulseScalar|AmpExperimental]");
    Serial.println("SEQ IN: MODE quiet|trial|inspect|source|system|detail");
    Serial.println("SEQ IN: MODE quiet = no sequence output");
    Serial.println("SEQ IN: MODE trial = trial verdict view");
    Serial.println("SEQ IN: MODE system = trial verdict + system health");
    Serial.println("SEQ IN: MODE source = source wrapper, then SEQ_SOURCE_CORE and SEQ_SOURCE_SPEC");
    Serial.println("SEQ IN: MODE inspect = canonical detector report inspect");
    Serial.println("SEQ IN: MODE detail = canonical trial/source/inspect report");
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
    Serial.println("SEQ IN: [sampleFirst=N] [sampleEvery=N] [sampleLead=MS] [sampleTail=MS] [sampleStep=MS] [sampleMax=N]");
    Serial.println("SEQ IN: sample dump is detector-input curve tooling, not RAW capture, not the audio envelope curve, not DetectorReport, not SEQ_SOURCE / SEQ_INSPECT / SEQ_DETAIL, and not canonical Analyzer truth");
    Serial.println("SEQ IN: sampleMax is the row budget over all dumped trials (default 5000); one trial must also fit the 256-row sampleRows buffer (default sampleStep=4)");
    Serial.println("SEQ OUT: SEQ start / SEQ running / SEQ_TRIAL / SEQ_INSPECT / SEQ_DETAIL / SEQ_SOURCE / SEQ_SOURCE_CORE / SEQ_SOURCE_SPEC / SEQ_SUMMARY / SEQ REPORT / AUDIO run");
    Serial.println("SEQ OUT: SAMPLES_BEGIN / SAMPLES_NOTE / SAMPLES_END");
    Serial.println("SEQ OUT: sample fields are t,value (value is the selected detector input stream)");
    Serial.println("SEQ OBS: passive observe mode for an already-running external emitter");
    Serial.println("SEQ IN: PROFILE TonalPulseFreq|TonalPulseScalar|AmpExperimental");
    Serial.println("SEQ PROFILE TonalPulseFreq");
    Serial.println("SEQ PROFILE TonalPulseScalar");
    Serial.println("SEQ PROFILE AmpExperimental");
    Serial.println("SEQ PARAM: freqScore=18000 freqContrast=50.0 freqReleaseScore=12000 freqReleaseContrast=50.0 scalar_observed_stream=AmpEnvelope|FrequencyTarget scalar_onset_threshold=18000 scalar_release_threshold=12000 scalar_cooldown_ms=50 scalar_min_duration_ms=60 scalar_max_duration_ms=300 scalar_min_peak_strength=15000 scalar_release_debounce_ms=30 scalar_require_carrier_quality=0|1 scalar_require_min_strength=0|1 scalar_min_strength=0 scalar_min_release_coverage_ms=90 scalar_min_longest_island_ms=80 scalar_max_gap_ms=10");
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
        Serial.println("CMD: PARAM freqScore=18000 freqContrast=50.0 freqReleaseScore=12000 freqReleaseContrast=50.0 scalar_observed_stream=AmpEnvelope|FrequencyTarget scalar_onset_threshold=18000 scalar_release_threshold=12000 scalar_cooldown_ms=50 scalar_min_duration_ms=60 scalar_max_duration_ms=300 scalar_min_peak_strength=15000 scalar_release_debounce_ms=30 scalar_require_carrier_quality=0|1 scalar_require_min_strength=0|1 scalar_min_strength=0 scalar_min_release_coverage_ms=90 scalar_min_longest_island_ms=80 scalar_max_gap_ms=10");
        Serial.println("CMD: PARAM STATUS");
        Serial.println("CMD: EMIT CHIRP freq=3200 dur=100");
        Serial.println("CMD: EMIT MODE REMOTE");
        Serial.println("CMD: EMIT MODE AUTO interval=2000 freq=3200 dur=100");
        Serial.println("CMD: EMIT SWEEP start=3000 stop=3500 step=100 dur=80 pause=1000");
        Serial.println("CMD: EMIT PULSES n pulse_ms gap_ms [freq=3200] [ramp=5] (e.g. EMIT PULSES 5 10 20)");
        Serial.println("CMD: RAW trigger f=3200 dur=100 post=1000 [pre=MS] mode=feat (default)|mode=pcm|mode=i2s|mode=both [pulses=N gap=MS hop=F1,F2,...] (pulses= plays a PULSES train instead of one CHIRP; dur= is then the pulse length)");
        Serial.println("CMD: SEQ");
        Serial.println("CMD: SEQ help");
        Serial.println("CMD: SEQ stop");
        Serial.println("CMD: SELFTEST | SELFTEST chirp n=5 gap_ms=1500 | SELFTEST help");
        return;
    }

    if (startsWithTokenIgnoreCase(line, "SELFTEST")) {
        handleSelfTestCommand(line);
        return;
    }

    if (startsWithTokenIgnoreCase(line, "PARAM")) {
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
            const bool freqOk = FrequencyMatchCriteria::parseToken(token, freqTuning);
            const bool scalarOk = parseScalarTransientToken(token, scalarTuning);
            if (!freqOk && !scalarOk) {
                Serial.print("ERR PARAM token=");
                Serial.print(token);
                Serial.println(" (unknown key or invalid value; unchanged)");
            }
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
        if (scalarTuning.requireCarrierQuality != oldScalarTuning.requireCarrierQuality) {
            printParamField("scalar_require_carrier_quality", scalarTuning.requireCarrierQuality, printedAny);
        }
        if (scalarTuning.requireMinStrength != oldScalarTuning.requireMinStrength) {
            printParamField("scalar_require_min_strength", scalarTuning.requireMinStrength, printedAny);
        }
        if (scalarTuning.minMatchedMeanStrength != oldScalarTuning.minMatchedMeanStrength) {
            printParamField("scalar_min_strength", scalarTuning.minMatchedMeanStrength, printedAny);
        }
        if (scalarTuning.minCoverageAboveReleaseMs != oldScalarTuning.minCoverageAboveReleaseMs) {
            printParamField("scalar_min_release_coverage_ms", scalarTuning.minCoverageAboveReleaseMs, printedAny);
        }
        if (scalarTuning.minLongestIslandMs != oldScalarTuning.minLongestIslandMs) {
            printParamField("scalar_min_longest_island_ms", scalarTuning.minLongestIslandMs, printedAny);
        }
        if (scalarTuning.maxGapMs != oldScalarTuning.maxGapMs) {
            printParamField("scalar_max_gap_ms", scalarTuning.maxGapMs, printedAny);
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

    if (startsWithTokenIgnoreCase(line, "RAW")) {
        strncpy(_commandScratch, line, sizeof(_commandScratch));
        _commandScratch[sizeof(_commandScratch) - 1] = '\0';

        unsigned long toneHz = runtime::kDefaultChirpFrequencyHz;
        unsigned long durationMs = 100;
        unsigned long postMs = 1000;
        unsigned long preMs = 0;
        unsigned long decim = 1;
        AnalyzerApp::RawCaptureMode rawMode = AnalyzerApp::RawCaptureMode::Features;
        AnalyzerApp::RawTriggerShape shape;

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
            } else if (startsWithTokenIgnoreCase(token, "pulses=")) {
                shape.pulseCount = static_cast<unsigned long>(strtoul(token + 7, nullptr, 10));
            } else if (startsWithTokenIgnoreCase(token, "gap=")) {
                shape.pulseGapMs = static_cast<unsigned long>(strtoul(token + 4, nullptr, 10));
            } else if (startsWithTokenIgnoreCase(token, "sweep=")) {
                shape.sweepToHz = static_cast<unsigned long>(strtoul(token + 6, nullptr, 10));
            } else if (startsWithTokenIgnoreCase(token, "hop=")) {
                // Same validation as SEQ start: digits and commas, fits the buffer.
                const char* list = token + 4;
                const size_t len = strlen(list);
                bool ok = len > 0 && len < sizeof(shape.hop);
                for (size_t i = 0; ok && i < len; ++i) {
                    ok = (list[i] >= '0' && list[i] <= '9') || list[i] == ',';
                }
                if (!ok) {
                    Serial.print("ERR RAW bad value token=");
                    Serial.println(token);
                    return;
                }
                strncpy(shape.hop, list, sizeof(shape.hop) - 1);
                shape.hop[sizeof(shape.hop) - 1] = '\0';
            } else if (startsWithTokenIgnoreCase(token, "mode=")) {
                const char* modeValue = token + 5;
                if (equalsIgnoreCase(modeValue, "feat") || equalsIgnoreCase(modeValue, "features") || equalsIgnoreCase(modeValue, "csv")) {
                    rawMode = AnalyzerApp::RawCaptureMode::Features;
                } else if (equalsIgnoreCase(modeValue, "pcm")) {
                    rawMode = AnalyzerApp::RawCaptureMode::Pcm;
                } else if (equalsIgnoreCase(modeValue, "i2s") || equalsIgnoreCase(modeValue, "raw")) {
                    rawMode = AnalyzerApp::RawCaptureMode::I2s;
                } else if (equalsIgnoreCase(modeValue, "both")) {
                    rawMode = AnalyzerApp::RawCaptureMode::Both;
                }
            }
        }

        if (runRawTrigger(toneHz, durationMs, postMs, preMs, decim, rawMode, shape)) {
            Serial.println("OK RAW");
        }
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
            Serial.println("ERR SEQ unknown mode use MODE quiet|trial|inspect|source|system|detail");
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
            // Parsed into copies and activated only when every token is valid
            // (issue #31): on an ERR the run used to start anyway, and the
            // output settings parsed before the bad token stuck.
            _pendingSequenceStart.active = false;
            AnalyzerApp::SeqOutputConfig outputConfig = _seqOutputConfig;
            _pendingSequenceStart.totalTrials = _seqOutputConfig.totalTrials;
            _pendingSequenceStart.periodMs = 2500;
            _pendingSequenceStart.windowEndOffsetMs = 2200;
            _pendingSequenceStart.toneHz = runtime::kDefaultChirpFrequencyHz;
            _pendingSequenceStart.durationMs = 100;
            _pendingSequenceStart.pulseCount = 0;
            _pendingSequenceStart.pulseGapMs = 0;
            _pendingSequenceStart.pulseRampSet = false;
            _pendingSequenceStart.pulseRampMs = 0;
            _pendingSequenceStart.pulseHop[0] = '\0';
            _pendingSequenceStart.sweepToHz = 0;
            _pendingSequenceStart.bandLowHz = 0;
            _pendingSequenceStart.bandHighHz = 0;
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
            // 4 ms: the default 850 ms window in 213 rows, inside the 256-row
            // per-trial buffer; at 1 ms (851 rows) every default dump was
            // refused (decision 2026-10-13, issue #31 B12).
            _pendingSequenceStart.sampleDumpStepMs = 4;
            _pendingSequenceStart.sampleDumpMaxRows = 5000;
            _pendingSequenceStart.startupDelayMs = 1000;
            _pendingSequenceStart.reportSettleMs = 500;
            _pendingSequenceStart.profileKind = _seqOutputConfig.profileKind;
            _pendingSequenceStart.externalEmitter = equalsIgnoreCase(token, "OBS");
            bool totalTrialsSet = false;
            bool windowSet = false;
            bool profileSeen = false;
            bool profileValid = true;
            while ((token = strtok_r(nullptr, " ", &savePtr)) != nullptr) {
                if (!totalTrialsSet && strchr(token, '=') == nullptr && strspn(token, "0123456789") == strlen(token)) {
                    _pendingSequenceStart.totalTrials = static_cast<unsigned long>(strtoul(token, nullptr, 10));
                    totalTrialsSet = true;
                } else if (startsWithTokenIgnoreCase(token, "tries=")) {
                    if (!parseUnsignedValue(token + 6, _pendingSequenceStart.totalTrials)) {
                        Serial.print("ERR SEQ bad value token=");
                        Serial.println(token);
                        return;
                    }
                    totalTrialsSet = true;
                } else if (startsWithTokenIgnoreCase(token, "period=")) {
                    if (!parseUnsignedValue(token + 7, _pendingSequenceStart.periodMs)) {
                        Serial.print("ERR SEQ bad value token=");
                        Serial.println(token);
                        return;
                    }
                } else if (startsWithTokenIgnoreCase(token, "window=")) {
                    windowSet = true;
                    if (!parseUnsignedValue(token + 7, _pendingSequenceStart.windowEndOffsetMs)) {
                        Serial.print("ERR SEQ bad value token=");
                        Serial.println(token);
                        return;
                    }
                } else if (startsWithTokenIgnoreCase(token, "freq=")) {
                    if (!parseUnsignedValue(token + 5, _pendingSequenceStart.toneHz)) {
                        Serial.print("ERR SEQ bad value token=");
                        Serial.println(token);
                        return;
                    }
                } else if (startsWithTokenIgnoreCase(token, "dur=")) {
                    if (!parseUnsignedValue(token + 4, _pendingSequenceStart.durationMs)) {
                        Serial.print("ERR SEQ bad value token=");
                        Serial.println(token);
                        return;
                    }
                } else if (startsWithTokenIgnoreCase(token, "band=")) {
                    const char* spec = token + 5;
                    if (equalsIgnoreCase(spec, "off")) {
                        _pendingSequenceStart.bandLowHz = 0;
                        _pendingSequenceStart.bandHighHz = 0;
                    } else {
                        char* dash = nullptr;
                        const unsigned long lo = strtoul(spec, &dash, 10);
                        const unsigned long hi = (dash != nullptr && *dash == '-') ? strtoul(dash + 1, nullptr, 10) : 0UL;
                        if (lo == 0 || hi < lo || hi > 7900UL) {
                            Serial.print("ERR SEQ bad value token=");
                            Serial.println(token);
                            return;
                        }
                        _pendingSequenceStart.bandLowHz = lo;
                        _pendingSequenceStart.bandHighHz = hi;
                    }
                } else if (startsWithTokenIgnoreCase(token, "hop=")) {
                    const char* list = token + 4;
                    const size_t len = strlen(list);
                    bool ok = len > 0 && len < sizeof(_pendingSequenceStart.pulseHop);
                    for (size_t i = 0; ok && i < len; ++i) {
                        ok = (list[i] >= '0' && list[i] <= '9') || list[i] == ',';
                    }
                    if (!ok) {
                        Serial.print("ERR SEQ bad value token=");
                        Serial.println(token);
                        return;
                    }
                    strncpy(_pendingSequenceStart.pulseHop, list, sizeof(_pendingSequenceStart.pulseHop) - 1);
                    _pendingSequenceStart.pulseHop[sizeof(_pendingSequenceStart.pulseHop) - 1] = '\0';
                } else if (startsWithTokenIgnoreCase(token, "sweep=")) {
                    if (!parseUnsignedValue(token + 6, _pendingSequenceStart.sweepToHz) ||
                        _pendingSequenceStart.sweepToHz > 7900UL) {
                        Serial.print("ERR SEQ bad value token=");
                        Serial.println(token);
                        return;
                    }
                } else if (startsWithTokenIgnoreCase(token, "pulses=")) {
                    if (!parseUnsignedValue(token + 7, _pendingSequenceStart.pulseCount)) {
                        Serial.print("ERR SEQ bad value token=");
                        Serial.println(token);
                        return;
                    }
                } else if (startsWithTokenIgnoreCase(token, "gap=")) {
                    if (!parseUnsignedValue(token + 4, _pendingSequenceStart.pulseGapMs)) {
                        Serial.print("ERR SEQ bad value token=");
                        Serial.println(token);
                        return;
                    }
                } else if (startsWithTokenIgnoreCase(token, "ramp=")) {
                    if (!parseUnsignedValue(token + 5, _pendingSequenceStart.pulseRampMs)) {
                        Serial.print("ERR SEQ bad value token=");
                        Serial.println(token);
                        return;
                    }
                    _pendingSequenceStart.pulseRampSet = true;
                } else if (startsWithTokenIgnoreCase(token, "delay=") || startsWithTokenIgnoreCase(token, "warmup=")) {
                    if (!parseUnsignedValue(strchr(token, '=') + 1, _pendingSequenceStart.startupDelayMs)) {
                        Serial.print("ERR SEQ bad value token=");
                        Serial.println(token);
                        return;
                    }
                } else if (startsWithTokenIgnoreCase(token, "report_settle=") || startsWithTokenIgnoreCase(token, "settle=")) {
                    if (!parseUnsignedValue(strchr(token, '=') + 1, _pendingSequenceStart.reportSettleMs)) {
                        Serial.print("ERR SEQ bad value token=");
                        Serial.println(token);
                        return;
                    }
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
                    outputConfig.mode = AnalyzerApp::sequenceOutputModeFromToken(token + 5, &valid);
                    if (!valid) {
                        Serial.print("ERR SEQ unknown mode=");
                        Serial.print(token + 5);
                    Serial.println(" use mode=quiet, mode=trial, mode=inspect, mode=source, mode=system or mode=detail");
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
                } else if (startsWithTokenIgnoreCase(token, "diag=") || startsWithTokenIgnoreCase(token, "diagnostics=")) {
                    const char* valueToken = strchr(token, '=') != nullptr ? strchr(token, '=') + 1 : nullptr;
                    bool valid = false;
                    const bool enabled = parseOnOffToken(valueToken, &valid);
                    if (!valid) {
                        Serial.println("ERR SEQ diagnostics use diag=on|off");
                        return;
                    }
                    outputConfig.diagnosticsEnabled = enabled;
                } else if (startsWithTokenIgnoreCase(token, "sampleFirst=")) {
                    _pendingSequenceStart.sampleDumpEnabled = true;
                    if (!parseUnsignedValue(token + 12, _pendingSequenceStart.sampleDumpFirstTrials)) {
                        Serial.print("ERR SEQ bad value token=");
                        Serial.println(token);
                        return;
                    }
                } else if (startsWithTokenIgnoreCase(token, "sampleEvery=")) {
                    _pendingSequenceStart.sampleDumpEnabled = true;
                    if (!parseUnsignedValue(token + 12, _pendingSequenceStart.sampleDumpEveryNth)) {
                        Serial.print("ERR SEQ bad value token=");
                        Serial.println(token);
                        return;
                    }
                } else if (startsWithTokenIgnoreCase(token, "sampleLead=")) {
                    _pendingSequenceStart.sampleDumpEnabled = true;
                    if (!parseUnsignedValue(token + 11, _pendingSequenceStart.sampleDumpLeadMs)) {
                        Serial.print("ERR SEQ bad value token=");
                        Serial.println(token);
                        return;
                    }
                } else if (startsWithTokenIgnoreCase(token, "sampleTail=")) {
                    _pendingSequenceStart.sampleDumpEnabled = true;
                    if (!parseUnsignedValue(token + 11, _pendingSequenceStart.sampleDumpTailMs)) {
                        Serial.print("ERR SEQ bad value token=");
                        Serial.println(token);
                        return;
                    }
                } else if (startsWithTokenIgnoreCase(token, "sampleStep=")) {
                    _pendingSequenceStart.sampleDumpEnabled = true;
                    if (!parseUnsignedValue(token + 11, _pendingSequenceStart.sampleDumpStepMs)) {
                        Serial.print("ERR SEQ bad value token=");
                        Serial.println(token);
                        return;
                    }
                } else if (startsWithTokenIgnoreCase(token, "sampleMax=")) {
                    _pendingSequenceStart.sampleDumpEnabled = true;
                    if (!parseUnsignedValue(token + 10, _pendingSequenceStart.sampleDumpMaxRows)) {
                        Serial.print("ERR SEQ bad value token=");
                        Serial.println(token);
                        return;
                    }
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
                } else if (equalsIgnoreCase(token, "start")) {
                    // "SEQ OBS start ..." (tools/bench/soak.py): the verb after OBS.
                } else {
                    // Unknown keys used to be ignored, so "perod=3000" ran on
                    // defaults with OK SEQ (issue #31 R2-19).
                    Serial.print("ERR SEQ unknown token=");
                    Serial.println(token);
                    return;
                }
            }

            // Same bounds the Emitter applies to PULSES; refused here so a
            // run never starts with every trial rejected on the Emitter.
            if (_pendingSequenceStart.pulseCount > 0 &&
                (_pendingSequenceStart.pulseCount > 50 || _pendingSequenceStart.durationMs == 0 ||
                 _pendingSequenceStart.durationMs > 1000 || _pendingSequenceStart.pulseGapMs == 0 ||
                 _pendingSequenceStart.pulseGapMs > 2000 ||
                 (_pendingSequenceStart.pulseRampSet && _pendingSequenceStart.pulseRampMs > 50))) {
                Serial.println("ERR SEQ pulses use pulses=1-50 dur=1-1000 gap=1-2000 [ramp=0-50]");
                return;
            }

            // OBS default window leaves room for the settle before the next
            // trigger: 2200 + 500 > 2500 credited early chirps of trial N+1
            // to trial N (issue #31 R2-5). An explicit window= is kept (and
            // warned about at start).
            if (_pendingSequenceStart.externalEmitter && !windowSet &&
                _pendingSequenceStart.periodMs > _pendingSequenceStart.reportSettleMs) {
                _pendingSequenceStart.windowEndOffsetMs = _pendingSequenceStart.periodMs - _pendingSequenceStart.reportSettleMs;
            }

            if (!profileSeen) {
                _pendingSequenceStart.profileKind = outputConfig.profileKind;
            }
            if (!profileValid) {
                return;
            }
            outputConfig.totalTrials = _pendingSequenceStart.totalTrials;
            if (_pendingSequenceStart.setupLabelStorage[0] == '\0') {
                strncpy(_pendingSequenceStart.setupLabelStorage, TEST_SETUP_LABEL, sizeof(_pendingSequenceStart.setupLabelStorage));
                _pendingSequenceStart.setupLabelStorage[sizeof(_pendingSequenceStart.setupLabelStorage) - 1] = '\0';
            }
            _pendingSequenceStart.setupLabel = _pendingSequenceStart.setupLabelStorage;
            _seqOutputConfig = outputConfig;
            _pendingSequenceStart.active = true;
            Serial.println("OK SEQ");
            return;
        }

    }

    if (startsWithTokenIgnoreCase(line, "EMIT ")) {
        if (startsWithTokenIgnoreCase(line, "EMIT CHIRP")) {
            sendEmitterCommand("MODE REMOTE");
            if (!waitForEmitterAck("OK MODE REMOTE", 1500)) {
                Serial.println("ERR EMIT CHIRP emitter_remote_claim_timeout");
                return;
            }
            sendEmitterCommand(line + 5);
            Serial.println("OK EMIT CHIRP");
            return;
        }
        if (startsWithTokenIgnoreCase(line, "EMIT PULSES")) {
            sendEmitterCommand("MODE REMOTE");
            if (!waitForEmitterAck("OK MODE REMOTE", 1500)) {
                Serial.println("ERR EMIT PULSES emitter_remote_claim_timeout");
                return;
            }
            sendEmitterCommand(line + 5);
            Serial.println("OK EMIT PULSES");
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
