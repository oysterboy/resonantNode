#include "PatternRules.h"

namespace {
using namespace detection;

LocalityClass localityFromAmpSupport(AmpSupportClass support) {
    switch (support) {
        case AmpSupportClass::Strong:
            return LocalityClass::Near;
        case AmpSupportClass::Medium:
            return LocalityClass::Mid;
        case AmpSupportClass::Weak:
        case AmpSupportClass::None:
            return LocalityClass::Far;
        case AmpSupportClass::Unknown:
        default:
            return LocalityClass::Unknown;
    }
}

PatternResultKind tonalKindFromLocality(LocalityClass locality) {
    switch (locality) {
        case LocalityClass::Near:
            return PatternResultKind::TonalPulseNear;
        case LocalityClass::Mid:
            return PatternResultKind::TonalPulseMid;
        case LocalityClass::Far:
            return PatternResultKind::TonalPulseFar;
        case LocalityClass::Unknown:
        default:
            return PatternResultKind::TonalPulse;
    }
}

PatternResultKind resultKindFromCandidate(const PatternCandidate& candidate) {
    if (candidate.kind == PatternCandidateKind::PulseSequence || candidate.signalCount > 1 || candidate.pulseCount > 1) {
        if (candidate.maxGapMs > 0 && candidate.maxGapMs < 20UL) {
            return PatternResultKind::TooDense;
        }
        if (candidate.maxGapMs > 0 && candidate.maxGapMs > 250UL) {
            return PatternResultKind::InvalidChirp;
        }
        return PatternResultKind::ValidChirp;
    }

    return tonalKindFromLocality(candidate.locality);
}

bool hasTransientEvidence(const PatternCandidate& candidate) {
    return candidate.transient.present ||
           candidate.durationMs > 0 ||
           candidate.peakStrength > 0.0f ||
           candidate.onsetStrength > 0.0f ||
           candidate.releaseStrength > 0.0f;
}

bool hasFrequencyEvidence(const PatternCandidate& candidate) {
    return candidate.frequency.present || candidate.frequencyFull.present;
}

PatternResult makeInvalidResult(const PatternCandidate& candidate,
                                unsigned long nowMs) {
    PatternResult result = {};
    result.candidate = candidate;
    if (!result.candidate.frequency.present && result.candidate.frequencyFull.present) {
        result.candidate.frequency = result.candidate.frequencyFull;
    }
    if (!result.candidate.frequencyFull.present && result.candidate.frequency.present) {
        result.candidate.frequencyFull = result.candidate.frequency;
    }
    result.freq = candidate.frequency;
    result.freqFull = candidate.frequencyFull;
    result.processedAtMs = nowMs;
    result.type = PatternType::Invalid;
    result.kind = PatternResultKind::Rejected;
    result.lineageId = candidate.lineageId;
    result.primarySlotIndex = candidate.primarySlotIndex;
    result.signalCount = candidate.signalCount;
    result.pulseCount = candidate.pulseCount;
    result.firstPulseMs = candidate.firstPulseMs;
    result.lastPulseMs = candidate.lastPulseMs;
    result.minGapMs = candidate.minGapMs;
    result.maxGapMs = candidate.maxGapMs;
    result.reasonCode = PatternReasonCode::DetectorRejected;
    result.rejectReason = PatternRejectReason::NoCandidate;
    result.source = PatternSource::ComparisonOnly;
    result.confidence = 0.0f;
    result.signalConfidence = 0.0f;
    result.frequencyConfidence = 0.0f;
    result.ampSupport = AmpSupportClass::Unknown;
    result.locality = LocalityClass::Unknown;
    result.duplicateRisk = false;
    result.duplicateRiskScore = 0.0f;
    result.candidateValid = false;
    result.tonalValid = false;
    result.behaviorEligible = false;
    result.valid = false;
    return result;
}

} // namespace

namespace detection {

PatternResult PatternRules::evaluate(
    const PatternCandidate& candidate,
    unsigned long nowMs,
    const FrequencyEvidenceEvaluation::Values& frequencyTuning
) const {
    if (hasFrequencyEvidence(candidate)) {
        const auto eval = FrequencyEvidenceEvaluation::evaluate(candidate.frequency, frequencyTuning);
        if (eval.matched && eval.validWindow) {
            return evaluateFrequencyPattern(candidate, nowMs, frequencyTuning);
        }
    }

    if (hasTransientEvidence(candidate)) {
        return evaluateAmpPattern(candidate, nowMs, frequencyTuning);
    }

    return makeInvalidResult(candidate, nowMs);
}

PatternResult PatternRules::evaluateFrequencyPattern(
    const PatternCandidate& candidate,
    unsigned long nowMs,
    const FrequencyEvidenceEvaluation::Values& frequencyTuning
) const {
    PatternResult result = {};
    result.candidate = candidate;
    if (!result.candidate.frequency.present && result.candidate.frequencyFull.present) {
        result.candidate.frequency = result.candidate.frequencyFull;
    }
    if (!result.candidate.frequencyFull.present && result.candidate.frequency.present) {
        result.candidate.frequencyFull = result.candidate.frequency;
    }
    result.processedAtMs = nowMs;
    result.lineageId = candidate.lineageId;
    result.primarySlotIndex = candidate.primarySlotIndex;
    result.signalCount = candidate.signalCount;
    result.pulseCount = candidate.pulseCount;
    result.firstPulseMs = candidate.firstPulseMs;
    result.lastPulseMs = candidate.lastPulseMs;
    result.minGapMs = candidate.minGapMs;
    result.maxGapMs = candidate.maxGapMs;
    result.candidateValid = true;
    result.tonalValid = true;
    result.behaviorEligible = true;
    result.valid = true;
    result.source = PatternSource::FrequencyPrimary;
    result.type = PatternType::ValidTonalTransient;
    result.kind = resultKindFromCandidate(candidate);
    result.reasonCode = PatternReasonCode::FromAcceptedTransient;
    result.rejectReason = PatternRejectReason::None;
    result.signalConfidence = candidate.signalConfidence > 0.0f ? candidate.signalConfidence : 1.0f;
    result.frequencyConfidence = candidate.frequencyConfidence;
    result.ampSupport = candidate.ampSupport;
    result.locality = candidate.locality != LocalityClass::Unknown
        ? candidate.locality
        : localityFromAmpSupport(candidate.ampSupport);
    result.duplicateRisk = candidate.duplicateRisk;
    result.duplicateRiskScore = candidate.duplicateRiskScore;
    result.confidence = result.signalConfidence;

    // Compatibility classification only. Signal acceptance already happened in SignalInspector.
    FrequencyEvidenceEvaluation::classifyPatternResult(result, frequencyTuning);
    result.confidence = result.tonalValid ? 1.0f : 0.0f;
    result.valid = true;
    if (result.kind == PatternResultKind::TooDense) {
        result.type = PatternType::Ambiguous;
        result.behaviorEligible = false;
        result.tonalValid = false;
        result.valid = false;
        result.rejectReason = PatternRejectReason::UnexpectedTiming;
    } else if (result.kind == PatternResultKind::InvalidChirp) {
        result.type = PatternType::Invalid;
        result.behaviorEligible = false;
        result.tonalValid = false;
        result.valid = false;
        result.rejectReason = PatternRejectReason::UnexpectedTiming;
    }
    return result;
}

PatternResult PatternRules::evaluateAmpPattern(
    const PatternCandidate& candidate,
    unsigned long nowMs,
    const FrequencyEvidenceEvaluation::Values& frequencyTuning
) const {
    PatternResult result = {};
    result.candidate = candidate;
    if (!result.candidate.frequency.present && result.candidate.frequencyFull.present) {
        result.candidate.frequency = result.candidate.frequencyFull;
    }
    if (!result.candidate.frequencyFull.present && result.candidate.frequency.present) {
        result.candidate.frequencyFull = result.candidate.frequency;
    }
    result.processedAtMs = nowMs;
    result.lineageId = candidate.lineageId;
    result.primarySlotIndex = candidate.primarySlotIndex;
    result.signalCount = candidate.signalCount;
    result.pulseCount = candidate.pulseCount;
    result.firstPulseMs = candidate.firstPulseMs;
    result.lastPulseMs = candidate.lastPulseMs;
    result.minGapMs = candidate.minGapMs;
    result.maxGapMs = candidate.maxGapMs;
    result.candidateValid = true;
    result.tonalValid = false;
    result.behaviorEligible = false;
    result.valid = true;
    result.source = PatternSource::AmpFallback;
    result.type = PatternType::TransientOnly;
    result.kind = resultKindFromCandidate(candidate);
    result.reasonCode = PatternReasonCode::FromAcceptedTransient;
    result.signalConfidence = candidate.signalConfidence > 0.0f ? candidate.signalConfidence : 0.5f;
    result.frequencyConfidence = candidate.frequencyConfidence;
    result.ampSupport = candidate.ampSupport;
    result.locality = candidate.locality != LocalityClass::Unknown
        ? candidate.locality
        : localityFromAmpSupport(candidate.ampSupport);
    result.duplicateRisk = candidate.duplicateRisk;
    result.duplicateRiskScore = candidate.duplicateRiskScore;
    result.confidence = result.signalConfidence;

    // Compatibility classification only. Signal acceptance already happened in SignalInspector.
    FrequencyEvidenceEvaluation::classifyPatternResult(result, frequencyTuning);
    if (!candidate.frequency.present) {
        result.rejectReason = PatternRejectReason::TransientOnly;
    }
    result.tonalValid = false;
    result.behaviorEligible = false;
    result.valid = true;
    result.confidence = result.signalConfidence;
    if (result.kind == PatternResultKind::TooDense) {
        result.type = PatternType::Ambiguous;
        result.behaviorEligible = false;
        result.valid = false;
        result.rejectReason = PatternRejectReason::UnexpectedTiming;
    } else if (result.kind == PatternResultKind::InvalidChirp) {
        result.type = PatternType::Invalid;
        result.behaviorEligible = false;
        result.valid = false;
        result.rejectReason = PatternRejectReason::UnexpectedTiming;
    }
    return result;
}

} // namespace detection
