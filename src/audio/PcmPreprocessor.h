#pragma once

#include <stdint.h>

#include "AudioPcm.h"
#include "../app/RuntimeDefaults.h"

namespace audio {

// Source-side PCM preprocessing, one instance per sample stream. Shared by
// AudioSourceI2S and the SELFTEST floor measurement so both see the same
// transform.
//
//   None             pass-through
//   FirstDifference  (x[n] - x[n-1]) / 2, gain |sin(pi f/fs)|: 0.59 at 3200 Hz,
//                    0.20 at 1000 Hz, 1.0 near Nyquist
//   DcBlocker        one-pole high-pass y[n] = x[n] - x[n-1] + a*y[n-1],
//                    a = 1 - 2 pi fc / fs, flat above the corner; the output
//                    is scaled by kDcBlockerOutputScale so existing amplitude
//                    thresholds (tuned on First Difference at 3200 Hz) keep
//                    their meaning at first
class PcmPreprocessor {
public:
    explicit PcmPreprocessor(runtime::PcmPreprocessMode mode = runtime::kPcmPreprocessMode,
                             uint32_t sampleRateHz = 16000U)
        : _mode(mode),
          _dcPole(1.0f - 6.2831853f * runtime::kDcBlockerCornerHz / static_cast<float>(sampleRateHz)) {}

    void reset() {
        _previous = 0;
        _hasPrevious = false;
        _dcOut = 0.0f;
    }

    PcmSample process(PcmSample current) {
        if (_mode == runtime::PcmPreprocessMode::None) {
            return current;
        }
        if (!_hasPrevious) {
            _previous = current;
            _hasPrevious = true;
            _dcOut = 0.0f;
            return 0;
        }
        if (_mode == runtime::PcmPreprocessMode::DcBlocker) {
            const float out = static_cast<float>(current - _previous) + _dcPole * _dcOut;
            _previous = current;
            _dcOut = out;
            return clampToCanonicalPcm(static_cast<PcmIntermediate>(out * runtime::kDcBlockerOutputScale));
        }
        const PcmIntermediate diff =
            static_cast<PcmIntermediate>(current) - static_cast<PcmIntermediate>(_previous);
        _previous = current;
        return clampToCanonicalPcm(diff / 2);
    }

private:
    runtime::PcmPreprocessMode _mode;
    float _dcPole;
    PcmSample _previous = 0;
    bool _hasPrevious = false;
    float _dcOut = 0.0f;
};

}  // namespace audio
