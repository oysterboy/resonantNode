#pragma once

#include <Arduino.h>

#include "BoardConfig.h"
#include "TimeBase.h"

#ifndef BUILD_VERSION
#define BUILD_VERSION "unknown-version"
#endif

#ifndef BUILD_GIT_SHA
#define BUILD_GIT_SHA "unavailable"
#endif

inline void printBuildIdentity(Print& out, const char* role) {
    out.print("BUILD role=");
    out.print(role != nullptr ? role : "unknown");
    out.print(" git=");
    out.print(BUILD_GIT_SHA);
    out.print(" date=");
    out.print(__DATE__);
    out.print(" time=");
    out.print(__TIME__);
    out.print(" version=");
    out.print(BUILD_VERSION);
    out.print(" board=");
    out.println(BOARD_NAME);
    // Wrap-test builds only (TIMEBASE_OFFSET_US, app/TimeBase.h): a line of
    // its own, so the BUILD banner above stays what the bench tools parse.
    if (timebase::kOffsetUs != 0) {
        const uint64_t nowUs = timebase::nowUs64();
        const uint64_t nextUsWrap = (nowUs | 0xFFFFFFFFULL) + 1ULL;
        out.print("TIMEBASE offset_us=");
        out.print(static_cast<unsigned long long>(timebase::kOffsetUs));
        out.print(" now_ms=");
        out.print(timebase::nowMs());
        out.print(" next_us_wrap_in_ms=");
        out.println(static_cast<unsigned long long>((nextUsWrap - nowUs) / 1000ULL));
    }
}
