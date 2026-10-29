#pragma once

#include <stddef.h>
#include <stdint.h>

#include "../../app/TimingUtils.h"
#include "../../audio/AudioPcm.h"

namespace detection {

/*
BinTrack (DET-015, step 11b; decisions/2026-10-26-det015-bin-track-inspector.md)

The strongest band bin per millisecond, with its level, for the ShapeInspector:
which bins an occurrence visited and in what order. FreqBandStream knows the
strongest bin on every update (band mode); DetectionRuntime notes it here per
frame. A ring of kMs one-ms entries (1.5 KB), enough for the longest occurrence
(max duration 300 ms) plus a margin; when time moves on, the skipped entries are
cleared, so an entry is always the newest kMs ms.

An entry holds the bin with the largest target value in that ms, as a code
(Hz / kHzPerCode, 0 = none): the band's bins are 250 Hz apart, so the code keeps
them distinct in a byte. Outside band mode the stream reports no bin and the
track stays empty, which the inspector reads as "no shape".

Times are 32-bit ms and compared wrap-safe (TimingUtils.h): with plain
comparisons the track stopped at the 2^32 ms wrap (49.7 days up) and the shape
rule went silent (critique D1.1, reproduced on the board 2026-10-26). 2^32 is a
multiple of kMs, so the slots stay continuous across the wrap.
Owns: the ring of the strongest bin per ms.
Does not: measure the bins (FreqBandStream) or judge a shape (ShapeInspector).
*/
class BinTrack {
public:
    static constexpr size_t kMs = 512;
    static constexpr uint32_t kHzPerCode = 50;

    void reset() {
        for (size_t i = 0; i < kMs; ++i) {
            _code[i] = 0;
            _level[i] = 0;
        }
        _started = false;
        _newestMs = 0;
    }

    // One frame: the strongest bin (0 when none) and the target value at nowMs.
    void note(uint32_t nowMs, unsigned long peakBinHz, audio::FrequencyScore16 level) {
        advanceTo(nowMs);
        if (peakBinHz == 0) {
            return;
        }
        const size_t slot = static_cast<size_t>(nowMs % kMs);
        if (_code[slot] == 0 || static_cast<uint16_t>(level) > _level[slot]) {
            _code[slot] = static_cast<uint8_t>(peakBinHz / kHzPerCode);
            _level[slot] = static_cast<uint16_t>(level);
        }
    }

    // The entry for ms: false for a ms with no bin, in the future, or older than the ring.
    bool at(uint32_t ms, uint32_t& binHz, uint16_t& level) const {
        if (!_started || timing::after(ms, _newestMs) || _newestMs - ms >= kMs) {
            return false;
        }
        const size_t slot = static_cast<size_t>(ms % kMs);
        if (_code[slot] == 0) {
            return false;
        }
        binHz = static_cast<uint32_t>(_code[slot]) * kHzPerCode;
        level = _level[slot];
        return true;
    }

    uint32_t newestMs() const { return _newestMs; }

    // The band's bin count, for BinShape's wrap rule (a step from the top bin to the bottom one; review 12.0 L3).
    // 0: not known, the rule falls back to "a drop of more than 3 bins" (right for the shipped 5-bin band).
    void setBandBins(uint32_t bins) { _bandBins = bins; }
    uint32_t bandBins() const { return _bandBins; }

private:
    void advanceTo(uint32_t nowMs) {
        if (!_started) {
            _started = true;
            _newestMs = nowMs;
            const size_t slot = static_cast<size_t>(nowMs % kMs);
            _code[slot] = 0;
            _level[slot] = 0;
            return;
        }
        if (!timing::after(nowMs, _newestMs)) {
            return;
        }
        const uint32_t ahead = nowMs - _newestMs;
        const uint32_t steps = ahead >= kMs ? static_cast<uint32_t>(kMs) : ahead;
        for (uint32_t k = 1; k <= steps; ++k) {
            const size_t slot = static_cast<size_t>(static_cast<uint32_t>(_newestMs + k) % kMs);
            _code[slot] = 0;
            _level[slot] = 0;
        }
        _newestMs = nowMs;
    }

    uint8_t _code[kMs] = {};
    uint16_t _level[kMs] = {};
    uint32_t _newestMs = 0;
    uint32_t _bandBins = 0;
    bool _started = false;
};

}  // namespace detection
