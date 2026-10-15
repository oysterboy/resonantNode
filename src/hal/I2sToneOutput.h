#pragma once

#include <stdint.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "../app/RuntimeDefaults.h"
#include "../output/ToneOutput.h"
#include "AudioSourceI2S.h"

/*
I2sToneOutput

ToneOutput for the D-AMP board: a sine with a short linear on/off ramp,
written to the TX side of the full-duplex I2S port that AudioSourceI2S owns
(mic and MAX98357A share BCLK/WS, so the amp can't be a separate port).

A small task renders one DMA buffer at a time and blocks in writeTx(), so
I2S paces it; toneOn/toneOff/setToneHz only set state and return at once.
The detection loop never writes TX, and a stalled loop can't cut a chirp.
Silence (zeros) when off. The same sample goes to both slots, so the amp's
SD_MODE channel select doesn't matter.

A pulse train (startPulseTrain, the Emitter's PULSES, DET-011) is timed
here per sample: toneOn/toneOff from the loop only land on 8 ms block
boundaries, too coarse for 5-20 ms pulses and gaps. onMs and gapMs are
what is heard: onMs of sound with the ramps inside it (ramp cut to onMs/2
if longer), gapMs of silence. A plain toneOn/toneOff chirp instead sounds
for its on time plus one ramp.
docs/decisions/2026-10-09-damp-i2s-hal-shape.md

Owns the waveform and its envelope. Does not choose when to emit, and does
not install or configure the port.
*/
class I2sToneOutput : public ToneOutput {
public:
    explicit I2sToneOutput(AudioSourceI2S& port,
                           float amplitude = runtime::kDefaultI2sToneAmplitude,
                           uint32_t rampMs = runtime::kDefaultI2sToneRampMs);

    // Starts the writer task (once). The port may be installed before or
    // after; until it is, the task idles.
    void begin() override;
    void setToneHz(uint32_t toneHz) override;
    void toneOn() override;
    void toneOff() override;
    bool startPulseTrain(uint32_t count, uint32_t onMs, uint32_t gapMs, uint32_t rampMs) override;
    void setPulseHop(const uint32_t* hz, size_t n) override;
    bool pulseTrainActive() const override;

    // Stops the writer task (tone off first; returns once the task is gone,
    // at most ~100 ms). begin() starts it again. For a build that needs the
    // tone only briefly (the Analyzer's SELFTEST amp check) and should not
    // keep a TX writer running afterwards.
    void end();

private:
    static void taskEntry(void* self);
    void run();
    void renderBlock(int32_t* frames, size_t frameCount);
    void takePulseTrainRequest(uint32_t sampleRateHz);
    uint32_t hopStep(uint32_t sampleRateHz);

    AudioSourceI2S& _port;
    float _amplitude;
    uint32_t _rampMs;
    TaskHandle_t volatile _task = nullptr;
    volatile bool _stopRequested = false;

    // Written by the caller, read by the task (32-bit stores are atomic).
    volatile uint32_t _toneHz = runtime::kDefaultChirpFrequencyHz;
    volatile bool _on = false;

    // Pulse-train handoff, caller <-> task, under _trainMux.
    mutable portMUX_TYPE _trainMux = portMUX_INITIALIZER_UNLOCKED;
    bool _trainRequested = false;
    bool _trainCancelRequested = false;
    bool _trainActive = false;
    uint32_t _reqTrainCount = 0;
    uint32_t _reqTrainOnMs = 0;
    uint32_t _reqTrainGapMs = 0;
    uint32_t _reqTrainRampMs = 0;
    static constexpr size_t kMaxHop = 8;
    uint32_t _reqHopHz[kMaxHop] = {};
    size_t _reqHopCount = 0;

    // Task-only render state.
    uint32_t _phase = 0;
    float _envelope = 0.0f;
    bool _trainRunning = false;   // pulses still to render
    bool _trainDraining = false;  // last pulse off, ramping to silence
    bool _trainOn = false;        // current segment: pulse (true) or gap
    uint32_t _trainPulsesLeft = 0;
    uint32_t _trainSegmentLeft = 0;  // samples left in the current segment
    uint32_t _trainOnSamples = 0;
    uint32_t _trainGapSamples = 0;
    uint32_t _trainRampMs = 0;
    uint32_t _hopHz[kMaxHop] = {};  // frequency per pulse, cycled; none = _toneHz
    size_t _hopCount = 0;
    size_t _hopIndex = 0;           // next hop entry
    uint32_t _trainStep = 0;        // phase step of the current pulse; 0 = use _toneHz
};
