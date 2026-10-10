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

    // Stops the writer task (tone off first; returns once the task is gone,
    // at most ~100 ms). begin() starts it again. For a build that needs the
    // tone only briefly (the Analyzer's SELFTEST amp check) and should not
    // keep a TX writer running afterwards.
    void end();

private:
    static void taskEntry(void* self);
    void run();
    void renderBlock(int32_t* frames, size_t frameCount);

    AudioSourceI2S& _port;
    float _amplitude;
    uint32_t _rampMs;
    TaskHandle_t volatile _task = nullptr;
    volatile bool _stopRequested = false;

    // Written by the caller, read by the task (32-bit stores are atomic).
    volatile uint32_t _toneHz = runtime::kDefaultChirpFrequencyHz;
    volatile bool _on = false;

    // Task-only render state.
    uint32_t _phase = 0;
    float _envelope = 0.0f;
};
