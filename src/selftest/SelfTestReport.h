#pragma once

#include <Arduino.h>
#include <stdint.h>

/*
SelfTestReport

Output format and command parsing for the SELFTEST serial command
(NODE-015, docs/refactors/selftest.md). Shared by the Node, Analyzer and
Emitter builds; knows nothing about detection, behavior or audio.

One line per check, then one summary line:
  SELFTEST check=<name> result=PASS|FAIL|SKIP key=value ...
  SELFTEST_SUMMARY result=PASS|FAIL pass=N fail=N skip=N role=<role>
Values never contain spaces; a SKIP or FAIL carries reason=<word>.
*/
namespace selftest {

enum class Result : uint8_t {
    Pass,
    Fail,
    Skip,
};

const char* resultName(Result result);

struct Tally {
    uint16_t pass = 0;
    uint16_t fail = 0;
    uint16_t skip = 0;

    void add(Result result);
};

// Line building: begin() prints "SELFTEST check=<name> result=<R>" and counts
// the result; field() appends " key=value"; end() ends the line.
void begin(Print& out, Tally& tally, const char* check, Result result);
void field(Print& out, const char* key, const char* value);
void field(Print& out, const char* key, long value);
void field(Print& out, const char* key, unsigned long value);
void field(Print& out, const char* key, float value, int digits = 1);
void end(Print& out);

// One-call SKIP line: SELFTEST check=<name> result=SKIP reason=<reason>.
void skip(Print& out, Tally& tally, const char* check, const char* reason);

void printBegin(Print& out, const char* role, const char* mode);
// check=board: MAC (chip base MAC, as esptool read_mac prints it), board,
// role, version, git hash, build date and uptime. Always PASS: it proves the
// console answers and names the firmware.
void printBoard(Print& out, Tally& tally, const char* role);
void printSummary(Print& out, const Tally& tally, const char* role);

struct Command {
    enum class Kind : uint8_t {
        None,      // not a SELFTEST line
        Full,      // SELFTEST
        External,  // SELFTEST external [wait_ms=N]
        Chirp,     // SELFTEST chirp [n=N] [gap_ms=N] [dur_ms=N]
        Help,      // SELFTEST help
        Unknown,   // SELFTEST <something else>
    };

    Kind kind = Kind::None;
    uint32_t waitMs = 10000;
    uint8_t chirps = 5;
    uint32_t gapMs = 1500;
    uint32_t durMs = 100;
};

Command parseCommand(const char* line);
void printHelp(Print& out, bool nodeChecks);

constexpr uint8_t kMaxChirps = 20;
constexpr uint32_t kMaxWaitMs = 120000;

} // namespace selftest
