#include "AnalyzerModeApp.h"
#include "../../app/FirmwareOta.h"

#include <Arduino.h>
#include <stdlib.h>
#include <string.h>
#include "../../app/ParamCommands.h"
#include "../../app/SerialOut.h"
#include "../../app/TextParse.h"
#include "AnalyzerCommandParse.h"

#include "tools/AnalyzerEmitterControl.h"
#include "../../app/EmitterLimits.h"
#include "../../app/SerialLine.h"

using text_parse::equalsIgnoreCase;
using text_parse::startsWithIgnoreCase;

namespace {

const char* onOffName(bool value) {
    return value ? "on" : "off";
}

constexpr const char* kRawBadValue = "ERR RAW bad value token=";
constexpr const char* kSeqBadValue = "ERR SEQ bad value token=";

void printBadValue(const char* prefix, const char* token) {
    serialOut().print(prefix);
    serialOut().println(token);
}

// key=N into out; on anything but a whole number the ERR line names the token and false refuses the command.
template <typename T>
bool parseTokenValue(const char* token, T& out, const char* badValuePrefix) {
    if (text_parse::parseUnsigned(text_parse::tokenValue(token), out)) {
        return true;
    }
    printBadValue(badValuePrefix, token);
    return false;
}

}  // namespace

// SEQ DIAG / FREQBAND / FREQUPDATEEVERYSAMPLES / MODE / WHEN / VERBOSE: one rule (step 11g C4).
// The change goes to the next start's output settings and, while a run is active, to the running
// run's copy as well. TRIES and PROFILE change the next start only.
template <typename Change>
void AnalyzerApp::changeSeqOutputConfig(Change change) {
    change(_seqOutputConfig);
    if (_sequenceTest.active) {
        change(_sequenceTest.config.outputConfig);
    }
}

void AnalyzerApp::printSequenceHelp() {
    serialOut().println("CMD: SEQ help");
    serialOut().println("CMD: SEQ");
    serialOut().println("CMD: SEQ stop");
    serialOut().println(
        "SEQ IN: start [N|tries=N] [period=MS] [window=MS] [freq=HZ] [dur=MS] [delay=MS] [report_settle=MS] [band=LOW-HIGH|off] [test=LABEL]");
    serialOut().println(
        "SEQ IN: OBS start [N|tries=N] [period=2000] [window=1800] [freq=HZ] [dur=MS] [delay=MS] [report_settle=MS] [test=LABEL]");
    serialOut().println(
        "SEQ IN: pulse train per trial (DET-011): start ... dur=PULSE_MS pulses=N gap=MS [ramp=MS] [hop=F1,F2,...]");
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
    serialOut().println(
        "SEQ IN: [sampleFirst=N] [sampleEvery=N] [sampleLead=MS] [sampleTail=MS] [sampleStep=MS] [sampleMax=N]");
    serialOut().println(
        "SEQ IN: sample dump is detector-input curve tooling, not RAW capture, not the audio envelope curve, not DetectorReport, not SEQ_SOURCE / SEQ_INSPECT / SEQ_EXPLAIN, and not canonical Analyzer truth");
    serialOut().println(
        "SEQ IN: sampleMax is the row budget over all dumped trials (default 5000); one trial must also fit the 256-row sampleRows buffer (default sampleStep=4)");
    serialOut().println(
        "SEQ OUT: SEQ start / SEQ running / SEQ_TRIAL / SEQ_INSPECT / SEQ_EXPLAIN / SEQ_SOURCE_CORE / SEQ_SOURCE_SPEC / SEQ_SUMMARY / SEQ_STATUS / SEQ REPORT / AUDIO run");
    serialOut().println("SEQ OUT: SAMPLES_BEGIN / SAMPLES_NOTE / SAMPLES_END");
    serialOut().println("SEQ OUT: sample fields are t,value (value is the selected detector input stream)");
    serialOut().println("SEQ OBS: passive observe mode for an already-running external emitter");
    serialOut().println("SEQ IN: PROFILE TonalPulseScalar|AmpExperimental");
    serialOut().println("SEQ PROFILE TonalPulseScalar");
    serialOut().println("SEQ PROFILE AmpExperimental");
    serialOut().println(
        "SEQ PARAM: PARAM SET detection.scalar_<key> <value> (PARAM LIST shows the keys, PARAM DUMP the values; applied at the next SEQ start)");
}

void AnalyzerApp::pollUsbConsole() {
    serial_line::pollLines(Serial, _usbLine, serialOut(),
                           "ERR line_too_long max=", [this](const char* line) { handleUsbLine(line); });
}

// The console, one handler per group (step 11j item 6; the group order is analyzer_commands::groupOf's). A SEQ or
// EMIT line whose sub-command none of its handlers knows ends as analyzer_unknown_cmd, as before the split.
void AnalyzerApp::handleUsbLine(const char* line) {
    switch (analyzer_commands::groupOf(line)) {
        case analyzer_commands::Group::Help:
            printUsbHelp();
            return;
        case analyzer_commands::Group::SelfTest:
            handleSelfTestCommand(line);
            return;
        case analyzer_commands::Group::Ota:
            ota::runCommand(line, serialOut());  // firmware over WiFi (step 11d); reboots on success
            return;
        case analyzer_commands::Group::Param:
            // The registry's grammar and the Node's lines (app/ParamCommands, step
            // 11e item 9). No store: the tuning is per run. A value takes effect at
            // the next SEQ start (effectiveSequenceProfile), so nothing applies here.
            param::handleParamCommand(line, _paramRegistry, nullptr, serialOut());
            return;
        case analyzer_commands::Group::Raw:
            handleRawCommand(line);
            return;
        case analyzer_commands::Group::Seq:
            if (handleSeqCommand(line)) {
                return;
            }
            break;
        case analyzer_commands::Group::Emit:
            if (handleEmitCommand(line)) {
                return;
            }
            break;
        case analyzer_commands::Group::Unknown:
            break;
    }

    serialOut().print("EVT analyzer_unknown_cmd line=");
    serialOut().println(line);
}

void AnalyzerApp::printUsbHelp() {
    serialOut().println(
        "CMD: PARAM LIST|GET <path>|SET <path> <value>|DUMP|RESET (detection.scalar_*, applied at the next SEQ start; SAVE|LOAD answer status=no_store)");
    serialOut().println("CMD: EMIT CHIRP freq=3200 dur=100");
    serialOut().println("CMD: EMIT MODE REMOTE");
    serialOut().println("CMD: EMIT MODE AUTO interval=2000 freq=3200 dur=100");
    serialOut().println("CMD: EMIT SWEEP start=3000 stop=3500 step=100 dur=80 pause=1000");
    serialOut().println("CMD: EMIT PULSES n pulse_ms gap_ms [freq=3200] [ramp=5] (e.g. EMIT PULSES 5 10 20)");
    serialOut().println(
        "CMD: RAW trigger f=3200 dur=100 post=1000 [pre=MS] [decim=N] mode=feat (default)|mode=pcm|mode=i2s|mode=both [pulses=N gap=MS hop=F1,F2,...] [sweep=END_HZ] [band=LOW-HIGH|off] (pulses= plays a PULSES train instead of one CHIRP; dur= is then the pulse length)");
    serialOut().println("CMD: SEQ");
    serialOut().println("CMD: SEQ help");
    serialOut().println("CMD: SEQ stop");
    serialOut().println(
        "CMD: SELFTEST | SELFTEST external | SELFTEST chirp n=5 gap_ms=1500 dur_ms=100 | SELFTEST help");
}

void AnalyzerApp::handleRawCommand(const char* line) {
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
        bool ok = true;
        if (startsWithIgnoreCase(token, "f=")) {
            ok = parseTokenValue(token, toneHz, kRawBadValue);
        } else if (startsWithIgnoreCase(token, "dur=")) {
            ok = parseTokenValue(token, durationMs, kRawBadValue);
        } else if (startsWithIgnoreCase(token, "post=")) {
            ok = parseTokenValue(token, postMs, kRawBadValue);
        } else if (startsWithIgnoreCase(token, "pre=")) {
            ok = parseTokenValue(token, preMs, kRawBadValue);
        } else if (startsWithIgnoreCase(token, "decim=")) {
            ok = parseTokenValue(token, decim, kRawBadValue);
        } else if (startsWithIgnoreCase(token, "pulses=")) {
            ok = parseTokenValue(token, shape.pulseCount, kRawBadValue);
        } else if (startsWithIgnoreCase(token, "gap=")) {
            ok = parseTokenValue(token, shape.pulseGapMs, kRawBadValue);
        } else if (startsWithIgnoreCase(token, "sweep=")) {
            ok = parseTokenValue(token, shape.sweepToHz, kRawBadValue);
        } else if (startsWithIgnoreCase(token, "band=")) {
            shape.bandSet = true;
            if (!analyzer_commands::parseBandSpec(text_parse::tokenValue(token), runtime::kMaxToneHz, shape.bandLowHz,
                                                  shape.bandHighHz)) {
                printBadValue("RAW_ERR bad value token=", token);
                return;
            }
        } else if (startsWithIgnoreCase(token, "hop=")) {
            // Same validation as SEQ start: digits and commas, fits the buffer.
            const char* list = text_parse::tokenValue(token);
            if (!analyzer_commands::validHopList(list, sizeof(shape.hop))) {
                printBadValue(kRawBadValue, token);
                return;
            }
            strncpy(shape.hop, list, sizeof(shape.hop) - 1);
            shape.hop[sizeof(shape.hop) - 1] = '\0';
        } else if (startsWithIgnoreCase(token, "mode=")) {
            const char* modeValue = text_parse::tokenValue(token);
            if (equalsIgnoreCase(modeValue, "feat") || equalsIgnoreCase(modeValue, "features") ||
                equalsIgnoreCase(modeValue, "csv")) {
                rawMode = AnalyzerApp::RawCaptureMode::Features;
            } else if (equalsIgnoreCase(modeValue, "pcm")) {
                rawMode = AnalyzerApp::RawCaptureMode::Pcm;
            } else if (equalsIgnoreCase(modeValue, "i2s") || equalsIgnoreCase(modeValue, "raw")) {
                rawMode = AnalyzerApp::RawCaptureMode::I2s;
            } else if (equalsIgnoreCase(modeValue, "both")) {
                rawMode = AnalyzerApp::RawCaptureMode::Both;
            }
        }
        if (!ok) {
            return;
        }
    }

    if (runRawTrigger(toneHz, durationMs, postMs, preMs, decim, rawMode, shape)) {
        serialOut().println("OK RAW");
    }
}

// SEQ: help, the output settings, the run commands, and START / OBS. False for a sub-command none of them knows.
bool AnalyzerApp::handleSeqCommand(const char* line) {
    strncpy(_commandScratch, line, sizeof(_commandScratch));
    _commandScratch[sizeof(_commandScratch) - 1] = '\0';

    char* savePtr = nullptr;
    char* token = strtok_r(_commandScratch, " ", &savePtr);
    if (token == nullptr) {
        printSequenceHelp();
        return true;
    }

    token = strtok_r(nullptr, " ", &savePtr);
    if (token == nullptr || equalsIgnoreCase(token, "HELP")) {
        printSequenceHelp();
        return true;
    }
    if (equalsIgnoreCase(token, "START") || equalsIgnoreCase(token, "OBS")) {
        handleSeqStart(token, savePtr);
        return true;
    }
    return handleSeqOutputSetting(token, savePtr) || handleSeqRunCommand(token, savePtr);
}

// SEQ DIAG / FREQBAND / FREQUPDATEEVERYSAMPLES / MODE / WHEN / VERBOSE (changeSeqOutputConfig's rule).
bool AnalyzerApp::handleSeqOutputSetting(const char* verb, char*& savePtr) {
    if (equalsIgnoreCase(verb, "DIAG") || equalsIgnoreCase(verb, "DIAGNOSTICS")) {
        const char* enabledToken = strtok_r(nullptr, " ", &savePtr);
        bool valid = false;
        const bool enabled = analyzer_commands::parseOnOff(enabledToken, &valid);
        if (!valid) {
            serialOut().println("ERR SEQ unknown diagnostics use DIAG on|off");
            return true;
        }
        changeSeqOutputConfig([enabled](SeqOutputConfig& config) { config.diagnosticsEnabled = enabled; });
        serialOut().print("OK SEQ DIAG ");
        serialOut().println(onOffName(enabled));
        return true;
    }

    if (equalsIgnoreCase(verb, "FREQBAND")) {
        const char* enabledToken = strtok_r(nullptr, " ", &savePtr);
        bool valid = false;
        const bool enabled = analyzer_commands::parseOnOff(enabledToken, &valid);
        if (!valid) {
            serialOut().println("ERR SEQ unknown freqband use FREQBAND on|off");
            return true;
        }
        changeSeqOutputConfig([enabled](SeqOutputConfig& config) { config.frequencyBandEnabled = enabled; });
        serialOut().print("OK SEQ FREQBAND ");
        serialOut().println(onOffName(enabled));
        return true;
    }

    if (equalsIgnoreCase(verb, "FREQUPDATEEVERYSAMPLES")) {
        const char* decimateToken = strtok_r(nullptr, " ", &savePtr);
        if (decimateToken == nullptr || *decimateToken == '\0') {
            serialOut().println("ERR SEQ missing freqUpdateEverySamples use FREQUPDATEEVERYSAMPLES 1|4|8|16");
            return true;
        }
        unsigned long decimation = 0;
        if (!text_parse::parseUnsigned(decimateToken, decimation) || decimation == 0UL) {
            serialOut().println("ERR SEQ freqUpdateEverySamples out of range use N>=1");
            return true;
        }
        changeSeqOutputConfig(
            [decimation](SeqOutputConfig& config) { config.frequencyUpdateEverySamples = decimation; });
        _freqBandStream.setFrequencyUpdateEverySamples(decimation);
        serialOut().print("OK SEQ FREQUPDATEEVERYSAMPLES ");
        serialOut().println(decimation);
        return true;
    }

    if (equalsIgnoreCase(verb, "MODE")) {
        const char* modeToken = strtok_r(nullptr, " ", &savePtr);
        bool valid = false;
        const AnalyzerApp::SeqOutputMode mode = AnalyzerApp::sequenceOutputModeFromToken(modeToken, &valid);
        if (!valid) {
            serialOut().println("ERR SEQ unknown mode use MODE quiet|trial|inspect|source|system|detail");
            return true;
        }
        changeSeqOutputConfig([mode](SeqOutputConfig& config) {
            config.mode = mode;
            if (mode == AnalyzerApp::SeqOutputMode::Quiet) {
                config.when = AnalyzerApp::SeqOutputWhen::Off;
                config.verbosity = 0;
            }
        });
        if (_sequenceTest.active && mode == AnalyzerApp::SeqOutputMode::Quiet) {
            // The running run only (C1): a later SEQ WHEN must not re-enable its diagnostics.
            _sequenceTest.config.outputConfig.diagnosticsEnabled = false;
        }
        serialOut().print("OK SEQ MODE ");
        serialOut().println(AnalyzerApp::sequenceOutputModeName(mode));
        return true;
    }

    if (equalsIgnoreCase(verb, "WHEN")) {
        const char* whenToken = strtok_r(nullptr, " ", &savePtr);
        bool valid = false;
        const AnalyzerApp::SeqOutputWhen when = AnalyzerApp::sequenceOutputWhenFromToken(whenToken, &valid);
        if (!valid) {
            serialOut().println("ERR SEQ unknown when use WHEN off|miss|all");
            return true;
        }
        changeSeqOutputConfig([when](SeqOutputConfig& config) { config.when = when; });
        serialOut().print("OK SEQ WHEN ");
        serialOut().println(AnalyzerApp::sequenceOutputWhenName(when));
        return true;
    }

    if (equalsIgnoreCase(verb, "VERBOSE")) {
        const char* verbosityToken = strtok_r(nullptr, " ", &savePtr);
        if (verbosityToken == nullptr || *verbosityToken == '\0') {
            serialOut().println("ERR SEQ missing verbosity use VERBOSE 0|1|2");
            return true;
        }
        unsigned long verbosity = 0;
        if (!text_parse::parseUnsigned(verbosityToken, verbosity) || verbosity > 2UL) {
            serialOut().println("ERR SEQ verbosity out of range use 0|1|2");
            return true;
        }
        changeSeqOutputConfig(
            [verbosity](SeqOutputConfig& config) { config.verbosity = static_cast<uint8_t>(verbosity); });
        serialOut().print("OK SEQ VERBOSE ");
        serialOut().println(static_cast<unsigned long>(_seqOutputConfig.verbosity));
        return true;
    }

    return false;
}

// SEQ STATUS / PROFILE / TRIES / SUMMARY / REPORT / STOP. PROFILE and TRIES change the next start only.
bool AnalyzerApp::handleSeqRunCommand(const char* verb, char*& savePtr) {
    if (equalsIgnoreCase(verb, "STATUS")) {
        printSequenceStatus();
        return true;
    }

    if (equalsIgnoreCase(verb, "PROFILE")) {
        const char* profileToken = strtok_r(nullptr, " ", &savePtr);
        if (profileToken == nullptr || *profileToken == '\0') {
            serialOut().print("SEQ PROFILE ");
            serialOut().println(detection::detectionProfileName(_seqOutputConfig.profileKind));
            return true;
        }

        detection::DetectionProfileKind profileKind = detection::DetectionProfileKind::TonalPulseScalar;
        if (!detection::detectionProfileKindFromName(profileToken, profileKind)) {
            serialOut().print("ERR SEQ unknown profile=");
            serialOut().print(profileToken);
            serialOut().println(" use PROFILE TonalPulseScalar or PROFILE AmpExperimental");
            return true;
        }

        _seqOutputConfig.profileKind = profileKind;
        serialOut().print("OK SEQ PROFILE ");
        serialOut().println(detection::detectionProfileName(_seqOutputConfig.profileKind));
        return true;
    }

    if (equalsIgnoreCase(verb, "TRIES")) {
        const char* trialsToken = strtok_r(nullptr, " ", &savePtr);
        if (trialsToken == nullptr || *trialsToken == '\0') {
            serialOut().println("ERR SEQ missing tries use TRIES N");
            return true;
        }
        unsigned long totalTrials = 0;
        if (!text_parse::parseUnsigned(trialsToken, totalTrials) || totalTrials == 0UL) {
            serialOut().println("ERR SEQ tries out of range use N>=1");
            return true;
        }
        _seqOutputConfig.totalTrials =
            totalTrials;  // the next start's shape only, as PROFILE: a running run keeps its own
        serialOut().print("OK SEQ TRIES ");
        serialOut().println(_seqOutputConfig.totalTrials);
        return true;
    }

    if (equalsIgnoreCase(verb, "SUMMARY")) {
        printSequenceSummaryClean();
        return true;
    }

    if (equalsIgnoreCase(verb, "REPORT")) {
        printSequenceReport();
        return true;
    }

    if (equalsIgnoreCase(verb, "STOP")) {
        printSequenceSummaryClean();
        stopSequenceTest();
        _pendingSequenceStart.active = false;  // a start + stop in one buffer does not start (review D1.4)
        serialOut().println("OK SEQ STOP");
        return true;
    }

    return false;
}

// SEQ START / OBS [tokens]. Parsed into a copy and handed to the pending start only when every token is valid
// (issue #31): on an ERR the run used to start anyway, and the output settings parsed before the bad token stuck.
// RunConfig's defaults are the start's defaults.
void AnalyzerApp::handleSeqStart(const char* verb, char*& savePtr) {
    // A run in progress (or one already queued) is not discarded by a second start: its trial in flight
    // and its summary would be lost (review D1.4, 2026-10-27). SEQ STOP first, as SELFTEST does.
    if (_sequenceTest.active || _pendingSequenceStart.active) {
        serialOut().println("ERR SEQ start refused reason=run_active (SEQ STOP first)");
        return;
    }
    AnalyzerApp::RunConfig request;
    AnalyzerApp::SeqOutputConfig outputConfig = _seqOutputConfig;
    request.totalTrials = _seqOutputConfig.totalTrials;
    request.setupLabel[0] = '\0';
    request.profileKind = _seqOutputConfig.profileKind;
    request.externalEmitter = equalsIgnoreCase(verb, "OBS");
    SeqStartSeen seen;
    const char* token = nullptr;
    while ((token = strtok_r(nullptr, " ", &savePtr)) != nullptr) {
        if (!applySeqStartToken(token, request, outputConfig, seen)) {
            return;
        }
    }

    // Same bounds the Emitter applies to PULSES; refused here so a
    // run never starts with every trial rejected on the Emitter.
    if (request.pulseCount > 0 &&
        (request.pulseCount > emitter_limits::kPulsesMaxCount || request.durationMs == 0 ||
         request.durationMs > emitter_limits::kPulsesMaxPulseMs || request.pulseGapMs == 0 ||
         request.pulseGapMs > emitter_limits::kPulsesMaxGapMs ||
         (request.pulseRampSet && request.pulseRampMs > emitter_limits::kPulsesMaxRampMs))) {
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
    if (request.externalEmitter && !seen.window && request.periodMs > request.reportSettleMs) {
        request.windowEndOffsetMs = request.periodMs - request.reportSettleMs;
    }

    if (!seen.profile) {
        request.profileKind = outputConfig.profileKind;
    }
    outputConfig.totalTrials = request.totalTrials;
    if (request.setupLabel[0] == '\0') {
        strncpy(request.setupLabel, TEST_SETUP_LABEL, sizeof(request.setupLabel));
        request.setupLabel[sizeof(request.setupLabel) - 1] = '\0';
    }
    _seqOutputConfig = outputConfig;
    _pendingSequenceStart.config = request;
    _pendingSequenceStart.active = true;
    serialOut().println("OK SEQ");
}

// One SEQ START token into the request or its output settings. False (after the ERR line) refuses the start.
bool AnalyzerApp::applySeqStartToken(const char* token, RunConfig& request, SeqOutputConfig& outputConfig,
                                     SeqStartSeen& seen) {
    if (!seen.totalTrials && strchr(token, '=') == nullptr && strspn(token, "0123456789") == strlen(token)) {
        text_parse::parseUnsigned(token, request.totalTrials);  // digits only, checked above
        seen.totalTrials = true;
        // 0 ran one trial and left a sticky tries=0 (review 12.0 L15): refused as SEQ TRIES 0 is.
        if (request.totalTrials == 0) {
            serialOut().println("ERR SEQ tries out of range use N>=1");
            return false;
        }
    } else if (startsWithIgnoreCase(token, "tries=")) {
        if (!parseTokenValue(token, request.totalTrials, kSeqBadValue)) {
            return false;
        }
        seen.totalTrials = true;
        if (request.totalTrials == 0) {
            serialOut().println("ERR SEQ tries out of range use N>=1");
            return false;
        }
    } else if (startsWithIgnoreCase(token, "period=")) {
        return parseTokenValue(token, request.periodMs, kSeqBadValue);
    } else if (startsWithIgnoreCase(token, "window=")) {
        seen.window = true;
        return parseTokenValue(token, request.windowEndOffsetMs, kSeqBadValue);
    } else if (startsWithIgnoreCase(token, "freq=")) {
        // A tone the Emitter plays, 1..kMaxToneHz (review 12.0 L14).
        if (!parseTokenValue(token, request.toneHz, kSeqBadValue)) {
            return false;
        }
        if (request.toneHz == 0 || request.toneHz > runtime::kMaxToneHz) {
            printBadValue(kSeqBadValue, token);
            return false;
        }
    } else if (startsWithIgnoreCase(token, "dur=")) {
        return parseTokenValue(token, request.durationMs, kSeqBadValue);
    } else if (startsWithIgnoreCase(token, "band=")) {
        if (!analyzer_commands::parseBandSpec(text_parse::tokenValue(token), runtime::kMaxToneHz, request.bandLowHz,
                                              request.bandHighHz)) {
            printBadValue(kSeqBadValue, token);
            return false;
        }
    } else if (startsWithIgnoreCase(token, "hop=")) {
        const char* list = text_parse::tokenValue(token);
        if (!analyzer_commands::validHopList(list, sizeof(request.pulseHop)) ||
            !analyzer_commands::hopListInRange(list, runtime::kMaxToneHz)) {
            printBadValue(kSeqBadValue, token);
            return false;
        }
        strncpy(request.pulseHop, list, sizeof(request.pulseHop) - 1);
        request.pulseHop[sizeof(request.pulseHop) - 1] = '\0';
    } else if (startsWithIgnoreCase(token, "sweep=")) {
        if (!text_parse::parseUnsigned(text_parse::tokenValue(token), request.sweepToHz) ||
            request.sweepToHz > runtime::kMaxToneHz) {
            printBadValue(kSeqBadValue, token);
            return false;
        }
    } else if (startsWithIgnoreCase(token, "pulses=")) {
        return parseTokenValue(token, request.pulseCount, kSeqBadValue);
    } else if (startsWithIgnoreCase(token, "gap=")) {
        return parseTokenValue(token, request.pulseGapMs, kSeqBadValue);
    } else if (startsWithIgnoreCase(token, "ramp=")) {
        if (!parseTokenValue(token, request.pulseRampMs, kSeqBadValue)) {
            return false;
        }
        request.pulseRampSet = true;
    } else if (startsWithIgnoreCase(token, "delay=") || startsWithIgnoreCase(token, "warmup=")) {
        return parseTokenValue(token, request.startupDelayMs, kSeqBadValue);
    } else if (startsWithIgnoreCase(token, "report_settle=") || startsWithIgnoreCase(token, "settle=")) {
        return parseTokenValue(token, request.reportSettleMs, kSeqBadValue);
    } else if (equalsIgnoreCase(token, "quiet")) {
        request.quiet = true;
    } else if (equalsIgnoreCase(token, "show=0")) {
        request.showDetails = false;
    } else if (equalsIgnoreCase(token, "show=1")) {
        request.showDetails = true;
    } else if (startsWithIgnoreCase(token, "profile=")) {
        const char* profileValue = text_parse::tokenValue(token);
        if (!detection::detectionProfileKindFromName(profileValue, request.profileKind)) {
            serialOut().print("ERR SEQ unknown profile=");
            serialOut().print(profileValue);
            serialOut().println(" use profile=TonalPulseScalar or profile=AmpExperimental");
            return false;
        }
        seen.profile = true;
    } else if (startsWithIgnoreCase(token, "mode=")) {
        bool valid = false;
        outputConfig.mode = AnalyzerApp::sequenceOutputModeFromToken(text_parse::tokenValue(token), &valid);
        if (!valid) {
            serialOut().print("ERR SEQ unknown mode=");
            serialOut().print(text_parse::tokenValue(token));
            serialOut().println(" use mode=quiet, mode=trial, mode=inspect, mode=source, mode=system or mode=detail");
            return false;
        }
        if (outputConfig.mode == AnalyzerApp::SeqOutputMode::Quiet) {
            outputConfig.when = AnalyzerApp::SeqOutputWhen::Off;
            outputConfig.verbosity = 0;
        }
    } else if (startsWithIgnoreCase(token, "when=")) {
        bool valid = false;
        outputConfig.when = AnalyzerApp::sequenceOutputWhenFromToken(text_parse::tokenValue(token), &valid);
        if (!valid) {
            serialOut().print("ERR SEQ unknown when=");
            serialOut().print(text_parse::tokenValue(token));
            serialOut().println(" use when=off, when=miss or when=all");
            return false;
        }
    } else if (startsWithIgnoreCase(token, "verbose=")) {
        unsigned long verbosity = 0;
        if (!text_parse::parseUnsigned(text_parse::tokenValue(token), verbosity) || verbosity > 2UL) {
            serialOut().print("ERR SEQ verbose=");
            serialOut().print(verbosity);
            serialOut().println(" use 0|1|2");
            return false;
        }
        outputConfig.verbosity = static_cast<uint8_t>(verbosity);
    } else if (startsWithIgnoreCase(token, "diag=") || startsWithIgnoreCase(token, "diagnostics=")) {
        const char* valueToken = strchr(token, '=') != nullptr ? strchr(token, '=') + 1 : nullptr;
        bool valid = false;
        const bool enabled = analyzer_commands::parseOnOff(valueToken, &valid);
        if (!valid) {
            serialOut().println("ERR SEQ diagnostics use diag=on|off");
            return false;
        }
        outputConfig.diagnosticsEnabled = enabled;
    } else if (startsWithIgnoreCase(token, "sampleFirst=")) {
        request.sampleDumpEnabled = true;
        return parseTokenValue(token, request.sampleDumpFirstTrials, kSeqBadValue);
    } else if (startsWithIgnoreCase(token, "sampleEvery=")) {
        request.sampleDumpEnabled = true;
        return parseTokenValue(token, request.sampleDumpEveryNth, kSeqBadValue);
    } else if (startsWithIgnoreCase(token, "sampleLead=")) {
        request.sampleDumpEnabled = true;
        return parseTokenValue(token, request.sampleDumpLeadMs, kSeqBadValue);
    } else if (startsWithIgnoreCase(token, "sampleTail=")) {
        request.sampleDumpEnabled = true;
        return parseTokenValue(token, request.sampleDumpTailMs, kSeqBadValue);
    } else if (startsWithIgnoreCase(token, "sampleStep=")) {
        request.sampleDumpEnabled = true;
        return parseTokenValue(token, request.sampleDumpStepMs, kSeqBadValue);
    } else if (startsWithIgnoreCase(token, "sampleMax=")) {
        request.sampleDumpEnabled = true;
        return parseTokenValue(token, request.sampleDumpMaxRows, kSeqBadValue);
    } else if (equalsIgnoreCase(token, "external")) {
        // Observing an already-running external emitter is an explicit mode.
        // Keep the current mode if the caller already requested OBS.
        request.externalEmitter = true;
    } else if (startsWithIgnoreCase(token, "test=")) {
        strncpy(request.setupLabel, text_parse::tokenValue(token), sizeof(request.setupLabel));
        request.setupLabel[sizeof(request.setupLabel) - 1] = '\0';
    } else if (equalsIgnoreCase(token, "labels")) {
        strncpy(request.setupLabel, "labels", sizeof(request.setupLabel));
        request.setupLabel[sizeof(request.setupLabel) - 1] = '\0';
    } else if (equalsIgnoreCase(token, "start")) {
        // "SEQ OBS start ..." (tools/bench/soak.py): the verb after OBS.
    } else {
        // Unknown keys used to be ignored, so "perod=3000" ran on
        // defaults with OK SEQ (issue #31 R2-19).
        serialOut().print("ERR SEQ unknown token=");
        serialOut().println(token);
        return false;
    }
    return true;
}

// EMIT <command> to the Emitter. False for a command the Analyzer does not forward.
bool AnalyzerApp::handleEmitCommand(const char* line) {
    // The Emitter drops a line longer than its link buffer: refuse it here instead of printing OK (review 12.0 L17).
    if ((startsWithIgnoreCase(line, "EMIT CHIRP") || startsWithIgnoreCase(line, "EMIT PULSES") ||
         startsWithIgnoreCase(line, "EMIT MODE") || startsWithIgnoreCase(line, "EMIT SWEEP")) &&
        strlen(text_parse::rest(line, "EMIT")) > serial_line::kEmitterLineMax) {
        serialOut().print("ERR EMIT line_too_long max=");
        serialOut().println(static_cast<unsigned>(serial_line::kEmitterLineMax));
        return true;
    }
    if (startsWithIgnoreCase(line, "EMIT CHIRP")) {
        sendClaimedEmitterCommand(line, "CHIRP");
        return true;
    }
    if (startsWithIgnoreCase(line, "EMIT PULSES")) {
        sendClaimedEmitterCommand(line, "PULSES");
        return true;
    }
    if (startsWithIgnoreCase(line, "EMIT MODE REMOTE")) {
        sendEmitterCommand("MODE REMOTE");
        serialOut().println("OK EMIT MODE REMOTE");
        return true;
    }
    if (startsWithIgnoreCase(line, "EMIT MODE AUTO")) {
        sendEmitterCommand(text_parse::rest(line, "EMIT"));
        serialOut().println("OK EMIT MODE AUTO");
        return true;
    }
    if (startsWithIgnoreCase(line, "EMIT SWEEP")) {
        sendEmitterCommand(text_parse::rest(line, "EMIT"));
        serialOut().println("OK EMIT SWEEP");
        return true;
    }
    return false;
}

// EMIT CHIRP / PULSES: claim the Emitter (MODE REMOTE, acked) before the command, as a SEQ start does.
void AnalyzerApp::sendClaimedEmitterCommand(const char* line, const char* name) {
    sendEmitterCommand("MODE REMOTE");
    if (!waitForEmitterAck("OK MODE REMOTE", kEmitterModeAckTimeoutMs)) {
        serialOut().print("ERR EMIT ");
        serialOut().print(name);
        serialOut().println(" emitter_remote_claim_timeout");
        return;
    }
    sendEmitterCommand(text_parse::rest(line, "EMIT"));
    serialOut().print("OK EMIT ");
    serialOut().println(name);
}
