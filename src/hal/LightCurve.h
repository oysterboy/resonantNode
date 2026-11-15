#pragma once

#include <stdint.h>

/*
light_curve

Owns: what a light level in 0..1 means on the wire. 1.0 is the brightest the build-time current cap allows for the
colour on all pixels, 0 is dark, and the steps in between follow the eye (CIE 1931 lightness), so 0.5 looks about
half as bright. Also the current estimate the cap is held against (decisions/2026-11-13-light-level-scalar-cap-in-hal.md).
Does not own: which level the light should show (Behavior and the light's effects, above the driver), the cap's
value (LIGHT_MAX_MA in BoardConfig.h) or the wire (PixelStrip).

Pure: no Arduino, host-tested in test/test_light_curve.
*/
namespace light_curve {

// One colour channel of one pixel at full level draws about this much. The data sheets say 20 mA; R1's strip (the
// black RGB reel) drew about 0.31 A where 20 mA estimated 500 mA (bench/sessions/2026-11-13-r1-light-power), about
// 12.4 mA a channel with the buck's efficiency assumed at 85 %. 13 keeps a little margin, so the cap still errs safe.
constexpr float kChannelFullMa = 13.0f;

// Lightness L* (level x 100) to relative luminance (0..1), CIE 1931. The two pieces meet at L* = 8.
inline float perceivedToLinear(float level) {
    if (!(level > 0.0f)) {  // also NaN
        return 0.0f;
    }
    if (level >= 1.0f) {
        return 1.0f;
    }
    const float l = level * 100.0f;
    if (l <= 8.0f) {
        return l / 903.3f;
    }
    const float t = (l + 16.0f) / 116.0f;
    return t * t * t;
}

// Estimated current of `pixels` pixels all showing `colour` (`channels` bytes, 0..255 each).
inline float estimateMa(const uint8_t* colour, uint8_t channels, uint16_t pixels) {
    float level = 0.0f;
    for (uint8_t i = 0; i < channels; ++i) {
        level += colour[i] / 255.0f;
    }
    return level * static_cast<float>(pixels) * kChannelFullMa;
}

// The factor that brings a frame estimated at wantedMa down to the cap; 1 when it fits already.
inline float capScale(float wantedMa, uint16_t capMa) {
    return wantedMa > static_cast<float>(capMa) ? static_cast<float>(capMa) / wantedMa : 1.0f;
}

// Hue (degrees, any value, wraps) and saturation (0..1) to RGB at full value, each channel 0..1.
inline void hsToRgb(float hueDeg, float sat, float rgb[3]) {
    float h = hueDeg - 360.0f * static_cast<float>(static_cast<int>(hueDeg / 360.0f));
    if (h < 0.0f) {
        h += 360.0f;
    }
    const float s = sat < 0.0f ? 0.0f : (sat > 1.0f ? 1.0f : sat);
    const float sector = h / 60.0f;
    const int i = static_cast<int>(sector) % 6;
    const float f = sector - static_cast<float>(static_cast<int>(sector));
    const float p = 1.0f - s;
    const float q = 1.0f - s * f;
    const float t = 1.0f - s * (1.0f - f);
    const float table[6][3] = {{1.0f, t, p}, {q, 1.0f, p}, {p, 1.0f, t}, {p, q, 1.0f}, {t, p, 1.0f}, {1.0f, p, q}};
    for (int c = 0; c < 3; ++c) {
        rgb[c] = table[i][c];
    }
}

// The one scale every HSB colour gets: full white (all three channels) on all pixels fits the cap, so any other
// colour fits too, and a change of hue does not pulse in brightness (decisions/2026-11-13-light-colour-model-and-
// states.md, option (b)).
inline float hsbScale(uint16_t pixels, uint16_t capMa) {
    return capScale(3.0f * static_cast<float>(pixels) * kChannelFullMa, capMa);
}

// RGB bytes for hue, saturation and a perceived brightness 0..1 (1.0: full white would sit at the cap). Rounded
// down, so the estimate never exceeds the cap.
inline void rgbAtHsb(float hueDeg, float sat, float bright, uint16_t pixels, uint16_t capMa, uint8_t out[3]) {
    float rgb[3];
    hsToRgb(hueDeg, sat, rgb);
    const float scale = 255.0f * hsbScale(pixels, capMa) * perceivedToLinear(bright);
    for (int c = 0; c < 3; ++c) {
        out[c] = static_cast<uint8_t>(rgb[c] * scale);
    }
}

// Channel totals over the whole strip for hue, saturation and a perceived brightness: the sum of one channel's bytes
// over all pixels, so a level between two byte values survives as a fraction spread over the pixels (ditherByte).
// Rounded down, so the frame never exceeds the cap; each pixel's share is at most 255.
inline void rgbTotalsAtHsb(float hueDeg, float sat, float bright, uint16_t pixels, uint16_t capMa, uint32_t out[3]) {
    float rgb[3];
    hsToRgb(hueDeg, sat, rgb);
    const float scale = 255.0f * hsbScale(pixels, capMa) * perceivedToLinear(bright) * static_cast<float>(pixels);
    for (int c = 0; c < 3; ++c) {
        out[c] = static_cast<uint32_t>(rgb[c] * scale);
    }
}

// A step coprime with `pixels` near 0.38 of it: the order in which the pixels take the extra byte (ditherByte)
// jumps around the strip, so the brighter pixels spread out instead of bunching at one end.
inline uint16_t ditherStride(uint16_t pixels) {
    if (pixels < 3) {
        return 1;
    }
    for (uint16_t s = static_cast<uint16_t>(pixels * 0.38f); s < pixels; ++s) {
        uint16_t a = s;
        uint16_t b = pixels;
        while (b != 0) {
            const uint16_t t = static_cast<uint16_t>(a % b);
            a = b;
            b = t;
        }
        if (a == 1 && s > 0) {
            return s;
        }
    }
    return 1;
}

// One pixel's byte of a channel total: total / pixels, plus one on the first (total % pixels) pixels in the stride's
// order. A total one higher lights exactly one more pixel one byte brighter, so a slow fade at the bottom moves in
// steps of 1/pixels of a byte instead of whole bytes (decisions/2026-11-15-node-light-one-colour-and-floor.md).
inline uint8_t ditherByte(uint32_t total, uint16_t pixels, uint16_t stride, uint16_t pixel) {
    if (pixels == 0) {
        return 0;
    }
    const uint32_t base = total / pixels;
    const uint32_t rest = total % pixels;
    const uint32_t rank = (static_cast<uint32_t>(pixel) * stride) % pixels;
    const uint32_t v = base + (rank < rest ? 1U : 0U);
    return static_cast<uint8_t>(v > 255U ? 255U : v);
}

// The bytes for `colour` at `level`: the colour scaled to the cap, then by the perceived curve. Rounded down, so the
// estimate of the result never exceeds the cap.
inline void colourAtLevel(float level, const uint8_t* colour, uint8_t channels, uint16_t pixels, uint16_t capMa,
                          uint8_t* out) {
    const float scale = capScale(estimateMa(colour, channels, pixels), capMa) * perceivedToLinear(level);
    for (uint8_t i = 0; i < channels; ++i) {
        out[i] = static_cast<uint8_t>(colour[i] * scale);
    }
}

}  // namespace light_curve
