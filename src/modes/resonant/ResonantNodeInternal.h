#pragma once

#include <ctype.h>
#include <string.h>

#include "../../app/TextParse.h"
#include "../../behavior/BehaviorProfile.h"
#include "../../detection/DetectionProfile.h"

/*
Helpers the Node's files share (owner 2026-10-25 split by role). Internal to
src/modes/resonant/; nothing outside includes it.
*/
namespace node_internal {

// Case-insensitive prefix match that ends on a token boundary: the next
// character is the end or a space, unless the prefix itself ends in '='
// (key=value). A plain prefix match ran "RB rebase forced" and
// "RB rebasefoo" as rebases (issue #31 R2-19).
inline bool startsWithTokenIgnoreCase(const char* line, const char* token) {
    char last = '\0';
    while (*token != '\0') {
        if (*line == '\0') {
            return false;
        }
        if (toupper(static_cast<unsigned char>(*line)) != toupper(static_cast<unsigned char>(*token))) {
            return false;
        }
        last = *token;
        ++line;
        ++token;
    }

    return last == '=' || *line == '\0' || *line == ' ';
}

inline bool equalsIgnoreCase(const char* a, const char* b) {
    return text_parse::equalsIgnoreCase(a, b);
}

// Settle after a forced rebase before outputs come back (core and RB rebase force).
constexpr unsigned long kRbPostRebaseSettleMs = 500;

}  // namespace node_internal

// Defined in ResonantNodeApp.cpp, used by the profile switch and the profile command.
BehaviorGateConfig makeTonalPulseBehaviorProfile();
const char* detectionProfileKindName(detection::DetectionProfileKind kind);
void printProfileComposition(const detection::DetectionProfile& profile);
