#pragma once

#include <math.h>
#include <stdint.h>

/*
TrillShape (step 24.4.1, roadmap OUT-006)

The trill's pulses shaped closer to a field cricket's: a cricket's pulse is one wing stroke, so it swells and fades
rather than switching, its pitch falls a little as the wing slows, and the first pulses of a chirp are quieter.
Three settings, each 0 = off; with all three 0 the device's output is the plain trill, bit for bit.

  fade   a raised-cosine rise and fall of fadeMs at each end of a trill pulse (held to half the pulse)
  droop  the pitch falls by droopHz across the pulse, centred on the pulse's own frequency, so the pulse's mean
         stays on its bin (held to kMaxDroopHz)
  swell  the first swellPulses trill pulses grow from half level to full; the rest play at full level

Only the trill's pulses are shaped: the climb after it is what the detector matches, and stays as it is.

Owns: the three per-sample shapes. Does not: decide which pulses are trill pulses (I2sToneOutput), choose the
values (output.trill_* params), or know detection. Pure, host-tested (test_trill_shape).
*/
namespace trill {

// About a field cricket's fall across one pulse at 4-5 kHz, with room; past it the pulse leaves its bin.
constexpr uint32_t kMaxDroopHz = 400;
// The first pulse of a swell plays at this fraction of full level.
constexpr float kSwellStart = 0.5f;

struct Shape {
    uint32_t fadeMs = 0;
    uint32_t droopHz = 0;
    uint32_t swellPulses = 0;

    bool any() const { return fadeMs > 0 || droopHz > 0 || swellPulses > 0; }
};

// Gain of the sample at pos (samples since the pulse began) in a pulse of pulseSamples heard samples; 1 outside
// the fades, 0 at and past the pulse's end. fadeSamples 0: 1 everywhere.
inline float fadeGain(uint32_t pos, uint32_t pulseSamples, uint32_t fadeSamples) {
    if (fadeSamples == 0) {
        return 1.0f;
    }
    if (pos >= pulseSamples) {
        return 0.0f;
    }
    if (fadeSamples > pulseSamples / 2) {
        fadeSamples = pulseSamples / 2;
    }
    if (fadeSamples == 0) {
        return 1.0f;
    }
    const uint32_t fromEnd = pulseSamples - pos;
    const uint32_t edge = pos < fromEnd ? pos : fromEnd;
    if (edge >= fadeSamples) {
        return 1.0f;
    }
    return 0.5f - 0.5f * cosf(static_cast<float>(M_PI) * static_cast<float>(edge) / static_cast<float>(fadeSamples));
}

// Frequency offset in Hz at pos: +droop/2 at the pulse's start, -droop/2 at its end, 0 when droopHz is 0.
inline float droopOffsetHz(uint32_t pos, uint32_t pulseSamples, uint32_t droopHz) {
    if (droopHz == 0 || pulseSamples == 0) {
        return 0.0f;
    }
    const float d = static_cast<float>(droopHz > kMaxDroopHz ? kMaxDroopHz : droopHz);
    const float x = static_cast<float>(pos < pulseSamples ? pos : pulseSamples) / static_cast<float>(pulseSamples);
    return d * (0.5f - x);
}

// Level of trill pulse k (0 = the first): kSwellStart rising to 1 over the first swellPulses, then 1.
inline float swellGain(uint32_t k, uint32_t swellPulses) {
    if (k >= swellPulses) {
        return 1.0f;
    }
    return kSwellStart + (1.0f - kSwellStart) * static_cast<float>(k) / static_cast<float>(swellPulses);
}

}  // namespace trill
