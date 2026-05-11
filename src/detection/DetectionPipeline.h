#pragma once

#include "../io/AudioSignal.h"

/*
DetectionPipeline

Owns the lightweight pattern-shaping layer between detector output and
behavior-level decisions.

Responsibilities:
- translate detector candidates into pattern candidates/results
- carry transient and frequency evidence through the pipeline
- classify pattern type and rejection reason strings for logging/debugging

Does NOT:
- read audio directly
- own detector thresholds or tuning policy
- decide behavior timing or output actions

File structure:
- classification enums
- evidence and candidate payloads
- pattern result container
- detector-candidate helpers
- name helpers for debug output
*/
namespace DetectionPipeline {

// Classification tags used by behavior, logging, and analyzer summaries.
enum class PatternType {
    None,
    ValidTransient,
    ValidTonalChirp,
    TransientOnly,
    FrequencyWeak,
    DuplicateAfterPrimary,
    UnexpectedNoise,
    Invalid,
    Ambiguous,
};

enum class PatternReasonCode {
    None,
    FromAcceptedTransient,
    DetectorRejected,
    AmbiguousEvidence,
    UnsupportedPattern,
};

enum class PatternRejectReason {
    None,
    NoCandidate,
    NoFrequencyEvidence,
    FrequencyWindowInvalid,
    FrequencyScoreTooLow,
    FrequencyContrastTooLow,
    FrequencyScoreAndContrastTooLow,
    TransientOnly,
    DuplicateAfterPrimary,
    UnexpectedTiming,
    UnexpectedNoise,
};

// Raw detector evidence captured for transient-trigger analysis.
struct TransientEvidence {
    bool present = false;

    uint64_t onsetSample = 0;
    uint64_t peakSample = 0;
    uint64_t releaseSample = 0;

    unsigned long startMs = 0;
    unsigned long heardAtMs = 0;
    unsigned long acceptedMs = 0;
    unsigned long durationMs = 0;

    float onsetStrength = 0.0f;
    float peakStrength = 0.0f;
    float releaseStrength = 0.0f;
    float ambientBaseline = 0.0f;

    bool audioOverflowDuringCandidate = false;
};

// Frequency evidence carried alongside a candidate for tonal classification.
struct FrequencyEvidence {
    bool present = false;
    bool matched = false;

    unsigned long targetHz = 0;
    unsigned long observedAtMs = 0;
    uint64_t windowStartSample = 0;
    uint64_t windowEndSample = 0;
    unsigned long windowSampleCount = 0;
    bool windowAvailable = false;

    float score = 0.0f;
    float confidence = 0.0f;

    float targetPower = 0.0f;
    float neighborPower = 0.0f;
    float totalEnergy = 0.0f;
    float spectralContrast = 0.0f;
    bool validWindow = false;
};

// Combined detector candidate payload passed through the pipeline.
struct PatternCandidate {
    uint64_t onsetSample = 0;
    uint64_t peakSample = 0;
    uint64_t releaseSample = 0;

    unsigned long startMs = 0;
    unsigned long heardAtMs = 0;
    unsigned long acceptedMs = 0;
    unsigned long durationMs = 0;

    float onsetStrength = 0.0f;
    float peakStrength = 0.0f;
    float releaseStrength = 0.0f;
    float ambientBaseline = 0.0f;

    bool audioOverflowDuringCandidate = false;

    TransientEvidence transient;
    FrequencyEvidence frequency;
    FrequencyEvidence frequencyFull;
};

// Final pipeline result consumed by behavior and analyzer reporting.
struct PatternResult {
    PatternType type = PatternType::None;
    PatternReasonCode reasonCode = PatternReasonCode::None;
    PatternRejectReason rejectReason = PatternRejectReason::None;
    float confidence = 0.0f;
    unsigned long processedAtMs = 0;
    PatternCandidate candidate = {};
    FrequencyEvidence freq = {};
    FrequencyEvidence freqFull = {};
    bool candidateValid = false;
    bool tonalValid = false;
    bool behaviorEligible = false;
    bool valid = false;
};

// Build a candidate only when the detector produced meaningful evidence.
inline bool isDetectorCandidateAccepted(const DetectorCandidate& in) {
    return in.durationMs > 0 || in.peakStrength > 0.0f || in.releaseMillisApprox != 0;
}

// Copy detector output into the pipeline candidate container.
inline PatternCandidate makePatternCandidate(const DetectorCandidate& in) {
    PatternCandidate out;
    out.onsetSample = in.onsetSample;
    out.peakSample = in.peakSample;
    out.releaseSample = in.releaseSample;
    out.startMs = in.onsetMillisApprox;
    out.heardAtMs = in.releaseMillisApprox != 0 ? in.releaseMillisApprox : in.onsetMillisApprox;
    out.acceptedMs = out.heardAtMs;
    out.durationMs = in.durationMs;
    out.onsetStrength = in.onsetStrength;
    out.peakStrength = in.peakStrength;
    out.releaseStrength = in.releaseStrength;
    out.ambientBaseline = in.ambientBaseline;
    out.audioOverflowDuringCandidate = in.audioOverflowDuringCandidate;
    out.transient.present = isDetectorCandidateAccepted(in);
    out.transient.onsetSample = in.onsetSample;
    out.transient.peakSample = in.peakSample;
    out.transient.releaseSample = in.releaseSample;
    out.transient.startMs = in.onsetMillisApprox;
    out.transient.heardAtMs = in.releaseMillisApprox != 0 ? in.releaseMillisApprox : in.onsetMillisApprox;
    out.transient.acceptedMs = out.transient.heardAtMs;
    out.transient.durationMs = in.durationMs;
    out.transient.onsetStrength = in.onsetStrength;
    out.transient.peakStrength = in.peakStrength;
    out.transient.releaseStrength = in.releaseStrength;
    out.transient.ambientBaseline = in.ambientBaseline;
    out.transient.audioOverflowDuringCandidate = in.audioOverflowDuringCandidate;
    out.frequency = {};
    return out;
}

// Initialize a pattern result from a detector candidate and optional frequency evidence.
inline bool processDetectorCandidate(const DetectorCandidate& in, PatternResult& out, unsigned long processedAtMs) {
    out = {};
    out.candidate = makePatternCandidate(in);
    out.freq = out.candidate.frequency;
    out.freqFull = out.candidate.frequencyFull;
    out.processedAtMs = processedAtMs;

    if (!isDetectorCandidateAccepted(in)) {
        out.type = PatternType::Invalid;
        out.reasonCode = PatternReasonCode::DetectorRejected;
        out.rejectReason = PatternRejectReason::NoCandidate;
        out.confidence = 0.0f;
        out.candidateValid = false;
        out.tonalValid = false;
        out.behaviorEligible = false;
        out.valid = false;
        return false;
    }

    out.type = PatternType::ValidTransient;
    out.reasonCode = PatternReasonCode::FromAcceptedTransient;
    out.rejectReason = PatternRejectReason::None;
    out.confidence = 1.0f;
    out.candidateValid = true;
    out.tonalValid = false;
    out.behaviorEligible = false;
    out.valid = true;
    return true;
}

// Convenience overload that attaches early frequency evidence to the candidate payload.
inline bool processDetectorCandidate(const DetectorCandidate& in, PatternResult& out, unsigned long processedAtMs, const FrequencyEvidence* frequencyEvidence) {
    const bool accepted = processDetectorCandidate(in, out, processedAtMs);
    if (frequencyEvidence != nullptr) {
        out.candidate.frequency = *frequencyEvidence;
    }
    return accepted;
}

// String helpers for compact debug/log output.
inline const char* patternTypeName(PatternType type) {
    switch (type) {
        case PatternType::None:
            return "none";
        case PatternType::ValidTransient:
            return "valid_transient";
        case PatternType::ValidTonalChirp:
            return "valid_tonal_chirp";
        case PatternType::TransientOnly:
            return "transient_only";
        case PatternType::FrequencyWeak:
            return "frequency_weak";
        case PatternType::DuplicateAfterPrimary:
            return "duplicate_after_primary";
        case PatternType::UnexpectedNoise:
            return "unexpected_noise";
        case PatternType::Invalid:
            return "invalid";
        case PatternType::Ambiguous:
            return "ambiguous";
    }

    return "unknown";
}

inline const char* patternReasonName(PatternReasonCode code) {
    switch (code) {
        case PatternReasonCode::None:
            return "none";
        case PatternReasonCode::FromAcceptedTransient:
            return "from_accepted_transient";
        case PatternReasonCode::DetectorRejected:
            return "detector_rejected";
        case PatternReasonCode::AmbiguousEvidence:
            return "ambiguous_evidence";
        case PatternReasonCode::UnsupportedPattern:
            return "unsupported_pattern";
    }

    return "unknown";
}

inline const char* patternRejectReasonName(PatternRejectReason reason) {
    switch (reason) {
        case PatternRejectReason::None:
            return "none";
        case PatternRejectReason::NoCandidate:
            return "no_candidate";
        case PatternRejectReason::NoFrequencyEvidence:
            return "no_frequency_evidence";
        case PatternRejectReason::FrequencyWindowInvalid:
            return "frequency_window_invalid";
        case PatternRejectReason::FrequencyScoreTooLow:
            return "freq_score_too_low";
        case PatternRejectReason::FrequencyContrastTooLow:
            return "freq_contrast_too_low";
        case PatternRejectReason::FrequencyScoreAndContrastTooLow:
            return "freq_score_and_contrast_too_low";
        case PatternRejectReason::TransientOnly:
            return "transient_only";
        case PatternRejectReason::DuplicateAfterPrimary:
            return "duplicate_after_primary";
        case PatternRejectReason::UnexpectedTiming:
            return "unexpected_timing";
        case PatternRejectReason::UnexpectedNoise:
            return "unexpected_noise";
    }

    return "unknown";
}

} // namespace DetectionPipeline
