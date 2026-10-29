#pragma once

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "../../app/TextParse.h"

/*
The Analyzer console's pure parsing (step 11j item 6): which handler a line goes to, and the token checks the
RAW and SEQ handlers share. No Arduino and no AnalyzerApp, so the host test (test/test_analyzer_commands) reaches
it; the handlers themselves are AnalyzerApp members (AnalyzerCommands.cpp).
*/
namespace analyzer_commands {

enum class Group {
    Help,      // HELP, the whole line
    SelfTest,  // SELFTEST ...
    Ota,       // OTA url=...
    Param,     // PARAM ...
    Raw,       // RAW trigger ...
    Seq,       // SEQ ...
    Emit,      // EMIT <command>, forwarded to the Emitter
    Unknown,
};

// The handler group of a console line: plain case-insensitive prefixes, in the order the console checks them
// (HELP is the whole line). A Seq or Emit line whose sub-command is unknown ends as analyzer_unknown_cmd too.
inline Group groupOf(const char* line) {
    if (text_parse::equalsIgnoreCase(line, "HELP")) {
        return Group::Help;
    }
    if (text_parse::startsWithIgnoreCase(line, "SELFTEST")) {
        return Group::SelfTest;
    }
    if (text_parse::startsWithIgnoreCase(line, "OTA")) {
        return Group::Ota;
    }
    if (text_parse::startsWithIgnoreCase(line, "PARAM")) {
        return Group::Param;
    }
    if (text_parse::startsWithIgnoreCase(line, "RAW")) {
        return Group::Raw;
    }
    if (text_parse::startsWithIgnoreCase(line, "SEQ")) {
        return Group::Seq;
    }
    if (text_parse::startsWithIgnoreCase(line, "EMIT ")) {
        return Group::Emit;
    }
    return Group::Unknown;
}

// on / 1 / true and off / 0 / false, any case. A missing or other token sets valid false (and returns true).
inline bool parseOnOff(const char* token, bool* valid) {
    if (valid != nullptr) {
        *valid = true;
    }
    if (token == nullptr || *token == '\0') {
        if (valid != nullptr) {
            *valid = false;
        }
        return true;
    }
    if (text_parse::equalsIgnoreCase(token, "on") || text_parse::equalsIgnoreCase(token, "1") ||
        text_parse::equalsIgnoreCase(token, "true")) {
        return true;
    }
    if (text_parse::equalsIgnoreCase(token, "off") || text_parse::equalsIgnoreCase(token, "0") ||
        text_parse::equalsIgnoreCase(token, "false")) {
        return false;
    }
    if (valid != nullptr) {
        *valid = false;
    }
    return true;
}

// band=LOW-HIGH or band=off (0-0). False for a missing low end, a high end below it or above maxHz; the outputs
// are then left unchanged.
inline bool parseBandSpec(const char* spec, unsigned long maxHz, unsigned long& lowHz, unsigned long& highHz) {
    if (text_parse::equalsIgnoreCase(spec, "off")) {
        lowHz = 0;
        highHz = 0;
        return true;
    }
    char* dash = nullptr;
    const unsigned long lo = strtoul(spec, &dash, 10);
    const unsigned long hi = (dash != nullptr && *dash == '-') ? strtoul(dash + 1, nullptr, 10) : 0UL;
    if (lo == 0 || hi < lo || hi > maxHz) {
        return false;
    }
    lowHz = lo;
    highHz = hi;
    return true;
}

// hop=F1,F2,...: digits and commas only, and it fits a buffer of `capacity` bytes with its terminator.
inline bool validHopList(const char* list, size_t capacity) {
    const size_t len = strlen(list);
    bool ok = len > 0 && len < capacity;
    for (size_t i = 0; ok && i < len; ++i) {
        ok = (list[i] >= '0' && list[i] <= '9') || list[i] == ',';
    }
    return ok;
}

// att=DB1,DB2,...: 1..maxEntries entries of 0..maxDb, none empty (the Emitter's own rule; review 14.1).
inline bool attListInRange(const char* list, size_t maxEntries, unsigned long maxDb) {
    size_t entries = 0;
    unsigned long value = 0;
    size_t digits = 0;
    for (const char* p = list;; ++p) {
        if (*p >= '0' && *p <= '9') {
            value = value * 10UL + static_cast<unsigned long>(*p - '0');
            if (++digits > 3 || value > maxDb) {
                return false;
            }
            continue;
        }
        if (digits == 0 || ++entries > maxEntries) {
            return false;
        }
        if (*p == '\0') {
            return true;
        }
        if (*p != ',') {
            return false;
        }
        value = 0;
        digits = 0;
    }
}

// Every entry of a hop list (validHopList's shape) is a tone in 1..maxHz, none empty (review 12.0 L14).
inline bool hopListInRange(const char* list, unsigned long maxHz) {
    unsigned long value = 0;
    size_t digits = 0;
    for (const char* p = list;; ++p) {
        if (*p >= '0' && *p <= '9') {
            value = value * 10UL + static_cast<unsigned long>(*p - '0');
            ++digits;
            if (value > maxHz) {
                return false;
            }
            continue;
        }
        if (digits == 0 || value == 0) {
            return false;
        }
        if (*p == '\0') {
            return true;
        }
        if (*p != ',') {
            return false;
        }
        value = 0;
        digits = 0;
    }
}

}  // namespace analyzer_commands
