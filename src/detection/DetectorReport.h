#pragma once

#include "DetectionTypes.h"
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
`detectorReason` stays string-based until detector reason enums are unified.
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
canonical report keeps scalar gate/lifecycle truth detector-owned.
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
FrequencyAcceptedDetail

Frequency accepted-event detail that does not belong in the generic accepted
shell.
*/
struct FrequencyAcceptedDetail {
    float score = 0.0f;
    float contrast = 0.0f;
};

/*
FrequencySelectedRejectDetail

Frequency selected-reject detail that does not belong in the generic selected
reject shell.
*/
struct FrequencySelectedRejectDetail {
    float score = 0.0f;
    float contrast = 0.0f;
};

/*
FrequencyThresholdDetail

Frequency thresholds that should remain detector-specific.
*/
struct FrequencyThresholdDetail {
    float scoreThreshold = 0.0f;
    float contrastThreshold = 0.0f;
};

/*
FrequencyAggregateDetail

Bounded frequency counters suitable for detector-owned report snapshots.
*/
struct FrequencyAggregateDetail {
    unsigned long scoreOkCount = 0;
    unsigned long contrastOkCount = 0;
    unsigned long bothOkCount = 0;
    unsigned long matchCount = 0;
};

/*
FrequencyInspectEvidence

Bounded frequency lifecycle/gate facts needed by Analyzer and detector-owned
reporting.
*/
struct FrequencyInspectEvidence {
    const char* rejectReason = "none";
    const char* noEmitReason = "none";
    const char* gateReason = "none";
    const char* pendingState = "none";
    bool readyOk = false;
    bool gateOpen = false;
    bool opened = false;
    bool released = false;
    bool emitted = false;
    bool validRelease = false;
    bool emitAllowed = false;
    unsigned long openMs = 0;
    unsigned long peakMs = 0;
    unsigned long releaseMs = 0;
    unsigned long durationMs = 0;
};

/*
FrequencyMatchDetectorReportDetail

Sectioned frequency detector-specific report detail.
*/
struct FrequencyMatchDetectorReportDetail {
    FrequencyAcceptedDetail accepted = {};
    FrequencySelectedRejectDetail selectedReject = {};
    FrequencyThresholdDetail thresholds = {};
    FrequencyAggregateDetail aggregates = {};
    FrequencyInspectEvidence inspect = {};
};

/*
DetectorReport

Minimal canonical detector-stage report.
Scalar and frequency detectors both populate this generic shell plus their
detector-specific detail namespace during the migration period.
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
