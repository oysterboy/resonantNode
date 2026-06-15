#pragma once

#include <stdint.h>

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

} // namespace timing
