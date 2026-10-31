#pragma once

#include <stdint.h>

#include "../evaluation/OccurrenceVerdict.h"
#include "../occurrences/Occurrence.h"

namespace detection {

/*
makeClapVerdict (ECHO-001, step 20.2): the echo room's verdict on one occurrence of the scalar detector, run on the
envelope alone (no band, no inspectors). Valid when the detector accepted the occurrence and its onset rose within
maxAttackMs (AttackSlope); the verdict carries the occurrence's timing and strength for Behavior, as makeVerdict
does for the Node, and no band facts (primaryLowestBinHz 0). Pure, host-tested (test_echo_detection).
*/
inline OccurrenceVerdict makeClapVerdict(const Occurrence& occurrence, long attackMs, uint32_t maxAttackMs) {
    OccurrenceVerdict v;
    v.type = VerdictType::SinglePulse;
    v.reasonCode = VerdictReasonCode::FromOccurrence;
    v.occurrenceCount = 1;
    v.occurrenceId = occurrence.occurrenceId;
    v.primaryStartMs = occurrence.startMs;
    v.primaryPeakMs = occurrence.peakMs;
    v.primaryHeardAtMs = occurrence.releaseMs != 0 ? occurrence.releaseMs : occurrence.startMs;
    v.primaryAcceptedMs = v.primaryHeardAtMs;
    v.primaryDurationMs = occurrence.durationMs;
    v.primaryStrength = occurrence.strength;
    v.proposalMatched = occurrence.present && occurrence.valid;
    v.supportMatched = attackMs >= 0 && static_cast<uint32_t>(attackMs) <= maxAttackMs;
    v.accepted = v.proposalMatched && v.supportMatched;
    v.valid = v.accepted;
    v.confidence = v.valid ? 1.0f : 0.0f;
    if (!v.valid) {
        v.reasonCode = VerdictReasonCode::SupportRequirementFailed;
        v.rejectReason = VerdictRejectReason::SupportTooLow;
    }
    return v;
}

}  // namespace detection
