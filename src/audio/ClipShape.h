#pragma once

#include <math.h>
#include <stddef.h>
#include <stdint.h>

/*
ClipShape (ECHO-004): the echo room's answer shaped before it plays, in place on the copied clip.

- damping 0..100 %: a one-pole low-pass, its corner from the clip's top (rate / 2) at 0 % down to 1 kHz at 100 %
  (log-spaced). Every replay is a recording of a replay, so the low-pass compounds: each generation duller and, with
  a broadband clap's energy mostly high, quieter; a chain fades like an echo instead of collecting the speaker's
  distortion (the owner, 2026-11-05: a high tone travelling towards a chain's end). 0 leaves the clip as recorded.
- fadeMs: a linear fade over the clip's last fadeMs (the hard stop was audible); the output's short ramps against
  clicks stay (clipEnvelope).

Pure; test_sound_output pins it. Owns nothing: the caller's buffer.
*/
namespace clipshape {

// The low-pass corner for a damping, in Hz; 0 for no filter.
inline float cornerHz(uint32_t dampingPct, uint32_t sampleRateHz) {
    if (dampingPct == 0 || sampleRateHz == 0) {
        return 0.0f;
    }
    const float top = static_cast<float>(sampleRateHz) / 2.0f;
    const float bottom = 1000.0f < top ? 1000.0f : top;
    const float d = static_cast<float>(dampingPct > 100U ? 100U : dampingPct) / 100.0f;
    return top * powf(bottom / top, d);
}

inline void shape(int16_t* samples, size_t count, uint32_t sampleRateHz, uint32_t dampingPct, uint32_t fadeMs) {
    if (samples == nullptr || count == 0) {
        return;
    }
    const float fc = cornerHz(dampingPct, sampleRateHz);
    if (fc > 0.0f) {
        const float a = 1.0f - expf(-2.0f * 3.14159265f * fc / static_cast<float>(sampleRateHz));
        float y = static_cast<float>(samples[0]);
        for (size_t i = 0; i < count; ++i) {
            y += a * (static_cast<float>(samples[i]) - y);
            samples[i] = static_cast<int16_t>(y > 32767.0f ? 32767.0f : (y < -32768.0f ? -32768.0f : y));
        }
    }
    size_t fade = static_cast<size_t>(static_cast<uint64_t>(sampleRateHz) * fadeMs / 1000U);
    if (fade > count) {
        fade = count;
    }
    for (size_t k = 0; k < fade; ++k) {
        // The last sample ends at 0; fade samples before it the gain rises linearly to 1.
        const size_t i = count - 1 - k;
        samples[i] = static_cast<int16_t>(static_cast<float>(samples[i]) * static_cast<float>(k) / static_cast<float>(fade));
    }
}

}  // namespace clipshape
