#pragma once

#include <stddef.h>
#include <stdint.h>

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

    long dtMs = -1;
};

struct AnalyzerAmpStrengthObservation {
    bool available = false;
    bool observedOnly = true;
    const char* supportBasis = "centered_magnitude_peak";
    const char* mode = "peak_absolute";
    const char* note = "none";

    int16_t windowStartMs = -20;
    int16_t windowEndMs = 120;

    float classificationValue = 0.0f;
    float centeredMagnitude = 0.0f;
    float peak = 0.0f;
    float mean = 0.0f;
    float last = 0.0f;
    float baseline = 0.0f;
    float lift = 0.0f;
    size_t sampleCount = 0;
    size_t sustainedCount = 0;
    unsigned long sustainedMs = 0;
    float sustainedThreshold = 0.0f;

    const char* strength = "unknown";
};

struct AnalyzerProfileDetail {
    const char* namespaceName = "none";
    const char* summary = "";
    const char* emitter = "unknown";
    const char* inspectionAcceptance = "unknown";
    const char* inspectionPlan = "unknown";
    const char* inspectionModules = "unknown";
    const char* evidenceTargets = "unknown";
    const char* requiredSupportTarget = "unknown";
    const char* ampStrength = "unknown";
    const char* ampStrengthMin = "medium";
    bool requireSupportForAcceptance = true;

    float freqScore = 0.0f;
    float freqContrast = 0.0f;
    float freqScoreMin = 0.0f;
    float freqContrastMin = 0.0f;

    float ampCenteredMagnitude = 0.0f;
    float ampLevel = 0.0f;
    float ampBase = 0.0f;
    float ampLift = 0.0f;
    AnalyzerAmpStrengthObservation ampStrengthObservation;
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

    bool artifactCaptured = false;
    bool artifactFallback = false;

    const char* artifactState = "CAPTURED";
    const char* artifactReason = "none";
    const char* pipelineSource = "actual_pipeline";
    bool pipelineFallback = false;
    const char* mainRejectReason = "none";

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
    unsigned long scoreOkFrames = 0;
    unsigned long contrastOkFrames = 0;
    unsigned long bothOkFrames = 0;
    unsigned long matchFrames = 0;
    unsigned long rejectFrames = 0;
    unsigned long longestMatchRunFrames = 0;
    unsigned long longestMatchRunMs = 0;
    unsigned long currentMatchRunFrames = 0;
    unsigned long currentMatchRunStartMs = 0;
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
    float minScore = 0.0f;
    float minContrast = 0.0f;
    float peakScore = 0.0f;
    float peakContrast = 0.0f;
    unsigned long peakWindowSampleCount = 0;

    const char* trialMissReason = "unknown";
    bool nearMiss = false;
    const char* nearMissReason = "none";
    const char* freqEvidenceClass = "none";
    const char* fmRejectReason = "none";
    const char* fmNoEmitReason = "none";
    const char* fmGateReason = "none";
    bool fmOpened = false;
    bool fmReleased = false;
    bool fmEmitted = false;
    bool fmDurationOk = false;
    bool fmValidRelease = false;
    bool fmEmitAllowed = false;
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

inline AnalyzerReport makeEmptyAnalyzerReport() {
    return AnalyzerReport{};
}

