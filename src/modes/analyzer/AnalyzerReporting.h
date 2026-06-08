#pragma once

#include <stddef.h>
#include <stdint.h>

#include "../../detection/inspector/InspectorTypes.h"

class FrequencyMatchDetector;

/*
AnalyzerReporting

Analyzer report data model and print helpers.
Reports DetectionRuntime gate-chain output, trial classification, field state,
and diagnostic details.
Does not own detection or behavior decisions.
*/
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
    InvalidAudio,
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
    PatternCandidateRejected,
    MultipleValidPatterns,
    MultipleCompetingPatterns,
    FieldTooDense,
    UnexpectedValidPatternWithoutTrigger,
    DuplicatePatternAfterPrimary,
    InvalidAudio,
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

enum class FrequencyEvidenceClass {
    Accepted,
    StrongNoOccurrence,
    Partial,
    Weak,
    None,
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
        case AnalyzerResult::InvalidAudio:
            return "invalid_audio";
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
            return "no_occurrence_candidate";
        case AnalyzerReason::OccurrenceSeenButRejected:
            return "occurrence_seen_but_rejected";
        case AnalyzerReason::InspectionFailed:
            return "inspection_failed";
        case AnalyzerReason::PatternCandidateRejected:
            return "pattern_candidate_rejected";
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
        case AnalyzerReason::InvalidAudio:
            return "invalid_audio";
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

inline const char* frequencyEvidenceClassLabel(FrequencyEvidenceClass value) {
    switch (value) {
        case FrequencyEvidenceClass::Accepted:
            return "accepted";
        case FrequencyEvidenceClass::StrongNoOccurrence:
            return "strong_no_occurrence";
        case FrequencyEvidenceClass::Partial:
            return "partial";
        case FrequencyEvidenceClass::Weak:
            return "weak";
        case FrequencyEvidenceClass::None:
        default:
            return "none";
    }
}

inline size_t frequencyEvidenceClassIndex(FrequencyEvidenceClass value) {
    switch (value) {
        case FrequencyEvidenceClass::Accepted:
            return 0U;
        case FrequencyEvidenceClass::StrongNoOccurrence:
            return 1U;
        case FrequencyEvidenceClass::Partial:
            return 2U;
        case FrequencyEvidenceClass::Weak:
            return 3U;
        case FrequencyEvidenceClass::None:
        default:
            return 4U;
    }
}

inline const char* supportTargetDisplayName(detection::EvidenceTarget value, bool supportGateEnabled) {
    switch (value) {
        case detection::EvidenceTarget::AmpStrength:
            return supportGateEnabled ? "AmpStrength" : "diagnostic_only:AmpStrength";
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
    const char* type = "none";
    bool accepted = false;
    bool candidateAccepted = false;
    bool patternMatched = false;
    bool supportMatched = false;
    bool behaviorEligible = false;

    float confidence = 0.0f;
    long dtMs = -1;

    const char* ampStrength = "unknown";
    const char* reason = "none";
    const char* rejectReason = "none";

    unsigned int involvedOccurrences = 0;
};

// Keep Analyzer-specific for now; later shared AudioReporting may reuse the
// snapshot-style observation vocabulary without pulling in classification logic.
struct AnalyzerOccurrenceObservation {
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
    float score = 0.0f;
    float contrast = 0.0f;
    float strength = 0.0f;
    float confidence = 0.0f;

    const char* mainRejectReason = "none";
    const char* rejectReason = "none";
};

struct AnalyzerInspectionObservation {
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
    const char* ampStrength = "unknown";
    const char* ampStrengthMin = "medium";
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

    bool artifactCaptured = false;
    bool artifactFallback = false;

    const char* artifactState = "CAPTURED";
    const char* artifactReason = "none";
    const char* pipelineSource = "actual_pipeline";
    bool pipelineFallback = false;
    const char* mainRejectReason = "none";
    unsigned long patternResultQueueOverflowCount = 0;

};

struct AnalyzerSourceCandidateSummary {
    bool present = false;
    const char* origin = "unknown";
    unsigned long candidateCount = 0;
    unsigned long rejectCount = 0;
    unsigned long bestDurationMs = 0;
    unsigned long secondBestDurationMs = 0;
    unsigned long bestOpenMs = 0;
    unsigned long bestPeakMs = 0;
    unsigned long bestLastMatchMs = 0;
    unsigned long bestCloseMs = 0;
    float bestPeakPrimary = 0.0f;
    float bestPeakSecondary = 0.0f;
    const char* bestRejectReason = "none";
    const char* bestGateReason = "none";
    const char* closeCause = "none";
    unsigned long scoreTooLowFrames = 0;
    unsigned long contrastTooLowFrames = 0;
    unsigned long scoreAndContrastTooLowFrames = 0;
    float maxPeakPrimary = 0.0f;
    unsigned long maxPeakPrimaryMs = 0;
    float maxPeakSecondary = 0.0f;
    unsigned long maxPeakSecondaryMs = 0;
    unsigned long totalMatchMs = 0;
    unsigned long totalGapMs = 0;
    unsigned long maxGapMs = 0;
    unsigned long islandCount = 0;
};

struct AnalyzerSourceCandidateSnapshot {
    bool present = false;
    unsigned long peakMs = 0;
    unsigned long durationMs = 0;
    unsigned long sampleCount = 0;
    float peakPrimary = 0.0f;
    float peakSecondary = 0.0f;
    const char* reason = "none";
    const char* gateReason = "none";
    const char* scope = "unknown";
};

struct AnalyzerFrequencyDiagnostic {
    unsigned long currentTrialId = 0;
    unsigned long acceptedTrialId = 0;
    const char* acceptedSource = "none";
    unsigned long windowStartMs = 0;
    unsigned long windowEndMs = 0;
    unsigned long diagFirstFrameMs = 0;
    unsigned long diagLastFrameMs = 0;
    unsigned long expectedWindowMs = 0;
    unsigned long expectedFrameCountEstimate = 0;
    bool diagFrameCountOk = false;
    bool acceptedPresent = false;
    long acceptedDtMs = -1;
    unsigned long acceptedStartMs = 0;
    unsigned long acceptedPeakMs = 0;
    unsigned long acceptedReleaseMs = 0;
    unsigned long acceptedDurationMs = 0;
    float acceptedStrength = 0.0f;
    float acceptedScore = 0.0f;
    float acceptedContrast = 0.0f;

    unsigned long frames = 0;
    unsigned long validFrames = 0;
    unsigned long freshFrames = 0;
    unsigned long heldFrames = 0;
    unsigned long historyScoreRecords = 0;
    unsigned long historyContrastRecords = 0;
    unsigned long scoreOkUpdates = 0;
    unsigned long contrastOkUpdates = 0;
    unsigned long bothOkUpdates = 0;
    unsigned long matchFrames = 0;
    unsigned long rejectFrames = 0;
    unsigned long releaseScoreOkFrames = 0;
    unsigned long releaseContrastOkFrames = 0;
    unsigned long releaseBothOkFrames = 0;
    unsigned long releaseScoreTooLowFrames = 0;
    unsigned long releaseContrastTooLowFrames = 0;
    unsigned long releaseScoreAndContrastTooLowFrames = 0;
    unsigned long releaseNoEvidenceFrames = 0;
    unsigned long diagLongestMatchStreakFrames = 0;
    unsigned long diagLongestMatchStreakMs = 0;
    unsigned long diagCurrentMatchStreakFrames = 0;
    unsigned long diagCurrentMatchStreakStartMs = 0;
    unsigned long windowMs = 0;
    unsigned long updateStepMs = 0;
    float overlapRatio = 0.0f;
    unsigned long bucketCount = 0;
    unsigned long valueCount = 0;
    unsigned long spanMs = 0;
    unsigned long latestValueAgeMs = 0;
    unsigned long freshUpdateCount = 0;
    unsigned long heldUpdateCount = 0;
    unsigned long matchedUpdateCount = 0;
    unsigned long candidateDurationMs = 0;
    unsigned long matchedSpanMs = 0;
    unsigned long matchedCoverageMs = 0;
    float freshCoverageRatio = 0.0f;
    const char* audioHealth = "unknown";
    unsigned long audioZeroishFrames = 0;
    unsigned long audioFlatlineFrames = 0;
    unsigned long audioLargeJumpFrames = 0;
    unsigned long audioRmsTooLowFrames = 0;
    unsigned long audioRmsTooHighFrames = 0;
    unsigned long audioMaxAbsDelta = 0;

    float sumScore = 0.0f;
    float sumContrast = 0.0f;
    float meanScore = 0.0f;
    float meanContrast = 0.0f;
    float scoreThreshold = 0.0f;
    float contrastThreshold = 0.0f;
    float maxScore = 0.0f;
    unsigned long maxScoreMs = 0;
    float maxContrast = 0.0f;
    unsigned long maxContrastMs = 0;
    float ampPeak = 0.0f;
    float ampMean = 0.0f;
    unsigned long ampPeakMs = 0;
    float minScore = 0.0f;
    float minContrast = 0.0f;
    float peakScore = 0.0f;
    float peakContrast = 0.0f;
    unsigned long peakSampleCount = 0;
    const char* targetEvidenceClass = "none";
    bool targetPresent = false;
    bool weakTarget = false;
    bool noTarget = true;
    AnalyzerSourceCandidateSummary sourceSummary = {};
    AnalyzerSourceCandidateSnapshot sourceLastCandidate = {};

    const char* analyzerMissReason = "none";
    bool nearMiss = false;
    const char* nearMissReason = "none";
    const char* freqEvidenceClass = "none";
    const char* sourceLastRejectReason = "none";
    const char* selectedRejectReason = "none";
    const char* selectedRejectGateReason = "none";
    bool fmOpened = false;
    bool fmReleased = false;
    bool fmEmitted = false;
    bool fmDurationOk = false;
    bool fmValidRelease = false;
    bool fmEmitAllowed = false;
    unsigned long acceptedCandidateId = 0;
    unsigned long selectedRejectCandidateId = 0;
    unsigned long lastCandidateId = 0;
    unsigned long lifecycleCandidateId = 0;
    unsigned long candidateLastMatchMs = 0;
    unsigned long fmDurationUsedMs = 0;
    unsigned long fmDurationPrintedMs = 0;
    unsigned long fmMinDurationUsedMs = 0;
    unsigned long fmMinDurationReportedMs = 0;
    bool fmDurationInconsistent = false;
    bool fmPrintedDurationInconsistent = false;
    const char* fmCloseCause = "none";
    unsigned long fmOpenMs = 0;
    unsigned long fmPeakMs = 0;
    unsigned long fmReleaseMs = 0;
    unsigned long fmDurationMs = 0;
    unsigned long fmMinDurationMs = 0;
    unsigned long fmMaxDurationMs = 0;
    bool sourceOccurrenceEmitted = false;
    bool runtimeEvidenceSeen = false;
    bool runtimeOccurrenceReceived = false;
    bool analyzerSeenOccurrence = false;
    bool detectionGateBlocked = false;
    const char* detectionGateReason = "none";
    bool inconsistent = false;

    const char* liveFreqReason = "none";
    const char* liveFreqWould = "none";
    const char* liveFreqState = "none";
    bool liveFreqReady = false;
    bool liveFreqGate = false;
    bool liveFreqPresent = false;
    bool liveFreqValid = false;
    bool liveFreqMatch = false;
};

struct AnalyzerScalarDiagnostic {
    unsigned long currentTrialId = 0;
    unsigned long acceptedTrialId = 0;
    const char* acceptedSource = "none";
    unsigned long windowStartMs = 0;
    unsigned long windowEndMs = 0;
    unsigned long expectedWindowMs = 0;
    unsigned long expectedFrameCountEstimate = 0;
    bool diagFrameCountOk = false;
    bool acceptedPresent = false;
    long acceptedDtMs = -1;
    unsigned long acceptedStartMs = 0;
    unsigned long acceptedPeakMs = 0;
    unsigned long acceptedReleaseMs = 0;
    unsigned long acceptedDurationMs = 0;
    float acceptedStrength = 0.0f;
    float acceptedScore = 0.0f;
    float acceptedContrast = 0.0f;

    AnalyzerSourceCandidateSummary sourceSummary = {};
    AnalyzerSourceCandidateSnapshot sourceLastCandidate = {};

    const char* trialMissReason = "unknown";
    const char* scalarRejectReason = "none";
    const char* scalarNoEmitReason = "none";
    const char* scalarGateReason = "none";
    bool scalarOpened = false;
    bool scalarReleased = false;
    bool scalarEmitted = false;
    bool scalarValidRelease = false;
    bool scalarEmitAllowed = false;
    unsigned long scalarOpenMs = 0;
    unsigned long scalarPeakMs = 0;
    unsigned long scalarReleaseMs = 0;
    unsigned long scalarDurationMs = 0;
    unsigned long scalarMinDurationMs = 0;
    unsigned long scalarMaxDurationMs = 0;
    float scalarPeakStrength = 0.0f;

    bool sourceOccurrenceEmitted = false;
    bool runtimeEvidenceSeen = false;
    bool runtimeOccurrenceReceived = false;
    bool analyzerSeenOccurrence = false;
    bool detectionGateBlocked = false;
    const char* detectionGateReason = "none";
    bool inconsistent = false;

    const char* liveScalarReason = "none";
    const char* liveScalarWould = "none";
    const char* liveScalarState = "none";
    bool liveScalarReady = false;
    bool liveScalarGate = false;
    bool liveScalarPresent = false;
    bool liveScalarValid = false;
    bool liveScalarMatch = false;
};

struct AnalyzerSourceStageReport {
    const char* sourceKind = "unknown";
    const char* sourceName = "unknown";

    bool acceptedPresent = false;
    bool sourceOccurrenceEmitted = false;
    bool runtimeEvidenceSeen = false;
    bool runtimeOccurrenceReceived = false;
    bool analyzerSeen = false;

    bool detectionGateBlocked = false;
    const char* detectionGateReason = "none";

    AnalyzerSourceCandidateSummary sourceSummary = {};
    AnalyzerSourceCandidateSnapshot lastCandidate = {};

    bool activeAtTrialStart = false;
    bool activeAtTrialEnd = false;
    bool openedThisTrial = false;
    bool closedThisTrial = false;
    bool emittedThisTrial = false;
    bool rejectedThisTrial = false;

    AnalyzerFrequencyDiagnostic frequencyMatch = {};
    AnalyzerScalarDiagnostic scalarTransient = {};
};

struct AnalyzerSummary {
    const char* profileName = "unknown";

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
    unsigned int invalidAudio = 0;
    unsigned int startupArtifacts = 0;

    float avgDtMs = -1.0f;
    float avgDurationMs = -1.0f;
    float avgConfidence = 0.0f;

    float duplicateRate = 0.0f;
    float unexpectedRate = 0.0f;

    AnalyzerReason mainMissReason = AnalyzerReason::None;
    AnalyzerReason mainRejectReason = AnalyzerReason::None;
};

struct AnalyzerReport {
    AnalyzerRunContext context;
    AnalyzerExpectedEvent expected;
    AnalyzerSourceStageReport source;
    const FrequencyMatchDetector* frequencyDetector = nullptr;

    AnalyzerPatternObservation primaryPattern;
    AnalyzerOccurrenceObservation occurrences;
    AnalyzerInspectionObservation inspection;
    AnalyzerFieldObservation field;
    AnalyzerFrequencyDiagnostic frequency;

    AnalyzerClassification classification;
    AnalyzerProfileDetail profileDetail;
    AnalyzerDebugSummary debug;
    AnalyzerScalarDiagnostic scalar;
};

inline FrequencyEvidenceClass classifyFrequencyEvidence(const AnalyzerReport& report) {
    if (report.frequency.acceptedPresent) {
        return FrequencyEvidenceClass::Accepted;
    }
    if (report.frequency.fmOpened && report.frequency.fmReleased && !report.frequency.fmEmitted) {
        return FrequencyEvidenceClass::StrongNoOccurrence;
    }
    if (report.frequency.scoreOkUpdates > 0 || report.frequency.contrastOkUpdates > 0) {
        return FrequencyEvidenceClass::Partial;
    }
    if (report.frequency.maxScore > 0.0f) {
        return FrequencyEvidenceClass::Weak;
    }
    return FrequencyEvidenceClass::None;
}

inline AnalyzerReport makeEmptyAnalyzerReport() {
    return AnalyzerReport{};
}

