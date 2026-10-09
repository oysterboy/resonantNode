#pragma once

#include <Arduino.h>

#include "BoardConfig.h"

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
}
