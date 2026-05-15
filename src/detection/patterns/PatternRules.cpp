#include "PatternRules.h"

namespace {

DetectionPipeline::LocalityClass localityFromAmpSupport(DetectionPipeline::AmpSupportClass support) {
    switch (support) {
        case DetectionPipeline::AmpSupportClass::Strong:
            return DetectionPipeline::LocalityClass::Near;
        case DetectionPipeline::AmpSupportClass::Medium:
            return DetectionPipeline::LocalityClass::Mid;
        case DetectionPipeline::AmpSupportClass::Weak:
        case DetectionPipeline::AmpSupportClass::None:
            return DetectionPipeline::LocalityClass::Far;
        case DetectionPipeline::AmpSupportClass::Unknown:
        default:
            return DetectionPipeline::LocalityClass::Unknown;
    }
}

DetectionPipeline::PatternResultKind tonalKindFromLocality(DetectionPipeline::LocalityClass locality) {
    switch (locality) {
        case DetectionPipeline::LocalityClass::Near:
            return DetectionPipeline::PatternResultKind::TonalPulseNear;
        case DetectionPipeline::LocalityClass::Mid:
            return DetectionPipeline::PatternResultKind::TonalPulseMid;
        case DetectionPipeline::LocalityClass::Far:
            return DetectionPipeline::PatternResultKind::TonalPulseFar;
        case DetectionPipeline::LocalityClass::Unknown:
        default:
            return DetectionPipeline::PatternResultKind::TonalPulse;
    }
}

DetectionPipeline::PatternResultKind resultKindFromCandidate(const DetectionPipeline::PatternCandidate& candidate) {
    if (candidate.kind == DetectionPipeline::PatternCandidateKind::PulseSequence || candidate.signalCount > 1 || candidate.pulseCount > 1) {
        if (candidate.maxGapMs > 0 && candidate.maxGapMs < 20UL) {
            return DetectionPipeline::PatternResultKind::TooDense;
        }
        if (candidate.maxGapMs > 0 && candidate.maxGapMs > 250UL) {
            return DetectionPipeline::PatternResultKind::InvalidChirp;
        }
        return DetectionPipeline::PatternResultKind::ValidChirp;
    }

    return tonalKindFromLocality(candidate.locality);
}

bool hasTransientEvidence(const DetectionPipeline::PatternCandidate& candidate) {
    return candidate.transient.present ||
           candidate.durationMs > 0 ||
           candidate.peakStrength > 0.0f ||
           candidate.onsetStrength > 0.0f ||
           candidate.releaseStrength > 0.0f;
}

bool hasFrequencyEvidence(const DetectionPipeline::PatternCandidate& candidate) {
    return candidate.frequency.present || candidate.frequencyFull.present;
}

DetectionPipeline::PatternResult makeInvalidResult(const DetectionPipeline::PatternCandidate& candidate,
                                                  unsigned long nowMs) {
    DetectionPipeline::PatternResult result = {};
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
    result.type = DetectionPipeline::PatternType::Invalid;
    result.kind = DetectionPipeline::PatternResultKind::Rejected;
    result.lineageId = candidate.lineageId;
    result.primarySlotIndex = candidate.primarySlotIndex;
    result.signalCount = candidate.signalCount;
    result.pulseCount = candidate.pulseCount;
    result.firstPulseMs = candidate.firstPulseMs;
    result.lastPulseMs = candidate.lastPulseMs;
    result.minGapMs = candidate.minGapMs;
    result.maxGapMs = candidate.maxGapMs;
    result.reasonCode = DetectionPipeline::PatternReasonCode::DetectorRejected;
    result.rejectReason = DetectionPipeline::PatternRejectReason::NoCandidate;
    result.source = DetectionPipeline::PatternSource::ComparisonOnly;
    result.confidence = 0.0f;
    result.signalConfidence = 0.0f;
    result.frequencyConfidence = 0.0f;
    result.ampSupport = DetectionPipeline::AmpSupportClass::Unknown;
    result.locality = DetectionPipeline::LocalityClass::Unknown;
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
    result.source = DetectionPipeline::PatternSource::FrequencyPrimary;
    result.type = DetectionPipeline::PatternType::ValidTonalTransient;
    result.kind = resultKindFromCandidate(candidate);
    result.reasonCode = DetectionPipeline::PatternReasonCode::FromAcceptedTransient;
    result.rejectReason = DetectionPipeline::PatternRejectReason::None;
    result.signalConfidence = candidate.signalConfidence > 0.0f ? candidate.signalConfidence : 1.0f;
    result.frequencyConfidence = candidate.frequencyConfidence;
    result.ampSupport = candidate.ampSupport;
    result.locality = candidate.locality != DetectionPipeline::LocalityClass::Unknown
        ? candidate.locality
        : localityFromAmpSupport(candidate.ampSupport);
    result.duplicateRisk = candidate.duplicateRisk;
    result.duplicateRiskScore = candidate.duplicateRiskScore;
    result.confidence = result.signalConfidence;

    // Compatibility classification only. Signal acceptance already happened in SignalInspector.
    FrequencyEvidenceEvaluation::classifyPatternResult(result, frequencyTuning);
    result.confidence = result.tonalValid ? 1.0f : 0.0f;
    result.valid = true;
    if (result.kind == DetectionPipeline::PatternResultKind::TooDense) {
        result.type = DetectionPipeline::PatternType::Ambiguous;
        result.behaviorEligible = false;
        result.tonalValid = false;
        result.valid = false;
        result.rejectReason = DetectionPipeline::PatternRejectReason::UnexpectedTiming;
    } else if (result.kind == DetectionPipeline::PatternResultKind::InvalidChirp) {
        result.type = DetectionPipeline::PatternType::Invalid;
        result.behaviorEligible = false;
        result.tonalValid = false;
        result.valid = false;
        result.rejectReason = DetectionPipeline::PatternRejectReason::UnexpectedTiming;
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
    result.source = DetectionPipeline::PatternSource::AmpFallback;
    result.type = DetectionPipeline::PatternType::TransientOnly;
    result.kind = resultKindFromCandidate(candidate);
    result.reasonCode = DetectionPipeline::PatternReasonCode::FromAcceptedTransient;
    result.signalConfidence = candidate.signalConfidence > 0.0f ? candidate.signalConfidence : 0.5f;
    result.frequencyConfidence = candidate.frequencyConfidence;
    result.ampSupport = candidate.ampSupport;
    result.locality = candidate.locality != DetectionPipeline::LocalityClass::Unknown
        ? candidate.locality
        : localityFromAmpSupport(candidate.ampSupport);
    result.duplicateRisk = candidate.duplicateRisk;
    result.duplicateRiskScore = candidate.duplicateRiskScore;
    result.confidence = result.signalConfidence;

    // Compatibility classification only. Signal acceptance already happened in SignalInspector.
    FrequencyEvidenceEvaluation::classifyPatternResult(result, frequencyTuning);
    if (!candidate.frequency.present) {
        result.rejectReason = DetectionPipeline::PatternRejectReason::TransientOnly;
    }
    result.tonalValid = false;
    result.behaviorEligible = false;
    result.valid = true;
    result.confidence = result.signalConfidence;
    if (result.kind == DetectionPipeline::PatternResultKind::TooDense) {
        result.type = DetectionPipeline::PatternType::Ambiguous;
        result.behaviorEligible = false;
        result.valid = false;
        result.rejectReason = DetectionPipeline::PatternRejectReason::UnexpectedTiming;
    } else if (result.kind == DetectionPipeline::PatternResultKind::InvalidChirp) {
        result.type = DetectionPipeline::PatternType::Invalid;
        result.behaviorEligible = false;
        result.valid = false;
        result.rejectReason = DetectionPipeline::PatternRejectReason::UnexpectedTiming;
    }
    return result;
}

} // namespace detection
