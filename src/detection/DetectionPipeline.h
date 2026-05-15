#pragma once

#include "../io/AudioSignal.h"
#include "patterns/PatternCandidate.h"
#include "patterns/PatternResult.h"

/*
DetectionPipeline

Owns the lightweight compatibility layer between detector output and
behavior-level decisions.

Responsibilities:
- translate detector candidates from stream-extractor/detector stages into pattern candidates/results
- carry transient and frequency evidence through the pipeline
- classify pattern type and rejection reason strings for logging/debugging
- keep the compatibility aliases for legacy and analyzer paths

Does NOT:
- read audio directly
- own detector thresholds or tuning policy
- decide behavior timing or output actions

Roadmap v0.3 note:
- canonical pattern payloads now live in the dedicated pattern headers
- this header keeps the compatibility namespace, helper functions, and legacy aliases
*/
namespace DetectionPipeline {

using detection::AmpSupportClass;
using detection::FrequencyEvidence;
using detection::LocalityClass;
using detection::PatternCandidate;
using detection::PatternCandidateKind;
using detection::PatternReasonCode;
using detection::PatternRejectReason;
using detection::PatternResult;
using detection::PatternResultKind;
using detection::PatternSource;
using detection::PatternType;
using detection::TransientEvidence;

// Build a candidate only when the detector produced meaningful evidence.
inline bool isDetectorCandidateAccepted(const DetectorCandidate& in) {
    return in.durationMs > 0 || in.peakStrength > 0.0f || in.releaseMillisApprox != 0;
}

// Copy detector output into the pipeline candidate container.
inline PatternCandidate makePatternCandidate(const DetectorCandidate& in) {
    PatternCandidate out;
    out.kind = PatternCandidateKind::SinglePulse;
    out.lineageId = static_cast<uint32_t>(in.onsetSample & 0xFFFFFFFFu);
    out.primarySlotIndex = 0;
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
    out.source = PatternSource::ComparisonOnly;
    out.kind = PatternResultKind::Rejected;
    out.lineageId = static_cast<uint32_t>(in.onsetSample & 0xFFFFFFFFu);
    out.primarySlotIndex = 0;
    out.candidate = makePatternCandidate(in);
    out.freq = out.candidate.frequency;
    out.freqFull = out.candidate.frequencyFull;
    out.processedAtMs = processedAtMs;

    if (!isDetectorCandidateAccepted(in)) {
        out.kind = PatternResultKind::Rejected;
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
    out.kind = PatternResultKind::Residual;
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

// String helpers for compact debug/log output.
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

} // namespace DetectionPipeline
