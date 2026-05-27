#include "PatternRules.h"

// PatternRules converts PatternCandidates into PatternResults.
namespace detection {

void PatternRules::configure(const PatternRulesConfig& config) {
    _config = config;
}

namespace {

StrengthClass supportStrengthForTarget(const PatternCandidate& candidate, EvidenceTarget target) {
    switch (target) {
        case EvidenceTarget::AmpStrength:
            return candidate.ampStrength;
        case EvidenceTarget::FrequencyScoreStrength:
            return candidate.frequencyScoreStrength;
        case EvidenceTarget::FrequencyContrastQuality:
            return candidate.frequencyContrastQuality;
        case EvidenceTarget::TargetBandStrength:
            return candidate.targetBandStrength;
        case EvidenceTarget::None:
        default:
            return StrengthClass::Unknown;
    }
}

PatternResultKind resultKindFromCandidate(const PatternCandidate& candidate) {
    if (candidate.kind == PatternCandidateKind::PulseSequence || candidate.occurrenceCount > 1 || candidate.pulseCount > 1) {
        if (candidate.maxGapMs > 0 && candidate.maxGapMs < 20UL) {
            return PatternResultKind::TooDense;
        }
        if (candidate.maxGapMs > 0 && candidate.maxGapMs > 250UL) {
            return PatternResultKind::Invalid;
        }
        return PatternResultKind::Valid;
    }

    return PatternResultKind::Valid;
}

PatternRejectReason supportRejectReason(StrengthClass supportStrength) {
    return supportStrength == StrengthClass::Unknown
        ? PatternRejectReason::MissingSupport
        : PatternRejectReason::SupportTooLow;
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
    result.occurrenceCount = candidate.occurrenceCount;
    result.pulseCount = candidate.pulseCount;
    result.firstPulseMs = candidate.firstPulseMs;
    result.lastPulseMs = candidate.lastPulseMs;
    result.minGapMs = candidate.minGapMs;
    result.maxGapMs = candidate.maxGapMs;
    result.reasonCode = PatternReasonCode::FromOccurrence;
    result.rejectReason = PatternRejectReason::InvalidOccurrence;
    result.confidence = 0.0f;
    result.ampStrength = StrengthClass::Unknown;
    result.ampStrengthEvidence = candidate.ampStrengthEvidence;
    result.frequencyScoreStrength = candidate.frequencyScoreStrength;
    result.frequencyContrastQuality = candidate.frequencyContrastQuality;
    result.targetBandStrength = candidate.targetBandStrength;
    result.patternCandidateAccepted = false;
    result.patternMatched = false;
    result.supportMatched = false;
    result.valid = false;
    return result;
}

} // namespace

PatternResult PatternRules::evaluate(
    const PatternCandidate& candidate,
    unsigned long nowMs
) const {
    if (!candidate.valid) {
        return makeInvalidResult(candidate, nowMs);
    }

    return evaluateFrequencyPattern(candidate, nowMs);
}

PatternResult PatternRules::evaluateFrequencyPattern(
    const PatternCandidate& candidate,
    unsigned long nowMs
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
    result.occurrenceCount = candidate.occurrenceCount;
    result.pulseCount = candidate.pulseCount;
    result.firstPulseMs = candidate.firstPulseMs;
    result.lastPulseMs = candidate.lastPulseMs;
    result.minGapMs = candidate.minGapMs;
    result.maxGapMs = candidate.maxGapMs;
    result.patternCandidateAccepted = true;
    result.patternMatched = true;
    result.supportMatched = true;
    result.valid = true;
    result.type = PatternType::ValidPattern;
    result.kind = resultKindFromCandidate(candidate);
    result.reasonCode = PatternReasonCode::FromOccurrence;
    result.rejectReason = PatternRejectReason::None;
    result.ampStrength = candidate.ampStrength;
    result.ampStrengthEvidence = candidate.ampStrengthEvidence;
    result.frequencyScoreStrength = candidate.frequencyScoreStrength;
    result.frequencyContrastQuality = candidate.frequencyContrastQuality;
    result.targetBandStrength = candidate.targetBandStrength;
    result.supportMatched = true;
    if (_config.requireSupportForAcceptance) {
        const StrengthClass supportStrength = supportStrengthForTarget(candidate, _config.requiredSupportTarget);
        result.supportMatched = supportStrength >= _config.minimumSupportStrength;
    }
    if (!result.supportMatched) {
        result.kind = PatternResultKind::Rejected;
        result.rejectReason = supportRejectReason(supportStrengthForTarget(candidate, _config.requiredSupportTarget));
        result.reasonCode = PatternReasonCode::UnsupportedPattern;
    }
    result.valid = result.patternMatched && result.supportMatched;
    result.confidence = result.valid ? 1.0f : 0.0f;
    if (result.kind == PatternResultKind::TooDense) {
        result.type = PatternType::Ambiguous;
        result.valid = false;
        result.rejectReason = PatternRejectReason::UnexpectedTiming;
    } else if (result.kind == PatternResultKind::Invalid) {
        result.type = PatternType::Invalid;
        result.valid = false;
        result.rejectReason = PatternRejectReason::UnexpectedTiming;
    }
    return result;
}

} // namespace detection

