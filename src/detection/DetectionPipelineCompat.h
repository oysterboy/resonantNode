#pragma once

#include "../io/AudioSignal.h"
#include "DetectionPipeline.h"

namespace DetectionPipeline {

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

} // namespace DetectionPipeline
