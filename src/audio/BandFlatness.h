#pragma once

#include <math.h>
#include <stddef.h>
#include <stdint.h>

/*
BandFlatness (ECHO-002, step 20.3)

How flat the spectrum is over a stretch of samples, 0 (one tone) .. 1 (white noise): eight bandpass biquads spaced
evenly in log frequency (300 .. 10,000 Hz, Q 2.5), each band's energy divided by its centre (energy per Hz, so white
noise reads flat across the constant-Q bands), then the bands' geometric mean over their mean. Whispernet's timbre
check: on the desk (2026-11-01, bench/sessions/2026-11-01-step20.3-whisper-desk) the room's false passes (rustling,
typing) read 0.32-0.60, a whisper through the speaker 0.06-0.16, speech 0.02-0.05. Fed per sample, read per block.
Pure, host-tested (test_band_flatness).

Owns: the filters' coefficients and state, the energies since the last take().
Does not: decide what is flat enough (the Flatness inspection module), or know the block's level.
*/
class BandFlatness {
public:
    static constexpr size_t kBands = 8;

    // The bands for a sample rate; the top band must stay under the rate's half (24 kHz and up).
    void begin(uint32_t rateHz, float lowHz = 300.0f, float highHz = 10000.0f, float q = 2.5f) {
        const float ratio = powf(highHz / lowHz, 1.0f / static_cast<float>(kBands - 1));
        float f = lowHz;
        for (size_t i = 0; i < kBands; ++i) {
            const float w0 = 6.2831853f * f / static_cast<float>(rateHz);
            const float alpha = sinf(w0) / (2.0f * q);
            const float a0 = 1.0f + alpha;
            Band& b = _bands[i];
            b.b0 = alpha / a0;  // RBJ bandpass, 0 dB peak; b1 = 0, b2 = -b0
            b.a1 = -2.0f * cosf(w0) / a0;
            b.a2 = (1.0f - alpha) / a0;
            b.centre = f;
            b.z1 = b.z2 = 0.0f;
            b.energy = 0.0f;
            f *= ratio;
        }
        _count = 0;
    }

    // One sample, any scale (the ratio does not depend on it).
    void push(float x) {
        for (size_t i = 0; i < kBands; ++i) {
            Band& b = _bands[i];
            const float y = b.b0 * x + b.z1;
            b.z1 = -b.a1 * y + b.z2;
            b.z2 = -b.b0 * x - b.a2 * y;
            b.energy += y * y;
        }
        ++_count;
    }

    // The flatness of the samples since the last take(), and the energies cleared. False when none came.
    bool take(float& flatness) {
        if (_count == 0) {
            return false;
        }
        float logSum = 0.0f;
        float sum = 0.0f;
        for (size_t i = 0; i < kBands; ++i) {
            const float e = _bands[i].energy / _bands[i].centre + 1e-20f;
            logSum += logf(e);
            sum += e;
            _bands[i].energy = 0.0f;
        }
        _count = 0;
        const float mean = sum / static_cast<float>(kBands);
        flatness = expf(logSum / static_cast<float>(kBands)) / mean;
        return true;
    }

private:
    struct Band {
        float b0 = 0.0f;
        float a1 = 0.0f;
        float a2 = 0.0f;
        float centre = 1.0f;
        float z1 = 0.0f;
        float z2 = 0.0f;
        float energy = 0.0f;
    };
    Band _bands[kBands];
    size_t _count = 0;
};
