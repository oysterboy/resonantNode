#pragma once

#include <stdint.h>

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
    NoSignalCandidate,
    SignalSeenButRejected,
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
            return "valid_pattern_in_expected_window";
        case AnalyzerReason::ValidPatternBeforeWindow:
            return "valid_pattern_before_window";
        case AnalyzerReason::ValidPatternAfterWindow:
            return "valid_pattern_after_window";
        case AnalyzerReason::NoSignalCandidate:
            return "no_signal_candidate";
        case AnalyzerReason::SignalSeenButRejected:
            return "signal_seen_but_rejected";
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

    float confidence = 0.0f;
    long dtMs = -1;

    const char* locality = "unknown";
    const char* sourceClass = "unknown";
    const char* reason = "none";

    unsigned int involvedSignals = 0;
};

// Keep Analyzer-specific for now; later shared AudioReporting may reuse the
// snapshot-style observation vocabulary without pulling in classification logic.
struct AnalyzerSignalObservation {
    unsigned int total = 0;
    unsigned int accepted = 0;
    unsigned int rejected = 0;

    const char* primarySource = "none";
    long primaryDtMs = -1;
    unsigned long primaryDurationMs = 0;
    float primaryStrength = 0.0f;
    float primaryConfidence = 0.0f;

    const char* mainRejectReason = "none";
    bool duplicateRisk = false;
};

struct AnalyzerInspectionObservation {
    unsigned int inspected = 0;
    unsigned int accepted = 0;
    unsigned int rejected = 0;

    const char* primaryEvidence = "none";
    const char* locality = "unknown";
    const char* supportClass = "unknown";
    const char* mainRejectReason = "none";
};

struct AnalyzerFieldObservation {
    const char* state = "unknown";
    float activity = 0.0f;
    float density = 0.0f;

    unsigned int recentValidPatterns = 0;
    unsigned int recentRejects = 0;
};

struct AnalyzerClassification {
    AnalyzerResult result = AnalyzerResult::Unknown;
    AnalyzerReason reason = AnalyzerReason::Unknown;

    long dtMs = -1;
    float confidence = 0.0f;
};

struct AnalyzerAmpWindowObservation {
    bool available = false;
    bool observedOnly = true;
    const char* note = "none";

    int16_t windowStartMs = -20;
    int16_t windowEndMs = 120;

    float peak = 0.0f;
    float baseline = 0.0f;
    float lift = 0.0f;
    float norm = 0.0f;

    const char* supportClass = "unknown";
    const char* localityClass = "unknown";
};

struct AnalyzerProfileDetail {
    const char* namespaceName = "none";
    const char* summary = "";

    float freqScore = 0.0f;
    float freqContrast = 0.0f;

    float ampLevel = 0.0f;
    float ampBase = 0.0f;
    float ampLift = 0.0f;
    float ampNorm = 0.0f;

    const char* ampLocality = "unknown";
    AnalyzerAmpWindowObservation ampWindow;
};

struct AnalyzerDebugSummary {
    unsigned int signals = 0;
    unsigned int inspected = 0;
    unsigned int patterns = 0;

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

    bool parityCompared = false;
    bool parityMatch = true;
    bool parityAcceptedMatch = true;
    bool parityTypeMatch = true;
    bool parityLocalityMatch = true;
    bool paritySourceMatch = true;
    bool parityReasonMatch = true;
    bool parityTimingClose = true;
    bool parityConfidenceClose = true;
    float parityConfidenceDelta = 0.0f;
    long parityTimingDeltaMs = 0;
    const char* parityReason = "none";
    const char* paritySummary = "none";
};

struct AnalyzerSummary {
    const char* profileName = "unknown";

    unsigned int trials = 0;
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
    AnalyzerSignalObservation signals;
    AnalyzerInspectionObservation inspection;
    AnalyzerFieldObservation field;

    AnalyzerClassification classification;
    AnalyzerProfileDetail profileDetail;
    AnalyzerDebugSummary debug;
};

inline AnalyzerReport makeEmptyAnalyzerReport() {
    return AnalyzerReport{};
}
