#include "SelfTestReport.h"

#include <ctype.h>
#include <esp_system.h>
#include <stdlib.h>
#include <string.h>

#include "../app/BuildInfo.h"

namespace selftest {

namespace {

bool tokenEquals(const char* token, size_t length, const char* word) {
    const size_t wordLength = strlen(word);
    if (length != wordLength) {
        return false;
    }
    for (size_t i = 0; i < length; ++i) {
        if (toupper(static_cast<unsigned char>(token[i])) != toupper(static_cast<unsigned char>(word[i]))) {
            return false;
        }
    }
    return true;
}

// Value of "key=value" in token (case-insensitive key), or nullptr.
const char* valueOf(const char* token, size_t length, const char* key) {
    const size_t keyLength = strlen(key);
    if (length <= keyLength + 1 || token[keyLength] != '=') {
        return nullptr;
    }
    return tokenEquals(token, keyLength, key) ? token + keyLength + 1 : nullptr;
}

uint32_t clampU32(unsigned long value, uint32_t lo, uint32_t hi) {
    return value < lo ? lo : (value > hi ? hi : static_cast<uint32_t>(value));
}

} // namespace

const char* resultName(Result result) {
    switch (result) {
        case Result::Pass:
            return "PASS";
        case Result::Fail:
            return "FAIL";
        case Result::Skip:
            return "SKIP";
    }
    return "FAIL";
}

void Tally::add(Result result) {
    switch (result) {
        case Result::Pass:
            ++pass;
            break;
        case Result::Fail:
            ++fail;
            break;
        case Result::Skip:
            ++skip;
            break;
    }
}

void begin(Print& out, Tally& tally, const char* check, Result result) {
    tally.add(result);
    out.print("SELFTEST check=");
    out.print(check);
    out.print(" result=");
    out.print(resultName(result));
}

void field(Print& out, const char* key, const char* value) {
    out.print(' ');
    out.print(key);
    out.print('=');
    out.print(value != nullptr && *value != '\0' ? value : "-");
}

void field(Print& out, const char* key, long value) {
    out.print(' ');
    out.print(key);
    out.print('=');
    out.print(value);
}

void field(Print& out, const char* key, unsigned long value) {
    out.print(' ');
    out.print(key);
    out.print('=');
    out.print(value);
}

void field(Print& out, const char* key, float value, int digits) {
    out.print(' ');
    out.print(key);
    out.print('=');
    out.print(value, digits);
}

void end(Print& out) {
    out.println();
}

void skip(Print& out, Tally& tally, const char* check, const char* reason) {
    begin(out, tally, check, Result::Skip);
    field(out, "reason", reason);
    end(out);
}

void printBegin(Print& out, const char* role, const char* mode) {
    out.print("SELFTEST_BEGIN role=");
    out.print(role);
    out.print(" mode=");
    out.print(mode);
    out.print(" board=");
    out.println(BOARD_NAME);
}

void printBoard(Print& out, Tally& tally, const char* role) {
    uint8_t mac[6] = {};
    const bool macOk = esp_efuse_mac_get_default(mac) == ESP_OK;
    char macText[18];
    snprintf(macText, sizeof(macText), "%02x:%02x:%02x:%02x:%02x:%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    // "Oct 10 2026" -> "Oct_10_2026" so the value has no spaces.
    char date[16];
    strncpy(date, __DATE__, sizeof(date));
    date[sizeof(date) - 1] = '\0';
    for (char* c = date; *c != '\0'; ++c) {
        if (*c == ' ') {
            *c = '_';
        }
    }

    begin(out, tally, "board", macOk ? Result::Pass : Result::Fail);
    field(out, "mac", macOk ? macText : "unreadable");
    field(out, "board", BOARD_NAME);
    field(out, "role", role);
    field(out, "version", BUILD_VERSION);
    field(out, "git", BUILD_GIT_SHA);
    field(out, "built", date);
    field(out, "built_time", __TIME__);
    field(out, "uptime_ms", static_cast<unsigned long>(millis()));
    end(out);
}

void printSummary(Print& out, const Tally& tally, const char* role) {
    out.print("SELFTEST_SUMMARY result=");
    out.print(tally.fail == 0 ? "PASS" : "FAIL");
    out.print(" pass=");
    out.print(tally.pass);
    out.print(" fail=");
    out.print(tally.fail);
    out.print(" skip=");
    out.print(tally.skip);
    out.print(" role=");
    out.println(role);
}

Command parseCommand(const char* line) {
    Command command;
    if (line == nullptr) {
        return command;
    }
    while (*line == ' ') {
        ++line;
    }
    const char* cursor = line;
    bool first = true;
    bool sawSub = false;
    while (*cursor != '\0') {
        while (*cursor == ' ') {
            ++cursor;
        }
        if (*cursor == '\0') {
            break;
        }
        const char* token = cursor;
        while (*cursor != '\0' && *cursor != ' ') {
            ++cursor;
        }
        const size_t length = static_cast<size_t>(cursor - token);
        if (first) {
            if (!tokenEquals(token, length, "SELFTEST")) {
                return command;  // Kind::None
            }
            command.kind = Command::Kind::Full;
            first = false;
            continue;
        }
        const char* value = nullptr;
        if (!sawSub && memchr(token, '=', length) == nullptr) {
            sawSub = true;
            if (tokenEquals(token, length, "external")) {
                command.kind = Command::Kind::External;
            } else if (tokenEquals(token, length, "chirp")) {
                command.kind = Command::Kind::Chirp;
            } else if (tokenEquals(token, length, "help")) {
                command.kind = Command::Kind::Help;
            } else {
                command.kind = Command::Kind::Unknown;
            }
        } else if ((value = valueOf(token, length, "wait_ms")) != nullptr) {
            command.waitMs = clampU32(strtoul(value, nullptr, 10), 100, kMaxWaitMs);
        } else if ((value = valueOf(token, length, "n")) != nullptr) {
            command.chirps = static_cast<uint8_t>(clampU32(strtoul(value, nullptr, 10), 1, kMaxChirps));
        } else if ((value = valueOf(token, length, "gap_ms")) != nullptr) {
            command.gapMs = clampU32(strtoul(value, nullptr, 10), 300, 10000);
        } else if ((value = valueOf(token, length, "dur_ms")) != nullptr) {
            command.durMs = clampU32(strtoul(value, nullptr, 10), 20, 1000);
        }
    }
    return command;
}

void printHelp(Print& out, bool nodeChecks) {
    out.print("SELFTEST_HELP SELFTEST   board, mic, amp");
    out.println(nodeChecks ? ", quiet_boot, own_chirp_suppressed" : "");
    if (nodeChecks) {
        out.println("SELFTEST_HELP SELFTEST external wait_ms=N   board + external_chirp (another board chirps)");
    }
    out.println("SELFTEST_HELP SELFTEST chirp n=5 gap_ms=1500 dur_ms=100   emit only (helper for external_chirp)");
    out.println("SELFTEST_HELP format: docs/refactors/selftest.md");
}

} // namespace selftest
