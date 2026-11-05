#pragma once

#include <math.h>
#include <stddef.h>
#include <stdint.h>

/*
BandBalance (ECHO-002, 2026-11-05): two bandpass filters on Whispernet's mic, the low band (200-700 Hz) and the
whisper band (1.5-4.5 kHz), RBJ biquads at each band's geometric centre with Q = centre / width. Per sample the
whisper band's output (for the level when whisper.level_band is on); per block the low band's RMS (for the low-band
check). Labelled recordings (bench/sessions/2026-11-05-labels-c920): a whisper adds 7-14 dB over the room in 1.5-4.5
kHz and 2-9 dB below 700 Hz; speech, typing, chair, paper and clothes add 12-21 dB (median) below 700 Hz.
Pure; host-tested (test_band_flatness).

Owns: the two filters' state, the low band's energy since the last take().
Does not: know the room (Whispernet's RoomLevel) or decide anything (the timbre module's range).
*/
class BandBalance {
public:
    void begin(uint32_t rateHz) {
        setBand(_low, rateHz, 200.0f, 700.0f);
        setBand(_mid, rateHz, 1500.0f, 4500.0f);
        _lowEnergy = 0.0f;
        _count = 0;
    }

    // One sample in; the whisper band's output out (same scale).
    float push(float x) {
        const float lo = run(_low, x);
        _lowEnergy += lo * lo;
        ++_count;
        return run(_mid, x);
    }

    // The low band's RMS since the last take(), cleared. False when no sample came.
    bool takeLowRms(float& rms) {
        if (_count == 0) {
            return false;
        }
        rms = sqrtf(_lowEnergy / static_cast<float>(_count));
        _lowEnergy = 0.0f;
        _count = 0;
        return true;
    }

private:
    struct Biquad {
        float b0 = 0.0f;
        float a1 = 0.0f;
        float a2 = 0.0f;
        float z1 = 0.0f;
        float z2 = 0.0f;
    };
    static void setBand(Biquad& b, uint32_t rateHz, float lowHz, float highHz) {
        const float centre = sqrtf(lowHz * highHz);
        const float q = centre / (highHz - lowHz);
        const float w0 = 6.2831853f * centre / static_cast<float>(rateHz);
        const float alpha = sinf(w0) / (2.0f * q);
        const float a0 = 1.0f + alpha;
        b.b0 = alpha / a0;  // RBJ bandpass, 0 dB peak; b1 = 0, b2 = -b0
        b.a1 = -2.0f * cosf(w0) / a0;
        b.a2 = (1.0f - alpha) / a0;
        b.z1 = b.z2 = 0.0f;
    }
    static float run(Biquad& b, float x) {
        const float y = b.b0 * x + b.z1;
        b.z1 = -b.a1 * y + b.z2;
        b.z2 = -b.b0 * x - b.a2 * y;
        return y;
    }

    Biquad _low;
    Biquad _mid;
    float _lowEnergy = 0.0f;
    size_t _count = 0;
};
