#pragma once

#include <stdint.h>

// Wrap-safe helpers for 32-bit absolute times (ms from timebase::nowMs() or
// AudioSamplePacket::timeMs; also usable for 32-bit us stamps). Ordering is
// decided by the signed difference, so it stays right across the 2^32 wrap
// as long as the two times are less than 2^31 apart (24.8 days in ms).
// Never compare two absolute times with a plain <, >, <=, >= (issue #30).
namespace timing {

inline bool elapsedSince(uint32_t now, uint32_t then, uint32_t durationMs) {
    return static_cast<uint32_t>(now - then) >= durationMs;
}

inline bool atOrAfter(uint32_t now, uint32_t deadline) {
    return static_cast<int32_t>(now - deadline) >= 0;
}

inline bool beforeDeadline(uint32_t now, uint32_t deadline) {
    return static_cast<int32_t>(now - deadline) < 0;
}

// a strictly later than b (the wrap-safe `a > b`).
inline bool after(uint32_t a, uint32_t b) {
    return static_cast<int32_t>(a - b) > 0;
}

// a at or before b (the wrap-safe `a <= b`).
inline bool atOrBefore(uint32_t a, uint32_t b) {
    return static_cast<int32_t>(a - b) <= 0;
}

// start <= t <= end, for a window that does not span more than 2^32 - 1.
inline bool inWindow(uint32_t t, uint32_t start, uint32_t end) {
    return static_cast<uint32_t>(t - start) <= static_cast<uint32_t>(end - start);
}

// Signed a - b (the wrap-safe `long(a) - long(b)`).
inline int32_t delta(uint32_t a, uint32_t b) {
    return static_cast<int32_t>(a - b);
}

// a - b when a is at or after b, else 0 (the wrap-safe `a >= b ? a - b : 0`).
inline uint32_t elapsedOrZero(uint32_t a, uint32_t b) {
    return atOrAfter(a, b) ? static_cast<uint32_t>(a - b) : 0U;
}

} // namespace timing
