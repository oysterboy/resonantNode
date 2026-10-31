#pragma once

#include <stddef.h>

/*
The gain envelope of a played clip (ECHO-001): a linear ramp in over the first rampSamples and out over the last,
so a clip cut out of the capture ring starts and ends without a click. A clip shorter than two ramps ramps over half
its length each way. Pure; I2sToneOutput applies it per sample, test_sound_output pins it.
*/
inline float clipEnvelope(size_t pos, size_t count, size_t rampSamples) {
    if (pos >= count) {
        return 0.0f;
    }
    size_t ramp = rampSamples;
    if (ramp * 2 > count) {
        ramp = count / 2;
    }
    if (ramp == 0) {
        return 1.0f;
    }
    const size_t fromEnd = count - 1 - pos;
    const size_t edge = pos < fromEnd ? pos : fromEnd;
    return edge >= ramp ? 1.0f : static_cast<float>(edge + 1) / static_cast<float>(ramp + 1);
}
