#pragma once

#include <ctype.h>
#include <string.h>

#include "../../app/TextParse.h"
#include "../../behavior/BehaviorProfile.h"
#include "../../detection/DetectionProfile.h"

/*
Helpers the Node's files share (owner 2026-10-25 split by role). Internal to
src/modes/resonant/; nothing outside includes it. The text helpers are app/TextParse.h's
(text_parse::startsWithWordIgnoreCase: the Node's verbs end on a token boundary).
*/

// Defined in ResonantNodeApp.cpp, used by the profile switch and the profile command.
BehaviorGateConfig makeTonalPulseBehaviorProfile();
const char* detectionProfileKindName(detection::DetectionProfileKind kind);
void printProfileComposition(const detection::DetectionProfile& profile);
