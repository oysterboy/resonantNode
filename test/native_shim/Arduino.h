#pragma once

// ESP32 placement attribute (code in IRAM); no meaning on the host.
#ifndef IRAM_ATTR
#define IRAM_ATTR
#endif

/*
Arduino.h stand-in for the host build (PlatformIO env:native-replay, NODE-006).

The detection, audio and behavior sources include <Arduino.h> and use a
handful of its names: PI, delay(), random(), and Serial.print for the
report printers. This header supplies exactly those on the host, nothing
else, so a new Arduino dependency in a detection file fails the host build
instead of silently passing. Time comes from app/TimeBase.h (its host
branch), never from millis()/micros(), the same rule as on the board.
*/

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef PI
#define PI 3.1415926535897932384626433832795
#endif
#ifndef TWO_PI
#define TWO_PI 6.283185307179586476925286766559
#endif

inline void delay(unsigned long) {}
inline void yield() {}

// millis()/micros() exist so legacy includes compile; they do not advance.
// Board code reads timebase::nowMs() instead (issue #30).
inline unsigned long millis() {
    return 0;
}
inline unsigned long micros() {
    return 0;
}

inline long random(long maxValue) {
    return maxValue <= 0 ? 0 : static_cast<long>(rand() % maxValue);
}
inline long random(long minValue, long maxValue) {
    return maxValue <= minValue ? minValue : minValue + random(maxValue - minValue);
}
inline void randomSeed(unsigned long seed) {
    srand(static_cast<unsigned int>(seed));
}

template <typename T>
inline T constrain(T value, T low, T high) {
    return value < low ? low : (value > high ? high : value);
}

// Arduino's Print, writing to stdout. Only the overloads the repository uses.
// HostSerial (Serial) is a Print, as on the board; lib/ParamRegistry takes a
// Print& for LIST/DUMP.
class Print {
public:
    size_t print(const char* text) { return text != nullptr ? static_cast<size_t>(fputs(text, stdout)) : 0; }
    size_t print(char c) {
        fputc(c, stdout);
        return 1;
    }
    size_t print(int v) { return static_cast<size_t>(printf("%d", v)); }
    size_t print(unsigned int v) { return static_cast<size_t>(printf("%u", v)); }
    size_t print(long v) { return static_cast<size_t>(printf("%ld", v)); }
    size_t print(unsigned long v) { return static_cast<size_t>(printf("%lu", v)); }
    size_t print(long long v) { return static_cast<size_t>(printf("%lld", v)); }
    size_t print(unsigned long long v) { return static_cast<size_t>(printf("%llu", v)); }
    size_t print(double v, int digits = 2) { return static_cast<size_t>(printf("%.*f", digits, v)); }
    size_t print(float v, int digits = 2) { return print(static_cast<double>(v), digits); }
    template <typename T>
    size_t println(T v) {
        const size_t n = print(v);
        fputc('\n', stdout);
        return n + 1;
    }
    template <typename T>
    size_t println(T v, int digits) {
        const size_t n = print(v, digits);
        fputc('\n', stdout);
        return n + 1;
    }
    size_t println() {
        fputc('\n', stdout);
        return 1;
    }
};

class HostSerial : public Print {
public:
    void begin(unsigned long) {}
    void flush() { fflush(stdout); }
};

inline HostSerial Serial;
