#include "PatternRules.h"

// PatternRules converts PatternCandidates into PatternResults.
namespace detection {

void PatternRules::configure(const PatternRulesConfig& config) {
    _config = config;
}

namespace {

PatternResultKind resultKindFromCandidate(const PatternCandidate& candidate) {
    if (candidate.kind == PatternCandidateKind::PulseSequence || candidate.occurrenceCount > 1 || candidate.pulseCount > 1) {
        if (candidate.maxGapMs > 0 && candidate.maxGapMs < 20UL) {
            return PatternResultKind::TooDense;
        }
        if (candidate.maxGapMs > 0 && candidate.maxGapMs > 250UL) {
            return PatternResultKind::InvalidChirp;
        }
        return PatternResultKind::ValidChirp;
    }

    return PatternResultKind::Pattern;
}

PatternRejectReason supportRejectReason(const PatternCandidate& candidate) {
    if (candidate.broadAmpStrength == StrengthClass::Unknown) {
        return PatternRejectReason::MissingSupport;
    }
    return PatternRejectReason::SupportTooLow;
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
    result.reasonCode = PatternReasonCode::DetectorRejected;
    result.rejectReason = PatternRejectReason::NoCandidate;
    result.confidence = 0.0f;
    result.signalConfidence = 0.0f;
    result.frequencyConfidence = 0.0f;
    result.broadAmpStrength = StrengthClass::Unknown;
    result.broadAmp = candidate.broadAmp;
    result.duplicateRisk = false;
    result.duplicateRiskScore = 0.0f;
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
    if (candidate.frequency.present && candidate.frequency.matched) {
        return evaluateFrequencyPattern(candidate, nowMs);
    }

    return makeInvalidResult(candidate, nowMs);
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
    result.reasonCode = PatternReasonCode::FromFrequencyMatch;
    result.rejectReason = PatternRejectReason::None;
    result.signalConfidence = candidate.signalConfidence > 0.0f ? candidate.signalConfidence : 1.0f;
    result.frequencyConfidence = candidate.frequencyConfidence;
    result.broadAmpStrength = candidate.broadAmpStrength;
    result.broadAmp = candidate.broadAmp;
    result.duplicateRisk = candidate.duplicateRisk;
    result.duplicateRiskScore = candidate.duplicateRiskScore;
    result.supportMatched = true;
    if (_config.requireSupportForAcceptance) {
        switch (_config.supportSource) {
            case PatternSupportSource::None:
                result.supportMatched = true;
                break;
            case PatternSupportSource::BroadAmp:
                result.supportMatched = candidate.broadAmpStrength >= _config.minimumSupport;
                break;
            case PatternSupportSource::TargetBand:
                result.supportMatched = false;
                break;
        }
    }
    if (!result.supportMatched) {
        result.kind = PatternResultKind::Rejected;
        result.rejectReason = _config.supportSource == PatternSupportSource::TargetBand
            ? PatternRejectReason::MissingSupport
            : supportRejectReason(candidate);
        result.reasonCode = PatternReasonCode::UnsupportedPattern;
    }
    result.valid = result.patternMatched && result.supportMatched;
    result.confidence = result.valid ? 1.0f : 0.0f;
    if (result.kind == PatternResultKind::TooDense) {
        result.type = PatternType::Ambiguous;
        result.valid = false;
        result.rejectReason = PatternRejectReason::UnexpectedTiming;
    } else if (result.kind == PatternResultKind::InvalidChirp) {
        result.type = PatternType::Invalid;
        result.valid = false;
        result.rejectReason = PatternRejectReason::UnexpectedTiming;
    }
    return result;
}

} // namespace detection

