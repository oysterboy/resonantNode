#pragma once

#include <stdint.h>

#if defined(ESP_PLATFORM)
#include <esp_timer.h>
#endif

/*
TimeBase

The one clock for audio, detection, Analyzer and behavior (issue #30,
docs/refactors/archive/micros-wrap.md).

- nowUs64(): esp_timer_get_time() + TIMEBASE_OFFSET_US. 64 bits, never wraps.
  The audio sample clock (AudioSourceI2S) stamps blocks with it.
- nowMs():   (unsigned long)(nowUs64() / 1000). Same formula as Arduino
  millis() (esp32-hal-misc.c), so with offset 0 it equals millis(). A sample
  time in ms (msFromUs64(sampleUs64)) and nowMs() are always on one axis;
  ms wraps at 2^32 ms = 49.7 days and is compared with app/TimingUtils.h.
- nowUs():   low 32 bits of nowUs64(), the micros() replacement. Only for
  durations (unsigned difference); it wraps every 71.6 min.

Never derive absolute ms from a 32-bit us value: (us32 / 1000) jumps back by
4,294,967 ms at every us wrap while nowMs() keeps counting. That was the
deafness 71.6 min after boot.

TIMEBASE_OFFSET_US (debug build flag, default 0) moves this clock forward so
the 32-bit us wrap (2^32 us) happens minutes after boot, e.g.
  -D TIMEBASE_OFFSET_US=4114967296ULL   -> us wrap 180 s after boot
  -D TIMEBASE_OFFSET_US=4294787296000ULL -> ms wrap 180 s after boot
Code in the audio/detection/Analyzer/behavior paths must read time only from
here, never from millis()/micros(), or the offset itself creates a mismatch.
*/

#ifndef TIMEBASE_OFFSET_US
#define TIMEBASE_OFFSET_US 0ULL
#endif

namespace timebase {

constexpr uint64_t kOffsetUs = static_cast<uint64_t>(TIMEBASE_OFFSET_US);

// --- pure conversions (host-testable) ---------------------------------------

inline uint32_t msFromUs64(uint64_t us) {
    return static_cast<uint32_t>(us / 1000ULL);
}

inline uint32_t lowUs32(uint64_t us) {
    return static_cast<uint32_t>(us);
}

// msFromUs64() for a monotonic per-sample stream without a 64-bit division per
// sample (Xtensa has no 64-bit divide): the division runs once per new
// millisecond, every other call is a subtract and a compare.
class MsFromUs64 {
public:
    uint32_t operator()(uint64_t us) {
        if (_valid && us >= _msStartUs && us - _msStartUs < 1000ULL) {
            return _ms;
        }
        const uint64_t ms = us / 1000ULL;
        _msStartUs = ms * 1000ULL;
        _ms = static_cast<uint32_t>(ms);
        _valid = true;
        return _ms;
    }

    void reset() { _valid = false; }

private:
    uint64_t _msStartUs = 0;
    uint32_t _ms = 0;
    bool _valid = false;
};

// --- the clock --------------------------------------------------------------

#if defined(ESP_PLATFORM)
inline uint64_t nowUs64() {
    return static_cast<uint64_t>(esp_timer_get_time()) + kOffsetUs;
}

inline unsigned long nowMs() {
    return static_cast<unsigned long>(nowUs64() / 1000ULL);
}

inline unsigned long nowUs() {
    return static_cast<unsigned long>(nowUs64());
}
#else
// Host build (PlatformIO env:native-replay, NODE-006): the clock is whatever
// the replay harness sets, so replayed samples and the detection they drive
// share one axis exactly as on the board. Nothing advances it on its own.
namespace host {
inline uint64_t& clockUs64() {
    static uint64_t value = 0;
    return value;
}
inline void setNowUs64(uint64_t us) {
    clockUs64() = us;
}
} // namespace host

inline uint64_t nowUs64() {
    return host::clockUs64() + kOffsetUs;
}

inline unsigned long nowMs() {
    return static_cast<unsigned long>(nowUs64() / 1000ULL);
}

inline unsigned long nowUs() {
    return static_cast<unsigned long>(nowUs64());
}
#endif

} // namespace timebase
