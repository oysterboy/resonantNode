#pragma once

#include "../DetectionCoreTypes.h"
#include "DetectorReject.h"

namespace detection {

/*
OccurrenceSummary

What a detector measured over one closed occurrence, accepted or rejected, frozen into DetectorReport. Only keep
fields another detector could plausibly expose with the same meaning.
*/
struct OccurrenceSummary {
    bool present = false;
    unsigned long occurrenceId = 0;
    unsigned long startMs = 0;
    unsigned long peakMs = 0;
    unsigned long endMs = 0;
    unsigned long durationMs = 0;
    float strength = 0.0f;  // the occurrence's strength; the scalar detector's is its peak (the same value as peak)
    float confidence = 0.0f;
    float peak = 0.0f;
    float mean = 0.0f;
    float rms = 0.0f;
    float meanStrength = 0.0f;
    float matchedMeanStrength = 0.0f;
    unsigned int strengthCount = 0;
    unsigned int matchedStrengthCount = 0;
    unsigned long coverageAboveAttackMs = 0;
    unsigned long coverageAboveReleaseMs = 0;
    unsigned long sustainedMs = 0;
    unsigned int islandCount = 0;
    unsigned int gapCount = 0;
    unsigned long islandMaxMs = 0;
    unsigned long gapMaxMs = 0;
};

// The accepted occurrence: the summary alone.
using AcceptedOccurrenceSummary = OccurrenceSummary;

/*
SelectedRejectSummary

The selected (best) reject: the summary plus why it was rejected. `detectorReason` stays string-based until detector
reason enums are unified.
*/
struct SelectedRejectSummary : OccurrenceSummary {
    DetectorRejectClass rejectClass = DetectorRejectClass::None;
    const char* detectorReason = "none";
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
};

/*
ScalarSelectedRejectDetail

Scalar-specific selected-reject detail. Fields may remain partially populated
while the scalar detector still owns only a subset of the scalar report truth.
*/
struct ScalarSelectedRejectDetail {
    bool present = false;
    float value = 0.0f;
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
    float bestRejectedValue = 0.0f;
};

/*
ScalarCandidateEvidence

Bounded scalar detector evidence needed for Analyzer / SEQ_INSPECT while the
canonical report keeps scalar gate/lifecycle truth detector-owned.
*/
struct ScalarCandidateEvidence {
    const char* rejectReason = "none";
    const char* noEmitReason = "none";
    const char* gateReason = "none";
    bool matchedMeanPassed = false;
    bool carrierQualityRequired = false;
    bool carrierCoveragePassed = true;
    bool carrierIslandPassed = true;
    bool carrierGapPassed = true;
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
    ScalarCandidateEvidence inspect = {};
};

/*
DetectorReport

Minimal canonical detector-stage report.
The scalar detector populates this generic shell plus its detector-specific
detail namespace.
*/
struct DetectorReport {
    DetectorId detectorId = DetectorId::Unknown;
    unsigned long reportStartMs = 0;
    unsigned long reportEndMs = 0;
    const char* sourceSelection = "none";
    unsigned long sourceOccurrenceId = 0;
    unsigned long sourceCandidateId = 0;
    bool sourceReportMatched = false;
    AcceptedOccurrenceSummary accepted = {};
    SelectedRejectSummary selectedReject = {};
    ThresholdSummary thresholds = {};
    AggregateCountSummary aggregates = {};
    ScalarDetectorReportDetail scalar = {};
};

} // namespace detection
