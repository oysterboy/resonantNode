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
    unsigned long requiredMaxDurationMs = 0;
    float strength = 0.0f;
    float confidence = 0.0f;
};

/*
AcceptedOccurrenceSummary

Compact accepted-occurrence facts owned by DetectorReport during the migration
away from DetectionDiagnostics.
*/
struct AcceptedOccurrenceSummary {
    unsigned long startMs = 0;
    unsigned long peakMs = 0;
    unsigned long endMs = 0;
    unsigned long durationMs = 0;
    float strength = 0.0f;
    float score = 0.0f;
    float contrast = 0.0f;
    float confidence = 0.0f;
};

/*
ScalarDetectorReportDetail

Minimal scalar-transient detector detail for the first active DetectorReport
path. Frequency-specific detail remains deferred.
*/
struct ScalarDetectorReportDetail {
    const char* rejectReason = "none";
    const char* noEmitReason = "none";
    const char* gateReason = "none";
    bool opened = false;
    bool released = false;
    bool validRelease = false;
    bool emitAllowed = false;
    unsigned long openMs = 0;
    unsigned long peakMs = 0;
    unsigned long releaseMs = 0;
    unsigned long durationMs = 0;
    unsigned long minDurationMs = 0;
    unsigned long maxDurationMs = 0;
    float peakStrength = 0.0f;
};

/*
DetectorReport

Minimal canonical detector-stage report.
The scalar-transient path is now populated during migration; frequency still
uses DetectionDiagnostics until later passes.
*/
struct DetectorReport {
    DetectorId detectorId = DetectorId::Unknown;
    unsigned long reportStartMs = 0;
    unsigned long reportEndMs = 0;
    bool acceptedPresent = false;
    AcceptedOccurrenceSummary acceptedOccurrence = {};
    bool selectedRejectPresent = false;
    RejectedCandidateSummary selectedReject = {};
    ScalarDetectorReportDetail scalarTransient = {};
};

} // namespace detection
