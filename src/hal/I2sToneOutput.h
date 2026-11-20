#pragma once

#include <stdint.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "../app/RuntimeDefaults.h"
#include "../output/BroadbandBurst.h"
#include "../output/TrillShape.h"
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
    explicit I2sToneOutput(AudioSourceI2S& port, float amplitude = runtime::kDefaultI2sToneAmplitude,
                           uint32_t rampMs = runtime::kDefaultI2sToneRampMs);

    // Starts the writer task (once). The port may be installed before or
    // after; until it is, the task idles.
    void begin() override;
    void setToneHz(uint32_t toneHz) override;
    void toneOn() override;
    void toneOff() override;
    bool startPulseTrain(uint32_t count, uint32_t onMs, uint32_t gapMs, uint32_t rampMs) override;
    void setPulseHop(const uint32_t* hz, size_t n) override;
    void setPulseLeadGap(uint32_t pulses, uint32_t gapMs) override;
    void setPulseBroadband(uint32_t levelPct, bool trillOnly, uint32_t lowHz) override;
    void setPulseTrillShape(uint32_t fadeMs, uint32_t droopHz, uint32_t swellPulses, uint32_t scope) override;
    void setPulseTrillLength(uint32_t endMs, uint32_t jitterMs) override;
    void setPulseGains(const float* gain, size_t n) override;
    bool pulseTrainActive() const override;
    void setSweep(uint32_t endHz, uint32_t durationMs) override;
    bool startClip(const int16_t* samples, size_t count, float gain) override;
    bool clipActive() const override;

    // Stops the writer task (tone off first; returns once the task is gone,
    // at most ~100 ms). begin() starts it again. For a build that needs the
    // tone only briefly (the Analyzer's SELFTEST amp check) and should not
    // keep a TX writer running afterwards.
    void end();

private:
    static void taskEntry(void* self);
    void run();
    void renderBlock(int32_t* frames, size_t frameCount);
    bool renderClip(int32_t* frames, size_t frameCount, uint32_t sampleRateHz);
    void takePulseTrainRequest(uint32_t sampleRateHz);
    uint32_t hopStep(uint32_t sampleRateHz);
    float nextGain();

    AudioSourceI2S& _port;
    float _amplitude;
    uint32_t _rampMs;
    TaskHandle_t volatile _task = nullptr;
    volatile bool _stopRequested = false;

    // Written by the caller, read by the task (32-bit stores are atomic).
    volatile uint32_t _toneHz = runtime::kDefaultChirpFrequencyHz;
    volatile bool _on = false;
    // Sweep inside a plain tone (setSweep): glide target and length; 0 = steady.
    volatile uint32_t _sweepEndHz = 0;
    volatile uint32_t _sweepMs = 0;

    // Pulse-train handoff, caller <-> task, under _trainMux.
    mutable portMUX_TYPE _trainMux = portMUX_INITIALIZER_UNLOCKED;
    bool _trainRequested = false;
    bool _trainCancelRequested = false;
    bool _trainActive = false;
    uint32_t _reqTrainCount = 0;
    uint32_t _reqTrainOnMs = 0;
    uint32_t _reqTrainGapMs = 0;
    uint32_t _reqTrainRampMs = 0;
    static constexpr size_t kMaxHop = runtime::kMaxTrainPulses;
    uint32_t _reqHopHz[kMaxHop] = {};
    size_t _reqHopCount = 0;
    float _reqGain[kMaxHop] = {};
    size_t _reqGainCount = 0;
    uint32_t _reqLeadPulses = 0;
    uint32_t _reqLeadGapMs = 0;
    uint32_t _reqBroadbandPct = 0;
    bool _reqBroadbandTrillOnly = false;
    uint32_t _reqBroadbandLowHz = 0;
    trill::Shape _reqTrillShape;
    trill::Length _reqTrillLength;

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
    uint32_t _leadPulsesLeft = 0;     // pulses still followed by the lead gap (the trill)
    uint32_t _leadGapSamples = 0;
    // The broadband burst of the running train (output/BroadbandBurst.h): level 0 = none, the tone path untouched.
    broadband::BroadbandBurst _burst;
    uint32_t _bbPct = 0;
    bool _bbTrillOnly = false;
    void startPulseBurst();  // a pulse begins: a burst when the train's settings ask for one on this pulse
    // The trill shape of the running train (output/TrillShape.h); none = the tone path untouched.
    trill::Shape _trillShape;
    uint32_t _trillPulses = 0;        // the train's trill pulses, to number the one that is playing
    uint32_t _trainPulseSamples = 0;  // heard samples of one pulse (onMs)
    uint32_t _trillFadeSamples = 0;
    uint32_t _pulsePos = 0;           // samples since the current pulse began
    bool _pulseShaped = false;        // the shape is on and reaches the current pulse (trill::shapes)
    uint32_t _pulseFadeSamples = 0;   // the current pulse's fades; 0 = none (trill::fades)
    float _pulseSwell = 1.0f;         // the current trill pulse's level
    void startPulseShape();           // a pulse begins: take its trill shape
    // The trill pulse lengths of the running train (trill::Length); none = every pulse _trainOnMsBase long.
    trill::Length _trillLength;
    uint32_t _trainOnMsBase = 0;      // the train's pulse length (onMs)
    uint32_t _trainRampSamples = 0;
    uint32_t _basePulseSamples = 0;   // heard samples of a pulse at _trainOnMsBase
    void startPulseLength(uint32_t sampleRateHz);  // a pulse begins: its length (overrides the segment counter)
    uint32_t _trainRampMs = 0;
    uint32_t _hopHz[kMaxHop] = {};  // frequency per pulse, cycled; none = _toneHz
    size_t _hopCount = 0;
    size_t _hopIndex = 0;           // next hop entry
    uint32_t _trainStep = 0;        // phase step of the current pulse; 0 = use _toneHz
    // Gain per pulse, cycled; none = every pulse at _amplitude. Changes between pulses only (the envelope is 0).
    float _gain[kMaxHop] = {};
    size_t _gainCount = 0;
    size_t _gainIndex = 0;
    float _trainGain = 1.0f;
    // Clip handoff, caller <-> task, under _trainMux; the task plays it in place of the tone.
    const int16_t* _reqClip = nullptr;
    size_t _reqClipCount = 0;
    float _reqClipGain = 1.0f;
    bool _clipRequested = false;
    bool _clipActive = false;
    // Task-only clip state.
    const int16_t* _clip = nullptr;
    size_t _clipCount = 0;
    size_t _clipPos = 0;
    float _clipGain = 1.0f;

    // Sweep render state: samples since the tone started and the current
    // phase step as a float accumulator (one add per sample).
    uint32_t _sweepPos = 0;
    float _sweepStepF = 0.0f;
};
