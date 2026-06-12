#pragma once

#include "PatternTypes.h"

namespace detection {

inline const char* patternResultKindName(PatternResultKind kind) {
    switch (kind) {
        case PatternResultKind::Unknown:
            return "unknown";
        case PatternResultKind::Valid:
            return "valid";
        case PatternResultKind::Invalid:
            return "invalid";
        case PatternResultKind::TooDense:
            return "too_dense";
        case PatternResultKind::Rejected:
            return "rejected";
    }

    return "unknown";
}

inline const char* patternTypeName(PatternType type) {
    switch (type) {
        case PatternType::None:
            return "none";
        case PatternType::SinglePulse:
            return "single_pulse";
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
        case PatternReasonCode::FromFrequencyMatch:
            return "from_frequency_match";
        case PatternReasonCode::FromOccurrence:
            return "from_occurrence";
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
        case PatternRejectReason::InvalidOccurrence:
            return "invalid_occurrence";
        case PatternRejectReason::NoFrequencyEvidence:
            return "no_frequency_evidence";
        case PatternRejectReason::FrequencyWindowInvalid:
            return "frequency_window_invalid";
        case PatternRejectReason::FrequencyScoreTooLow:
            return "score_too_low";
        case PatternRejectReason::FrequencyContrastTooLow:
            return "contrast_too_low";
        case PatternRejectReason::FrequencyScoreAndContrastTooLow:
            return "score_and_contrast_too_low";
        case PatternRejectReason::MissingSupport:
            return "missing_support";
        case PatternRejectReason::SupportTooLow:
            return "support_too_low";
        case PatternRejectReason::DuplicateAfterPrimary:
            return "duplicate_after_primary";
        case PatternRejectReason::UnexpectedTiming:
            return "unexpected_timing";
        case PatternRejectReason::UnexpectedNoise:
            return "unexpected_noise";
    }

    return "unknown";
}

} // namespace detection
