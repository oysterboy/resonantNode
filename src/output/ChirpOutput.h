#pragma once
#include <Arduino.h>

#include "../app/RuntimeDefaults.h"
#include "ToneOutput.h"

/*
Concrete chirp output device.

Owns waveform emission and reports output lifecycle.

Does NOT:
- decide when to chirp
- own behavior logic
*/

class ChirpOutput {
public:
    enum class ChirpPattern {
        Single,
        Triple,
        Idle
    };

    explicit ChirpOutput(ToneOutput& toneOutput, uint32_t toneHz = runtime::kDefaultChirpFrequencyHz);

    void begin();
    void setToneHz(uint32_t toneHz);
    uint32_t toneHz() const;
    void setTiming(unsigned long chirpOnMs, unsigned long chirpPauseMs);
    void start(ChirpPattern pattern = ChirpPattern::Single);
    // `count` pulses of onMs with gapMs pauses at the current tone (the
    // Emitter's PULSES, DET-011). Timed by the ToneOutput when it can
    // (I2S: per sample, rampMs edges); otherwise from update(), with the
    // device's own edges and the loop's millisecond resolution.
    void startPulseTrain(uint32_t count, unsigned long onMs, unsigned long gapMs, uint32_t rampMs);
    // Frequency per pulse for the next startPulseTrain() (hop); n = 0 clears.
    void setPulseHop(const uint32_t* hz, size_t n) { _toneOutput.setPulseHop(hz, n); }
    void stop();
    void update();
    bool isActive() const;
    bool finished();

    // The device this output drives. SELFTEST's amp check plays its test
    // tones through it directly (fixed 100 ms tones, timed by the check);
    // nothing else should bypass start()/update().
    ToneOutput& toneOutput();

private:
    ToneOutput& _toneOutput;
    bool _active = false;
    bool _finished = false;
    bool _deviceTimedTrain = false;
    ChirpPattern _activePattern = ChirpPattern::Single;
    int _phase = 0;
    int _beepCount = 1;
    unsigned long _phaseStartMs = 0;
    unsigned long _activeChirpOnMs = 100;
    unsigned long _activeChirpPauseMs = 150;
    uint32_t _toneHz;
    unsigned long _chirpOnMs = 100;
    unsigned long _chirpPauseMs = 150;
    unsigned long _tripleChirpOnMs = 100;
    unsigned long _tripleChirpPauseMs = 12;
    unsigned long _idleChirpOnMs = 500;
    unsigned long _idleChirpPauseMs = 200;
    uint32_t _idleFirstPulseToneHz = 2000;
};
