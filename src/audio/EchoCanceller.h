#pragma once

#include <math.h>
#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <new>

/*
EchoCanceller (ECHO-002, 2026-11-04)

The node's own output taken out of its mic: a normalized LMS filter learns the path from the speaker to the mic
(the samples it played, already aligned to the mic by the bulk delay) and subtracts its estimate, so what remains is
the room, not the node. Whispernet then need not go deaf while it plays (at gain 64 it heard itself louder than the
whisper), and a phrase need not end where its replay starts.

Per sample: estimate = w . ref[n .. n-L+1], err = mic - estimate; while the reference is playing and the residual is
not far above the estimate's level (another voice in the room: "double talk", the filter would learn it), w moves by
mu * err * ref / (|ref|^2 + delta). The residual is clipped at 4x its running RMS for the update, so a sudden near
voice cannot throw the filter. ERLE (how much quieter the residual is than the mic, in dB) is smoothed for the
status. Pure, host-tested (test_echo_canceller).

Owns: the taps, the reference history, the running powers.
Does not: align the reference (the caller passes ref already delayed to the mic), or decide what to do with it.
*/
class EchoCanceller {
public:
    // Allocates 2 x taps floats of history and taps weights. False when out of heap.
    bool begin(size_t taps) {
        _taps = taps;
        _w.reset(new (std::nothrow) float[taps]);
        _x.reset(new (std::nothrow) float[2 * taps]);  // doubled: the window stays contiguous
        if (!_w || !_x || taps == 0) {
            _taps = 0;
            return false;
        }
        reset();
        return true;
    }
    void reset() {
        for (size_t i = 0; i < _taps; ++i) {
            _w[i] = 0.0f;
            _x[i] = _x[i + _taps] = 0.0f;
        }
        _pos = 0;
        _refPower = 0.0f;
        _micPow = _errPow = _estPow = 0.0f;
        _errRms = 0.0f;
        _erle = 0.0f;
        _adapted = 0;
    }
    void setStep(float mu) { _mu = mu; }
    size_t taps() const { return _taps; }

    // One sample: ref (what was played, aligned to this mic sample), mic; returns the mic without the node's echo.
    float process(float ref, float mic) {
        if (_taps == 0) {
            return mic;
        }
        // Newest first in the window _x[_pos .. _pos + taps).
        const float leaving = _x[_pos + _taps - 1];
        _pos = _pos == 0 ? _taps - 1 : _pos - 1;
        _x[_pos] = ref;
        _x[_pos + _taps] = ref;
        _refPower += ref * ref - leaving * leaving;
        if (_refPower < 0.0f) {
            _refPower = 0.0f;  // rounding
        }
        const float* x = &_x[_pos];
        float est = 0.0f;
        for (size_t i = 0; i < _taps; ++i) {
            est += _w[i] * x[i];
        }
        const float err = mic - est;
        // Running powers (~20 ms at 24 kHz) for ERLE, double talk and the clip.
        _micPow += (mic * mic - _micPow) * kSmooth;
        _errPow += (err * err - _errPow) * kSmooth;
        _estPow += (est * est - _estPow) * kSmooth;
        _errRms = sqrtf(_errPow);
        const bool playing = _refPower > _refFloor * static_cast<float>(_taps);
        const bool doubleTalk = _errPow > 4.0f * _estPow && _estPow > 0.0f && _adapted > kWarmup;
        if (playing && !doubleTalk) {
            float e = err;
            const float lim = 4.0f * _errRms + 1e-9f;
            e = e > lim ? lim : (e < -lim ? -lim : e);
            // Once it cancels well it learns slower (mu / 4 from 20 dB on): a voice in the room during the replay then
            // bends the filter less (the host test: 9.5 dB of voice-to-damage at a fixed mu, too little).
            const float slow = _erle <= 10.0f ? 1.0f : (_erle >= 20.0f ? 0.25f : 1.0f - 0.075f * (_erle - 10.0f));
            const float g = _mu * slow * e / (_refPower + kDelta);
            for (size_t i = 0; i < _taps; ++i) {
                _w[i] += g * x[i];
            }
            ++_adapted;
        }
        if (playing) {
            _erle += (10.0f * log10f((_micPow + 1e-12f) / (_errPow + 1e-12f)) - _erle) * kSmoothSlow;
        }
        return err;
    }

    // The smoothed echo return loss enhancement while playing, dB.
    float erleDb() const { return _erle; }
    // A reference quieter than this per sample counts as not playing (no learning on silence).
    void setRefFloor(float perSamplePower) { _refFloor = perSamplePower; }

private:
    static constexpr float kSmooth = 1.0f / 480.0f;
    static constexpr float kSmoothSlow = 1.0f / 24000.0f;
    static constexpr float kDelta = 1e-6f;
    static constexpr uint32_t kWarmup = 24000;  // ~1 s of learning before double talk may freeze it
    std::unique_ptr<float[]> _w;
    std::unique_ptr<float[]> _x;
    size_t _taps = 0;
    size_t _pos = 0;
    float _mu = 0.2f;
    float _refPower = 0.0f;
    float _refFloor = 1e-8f;
    float _micPow = 0.0f;
    float _errPow = 0.0f;
    float _estPow = 0.0f;
    float _errRms = 0.0f;
    float _erle = 0.0f;
    uint32_t _adapted = 0;
};
