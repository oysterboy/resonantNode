#include "AnalyzerModeApp.h"
#include "../../detection/analyzer/AnalyzerText.h"

#include <Arduino.h>
#include <stdlib.h>
#include <string.h>
#include "../../app/ParamCommands.h"
#include "../../app/SerialOut.h"
#include "../../app/TextParse.h"

#include "tools/AnalyzerEmitterControl.h"
#include "../../app/EmitterLimits.h"

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
    serialOut().println("CMD: SEQ help");
    serialOut().println("CMD: SEQ");
    serialOut().println("CMD: SEQ stop");
    serialOut().println("SEQ IN: start [N|tries=N] [period=MS] [window=MS] [freq=HZ] [dur=MS] [delay=MS] [report_settle=MS] [band=LOW-HIGH|off] [test=LABEL]");
    serialOut().println("SEQ IN: OBS start [N|tries=N] [period=2000] [window=1800] [freq=HZ] [dur=MS] [delay=MS] [report_settle=MS] [test=LABEL]");
    serialOut().println("SEQ IN: pulse train per trial (DET-011): start ... dur=PULSE_MS pulses=N gap=MS [ramp=MS] [hop=F1,F2,...]");
    serialOut().println("SEQ IN: sweep inside the chirp (OUT-005): start ... freq=START_HZ dur=MS sweep=END_HZ");
    serialOut().println("SEQ IN: TRIES N");
    serialOut().println("SEQ IN: [profile=TonalPulseScalar|AmpExperimental]");
    serialOut().println("SEQ IN: MODE quiet|trial|inspect|source|system|detail");
    serialOut().println("SEQ IN: MODE quiet = no sequence output");
    serialOut().println("SEQ IN: MODE trial = trial verdict view");
    serialOut().println("SEQ IN: MODE system = trial verdict + system health");
    serialOut().println("SEQ IN: MODE source = source wrapper, then SEQ_SOURCE_CORE and SEQ_SOURCE_SPEC");
    serialOut().println("SEQ IN: MODE inspect = canonical detector report inspect");
    serialOut().println("SEQ IN: MODE detail = canonical trial/source/inspect report");
    serialOut().println("SEQ IN: PROFILE TonalPulseScalar|AmpExperimental");
    serialOut().println("SEQ IN: DIAG on|off");
    serialOut().println("SEQ IN: FREQBAND on|off");
    serialOut().println("SEQ IN: FREQUPDATEEVERYSAMPLES 1|4|8|16");
    serialOut().println("SEQ IN: WHEN off|miss|all");
    serialOut().println("SEQ IN: VERBOSE 0|1|2 (0=compact, 1=summary, 2=deep debug)");
    serialOut().println("SEQ IN: SUMMARY");
    serialOut().println("SEQ IN: REPORT");
    serialOut().println("SEQ IN: TRIES N");
    serialOut().println("SEQ IN: STATUS");
    serialOut().println("SEQ IN: [sampleFirst=N] [sampleEvery=N] [sampleLead=MS] [sampleTail=MS] [sampleStep=MS] [sampleMax=N]");
    serialOut().println("SEQ IN: sample dump is detector-input curve tooling, not RAW capture, not the audio envelope curve, not DetectorReport, not SEQ_SOURCE / SEQ_INSPECT / SEQ_EXPLAIN, and not canonical Analyzer truth");
    serialOut().println("SEQ IN: sampleMax is the row budget over all dumped trials (default 5000); one trial must also fit the 256-row sampleRows buffer (default sampleStep=4)");
    serialOut().println("SEQ OUT: SEQ start / SEQ running / SEQ_TRIAL / SEQ_INSPECT / SEQ_EXPLAIN / SEQ_SOURCE_CORE / SEQ_SOURCE_SPEC / SEQ_SUMMARY / SEQ_STATUS / SEQ REPORT / AUDIO run");
    serialOut().println("SEQ OUT: SAMPLES_BEGIN / SAMPLES_NOTE / SAMPLES_END");
    serialOut().println("SEQ OUT: sample fields are t,value (value is the selected detector input stream)");
    serialOut().println("SEQ OBS: passive observe mode for an already-running external emitter");
    serialOut().println("SEQ IN: PROFILE TonalPulseScalar|AmpExperimental");
    serialOut().println("SEQ PROFILE TonalPulseScalar");
    serialOut().println("SEQ PROFILE AmpExperimental");
    serialOut().println("SEQ PARAM: PARAM SET detection.scalar_<key> <value> (PARAM LIST shows the keys, PARAM DUMP the values; applied at the next SEQ start)");
}

void AnalyzerApp::pollUsbConsole() {
    while (Serial.available() > 0) {
        if (!_usbLine.feed(static_cast<char>(Serial.read()))) {
            continue;
        }
        if (_usbLine.tooLong()) {
            serialOut().print("ERR line_too_long max=");
            serialOut().println(static_cast<unsigned long>(_usbLine.kCapacity));
        } else if (_usbLine.length() > 0) {
            handleUsbLine(_usbLine.line());
        }
    }
}

void AnalyzerApp::handleUsbLine(const char* line) {
    if (equalsIgnoreCase(line, "HELP")) {
        serialOut().println("CMD: PARAM LIST|GET <path>|SET <path> <value>|DUMP|RESET (detection.scalar_*, applied at the next SEQ start; SAVE|LOAD answer status=no_store)");
        serialOut().println("CMD: EMIT CHIRP freq=3200 dur=100");
        serialOut().println("CMD: EMIT MODE REMOTE");
        serialOut().println("CMD: EMIT MODE AUTO interval=2000 freq=3200 dur=100");
        serialOut().println("CMD: EMIT SWEEP start=3000 stop=3500 step=100 dur=80 pause=1000");
        serialOut().println("CMD: EMIT PULSES n pulse_ms gap_ms [freq=3200] [ramp=5] (e.g. EMIT PULSES 5 10 20)");
        serialOut().println("CMD: RAW trigger f=3200 dur=100 post=1000 [pre=MS] [decim=N] mode=feat (default)|mode=pcm|mode=i2s|mode=both [pulses=N gap=MS hop=F1,F2,...] [sweep=END_HZ] [band=LOW-HIGH|off] (pulses= plays a PULSES train instead of one CHIRP; dur= is then the pulse length)");
        serialOut().println("CMD: SEQ");
        serialOut().println("CMD: SEQ help");
        serialOut().println("CMD: SEQ stop");
        serialOut().println("CMD: SELFTEST | SELFTEST external | SELFTEST chirp n=5 gap_ms=1500 dur_ms=100 | SELFTEST help");
        return;
    }

    if (startsWithTokenIgnoreCase(line, "SELFTEST")) {
        handleSelfTestCommand(line);
        return;
    }

    if (startsWithTokenIgnoreCase(line, "PARAM")) {
        // The registry's grammar and the Node's lines (app/ParamCommands, step
        // 11e item 9). No store: the tuning is per run. A value takes effect at
        // the next SEQ start (effectiveSequenceProfile), so nothing applies here.
        param::handleParamCommand(line, _paramRegistry, nullptr, serialOut());
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
                if (!text_parse::parseUnsigned(text_parse::tokenValue(token), toneHz)) {
                    serialOut().print("ERR RAW bad value token=");
                    serialOut().println(token);
                    return;
                }
            } else if (startsWithTokenIgnoreCase(token, "dur=")) {
                if (!text_parse::parseUnsigned(text_parse::tokenValue(token), durationMs)) {
                    serialOut().print("ERR RAW bad value token=");
                    serialOut().println(token);
                    return;
                }
            } else if (startsWithTokenIgnoreCase(token, "post=")) {
                if (!text_parse::parseUnsigned(text_parse::tokenValue(token), postMs)) {
                    serialOut().print("ERR RAW bad value token=");
                    serialOut().println(token);
                    return;
                }
            } else if (startsWithTokenIgnoreCase(token, "pre=")) {
                if (!text_parse::parseUnsigned(text_parse::tokenValue(token), preMs)) {
                    serialOut().print("ERR RAW bad value token=");
                    serialOut().println(token);
                    return;
                }
            } else if (startsWithTokenIgnoreCase(token, "decim=")) {
                if (!text_parse::parseUnsigned(text_parse::tokenValue(token), decim)) {
                    serialOut().print("ERR RAW bad value token=");
                    serialOut().println(token);
                    return;
                }
            } else if (startsWithTokenIgnoreCase(token, "pulses=")) {
                if (!text_parse::parseUnsigned(text_parse::tokenValue(token), shape.pulseCount)) {
                    serialOut().print("ERR RAW bad value token=");
                    serialOut().println(token);
                    return;
                }
            } else if (startsWithTokenIgnoreCase(token, "gap=")) {
                if (!text_parse::parseUnsigned(text_parse::tokenValue(token), shape.pulseGapMs)) {
                    serialOut().print("ERR RAW bad value token=");
                    serialOut().println(token);
                    return;
                }
            } else if (startsWithTokenIgnoreCase(token, "sweep=")) {
                if (!text_parse::parseUnsigned(text_parse::tokenValue(token), shape.sweepToHz)) {
                    serialOut().print("ERR RAW bad value token=");
                    serialOut().println(token);
                    return;
                }
            } else if (startsWithTokenIgnoreCase(token, "band=")) {
                const char* spec = text_parse::tokenValue(token);
                shape.bandSet = true;
                if (equalsIgnoreCase(spec, "off")) {
                    shape.bandLowHz = 0;
                    shape.bandHighHz = 0;
                } else {
                    char* dash = nullptr;
                    const unsigned long lo = strtoul(spec, &dash, 10);
                    const unsigned long hi = (dash != nullptr && *dash == '-') ? strtoul(dash + 1, nullptr, 10) : 0UL;
                    if (lo == 0 || hi < lo || hi > runtime::kMaxToneHz) {
                        serialOut().print("RAW_ERR bad value token=");
                        serialOut().println(token);
                        return;
                    }
                    shape.bandLowHz = lo;
                    shape.bandHighHz = hi;
                }
            } else if (startsWithTokenIgnoreCase(token, "hop=")) {
                // Same validation as SEQ start: digits and commas, fits the buffer.
                const char* list = text_parse::tokenValue(token);
                const size_t len = strlen(list);
                bool ok = len > 0 && len < sizeof(shape.hop);
                for (size_t i = 0; ok && i < len; ++i) {
                    ok = (list[i] >= '0' && list[i] <= '9') || list[i] == ',';
                }
                if (!ok) {
                    serialOut().print("ERR RAW bad value token=");
                    serialOut().println(token);
                    return;
                }
                strncpy(shape.hop, list, sizeof(shape.hop) - 1);
                shape.hop[sizeof(shape.hop) - 1] = '\0';
            } else if (startsWithTokenIgnoreCase(token, "mode=")) {
                const char* modeValue = text_parse::tokenValue(token);
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
            serialOut().println("OK RAW");
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
            serialOut().println("ERR SEQ unknown diagnostics use DIAG on|off");
            return;
        }
        _seqOutputConfig.diagnosticsEnabled = enabled;
        if (_sequenceTest.active) {
            _sequenceTest.outputConfig.diagnosticsEnabled = enabled;
        }
        serialOut().print("OK SEQ DIAG ");
        serialOut().println(onOffName(enabled));
        return;
    }

    if (equalsIgnoreCase(token, "FREQBAND")) {
        const char* enabledToken = strtok_r(nullptr, " ", &savePtr);
        bool valid = false;
        const bool enabled = parseOnOffToken(enabledToken, &valid);
        if (!valid) {
            serialOut().println("ERR SEQ unknown freqband use FREQBAND on|off");
            return;
        }
        _seqOutputConfig.frequencyBandEnabled = enabled;
        if (_sequenceTest.active) {
            _sequenceTest.outputConfig.frequencyBandEnabled = enabled;
        }
        serialOut().print("OK SEQ FREQBAND ");
        serialOut().println(onOffName(enabled));
        return;
    }

    if (equalsIgnoreCase(token, "FREQUPDATEEVERYSAMPLES")) {
        const char* decimateToken = strtok_r(nullptr, " ", &savePtr);
        if (decimateToken == nullptr || *decimateToken == '\0') {
            serialOut().println("ERR SEQ missing freqUpdateEverySamples use FREQUPDATEEVERYSAMPLES 1|4|8|16");
            return;
        }
        unsigned long decimation = 0;
        if (!text_parse::parseUnsigned(decimateToken, decimation) || decimation == 0UL) {
            serialOut().println("ERR SEQ freqUpdateEverySamples out of range use N>=1");
            return;
        }
        _seqOutputConfig.frequencyUpdateEverySamples = decimation;
        _freqBandStream.setFrequencyUpdateEverySamples(decimation);
        if (_sequenceTest.active) {
            _sequenceTest.outputConfig.frequencyUpdateEverySamples = decimation;
        }
        serialOut().print("OK SEQ FREQUPDATEEVERYSAMPLES ");
        serialOut().println(decimation);
        return;
    }

        if (equalsIgnoreCase(token, "MODE")) {
            const char* modeToken = strtok_r(nullptr, " ", &savePtr);
            bool valid = false;
            const AnalyzerApp::SeqOutputMode mode = AnalyzerApp::sequenceOutputModeFromToken(modeToken, &valid);
            if (!valid) {
            serialOut().println("ERR SEQ unknown mode use MODE quiet|trial|inspect|source|system|detail");
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
                    _sequenceTest.outputConfig.diagnosticsEnabled = false;  // a later SEQ WHEN must not re-enable them
                }
            }
            serialOut().print("OK SEQ MODE ");
            serialOut().println(AnalyzerApp::sequenceOutputModeName(mode));
            return;
        }

        if (equalsIgnoreCase(token, "WHEN")) {
            const char* whenToken = strtok_r(nullptr, " ", &savePtr);
            bool valid = false;
            const AnalyzerApp::SeqOutputWhen when = AnalyzerApp::sequenceOutputWhenFromToken(whenToken, &valid);
            if (!valid) {
                serialOut().println("ERR SEQ unknown when use WHEN off|miss|all");
                return;
            }
            _seqOutputConfig.when = when;
            if (_sequenceTest.active) {
                _sequenceTest.outputConfig.when = when;
            }
            serialOut().print("OK SEQ WHEN ");
            serialOut().println(AnalyzerApp::sequenceOutputWhenName(when));
            return;
        }

        if (equalsIgnoreCase(token, "PROFILE")) {
            const char* profileToken = strtok_r(nullptr, " ", &savePtr);
            if (profileToken == nullptr || *profileToken == '\0') {
                serialOut().print("SEQ PROFILE ");
                serialOut().println(detection::detectionProfileName(_seqOutputConfig.profileKind));
                return;
            }

            detection::DetectionProfileKind profileKind = detection::DetectionProfileKind::TonalPulseScalar;
            const bool validProfile = detection::detectionProfileKindFromName(profileToken, profileKind);

            if (!validProfile) {
                serialOut().print("ERR SEQ unknown profile=");
                serialOut().print(profileToken);
                serialOut().println(" use PROFILE TonalPulseScalar or PROFILE AmpExperimental");
                return;
            }

            _seqOutputConfig.profileKind = profileKind;
            serialOut().print("OK SEQ PROFILE ");
            serialOut().println(detection::detectionProfileName(_seqOutputConfig.profileKind));
            return;
        }

        if (equalsIgnoreCase(token, "VERBOSE")) {
            const char* verbosityToken = strtok_r(nullptr, " ", &savePtr);
            if (verbosityToken == nullptr || *verbosityToken == '\0') {
                serialOut().println("ERR SEQ missing verbosity use VERBOSE 0|1|2");
                return;
            }
            unsigned long verbosity = 0;
            if (!text_parse::parseUnsigned(verbosityToken, verbosity) || verbosity > 2UL) {
                serialOut().println("ERR SEQ verbosity out of range use 0|1|2");
                return;
            }
            _seqOutputConfig.verbosity = static_cast<uint8_t>(verbosity);
            if (_sequenceTest.active) {
                _sequenceTest.outputConfig.verbosity = _seqOutputConfig.verbosity;
            }
            serialOut().print("OK SEQ VERBOSE ");
            serialOut().println(static_cast<unsigned long>(_seqOutputConfig.verbosity));
            return;
        }

        if (equalsIgnoreCase(token, "TRIES")) {
            const char* trialsToken = strtok_r(nullptr, " ", &savePtr);
            if (trialsToken == nullptr || *trialsToken == '\0') {
                serialOut().println("ERR SEQ missing tries use TRIES N");
                return;
            }
            unsigned long totalTrials = 0;
            if (!text_parse::parseUnsigned(trialsToken, totalTrials) || totalTrials == 0UL) {
                serialOut().println("ERR SEQ tries out of range use N>=1");
                return;
            }
            _seqOutputConfig.totalTrials = totalTrials;
            serialOut().print("OK SEQ TRIES ");
            serialOut().println(_seqOutputConfig.totalTrials);
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
            _pendingSequenceStart.active = false;  // a start + stop in one buffer does not start (review D1.4)
            serialOut().println("OK SEQ STOP");
            return;
        }

        if (equalsIgnoreCase(token, "START") || equalsIgnoreCase(token, "OBS")) {
            // A run in progress (or one already queued) is not discarded by a second start: its trial in flight
            // and its summary would be lost (review D1.4, 2026-10-27). SEQ STOP first, as SELFTEST does.
            if (_sequenceTest.active || _pendingSequenceStart.active) {
                serialOut().println("ERR SEQ start refused reason=run_active (SEQ STOP first)");
                return;
            }
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
                    text_parse::parseUnsigned(token, _pendingSequenceStart.totalTrials);  // digits only, checked above
                    totalTrialsSet = true;
                } else if (startsWithTokenIgnoreCase(token, "tries=")) {
                    if (!text_parse::parseUnsigned(text_parse::tokenValue(token), _pendingSequenceStart.totalTrials)) {
                        serialOut().print("ERR SEQ bad value token=");
                        serialOut().println(token);
                        return;
                    }
                    totalTrialsSet = true;
                } else if (startsWithTokenIgnoreCase(token, "period=")) {
                    if (!text_parse::parseUnsigned(text_parse::tokenValue(token), _pendingSequenceStart.periodMs)) {
                        serialOut().print("ERR SEQ bad value token=");
                        serialOut().println(token);
                        return;
                    }
                } else if (startsWithTokenIgnoreCase(token, "window=")) {
                    windowSet = true;
                    if (!text_parse::parseUnsigned(text_parse::tokenValue(token), _pendingSequenceStart.windowEndOffsetMs)) {
                        serialOut().print("ERR SEQ bad value token=");
                        serialOut().println(token);
                        return;
                    }
                } else if (startsWithTokenIgnoreCase(token, "freq=")) {
                    if (!text_parse::parseUnsigned(text_parse::tokenValue(token), _pendingSequenceStart.toneHz)) {
                        serialOut().print("ERR SEQ bad value token=");
                        serialOut().println(token);
                        return;
                    }
                } else if (startsWithTokenIgnoreCase(token, "dur=")) {
                    if (!text_parse::parseUnsigned(text_parse::tokenValue(token), _pendingSequenceStart.durationMs)) {
                        serialOut().print("ERR SEQ bad value token=");
                        serialOut().println(token);
                        return;
                    }
                } else if (startsWithTokenIgnoreCase(token, "band=")) {
                    const char* spec = text_parse::tokenValue(token);
                    if (equalsIgnoreCase(spec, "off")) {
                        _pendingSequenceStart.bandLowHz = 0;
                        _pendingSequenceStart.bandHighHz = 0;
                    } else {
                        char* dash = nullptr;
                        const unsigned long lo = strtoul(spec, &dash, 10);
                        const unsigned long hi = (dash != nullptr && *dash == '-') ? strtoul(dash + 1, nullptr, 10) : 0UL;
                        if (lo == 0 || hi < lo || hi > runtime::kMaxToneHz) {
                            serialOut().print("ERR SEQ bad value token=");
                            serialOut().println(token);
                            return;
                        }
                        _pendingSequenceStart.bandLowHz = lo;
                        _pendingSequenceStart.bandHighHz = hi;
                    }
                } else if (startsWithTokenIgnoreCase(token, "hop=")) {
                    const char* list = text_parse::tokenValue(token);
                    const size_t len = strlen(list);
                    bool ok = len > 0 && len < sizeof(_pendingSequenceStart.pulseHop);
                    for (size_t i = 0; ok && i < len; ++i) {
                        ok = (list[i] >= '0' && list[i] <= '9') || list[i] == ',';
                    }
                    if (!ok) {
                        serialOut().print("ERR SEQ bad value token=");
                        serialOut().println(token);
                        return;
                    }
                    strncpy(_pendingSequenceStart.pulseHop, list, sizeof(_pendingSequenceStart.pulseHop) - 1);
                    _pendingSequenceStart.pulseHop[sizeof(_pendingSequenceStart.pulseHop) - 1] = '\0';
                } else if (startsWithTokenIgnoreCase(token, "sweep=")) {
                    if (!text_parse::parseUnsigned(text_parse::tokenValue(token), _pendingSequenceStart.sweepToHz) ||
                        _pendingSequenceStart.sweepToHz > runtime::kMaxToneHz) {
                        serialOut().print("ERR SEQ bad value token=");
                        serialOut().println(token);
                        return;
                    }
                } else if (startsWithTokenIgnoreCase(token, "pulses=")) {
                    if (!text_parse::parseUnsigned(text_parse::tokenValue(token), _pendingSequenceStart.pulseCount)) {
                        serialOut().print("ERR SEQ bad value token=");
                        serialOut().println(token);
                        return;
                    }
                } else if (startsWithTokenIgnoreCase(token, "gap=")) {
                    if (!text_parse::parseUnsigned(text_parse::tokenValue(token), _pendingSequenceStart.pulseGapMs)) {
                        serialOut().print("ERR SEQ bad value token=");
                        serialOut().println(token);
                        return;
                    }
                } else if (startsWithTokenIgnoreCase(token, "ramp=")) {
                    if (!text_parse::parseUnsigned(text_parse::tokenValue(token), _pendingSequenceStart.pulseRampMs)) {
                        serialOut().print("ERR SEQ bad value token=");
                        serialOut().println(token);
                        return;
                    }
                    _pendingSequenceStart.pulseRampSet = true;
                } else if (startsWithTokenIgnoreCase(token, "delay=") || startsWithTokenIgnoreCase(token, "warmup=")) {
                    if (!text_parse::parseUnsigned(strchr(token, '=') + 1, _pendingSequenceStart.startupDelayMs)) {
                        serialOut().print("ERR SEQ bad value token=");
                        serialOut().println(token);
                        return;
                    }
                } else if (startsWithTokenIgnoreCase(token, "report_settle=") || startsWithTokenIgnoreCase(token, "settle=")) {
                    if (!text_parse::parseUnsigned(strchr(token, '=') + 1, _pendingSequenceStart.reportSettleMs)) {
                        serialOut().print("ERR SEQ bad value token=");
                        serialOut().println(token);
                        return;
                    }
                } else if (equalsIgnoreCase(token, "quiet")) {
                    _pendingSequenceStart.quiet = true;
                } else if (equalsIgnoreCase(token, "show=0")) {
                    _pendingSequenceStart.showDetails = false;
                } else if (equalsIgnoreCase(token, "show=1")) {
                    _pendingSequenceStart.showDetails = true;
                } else if (startsWithTokenIgnoreCase(token, "profile=")) {
                    const char* profileValue = text_parse::tokenValue(token);
                    if (detection::detectionProfileKindFromName(profileValue, _pendingSequenceStart.profileKind)) {
                        profileSeen = true;
                    } else {
                        profileValid = false;
                        serialOut().print("ERR SEQ unknown profile=");
                        serialOut().print(profileValue);
                        serialOut().println(" use profile=TonalPulseScalar or profile=AmpExperimental");
                        return;
                    }
                } else if (startsWithTokenIgnoreCase(token, "mode=")) {
                    bool valid = false;
                    outputConfig.mode = AnalyzerApp::sequenceOutputModeFromToken(text_parse::tokenValue(token), &valid);
                    if (!valid) {
                        serialOut().print("ERR SEQ unknown mode=");
                        serialOut().print(text_parse::tokenValue(token));
                    serialOut().println(" use mode=quiet, mode=trial, mode=inspect, mode=source, mode=system or mode=detail");
                        return;
                    }
                    if (outputConfig.mode == AnalyzerApp::SeqOutputMode::Quiet) {
                        outputConfig.when = AnalyzerApp::SeqOutputWhen::Off;
                        outputConfig.verbosity = 0;
                    }
                } else if (startsWithTokenIgnoreCase(token, "when=")) {
                    bool valid = false;
                    outputConfig.when = AnalyzerApp::sequenceOutputWhenFromToken(text_parse::tokenValue(token), &valid);
                    if (!valid) {
                        serialOut().print("ERR SEQ unknown when=");
                        serialOut().print(text_parse::tokenValue(token));
                        serialOut().println(" use when=off, when=miss or when=all");
                        return;
                    }
                } else if (startsWithTokenIgnoreCase(token, "verbose=")) {
                    unsigned long verbosity = 0;
                    if (!text_parse::parseUnsigned(text_parse::tokenValue(token), verbosity) || verbosity > 2UL) {
                        serialOut().print("ERR SEQ verbose=");
                        serialOut().print(verbosity);
                        serialOut().println(" use 0|1|2");
                        return;
                    }
                    outputConfig.verbosity = static_cast<uint8_t>(verbosity);
                } else if (startsWithTokenIgnoreCase(token, "diag=") || startsWithTokenIgnoreCase(token, "diagnostics=")) {
                    const char* valueToken = strchr(token, '=') != nullptr ? strchr(token, '=') + 1 : nullptr;
                    bool valid = false;
                    const bool enabled = parseOnOffToken(valueToken, &valid);
                    if (!valid) {
                        serialOut().println("ERR SEQ diagnostics use diag=on|off");
                        return;
                    }
                    outputConfig.diagnosticsEnabled = enabled;
                } else if (startsWithTokenIgnoreCase(token, "sampleFirst=")) {
                    _pendingSequenceStart.sampleDumpEnabled = true;
                    if (!text_parse::parseUnsigned(text_parse::tokenValue(token), _pendingSequenceStart.sampleDumpFirstTrials)) {
                        serialOut().print("ERR SEQ bad value token=");
                        serialOut().println(token);
                        return;
                    }
                } else if (startsWithTokenIgnoreCase(token, "sampleEvery=")) {
                    _pendingSequenceStart.sampleDumpEnabled = true;
                    if (!text_parse::parseUnsigned(text_parse::tokenValue(token), _pendingSequenceStart.sampleDumpEveryNth)) {
                        serialOut().print("ERR SEQ bad value token=");
                        serialOut().println(token);
                        return;
                    }
                } else if (startsWithTokenIgnoreCase(token, "sampleLead=")) {
                    _pendingSequenceStart.sampleDumpEnabled = true;
                    if (!text_parse::parseUnsigned(text_parse::tokenValue(token), _pendingSequenceStart.sampleDumpLeadMs)) {
                        serialOut().print("ERR SEQ bad value token=");
                        serialOut().println(token);
                        return;
                    }
                } else if (startsWithTokenIgnoreCase(token, "sampleTail=")) {
                    _pendingSequenceStart.sampleDumpEnabled = true;
                    if (!text_parse::parseUnsigned(text_parse::tokenValue(token), _pendingSequenceStart.sampleDumpTailMs)) {
                        serialOut().print("ERR SEQ bad value token=");
                        serialOut().println(token);
                        return;
                    }
                } else if (startsWithTokenIgnoreCase(token, "sampleStep=")) {
                    _pendingSequenceStart.sampleDumpEnabled = true;
                    if (!text_parse::parseUnsigned(text_parse::tokenValue(token), _pendingSequenceStart.sampleDumpStepMs)) {
                        serialOut().print("ERR SEQ bad value token=");
                        serialOut().println(token);
                        return;
                    }
                } else if (startsWithTokenIgnoreCase(token, "sampleMax=")) {
                    _pendingSequenceStart.sampleDumpEnabled = true;
                    if (!text_parse::parseUnsigned(text_parse::tokenValue(token), _pendingSequenceStart.sampleDumpMaxRows)) {
                        serialOut().print("ERR SEQ bad value token=");
                        serialOut().println(token);
                        return;
                    }
                } else if (equalsIgnoreCase(token, "external")) {
                    // Observing an already-running external emitter is an explicit mode.
                    // Keep the current mode if the caller already requested OBS.
                    _pendingSequenceStart.externalEmitter = true;
                } else if (startsWithTokenIgnoreCase(token, "test=")) {
                    strncpy(_pendingSequenceStart.setupLabelStorage, text_parse::tokenValue(token), sizeof(_pendingSequenceStart.setupLabelStorage));
                    _pendingSequenceStart.setupLabelStorage[sizeof(_pendingSequenceStart.setupLabelStorage) - 1] = '\0';
                } else if (equalsIgnoreCase(token, "labels")) {
                    strncpy(_pendingSequenceStart.setupLabelStorage, "labels", sizeof(_pendingSequenceStart.setupLabelStorage));
                    _pendingSequenceStart.setupLabelStorage[sizeof(_pendingSequenceStart.setupLabelStorage) - 1] = '\0';
                } else if (equalsIgnoreCase(token, "start")) {
                    // "SEQ OBS start ..." (tools/bench/soak.py): the verb after OBS.
                } else {
                    // Unknown keys used to be ignored, so "perod=3000" ran on
                    // defaults with OK SEQ (issue #31 R2-19).
                    serialOut().print("ERR SEQ unknown token=");
                    serialOut().println(token);
                    return;
                }
            }

            // Same bounds the Emitter applies to PULSES; refused here so a
            // run never starts with every trial rejected on the Emitter.
            if (_pendingSequenceStart.pulseCount > 0 &&
                (_pendingSequenceStart.pulseCount > emitter_limits::kPulsesMaxCount ||
                 _pendingSequenceStart.durationMs == 0 ||
                 _pendingSequenceStart.durationMs > emitter_limits::kPulsesMaxPulseMs ||
                 _pendingSequenceStart.pulseGapMs == 0 ||
                 _pendingSequenceStart.pulseGapMs > emitter_limits::kPulsesMaxGapMs ||
                 (_pendingSequenceStart.pulseRampSet &&
                  _pendingSequenceStart.pulseRampMs > emitter_limits::kPulsesMaxRampMs))) {
                serialOut().print("ERR SEQ pulses use pulses=1-");
                serialOut().print(emitter_limits::kPulsesMaxCount);
                serialOut().print(" dur=1-");
                serialOut().print(emitter_limits::kPulsesMaxPulseMs);
                serialOut().print(" gap=1-");
                serialOut().print(emitter_limits::kPulsesMaxGapMs);
                serialOut().print(" [ramp=0-");
                serialOut().print(emitter_limits::kPulsesMaxRampMs);
                serialOut().println("]");
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
            serialOut().println("OK SEQ");
            return;
        }

    }

    if (startsWithTokenIgnoreCase(line, "EMIT ")) {
        if (startsWithTokenIgnoreCase(line, "EMIT CHIRP")) {
            sendEmitterCommand("MODE REMOTE");
            if (!waitForEmitterAck("OK MODE REMOTE", kEmitterModeAckTimeoutMs)) {
                serialOut().println("ERR EMIT CHIRP emitter_remote_claim_timeout");
                return;
            }
            sendEmitterCommand(text_parse::rest(line, "EMIT"));
            serialOut().println("OK EMIT CHIRP");
            return;
        }
        if (startsWithTokenIgnoreCase(line, "EMIT PULSES")) {
            sendEmitterCommand("MODE REMOTE");
            if (!waitForEmitterAck("OK MODE REMOTE", kEmitterModeAckTimeoutMs)) {
                serialOut().println("ERR EMIT PULSES emitter_remote_claim_timeout");
                return;
            }
            sendEmitterCommand(text_parse::rest(line, "EMIT"));
            serialOut().println("OK EMIT PULSES");
            return;
        }
        if (startsWithTokenIgnoreCase(line, "EMIT MODE REMOTE")) {
            sendEmitterCommand("MODE REMOTE");
            serialOut().println("OK EMIT MODE REMOTE");
            return;
        }
        if (startsWithTokenIgnoreCase(line, "EMIT MODE AUTO")) {
            sendEmitterCommand(text_parse::rest(line, "EMIT"));
            serialOut().println("OK EMIT MODE AUTO");
            return;
        }
        if (startsWithTokenIgnoreCase(line, "EMIT SWEEP")) {
            sendEmitterCommand(text_parse::rest(line, "EMIT"));
            serialOut().println("OK EMIT SWEEP");
            return;
        }
    }

    serialOut().print("EVT analyzer_unknown_cmd line=");
    serialOut().println(line);
}
