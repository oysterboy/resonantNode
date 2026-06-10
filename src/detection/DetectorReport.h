#pragma once

#include "DetectorDescriptor.h"
#include "DetectorReject.h"

namespace detection {

/*
AcceptedOccurrenceSummary

Generic accepted-occurrence shell frozen into DetectorReport.
Only keep fields another detector could plausibly expose with the same meaning.
*/
struct AcceptedOccurrenceSummary {
    bool present = false;
    unsigned long startMs = 0;
    unsigned long peakMs = 0;
    unsigned long endMs = 0;
    unsigned long durationMs = 0;
    float strength = 0.0f;
    float confidence = 0.0f;
};

/*
SelectedRejectSummary

Generic selected-reject shell frozen into DetectorReport.
`detectorReason` stays string-based for now to avoid disruptive reason-enum
plumbing during the scalar-first migration.
*/
struct SelectedRejectSummary {
    bool present = false;
    DetectorRejectClass rejectClass = DetectorRejectClass::None;
    const char* detectorReason = "none";
    unsigned long startMs = 0;
    unsigned long peakMs = 0;
    unsigned long endMs = 0;
    unsigned long durationMs = 0;
    float strength = 0.0f;
    float confidence = 0.0f;
};

/*
ThresholdSummary

Generic shared thresholds that have the same meaning across detector families.
*/
struct ThresholdSummary {
    unsigned long minDurationMs = 0;
    unsigned long maxDurationMs = 0;
};

/*
AggregateCountSummary

Generic shared aggregate counts.
*/
struct AggregateCountSummary {
    unsigned long acceptedCount = 0;
    unsigned long rejectedCount = 0;
};

/*
ScalarAcceptedDetail

Carrier-agnostic scalar accepted-event detail. Do not rename this back to an
AMP-specific public report shape.
*/
struct ScalarAcceptedDetail {
    bool present = false;
    float value = 0.0f;
    float baseline = 0.0f;
    float lift = 0.0f;
    float normalized = 0.0f;
};

/*
ScalarSelectedRejectDetail

Scalar-specific selected-reject detail. Fields may remain partially populated
while the scalar detector still owns only a subset of the scalar report truth.
*/
struct ScalarSelectedRejectDetail {
    bool present = false;
    float value = 0.0f;
    float baseline = 0.0f;
    float lift = 0.0f;
    float normalized = 0.0f;
    bool opened = false;
    bool crossedOnset = false;
    bool crossedRelease = false;
};

/*
ScalarThresholdDetail

Scalar-specific thresholds that should not be forced into the generic shell.
*/
struct ScalarThresholdDetail {
    float onsetThreshold = 0.0f;
    float releaseThreshold = 0.0f;
    float minStrength = 0.0f;
};

/*
ScalarAggregateDetail

Scalar-specific aggregate counts and bounded report facts.
*/
struct ScalarAggregateDetail {
    unsigned long tooShortCount = 0;
    unsigned long tooLongCount = 0;
    unsigned long strengthTooLowCount = 0;
    float maxRejectedLift = 0.0f;
    float bestRejectedValue = 0.0f;
};

/*
ScalarInspectEvidence

Bounded scalar detector evidence needed for Analyzer / SEQ_INSPECT while the
legacy scalar output still expects gate/lifecycle truth.
*/
struct ScalarInspectEvidence {
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
    float peakStrength = 0.0f;
};

/*
ScalarDetectorReportDetail

Sectioned scalar detector-specific report detail.
*/
struct ScalarDetectorReportDetail {
    ScalarAcceptedDetail accepted = {};
    ScalarSelectedRejectDetail selectedReject = {};
    ScalarThresholdDetail thresholds = {};
    ScalarAggregateDetail aggregates = {};
    ScalarInspectEvidence inspect = {};
};

/*
FrequencyMatchDetectorReportDetail

Placeholder parity block. Frequency still uses DetectionDiagnostics until a
later pass migrates it into the same report snapshot model.
*/
struct FrequencyMatchDetectorReportDetail {
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
    AcceptedOccurrenceSummary accepted = {};
    SelectedRejectSummary selectedReject = {};
    ThresholdSummary thresholds = {};
    AggregateCountSummary aggregates = {};
    ScalarDetectorReportDetail scalar = {};
    FrequencyMatchDetectorReportDetail frequency = {};
};

} // namespace detection
