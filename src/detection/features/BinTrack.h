#pragma once

#include <stddef.h>
#include <stdint.h>

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
    void note(unsigned long nowMs, unsigned long peakBinHz, audio::FrequencyScore16 level) {
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
    bool at(unsigned long ms, uint32_t& binHz, uint16_t& level) const {
        if (!_started || ms > _newestMs || _newestMs - ms >= kMs) {
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

    unsigned long newestMs() const { return _newestMs; }

private:
    void advanceTo(unsigned long nowMs) {
        if (!_started) {
            _started = true;
            _newestMs = nowMs;
            const size_t slot = static_cast<size_t>(nowMs % kMs);
            _code[slot] = 0;
            _level[slot] = 0;
            return;
        }
        if (nowMs <= _newestMs) {
            return;
        }
        const unsigned long steps = nowMs - _newestMs >= kMs ? kMs : nowMs - _newestMs;
        for (unsigned long k = 1; k <= steps; ++k) {
            const size_t slot = static_cast<size_t>((_newestMs + k) % kMs);
            _code[slot] = 0;
            _level[slot] = 0;
        }
        _newestMs = nowMs;
    }

    uint8_t _code[kMs] = {};
    uint16_t _level[kMs] = {};
    unsigned long _newestMs = 0;
    bool _started = false;
};

}  // namespace detection
