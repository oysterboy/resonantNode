#include "PatternRules.h"

namespace {

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
    result.reasonCode = DetectionPipeline::PatternReasonCode::DetectorRejected;
    result.rejectReason = DetectionPipeline::PatternRejectReason::NoCandidate;
    result.source = DetectionPipeline::PatternSource::ComparisonOnly;
    result.confidence = 0.0f;
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
    result.candidateValid = true;
    result.tonalValid = true;
    result.behaviorEligible = true;
    result.valid = true;
    result.source = DetectionPipeline::PatternSource::FrequencyPrimary;
    result.type = DetectionPipeline::PatternType::ValidTonalTransient;
    result.reasonCode = DetectionPipeline::PatternReasonCode::FromAcceptedTransient;
    result.rejectReason = DetectionPipeline::PatternRejectReason::None;
    result.confidence = 1.0f;

    // Compatibility classification only. Signal acceptance already happened in SignalInspector.
    FrequencyEvidenceEvaluation::classifyPatternResult(result, frequencyTuning);
    result.confidence = result.tonalValid ? 1.0f : 0.0f;
    result.valid = true;
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
    result.candidateValid = true;
    result.tonalValid = false;
    result.behaviorEligible = false;
    result.valid = true;
    result.source = DetectionPipeline::PatternSource::AmpFallback;
    result.type = DetectionPipeline::PatternType::TransientOnly;
    result.reasonCode = DetectionPipeline::PatternReasonCode::FromAcceptedTransient;
    result.confidence = 0.5f;

    // Compatibility classification only. Signal acceptance already happened in SignalInspector.
    FrequencyEvidenceEvaluation::classifyPatternResult(result, frequencyTuning);
    if (!candidate.frequency.present) {
        result.rejectReason = DetectionPipeline::PatternRejectReason::TransientOnly;
    }
    result.tonalValid = false;
    result.behaviorEligible = false;
    result.valid = true;
    result.confidence = 0.5f;
    return result;
}

} // namespace detection
