#pragma once

#include <stdint.h>

#include "AudioPcm.h"

/*
EnvelopeFollower (ECHO-001 / ECHO-002)

The amplitude envelope the echo modes detect on: |sample| through a one-pole smoother, as a shift (2^shift samples:
~1.3 ms at 48 kHz with shift 6, the same at 24 kHz with shift 5). Samples are canonical PCM (24-bit, DC already
removed by the source); strength() is the envelope in Strength16, the scale the scalar detector's thresholds use.
Pure; the echo shell runs it per sample, the host echo replay on WAV files.

Owns: the envelope value. Does not: detect or decide anything.
*/
class EnvelopeFollower {
public:
    void setShift(int shift) { _shift = shift; }
    // The shift giving ~1.3 ms of smoothing at a rate (64 samples at 48 kHz).
    static int shiftForRate(uint32_t sampleRateHz) {
        int shift = 0;
        while ((static_cast<uint32_t>(1) << (shift + 1)) * 1000U <= sampleRateHz * 2U && shift < 12) {
            ++shift;
        }
        return shift;
    }
    void reset() { _envelope = 0; }

    int32_t push(int32_t sample) {
        const int32_t magnitude = sample < 0 ? -sample : sample;
        _envelope += (magnitude - _envelope) >> _shift;
        return _envelope;
    }

    int32_t value() const { return _envelope; }
    float strength() const {
        return static_cast<float>(audio::pcmMagnitudeToStrength(static_cast<audio::PcmSample>(_envelope)));
    }

private:
    int _shift = 6;
    int32_t _envelope = 0;
};
