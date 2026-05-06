#pragma once

#include "../io/AudioSignal.h"

namespace DetectionPipeline {

enum class PatternType {
    None,
    ValidTransient,
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
};

struct PatternResult {
    PatternType type = PatternType::None;
    PatternReasonCode reasonCode = PatternReasonCode::None;
    float confidence = 0.0f;
    PatternCandidate candidate = {};
    bool valid = false;
};

inline bool isDetectorCandidateAccepted(const DetectorCandidate& in) {
    return in.durationMs > 0 || in.peakStrength > 0.0f || in.releaseMillisApprox != 0;
}

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
    return out;
}

inline bool processDetectorCandidate(const DetectorCandidate& in, PatternResult& out) {
    out = {};
    out.candidate = makePatternCandidate(in);

    if (!isDetectorCandidateAccepted(in)) {
        out.type = PatternType::Invalid;
        out.reasonCode = PatternReasonCode::DetectorRejected;
        out.confidence = 0.0f;
        out.valid = false;
        return false;
    }

    out.type = PatternType::ValidTransient;
    out.reasonCode = PatternReasonCode::FromAcceptedTransient;
    out.confidence = 1.0f;
    out.valid = true;
    return true;
}

inline const char* patternTypeName(PatternType type) {
    switch (type) {
        case PatternType::None:
            return "none";
        case PatternType::ValidTransient:
            return "valid_transient";
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

} // namespace DetectionPipeline
