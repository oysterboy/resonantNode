#pragma once

#include "PatternTypes.h"

namespace detection {

inline const char* patternSourceName(PatternSource source) {
    switch (source) {
        case PatternSource::ComparisonOnly:
            return "comparison_only";
        case PatternSource::AmpFallback:
            return "amp_fallback";
        case PatternSource::FrequencyPrimary:
            return "frequency_primary";
    }

    return "unknown";
}

inline const char* patternCandidateKindName(PatternCandidateKind kind) {
    switch (kind) {
        case PatternCandidateKind::Unknown:
            return "unknown";
        case PatternCandidateKind::SinglePulse:
            return "single_pulse";
        case PatternCandidateKind::PulseSequence:
            return "pulse_sequence";
        case PatternCandidateKind::NoiseBurst:
            return "noise_burst";
        case PatternCandidateKind::ObjectHit:
            return "object_hit";
    }

    return "unknown";
}

inline const char* patternResultKindName(PatternResultKind kind) {
    switch (kind) {
        case PatternResultKind::Unknown:
            return "unknown";
        case PatternResultKind::TonalPulse:
            return "tonal_pulse";
        case PatternResultKind::TonalPulseNear:
            return "tonal_pulse_near";
        case PatternResultKind::TonalPulseMid:
            return "tonal_pulse_mid";
        case PatternResultKind::TonalPulseFar:
            return "tonal_pulse_far";
        case PatternResultKind::ValidChirp:
            return "valid_chirp";
        case PatternResultKind::InvalidChirp:
            return "invalid_chirp";
        case PatternResultKind::TooDense:
            return "too_dense";
        case PatternResultKind::Residual:
            return "residual";
        case PatternResultKind::Rejected:
            return "rejected";
    }

    return "unknown";
}

inline const char* patternTypeName(PatternType type) {
    switch (type) {
        case PatternType::None:
            return "none";
        case PatternType::ValidTransient:
            return "valid_transient";
        case PatternType::ValidTonalTransient:
            return "valid_tonal_transient";
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
            return "score_too_low";
        case PatternRejectReason::FrequencyContrastTooLow:
            return "contrast_too_low";
        case PatternRejectReason::FrequencyScoreAndContrastTooLow:
            return "score_and_contrast_too_low";
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

} // namespace detection
