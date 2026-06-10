#pragma once

#include "DetectorDescriptor.h"
#include "DetectorReject.h"

namespace detection {

/*
RejectedCandidateSummary

Minimal canonical summary for the selected rejected detector candidate.
This intentionally stays small during Pass A and does not absorb the current
DetectionDiagnostics dump.
*/
struct RejectedCandidateSummary {
    DetectorRejectClass rejectClass = DetectorRejectClass::None;
    const char* detectorReason = "none";
    unsigned long startMs = 0;
    unsigned long peakMs = 0;
    unsigned long endMs = 0;
    unsigned long durationMs = 0;
    unsigned long requiredMinDurationMs = 0;
    float strength = 0.0f;
    float confidence = 0.0f;
};

/*
DetectorReport

Minimal canonical detector-stage report placeholder.
Existing runtime code still uses DetectionDiagnostics during migration.
*/
struct DetectorReport {
    DetectorId detectorId = DetectorId::Unknown;
    unsigned long reportStartMs = 0;
    unsigned long reportEndMs = 0;
    bool acceptedPresent = false;
    bool selectedRejectPresent = false;
    RejectedCandidateSummary selectedReject = {};
};

} // namespace detection
