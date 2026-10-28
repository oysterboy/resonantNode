#pragma once

#include <stdint.h>

#include "../../detection/evaluation/OccurrenceVerdict.h"

/*
NodeCounters

The RB tallies the Node prints in RB summary and RB STATUS (heard.*):
cumulative since boot or the last rebase, reset by assignment
(`counters = NodeCounters{}`). Step 11i N1 (docs/refactors/node-split.md).
*/
struct NodeCounters {
    unsigned long pending = 0;           // every verdict popped
    unsigned long patternAccepted = 0;
    unsigned long validPatterns = 0;
    unsigned long chirpStarted = 0;
    unsigned long overflowPending = 0;
    uint64_t strengthSumScaled = 0;  // Strength16 * 100 per verdict; uint32 overflowed after ~1,300 (issue #31)
    uint64_t validStrengthSum = 0;   // valid verdicts only, whole Strength16 units: RB STATUS heard.* (10c drift)
    unsigned long durationSumMs = 0;

    void noteVerdict(const detection::OccurrenceVerdict& verdict) {
        ++pending;
        if (verdict.accepted) {
            ++patternAccepted;
        }
        if (verdict.valid) {
            ++validPatterns;
            validStrengthSum += static_cast<uint64_t>(verdict.primaryStrength + 0.5f);
        }
        if (verdict.primaryAudioOverflow) {
            ++overflowPending;
        }
        strengthSumScaled += static_cast<uint64_t>(verdict.primaryStrength * 100.0f);
        durationSumMs += verdict.primaryDurationMs;
    }
};
