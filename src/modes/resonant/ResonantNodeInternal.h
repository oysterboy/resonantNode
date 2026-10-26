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

// The Node's commands match on a token boundary (text_parse::startsWithWordIgnoreCase).
inline bool startsWithTokenIgnoreCase(const char* line, const char* token) {
    return text_parse::startsWithWordIgnoreCase(line, token);
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
