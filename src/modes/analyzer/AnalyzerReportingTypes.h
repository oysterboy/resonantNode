#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "../../detection/DetectorReport.h"
#include "../../detection/inspector/InspectorTypes.h"

/*
AnalyzerReportingTypes

Analyzer report data model and print helper declarations.
Reports canonical detector-stage output, trial classification, field state, and
neutral diagnostic details.
Does not own detection or behavior decisions.
*/

// ANALYZER_OUTPUT_BOUNDARY
//
// Analyzer output should stay on canonical report inputs. Do not add retired
// diagnostics bridge fields here.
//
// Future canonical output targets:
//
// SEQ_TRIAL:
//   Generic trial truth only.
//   Input: AnalyzerReport + PatternResult.
//   No detector-specific fields.
//
// SEQ_INSPECT:
//   Detector-stage acceptance/rejection explanation.
//   Input: DetectorReport / RejectedCandidateSummary.
//   May include namespaced detector detail.
//
// SEQ_SUMMARY:
//   Canonical aggregate trial result counts.
//
// SEQ_EXPLAIN:
//   Deep developer chain, rebuilt later from scoped reports.
//
// RAW_SAMPLE_CAPTURE:
//   Separate diagnostic tool, not a SEQ reporting mode.
enum class AnalyzerResult {
    Expected,
    Early,
    Late,
    Miss,
    Duplicate,
    Unexpected,
    Rejected,
    Ambiguous,
    TooDense,
    Unknown,
};

enum class AnalyzerReason {
    None,
    ValidPatternInExpectedWindow,
    ValidPatternBeforeWindow,
    ValidPatternAfterWindow,
    MissingPipelineResult,
    NoOccurrence,
    OccurrenceSeenButRejected,
    InspectionFailed,
    PatternRejected,
    MultipleValidPatterns,
    MultipleCompetingPatterns,
    FieldTooDense,
    UnexpectedValidPatternWithoutTrigger,
    DuplicatePatternAfterPrimary,
    Unknown,
};

enum class AnalyzerStage {
    None,
    Source,
    Inspect,
    Pattern,
    Analyzer,
    Field,
};

inline const char* analyzerResultName(AnalyzerResult value) {
    switch (value) {
        case AnalyzerResult::Expected:
            return "expected";
        case AnalyzerResult::Early:
            return "early";
        case AnalyzerResult::Late:
            return "late";
        case AnalyzerResult::Miss:
            return "miss";
        case AnalyzerResult::Duplicate:
            return "duplicate";
        case AnalyzerResult::Unexpected:
            return "unexpected";
        case AnalyzerResult::Rejected:
            return "rejected";
        case AnalyzerResult::Ambiguous:
            return "ambiguous";
        case AnalyzerResult::TooDense:
            return "too_dense";
        case AnalyzerResult::Unknown:
        default:
            return "unknown";
    }
}

inline const char* analyzerReasonName(AnalyzerReason value) {
    switch (value) {
        case AnalyzerReason::None:
            return "none";
        case AnalyzerReason::ValidPatternInExpectedWindow:
            return "result_in_expected_window";
        case AnalyzerReason::ValidPatternBeforeWindow:
            return "result_before_window";
        case AnalyzerReason::ValidPatternAfterWindow:
            return "result_after_window";
        case AnalyzerReason::MissingPipelineResult:
            return "missing_pipeline_result";
        case AnalyzerReason::NoOccurrence:
            return "no_occurrence_pending";
        case AnalyzerReason::OccurrenceSeenButRejected:
            return "occurrence_seen_but_rejected";
        case AnalyzerReason::InspectionFailed:
            return "inspection_failed";
        case AnalyzerReason::PatternRejected:
            return "pattern_rejected";
        case AnalyzerReason::MultipleValidPatterns:
            return "multiple_valid_patterns";
        case AnalyzerReason::MultipleCompetingPatterns:
            return "multiple_competing_patterns";
        case AnalyzerReason::FieldTooDense:
            return "field_too_dense";
        case AnalyzerReason::UnexpectedValidPatternWithoutTrigger:
            return "unexpected_valid_pattern_without_trigger";
        case AnalyzerReason::DuplicatePatternAfterPrimary:
            return "duplicate_pattern_after_primary";
        case AnalyzerReason::Unknown:
        default:
            return "unknown";
    }
}

inline const char* analyzerStageName(AnalyzerStage value) {
    switch (value) {
        case AnalyzerStage::Source:
            return "source";
        case AnalyzerStage::Inspect:
            return "inspect";
        case AnalyzerStage::Pattern:
            return "pattern";
        case AnalyzerStage::Analyzer:
            return "analyzer";
        case AnalyzerStage::Field:
            return "field";
        case AnalyzerStage::None:
        default:
            return "none";
    }
}

inline const char* supportTargetDisplayName(detection::EvidenceTarget value, bool supportGateEnabled) {
    switch (value) {
        case detection::EvidenceTarget::SupportStrength:
            return supportGateEnabled ? "SupportStrength" : "diagnostic_only:SupportStrength";
        case detection::EvidenceTarget::FrequencyScoreStrength:
            return supportGateEnabled ? "FrequencyScoreStrength" : "diagnostic_only:FrequencyScoreStrength";
        case detection::EvidenceTarget::FrequencyContrastQuality:
            return supportGateEnabled ? "FrequencyContrastQuality" : "diagnostic_only:FrequencyContrastQuality";
        case detection::EvidenceTarget::TargetBandStrength:
            return supportGateEnabled ? "TargetBandStrength" : "diagnostic_only:TargetBandStrength";
        case detection::EvidenceTarget::None:
        default:
            return supportGateEnabled ? "None" : "diagnostic_only:None";
    }
}

struct AnalyzerRunContext {
    const char* profile = "none";
    const char* mode = "SEQ";
    unsigned long trial = 0;
    const char* trigger = "none";
    const char* target = "none";
    unsigned long timestampMs = 0;
    const char* build = "unknown";
};

struct AnalyzerExpectedEvent {
    unsigned long triggerMs = 0;
    unsigned long windowStartMs = 0;
    unsigned long windowEndMs = 0;
    const char* patternType = "none";
    const char* expectedSource = "none";
};

struct AnalyzerPatternObservation {
    // PatternResult-owned pattern truth. Keep detector-specific and
    // pattern-family-specific detail on PatternResult / canonical detail paths,
    // not in AnalyzerClassification.
    const char* type = "none";
    bool accepted = false;
    bool patternAccepted = false;
    bool patternMatched = false;
    bool supportMatched = false;
    bool behaviorEligible = false;

    float confidence = 0.0f;
    long dtMs = -1;

    const char* supportStrength = "unknown";
    const char* reason = "none";
    const char* rejectReason = "none";

    unsigned int involvedOccurrences = 0;
};

// Keep Analyzer-specific so shared audio reporting does not pull in analyzer
// classification logic.
struct AnalyzerOccurrenceObservation {
    // Occurrence / InspectedOccurrence-owned occurrence-stage truth.
    unsigned int total = 0;
    unsigned int accepted = 0;
    unsigned int rejected = 0;

    const char* kind = "none";
    const char* primarySource = "none";
    const char* detectorKind = "unknown";
    bool present = false;
    bool valid = false;
    unsigned long startMs = 0;
    unsigned long peakMs = 0;
    unsigned long releaseMs = 0;
    long primaryDtMs = -1;
    unsigned long primaryDurationMs = 0;
    float primaryStrength = 0.0f;
    float contrast = 0.0f;
    float strength = 0.0f;
    float confidence = 0.0f;

    const char* mainRejectReason = "none";
    const char* rejectReason = "none";
};

struct AnalyzerInspectionObservation {
    // Inspector-owned support/inspection truth for the selected trial payload.
    unsigned int inspected = 0;
    unsigned int accepted = 0;
    unsigned int rejected = 0;

    const char* primaryEvidence = "none";
    const char* moduleTarget = "unknown";
    const char* moduleStrengthClass = "unknown";
    const char* mainRejectReason = "none";
};

struct AnalyzerFieldObservation {
    const char* state = "unknown";
    float rawActivity = 0.0f;
    float validPatternActivity = 0.0f;

    unsigned int recentValidPatterns = 0;
    unsigned int recentRejects = 0;
};

struct AnalyzerClassification {
    // Generic trial classification only.
    // Detector-specific, occurrence-specific, and pattern-specific explanations
    // stay on DetectorReport, Occurrence/InspectedOccurrence, and PatternResult.
    AnalyzerResult result = AnalyzerResult::Unknown;
    AnalyzerReason reason = AnalyzerReason::Unknown;
    AnalyzerStage primaryStage = AnalyzerStage::None;

    long dtMs = -1;
};

struct AnalyzerProfileDetail {
    const char* namespaceName = "none";
    const char* summary = "";
    const char* emitter = "unknown";
    const char* inspectionAcceptance = "unknown";
    const char* inspectionPlan = "unknown";
    const char* inspectionModules = "unknown";
    size_t inspectionModuleCount = 0;
    const char* evidenceTargets = "unknown";
    const char* requiredSupportTarget = "unknown";
    const char* supportGate = "unknown";
    const char* supportStrengthMin = "medium";
    bool requireSupportForAcceptance = true;
    detection::ScalarInspectionObservation scalarObservation = {};
    size_t inspectionObservationCount = 0;
    detection::ScalarInspectionObservation inspectionObservations[detection::kMaxInspectionModules] = {};

    float freqScore = 0.0f;
    float freqContrast = 0.0f;
    float freqScoreMin = 0.0f;
    float freqContrastMin = 0.0f;

    float ampCenteredMagnitude = 0.0f;
    float ampLevel = 0.0f;
    float ampBase = 0.0f;
    float ampLift = 0.0f;
};

struct AnalyzerDebugSummary {
    unsigned int occurrences = 0;
    unsigned int inspected = 0;
    unsigned int patterns = 0;

    const char* audioHealth = "unknown";
    unsigned long audioZeroishFrames = 0;
    unsigned long audioFlatlineFrames = 0;
    unsigned long audioLargeJumpFrames = 0;
    unsigned long audioRmsTooLowFrames = 0;
    unsigned long audioRmsTooHighFrames = 0;
    unsigned long audioMaxAbsDelta = 0;

    unsigned int rejects = 0;
    unsigned int duplicates = 0;
    unsigned int unexpected = 0;
    bool startupArtifact = false;
    bool bufferOverrun = false;

    bool artifactCaptured = false;
    bool artifactFallback = false;

    const char* artifactState = "CAPTURED";
    const char* artifactReason = "none";
    const char* pipelineSource = "actual_pipeline";
    bool pipelineFallback = false;
    const char* mainRejectReason = "none";

};

struct AnalyzerCleanSummary {
    const char* profileName = "unknown";
    detection::DetectorId detectorId = detection::DetectorId::Unknown;

    unsigned int trials = 0;
    unsigned int completed = 0;
    unsigned int expected = 0;
    unsigned int early = 0;
    unsigned int late = 0;
    unsigned int miss = 0;
    unsigned int duplicate = 0;
    unsigned int unexpected = 0;
    unsigned int rejected = 0;
    unsigned int ambiguous = 0;
    unsigned int tooDense = 0;
    unsigned int bufferOverrun = 0;

    unsigned int detectorAccepted = 0;
    unsigned int detectorSelectedReject = 0;
    unsigned int validPattern = 0;
    unsigned int rejectedPattern = 0;

    long totalDtMs = 0;
    unsigned int dtCount = 0;
    float totalConfidence = 0.0f;
    unsigned int confidenceCount = 0;
};

struct AnalyzerReport {
    AnalyzerRunContext context;
    AnalyzerExpectedEvent expected;
    // Canonical detector-stage truth for clean inspect/explain/summary paths.
    const detection::DetectorReport* detectorReport = nullptr;

    AnalyzerPatternObservation primaryPattern;
    AnalyzerOccurrenceObservation occurrences;
    AnalyzerInspectionObservation inspection;
    AnalyzerFieldObservation field;

    AnalyzerClassification classification;
    AnalyzerProfileDetail profileDetail;
    AnalyzerDebugSummary debug;
};

inline AnalyzerReport makeEmptyAnalyzerReport() {
    return AnalyzerReport{};
}

