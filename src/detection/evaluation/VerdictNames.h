#pragma once

#include "VerdictTypes.h"

namespace detection {

inline const char* verdictTypeName(VerdictType type) {
    switch (type) {
        case VerdictType::None:
            return "none";
        case VerdictType::SinglePulse:
            return "single_pulse";
        case VerdictType::DuplicateAfterPrimary:
            return "duplicate_after_primary";
        case VerdictType::UnexpectedNoise:
            return "unexpected_noise";
        case VerdictType::Invalid:
            return "invalid";
        case VerdictType::Ambiguous:
            return "ambiguous";
    }

    return "unknown";
}

inline const char* verdictReasonName(VerdictReasonCode code) {
    switch (code) {
        case VerdictReasonCode::None:
            return "none";
        case VerdictReasonCode::FromFrequencyMatch:
            return "from_frequency_match";
        case VerdictReasonCode::FromOccurrence:
            return "from_occurrence";
        case VerdictReasonCode::DetectorRejected:
            return "detector_rejected";
        case VerdictReasonCode::AmbiguousEvidence:
            return "ambiguous_evidence";
        case VerdictReasonCode::SupportRequirementFailed:
            return "support_requirement_failed";
    }

    return "unknown";
}

inline const char* verdictRejectReasonName(VerdictRejectReason reason) {
    switch (reason) {
        case VerdictRejectReason::None:
            return "none";
        case VerdictRejectReason::NoProposal:
            return "no_proposal";
        case VerdictRejectReason::InvalidOccurrence:
            return "invalid_occurrence";
        case VerdictRejectReason::NoFrequencyEvidence:
            return "no_frequency_evidence";
        case VerdictRejectReason::FrequencyWindowInvalid:
            return "frequency_window_invalid";
        case VerdictRejectReason::FrequencyScoreTooLow:
            return "score_too_low";
        case VerdictRejectReason::FrequencyContrastTooLow:
            return "contrast_too_low";
        case VerdictRejectReason::FrequencyScoreAndContrastTooLow:
            return "score_and_contrast_too_low";
        case VerdictRejectReason::MissingSupport:
            return "missing_support";
        case VerdictRejectReason::SupportTooLow:
            return "support_too_low";
        case VerdictRejectReason::DuplicateAfterPrimary:
            return "duplicate_after_primary";
        case VerdictRejectReason::UnexpectedTiming:
            return "unexpected_timing";
        case VerdictRejectReason::UnexpectedNoise:
            return "unexpected_noise";
    }

    return "unknown";
}

} // namespace detection
