#pragma once

#include <math.h>
#include <stdint.h>

/*
BroadbandBurst (step 24.1, docs/research/broadband-localizable-call.md)

A short burst of high-passed noise for the start of a pulse, so people can place the call: a sharp onset with a wide
spectrum carries direction where a steady tone carries little. The high-pass corner (output.broadband_low_hz) sits
above the detection window, so the band bank, the contrast and the shape rule do not see it; the sine stays the
detection signal. The burst shares the DAC's full scale with the tone: mix() scales the tone down by 1 / (1 + level)
so the sum cannot clip (clipping would put harmonics into the bank), which is the whole cost to the tone: 20 log10(1 +
level) dB, 3.5 dB at 50 %.

Level 0 is off: toneScale() is exactly 1 and the caller leaves the tone path alone, so the output is bit for bit
today's.

Pure, host-tested (test_broadband_burst).
Owns: the noise, its high-pass and decay, and the tone / burst headroom split.
Does not: decide which pulses get a burst (I2sToneOutput), or choose the level (output.* params), or know detection.
*/
namespace broadband {

constexpr uint32_t kMaxLevelPct = 100;
// A click, not a hiss: long enough to carry a spectrum, short enough to end inside a 15 ms pulse.
constexpr uint32_t kBurstMs = 4;

// The filtered noise is about 0.2 rms at the default corner; this lifts its peak to about 0.9 of full scale, so the
// level is the burst's peak against the tone's (measured on the host, 400 bursts: peak 0.67 unscaled at 6500 Hz). A
// peak past 1 is cut, which only happens with a corner far below the default.
constexpr float kNoiseGain = 1.4f;

// Which pulses of a train carry the burst.
enum class Scope : uint8_t {
    AllPulses = 0,  // every pulse, the trill's and the climb's (variant A)
    TrillOnly = 1,  // the pulses before the climb (variant B)
};

inline float levelFraction(uint32_t levelPct) {
    return static_cast<float>(levelPct > kMaxLevelPct ? kMaxLevelPct : levelPct) / 100.0f;
}

// What the tone is multiplied by so tone and burst fit one full scale; exactly 1.0f at level 0.
inline float toneScale(uint32_t levelPct) {
    return levelPct == 0 ? 1.0f : 1.0f / (1.0f + levelFraction(levelPct));
}

// One mixed sample from a tone and a burst sample, both in -1..1; |result| <= 1.
inline float mix(float tone, float burst, uint32_t levelPct) {
    const float scale = toneScale(levelPct);
    return tone * scale + burst * levelFraction(levelPct) * scale;
}

class BroadbandBurst {
public:
    // The sample rate and the high-pass corner; clears the filter. Call before trigger().
    void configure(uint32_t sampleRateHz, uint32_t lowHz) {
        _burstSamples = (sampleRateHz * kBurstMs) / 1000U;
        if (_burstSamples == 0) {
            _burstSamples = 1;
        }
        // RBJ high-pass, Q = 1/sqrt(2), two sections in a row (24 dB an octave). With the click's sharp start the
        // 3.5 and 4.7 kHz bins sit about 20 dB under its 7 kHz bin at the default corner (test_broadband_burst).
        const float w0 = 6.2831853f * static_cast<float>(lowHz) / static_cast<float>(sampleRateHz);
        const float alpha = sinf(w0) / (2.0f * 0.70710678f);
        const float c = cosf(w0);
        const float a0 = 1.0f + alpha;
        _b0 = (1.0f + c) * 0.5f / a0;
        _b1 = -(1.0f + c) / a0;
        _b2 = _b0;
        _a1 = -2.0f * c / a0;
        _a2 = (1.0f - alpha) / a0;
        _x1 = _x2 = _y1 = _y2 = _z1 = _z2 = _w1 = _w2 = 0.0f;
        _left = 0;
    }

    // Starts a burst now (a pulse onset). The noise generator carries on, so every burst differs.
    void trigger() { _left = _burstSamples; }
    bool active() const { return _left > 0; }

    // The next sample, -1..1; 0 when no burst runs. Linear decay from the first sample (a sharp onset).
    float next() {
        if (_left == 0) {
            return 0.0f;
        }
        _rng ^= _rng << 13;
        _rng ^= _rng >> 17;
        _rng ^= _rng << 5;
        const float x = static_cast<float>(_rng) * (1.0f / 2147483648.0f) - 1.0f;
        const float y = _b0 * x + _b1 * _x1 + _b2 * _x2 - _a1 * _y1 - _a2 * _y2;
        _x2 = _x1;
        _x1 = x;
        _y2 = _y1;
        _y1 = y;
        const float z = _b0 * y + _b1 * _z1 + _b2 * _z2 - _a1 * _w1 - _a2 * _w2;
        _z2 = _z1;
        _z1 = y;
        _w2 = _w1;
        _w1 = z;
        const float env = static_cast<float>(_left) / static_cast<float>(_burstSamples);
        --_left;
        const float out = z * env * kNoiseGain;
        return out > 1.0f ? 1.0f : (out < -1.0f ? -1.0f : out);
    }

private:
    uint32_t _burstSamples = 64;
    uint32_t _left = 0;
    uint32_t _rng = 2463534242U;
    float _b0 = 0.0f, _b1 = 0.0f, _b2 = 0.0f, _a1 = 0.0f, _a2 = 0.0f;
    float _x1 = 0.0f, _x2 = 0.0f, _y1 = 0.0f, _y2 = 0.0f;  // section 1: input and output history
    float _z1 = 0.0f, _z2 = 0.0f, _w1 = 0.0f, _w2 = 0.0f;  // section 2
};

}  // namespace broadband
