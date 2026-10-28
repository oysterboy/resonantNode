#include "EmitterApp.h"
#include "../../app/FirmwareOta.h"
#include "../../app/EmitterLimits.h"

#include <Arduino.h>
#include <string.h>

#include "../../app/BuildInfo.h"
#include "../../app/TextParse.h"
#include "../../app/RuntimeDefaults.h"
#include "../../app/SerialLine.h"
#include "../../selftest/AudioSelfTest.h"
#include "../../selftest/SelfTestReport.h"
#include "../../app/TimeBase.h"
#include "../../app/TimingUtils.h"
#include "../../app/SerialOut.h"

using text_parse::startsWith;

namespace {
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

void sendEmitterMarker(const char* line) {
    Serial2.println(line);
}
}

EmitterApp::EmitterApp(int outputPin, int outputBtlPin, int rxPin, int txPin, unsigned long baudRate)
    : _rxPin(rxPin),
      _txPin(txPin),
      _baudRate(baudRate),
#if defined(BOARD_PIEZO)
      _toneOutput(outputPin),
      _toneOutputBTL(outputPin, outputBtlPin),
      _chirpOutput(outputBtlPin >= 0 ? static_cast<ToneOutput&>(_toneOutputBTL)
                                     : static_cast<ToneOutput&>(_toneOutput)) {
}
#else
      _i2sPort(runtime::kDefaultAudioI2SSckPin, runtime::kDefaultAudioI2SWsPin, runtime::kDefaultAudioI2SDataPin,
               static_cast<int>(runtime::kDefaultAudioI2SSampleRateHz),
               static_cast<int>(runtime::kDefaultAudioI2SBitsPerSample), runtime::kDefaultAudioI2SDataOutPin),
      _toneOutput(_i2sPort),
      _chirpOutput(_toneOutput) {
}
#endif

void EmitterApp::begin() {
    printBuildIdentity(serialOut(), "emitter");
    serialOut().println();

    Serial2.begin(_baudRate, SERIAL_8N1, _rxPin, _txPin);
#if !defined(BOARD_PIEZO)
    _i2sPort.begin();
#endif
    _chirpOutput.begin();
    configureAuto(_autoIntervalMs, _autoToneHz, _autoDurationMs);
    setMode(EmitterMode::Auto);
    _nextAutoChirpAtMs = timebase::nowMs();
    _controlLine.reset();
    _usbLine.reset();

    serialOut().print("EVT emitter_ready mode=");
    serialOut().print(modeName());
    serialOut().print(" interval_ms=");
    serialOut().print(_autoIntervalMs);
    serialOut().print(" freq_hz=");
    serialOut().print(_autoToneHz);
    serialOut().print(" dur_ms=");
    serialOut().println(_autoDurationMs);
}

void EmitterApp::update() {
    ota::confirmRunningImage(timebase::nowMs(), serialOut());
    pollControlSerial();
    pollUsbSerial();
    _chirpOutput.update();

    if (_chirpOutput.finished()) {
        char markerLine[96];
        snprintf(markerLine, sizeof(markerLine), "EMIT_DONE trial=%lu t_ms=%lu", _activeTrialId, timebase::nowMs());
        sendEmitterMarker(markerLine);
        snprintf(markerLine, sizeof(markerLine), "EMIT_DRIVE_OFF trial=%lu t_ms=%lu", _activeTrialId,
                 timebase::nowMs());
        sendEmitterMarker(markerLine);
        _activeTrialId = 0;
        serialOut().println("EVT emitter_chirp_finished");
    }

    if (_mode == EmitterMode::Auto && !_chirpOutput.isActive()) {
        const unsigned long now = timebase::nowMs();
        if (timing::atOrAfter(now, _nextAutoChirpAtMs)) {
            startChirp(_autoToneHz, _autoDurationMs);
            _nextAutoChirpAtMs = now + _autoIntervalMs;
        }
    }

    if (_mode == EmitterMode::Sweep) {
        advanceSweep(timebase::nowMs());
    }
}

void EmitterApp::pollControlSerial() {
    serial_line::pollLines(Serial2, _controlLine, serialOut(),
                           "ERR line_too_long max=", [this](const char* line) { handleLine(line); });
}

void EmitterApp::pollUsbSerial() {
    serial_line::pollLines(Serial, _usbLine, serialOut(), "ERR line_too_long max=", [this](const char* line) {
        if (strncmp(line, "SELFTEST", 8) == 0 || strncmp(line, "selftest", 8) == 0) {
            handleSelfTestCommand(line);
        } else if (strncmp(line, "OTA", 3) == 0 || strncmp(line, "ota", 3) == 0) {
            ota::runCommand(line, serialOut());  // firmware over WiFi (step 11d); reboots on success
        } else if (strncmp(line, "EMIT ", 5) == 0) {
            handleLine(text_parse::rest(line, "EMIT"));
        }
    });
}

void EmitterApp::handleSelfTestCommand(const char* line) {
    const selftest::Command command = selftest::parseCommand(line);
    if (command.kind == selftest::Command::Kind::None) {
        return;
    }
    if (command.kind == selftest::Command::Kind::Help || command.kind == selftest::Command::Kind::Unknown) {
        selftest::printHelp(serialOut(), false);
        return;
    }
    // Blocking: no AUTO / sweep chirp may start meanwhile; stop one in flight.
    if (_chirpOutput.isActive()) {
        _chirpOutput.stop();
        _activeTrialId = 0;
    }

#if defined(BOARD_PIEZO)
    AudioSourceI2S* mic = nullptr;  // the piezo Emitter build does not own the mic port
#else
    AudioSourceI2S* mic = &_i2sPort;
#endif

    if (command.kind == selftest::Command::Kind::Chirp) {
        selftest::emitChirpsBlocking(serialOut(), mic, _chirpOutput.toneOutput(), runtime::kDefaultChirpFrequencyHz,
                                     command.chirps, command.durMs, command.gapMs);
    } else {
        selftest::Tally tally;
        const bool external = command.kind == selftest::Command::Kind::External;
        selftest::printBegin(serialOut(), "emitter", external ? "external" : "full");
        selftest::printBoard(serialOut(), tally, "emitter");
        if (external) {
            selftest::skip(serialOut(), tally, "external_chirp", "not_node");
        } else if (mic == nullptr) {
            selftest::skip(serialOut(), tally, "mic", "no_mic_in_build");
            selftest::skip(serialOut(), tally, "amp", "no_mic_in_build");
        } else {
            if (selftest::runMicCheck(serialOut(), tally, *mic) != selftest::Result::Pass) {
                selftest::skip(serialOut(), tally, "amp", "mic_failed");
            } else {
                selftest::runAmpCheck(serialOut(), tally, *mic, _chirpOutput.toneOutput(),
                                      runtime::kDefaultChirpFrequencyHz, "i2s");
            }
        }
        selftest::printSummary(serialOut(), tally, "emitter");
    }

    // Back to the configured tone; AUTO resumes one interval from now.
    _chirpOutput.setToneHz(static_cast<uint32_t>(_mode == EmitterMode::Sweep ? _sweepCurrentHz : _autoToneHz));
    _nextAutoChirpAtMs = timebase::nowMs() + _autoIntervalMs;
    _nextSweepStepAtMs = timebase::nowMs() + _sweepPauseMs;
}

void EmitterApp::handleLine(const char* line) {
    // Echo every control line to USB so the Serial2 link can be checked from
    // the Emitter's own console.
    serialOut().print("EVT emitter_cmd line=");
    serialOut().println(line);

    char buffer[serial_line::kEmitterLineMax + 1];
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

    if (strcmp(token, "PULSES") == 0) {
        claimExternalControl();
        handlePulsesCommand(line);
        return;
    }

    if (strcmp(token, "AUTO") == 0 || strcmp(token, "REMOTE") == 0 || strcmp(token, "REMOTE_CONTROL") == 0 ||
        strcmp(token, "MODE") == 0 || strcmp(token, "SWEEP") == 0) {
        claimExternalControl();
        handleModeCommand(line);
    }
}

// The tone must be playable: on the I2S (D-AMP) output anything at or above
// half the sample rate aliases, e.g. freq=17000 played ~1 kHz while the
// ack said 17000 (issue #31 R2-20). The piezo LEDC output has no such limit.
static bool emitterToneHzPlayable(unsigned long toneHz) {
#if defined(BOARD_PIEZO)
    return toneHz > 0;
#else
    return toneHz > 0 && toneHz < runtime::kDefaultAudioI2SSampleRateHz / 2;
#endif
}

// A value that is not a plain number is refused, not read as 0 (step 11e item 8).
static void printBadValue(const char* command, const char* token) {
    serialOut().print("ERR ");
    serialOut().print(command);
    serialOut().print(" bad value token=");
    serialOut().println(token != nullptr ? token : "");
}

static void printToneHzRejected(const char* command, unsigned long toneHz) {
    serialOut().print("ERR ");
    serialOut().print(command);
    serialOut().print(" freq=");
    serialOut().print(toneHz);
    serialOut().println(" not playable");
}

void EmitterApp::handleChirpCommand(const char* line) {
    char buffer[serial_line::kEmitterLineMax + 1];
    strncpy(buffer, line, sizeof(buffer));
    buffer[sizeof(buffer) - 1] = '\0';

    unsigned long toneHz = runtime::kDefaultChirpFrequencyHz;
    unsigned long durationMs = 100;
    unsigned long trialId = 0;
    unsigned long sweepToHz = 0;
    char* savePtr = nullptr;
    char* token = strtok_r(buffer, " ", &savePtr);
    token = nextToken(savePtr);
    while (token != nullptr) {
        if (startsWith(token, "trial=")) {
            if (!text_parse::parseUnsigned(findValue(token), trialId)) {
                printBadValue("CHIRP", token);
                return;
            }
        } else if (startsWith(token, "freq=")) {
            if (!text_parse::parseUnsigned(findValue(token), toneHz)) {
                printBadValue("CHIRP", token);
                return;
            }
        } else if (startsWith(token, "dur=")) {
            if (!text_parse::parseUnsigned(findValue(token), durationMs)) {
                printBadValue("CHIRP", token);
                return;
            }
        } else if (startsWith(token, "sweep=")) {
            // Glide from freq= to this frequency over dur= (OUT-005).
            if (!text_parse::parseUnsigned(findValue(token), sweepToHz)) {
                printBadValue("CHIRP", token);
                return;
            }
        }

        token = nextToken(savePtr);
    }

    if (!emitterToneHzPlayable(toneHz)) {
        printToneHzRejected("CHIRP", toneHz);
        return;
    }
    if (sweepToHz != 0 && !emitterToneHzPlayable(sweepToHz)) {
        printToneHzRejected("CHIRP sweep", sweepToHz);
        return;
    }
    startChirp(toneHz, durationMs, trialId, sweepToHz);
}

// Bounds keep one train well inside a SEQ period and the 8 ms block/ramp
// arithmetic meaningful; they are not detector limits.
using emitter_limits::kPulsesMaxCount;
using emitter_limits::kPulsesMaxGapMs;
using emitter_limits::kPulsesMaxPulseMs;
using emitter_limits::kPulsesMaxRampMs;

void EmitterApp::handlePulsesCommand(const char* line) {
    char buffer[serial_line::kEmitterLineMax + 1];
    strncpy(buffer, line, sizeof(buffer));
    buffer[sizeof(buffer) - 1] = '\0';

    unsigned long toneHz = runtime::kDefaultChirpFrequencyHz;
    unsigned long count = 0;
    unsigned long pulseMs = 0;
    unsigned long gapMs = 0;
    unsigned long rampMs = runtime::kDefaultI2sToneRampMs;
    unsigned long trialId = 0;
    unsigned long positional = 0;
    bool hopBad = false;
    _pulseHopCount = 0;
    char* savePtr = nullptr;
    char* token = strtok_r(buffer, " ", &savePtr);
    token = nextToken(savePtr);
    while (token != nullptr) {
        if (startsWith(token, "hop=")) {
            const char* list = findValue(token);
            while (list != nullptr && *list != '\0') {
                char* end = nullptr;
                const unsigned long hz = strtoul(list, &end, 10);
                if (end == list || _pulseHopCount >= kPulseHopMax || !emitterToneHzPlayable(hz)) {
                    hopBad = true;
                    break;
                }
                _pulseHopHz[_pulseHopCount++] = static_cast<uint32_t>(hz);
                list = (*end == ',') ? end + 1 : end;
                if (*end != ',' && *end != '\0') {
                    hopBad = true;
                    break;
                }
            }
        } else if (startsWith(token, "trial=")) {
            if (!text_parse::parseUnsigned(findValue(token), trialId)) {
                printBadValue("PULSES", token);
                return;
            }
        } else if (startsWith(token, "freq=")) {
            if (!text_parse::parseUnsigned(findValue(token), toneHz)) {
                printBadValue("PULSES", token);
                return;
            }
        } else if (startsWith(token, "ramp=")) {
            if (!text_parse::parseUnsigned(findValue(token), rampMs)) {
                printBadValue("PULSES", token);
                return;
            }
        } else if (startsWith(token, "n=")) {
            if (!text_parse::parseUnsigned(findValue(token), count)) {
                printBadValue("PULSES", token);
                return;
            }
        } else if (startsWith(token, "pulse=")) {
            if (!text_parse::parseUnsigned(findValue(token), pulseMs)) {
                printBadValue("PULSES", token);
                return;
            }
        } else if (startsWith(token, "gap=")) {
            if (!text_parse::parseUnsigned(findValue(token), gapMs)) {
                printBadValue("PULSES", token);
                return;
            }
        } else if (strchr(token, '=') == nullptr) {
            unsigned long value = 0;
            if (!text_parse::parseUnsigned(token, value)) {
                printBadValue("PULSES", token);
                return;
            }
            if (positional == 0) {
                count = value;
            } else if (positional == 1) {
                pulseMs = value;
            } else if (positional == 2) {
                gapMs = value;
            }
            ++positional;
        }
        token = nextToken(savePtr);
    }

    if (count == 0 || count > kPulsesMaxCount || pulseMs == 0 || pulseMs > kPulsesMaxPulseMs || gapMs == 0 ||
        gapMs > kPulsesMaxGapMs || rampMs > kPulsesMaxRampMs) {
        serialOut().print("ERR PULSES n=");
        serialOut().print(count);
        serialOut().print(" pulse=");
        serialOut().print(pulseMs);
        serialOut().print(" gap=");
        serialOut().print(gapMs);
        serialOut().print(" ramp=");
        serialOut().print(rampMs);
        serialOut().print(" use PULSES n(1-");
        serialOut().print(kPulsesMaxCount);
        serialOut().print(") pulse_ms(1-");
        serialOut().print(kPulsesMaxPulseMs);
        serialOut().print(") gap_ms(1-");
        serialOut().print(kPulsesMaxGapMs);
        serialOut().print(") [freq=HZ] [ramp=0-");
        serialOut().print(kPulsesMaxRampMs);
        serialOut().println("] [trial=N]");
        return;
    }
    if (hopBad) {
        _pulseHopCount = 0;
        serialOut().println("ERR PULSES hop=F1,F2,... (up to 8 playable frequencies)");
        return;
    }
    if (!emitterToneHzPlayable(toneHz)) {
        printToneHzRejected("PULSES", toneHz);
        return;
    }
    startPulses(toneHz, count, pulseMs, gapMs, rampMs, trialId);
}

void EmitterApp::handleModeCommand(const char* line) {
    char buffer[serial_line::kEmitterLineMax + 1];
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
            if (startsWith(token, "interval=")) {
                if (!text_parse::parseUnsigned(findValue(token), intervalMs)) {
                    printBadValue("AUTO", token);
                    return;
                }
            } else if (startsWith(token, "freq=")) {
                if (!text_parse::parseUnsigned(findValue(token), toneHz)) {
                    printBadValue("AUTO", token);
                    return;
                }
            } else if (startsWith(token, "dur=")) {
                if (!text_parse::parseUnsigned(findValue(token), durationMs)) {
                    printBadValue("AUTO", token);
                    return;
                }
            }
            token = nextToken(savePtr);
        }

        if (!emitterToneHzPlayable(toneHz)) {
            printToneHzRejected("MODE AUTO", toneHz);
            return;
        }
        _chirpOutput.stop();
        configureAuto(intervalMs, toneHz, durationMs);
        setMode(EmitterMode::Auto);
        _nextAutoChirpAtMs = timebase::nowMs();
        char modeLine[96];
        snprintf(modeLine, sizeof(modeLine), "OK MODE %s interval=%lu freq=%lu dur=%lu", modeName(), _autoIntervalMs,
                 _autoToneHz, _autoDurationMs);
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
    char buffer[serial_line::kEmitterLineMax + 1];
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
        if (startsWith(token, "start=")) {
            if (!text_parse::parseUnsigned(findValue(token), startHz)) {
                printBadValue("SWEEP", token);
                return;
            }
        } else if (startsWith(token, "stop=")) {
            if (!text_parse::parseUnsigned(findValue(token), stopHz)) {
                printBadValue("SWEEP", token);
                return;
            }
        } else if (startsWith(token, "step=")) {
            if (!text_parse::parseUnsigned(findValue(token), stepHz)) {
                printBadValue("SWEEP", token);
                return;
            }
        } else if (startsWith(token, "dur=")) {
            if (!text_parse::parseUnsigned(findValue(token), durationMs)) {
                printBadValue("SWEEP", token);
                return;
            }
        } else if (startsWith(token, "pause=")) {
            if (!text_parse::parseUnsigned(findValue(token), pauseMs)) {
                printBadValue("SWEEP", token);
                return;
            }
        }
        token = nextToken(savePtr);
    }

    if (!emitterToneHzPlayable(startHz) || !emitterToneHzPlayable(stopHz)) {
        printToneHzRejected("SWEEP", emitterToneHzPlayable(startHz) ? stopHz : startHz);
        return;
    }
    configureSweep(startHz, stopHz, stepHz, durationMs, pauseMs);
    _chirpOutput.stop();
    setMode(EmitterMode::Sweep);
    _nextSweepStepAtMs = timebase::nowMs();
    char modeLine[128];
    snprintf(modeLine, sizeof(modeLine), "OK MODE %s start=%lu stop=%lu step=%lu dur=%lu pause=%lu", modeName(),
             _sweepStartHz, _sweepStopHz, _sweepStepHz, _sweepDurationMs, _sweepPauseMs);
    sendControlAck(modeLine);
}

void EmitterApp::startChirp(unsigned long toneHz, unsigned long durationMs, unsigned long trialId,
                            unsigned long sweepToHz) {
    if (_chirpOutput.isActive()) {
        _chirpOutput.stop();
    }
    _activeTrialId = trialId;
    _chirpOutput.setToneHz(static_cast<uint32_t>(toneHz));
    // A sweep is per chirp: set it for this one, or clear a previous one.
    _chirpOutput.setSweep(static_cast<uint32_t>(sweepToHz), sweepToHz != 0 ? static_cast<uint32_t>(durationMs) : 0U);
    _chirpOutput.setTiming(durationMs, 0);
    _chirpOutput.start();
    sendStartMarkers();
    if (_mode == EmitterMode::Auto) {
        _nextAutoChirpAtMs = timebase::nowMs() + _autoIntervalMs;
    }

    char chirpLine[128];
    if (sweepToHz != 0) {
        snprintf(chirpLine, sizeof(chirpLine), "OK CHIRP freq=%lu dur=%lu sweep=%lu", toneHz, durationMs, sweepToHz);
    } else {
        snprintf(chirpLine, sizeof(chirpLine), "OK CHIRP freq=%lu dur=%lu", toneHz, durationMs);
    }
    sendControlAck(chirpLine);
}

void EmitterApp::startPulses(unsigned long toneHz, unsigned long count, unsigned long pulseMs, unsigned long gapMs,
                             unsigned long rampMs, unsigned long trialId) {
    if (_chirpOutput.isActive()) {
        _chirpOutput.stop();
    }
    _activeTrialId = trialId;
    _chirpOutput.setToneHz(static_cast<uint32_t>(toneHz));
    _chirpOutput.setPulseHop(_pulseHopHz, _pulseHopCount);
    _chirpOutput.startPulseTrain(static_cast<uint32_t>(count), pulseMs, gapMs, static_cast<uint32_t>(rampMs));
    sendStartMarkers();

#if defined(BOARD_PIEZO)
    rampMs = 0;  // LEDC switches at once; edges land on the loop's ms ticks
#else
    if (rampMs > pulseMs / 2) {
        rampMs = pulseMs / 2;  // I2sToneOutput cuts it the same way
    }
#endif
    // pulse/gap are heard sound/silence (ramps inside the pulse), so span
    // is first sound to last silence.
    const unsigned long spanMs = count * pulseMs + (count - 1) * gapMs;
    char ackLine[128];
    snprintf(ackLine, sizeof(ackLine), "OK PULSES n=%lu pulse=%lu gap=%lu freq=%lu ramp=%lu span=%lu hop=%u", count,
             pulseMs, gapMs, toneHz, rampMs, spanMs, static_cast<unsigned>(_pulseHopCount));
    sendControlAck(ackLine);
}

void EmitterApp::sendStartMarkers() {
    char markerLine[96];
    snprintf(markerLine, sizeof(markerLine), "EMIT_START trial=%lu t_ms=%lu", _activeTrialId, timebase::nowMs());
    sendEmitterMarker(markerLine);
    snprintf(markerLine, sizeof(markerLine), "EMIT_DRIVE_ON trial=%lu t_ms=%lu", _activeTrialId, timebase::nowMs());
    sendEmitterMarker(markerLine);
}

void EmitterApp::setMode(EmitterMode mode) {
    _mode = mode;
}

void EmitterApp::claimExternalControl() {
    if (_mode == EmitterMode::RemoteControl) {
        return;
    }

    _chirpOutput.stop();
    _nextAutoChirpAtMs = timebase::nowMs();
    _nextSweepStepAtMs = timebase::nowMs();
    setMode(EmitterMode::RemoteControl);
}

void EmitterApp::configureAuto(unsigned long intervalMs, unsigned long toneHz, unsigned long durationMs) {
    _autoIntervalMs = intervalMs;
    _autoToneHz = toneHz;
    _autoDurationMs = durationMs;
    _chirpOutput.setToneHz(static_cast<uint32_t>(_autoToneHz));
    _chirpOutput.setTiming(_autoDurationMs, 0);
}

void EmitterApp::configureSweep(unsigned long startHz, unsigned long stopHz, unsigned long stepHz,
                                unsigned long durationMs, unsigned long pauseMs) {
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

    if (timing::beforeDeadline(now, _nextSweepStepAtMs)) {
        return;
    }

    _chirpOutput.setToneHz(static_cast<uint32_t>(_sweepCurrentHz));
    _chirpOutput.setTiming(_sweepDurationMs, 0);
    _chirpOutput.start();
    serialOut().print("EVT sweep_step freq=");
    serialOut().println(_sweepCurrentHz);

    // "next step past stop" without unsigned underflow when step > stop (issue #31).
    if (_sweepCurrentHz >= _sweepStopHz || _sweepStepHz > _sweepStopHz - _sweepCurrentHz) {
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
