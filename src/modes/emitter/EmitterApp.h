#pragma once

#include "../../app/RuntimeDefaults.h"
#if defined(BOARD_PIEZO)
#include "../../hal/PiezoToneOutputBTL.h"
#include "../../hal/PiezoToneOutput.h"
#else
#include "../../hal/AudioSourceI2S.h"
#include "../../hal/I2sToneOutput.h"
#endif
#include "../../app/SerialLine.h"
#include "../../output/ChirpOutput.h"

/*
EmitterApp

Standalone output-device mode used by Analyzer tests.
Receives serial commands and executes ChirpOutput/ToneOutput actions.
Does not perform detection or behavior decisions.
*/
class EmitterApp {
public:
    enum class EmitterMode {
        Auto,
        RemoteControl,
        Sweep
    };

    EmitterApp(int outputPin = 25,
               int rxPin = 16,
               int txPin = 17,
               unsigned long baudRate = 115200);
    EmitterApp(int outputPin,
               int outputBtlPin,
               int rxPin,
               int txPin,
               unsigned long baudRate);

    void begin();
    void update();

private:
    void pollControlSerial();
    // USB console: SELFTEST (NODE-015) and "EMIT <control line>", which runs
    // a control line as if it came over Serial2 (the Analyzer's EMIT syntax;
    // e.g. EMIT MODE REMOTE stops AUTO chirps from the USB side).
    void pollUsbSerial();
    void handleSelfTestCommand(const char* line);
    void handleLine(const char* line);
    void handleChirpCommand(const char* line);
    void handleModeCommand(const char* line);
    void handleSweepCommand(const char* line);
    // PULSES n pulse_ms gap_ms [freq=HZ] [ramp=MS] [trial=N] (DET-011);
    // n=/pulse=/gap= work as keys too.
    void handlePulsesCommand(const char* line);
    // sweepToHz != 0: the chirp glides from toneHz to sweepToHz over durationMs (OUT-005).
    void startChirp(unsigned long toneHz, unsigned long durationMs, unsigned long trialId = 0, unsigned long sweepToHz = 0);
    void startPulses(unsigned long toneHz, unsigned long count, unsigned long pulseMs, unsigned long gapMs,
                     unsigned long rampMs, unsigned long trialId);
    void sendStartMarkers();
    void setMode(EmitterMode mode);
    void claimExternalControl();
    void configureAuto(unsigned long intervalMs, unsigned long toneHz, unsigned long durationMs);
    void configureSweep(unsigned long startHz, unsigned long stopHz, unsigned long stepHz, unsigned long durationMs, unsigned long pauseMs);
    void advanceSweep(unsigned long now);
    const char* modeName() const;

    int _rxPin;
    int _txPin;
    unsigned long _baudRate;
    EmitterMode _mode = EmitterMode::Auto;
    unsigned long _autoIntervalMs = 2000;
    unsigned long _autoToneHz = runtime::kDefaultChirpFrequencyHz;
    unsigned long _autoDurationMs = 100;
    unsigned long _nextAutoChirpAtMs = 0;
    unsigned long _sweepStartHz = 3000;
    unsigned long _sweepStopHz = 3500;
    unsigned long _sweepStepHz = 100;
    unsigned long _sweepDurationMs = 80;
    unsigned long _sweepPauseMs = 1000;
    unsigned long _sweepCurrentHz = 1800;
    unsigned long _nextSweepStepAtMs = 0;
    unsigned long _activeTrialId = 0;
    // PULSES hop=F1,F2,...: a frequency per pulse, cycled (DET-013 / OUT-005).
    static constexpr size_t kPulseHopMax = 8;
    uint32_t _pulseHopHz[kPulseHopMax] = {};
    size_t _pulseHopCount = 0;
#if defined(BOARD_PIEZO)
    PiezoToneOutput _toneOutput;
    PiezoToneOutputBTL _toneOutputBTL;
#else
    // The amp shares the mic's I2S port; the Emitter owns the port through
    // the same class as the Node and never reads the mic.
    AudioSourceI2S _i2sPort;
    I2sToneOutput _toneOutput;
#endif
    ChirpOutput _chirpOutput;
    serial_line::LineReader<serial_line::kEmitterLineMax> _controlLine;  // Serial2, the Analyzer's commands
    serial_line::LineReader<serial_line::kEmitterLineMax> _usbLine;      // USB: SELFTEST and EMIT <command>
};
