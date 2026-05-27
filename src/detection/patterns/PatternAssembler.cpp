#include "PatternAssembler.h"

// PatternAssembler converts inspected signals into queued PatternCandidates.
namespace {

using PatternCandidate = detection::PatternCandidate;
using PatternCandidateKind = detection::PatternCandidateKind;

PatternCandidate makePatternCandidateFromSignal(const detection::InspectedOccurrence& occurrence) {
    PatternCandidate candidate = {};
    const detection::Occurrence& source = occurrence.occurrence;
    candidate.transient = {};
    candidate.valid = source.valid;
    candidate.kind = PatternCandidateKind::SinglePulse;
    candidate.lineageId = static_cast<uint32_t>(source.startSample & 0xFFFFFFFFu);
    candidate.primarySlotIndex = 0;
    candidate.occurrenceCount = 1;
    candidate.pulseCount = 1;
    candidate.occurrenceSlotCount = 1;
    switch (source.kind) {
        case detection::OccurrenceKind::FrequencyMatch:
            candidate.onsetSample = source.startSample;
            candidate.peakSample = source.peakSample;
            candidate.releaseSample = source.releaseSample;
            candidate.startMs = source.startMs;
            candidate.heardAtMs = source.releaseMs != 0 ? source.releaseMs : source.peakMs;
            candidate.acceptedMs = candidate.heardAtMs;
            candidate.durationMs = source.durationMs;
            candidate.onsetStrength = source.contrast;
            candidate.peakStrength = source.score;
            candidate.releaseStrength = source.contrast;
            candidate.ambientBaseline = 0.0f;
            candidate.ampStrength = source.ampStrength;
            candidate.ampStrengthEvidence = source.ampStrengthEvidence;
            candidate.frequencyScoreStrength = source.frequencyScoreStrength;
            candidate.frequencyContrastQuality = source.frequencyContrastQuality;
            candidate.targetBandStrength = source.targetBandStrength;
            candidate.duplicateRisk = source.duplicateRisk;
            candidate.duplicateRiskScore = source.duplicateRiskScore;
            candidate.firstPulseMs = candidate.acceptedMs;
            candidate.lastPulseMs = candidate.acceptedMs;
            candidate.occurrenceSlots[0].kindTag = static_cast<uint8_t>(source.kind);
            candidate.occurrenceSlots[0].sourceTag = static_cast<uint8_t>(source.source);
            candidate.occurrenceSlots[0].onsetSample = source.startSample;
            candidate.occurrenceSlots[0].peakSample = source.peakSample;
            candidate.occurrenceSlots[0].releaseSample = source.releaseSample;
            candidate.occurrenceSlots[0].startMs = source.startMs;
            candidate.occurrenceSlots[0].peakMs = source.peakMs;
            candidate.occurrenceSlots[0].releaseMs = source.releaseMs;
            candidate.occurrenceSlots[0].strength = source.score;
            // Frequency score/contrast are mapped into the candidate strength fields used by reporting.
            candidate.frequency = source.frequency;
            candidate.frequencyFull = source.frequency;
            candidate.transient.present = false;
            break;

        case detection::OccurrenceKind::AmpTransient:
            candidate.onsetSample = source.startSample;
            candidate.peakSample = source.peakSample;
            candidate.releaseSample = source.releaseSample;
            candidate.startMs = source.startMs;
            candidate.heardAtMs = source.releaseMs != 0 ? source.releaseMs : source.startMs;
            candidate.acceptedMs = candidate.heardAtMs;
            candidate.durationMs = source.durationMs;
            candidate.onsetStrength = source.transient.onsetStrength;
            candidate.peakStrength = source.strength;
            candidate.releaseStrength = source.transient.releaseStrength;
            candidate.ambientBaseline = source.transient.ambientBaseline;
            candidate.ampStrength = source.ampStrength;
            candidate.ampStrengthEvidence = source.ampStrengthEvidence;
            candidate.frequencyScoreStrength = source.frequencyScoreStrength;
            candidate.frequencyContrastQuality = source.frequencyContrastQuality;
            candidate.targetBandStrength = source.targetBandStrength;
            candidate.duplicateRisk = source.duplicateRisk;
            candidate.duplicateRiskScore = source.duplicateRiskScore;
            candidate.firstPulseMs = candidate.acceptedMs;
            candidate.lastPulseMs = candidate.acceptedMs;
            candidate.occurrenceSlots[0].kindTag = static_cast<uint8_t>(source.kind);
            candidate.occurrenceSlots[0].sourceTag = static_cast<uint8_t>(source.source);
            candidate.occurrenceSlots[0].onsetSample = source.startSample;
            candidate.occurrenceSlots[0].peakSample = source.peakSample;
            candidate.occurrenceSlots[0].releaseSample = source.releaseSample;
            candidate.occurrenceSlots[0].startMs = source.startMs;
            candidate.occurrenceSlots[0].peakMs = source.peakMs;
            candidate.occurrenceSlots[0].releaseMs = source.releaseMs;
            candidate.occurrenceSlots[0].strength = source.strength;
            candidate.transient = source.transient;
            candidate.frequency = source.frequency;
            candidate.frequencyFull = source.frequency;
            break;

        case detection::OccurrenceKind::None:
        default:
            candidate.kind = PatternCandidateKind::Unknown;
            break;
    }

    return candidate;
}

} // namespace

namespace detection {

void PatternAssembler::reset() {
    _readIndex = 0;
    _count = 0;
    _recentSignalReadIndex = 0;
    _recentOccurrenceCount = 0;
}

void PatternAssembler::acceptSignal(const InspectedOccurrence& occurrence) {
    acceptSignals(&occurrence, 1);
}

size_t PatternAssembler::acceptSignals(const InspectedOccurrence* signals, size_t count) {
    if (signals == nullptr || count == 0) {
        return 0;
    }

    size_t accepted = 0;
    for (size_t i = 0; i < count; ++i) {
        const InspectedOccurrence& occurrence = signals[i];
        pushRecentSignal(occurrence);

        if (!occurrence.accepted || !occurrence.occurrence.present) {
            continue;
        }

        switch (occurrence.occurrence.kind) {
            case OccurrenceKind::AmpTransient:
            case OccurrenceKind::FrequencyMatch:
                if (occurrence.occurrence.valid) {
                    const PatternCandidate candidate = makePatternCandidateFromSignal(occurrence);
                    if (candidate.transient.present || candidate.frequency.present || candidate.durationMs > 0 || candidate.peakStrength != 0.0f || candidate.onsetStrength != 0.0f) {
                        if (pushPatternCandidate(candidate)) {
                            ++accepted;
                        }
                    }

                }
                break;

            case OccurrenceKind::None:
            default:
                // Unsupported kinds are ignored in this pass.
                break;
        }
    }

    return accepted;
}

size_t PatternAssembler::assemble(const InspectedOccurrence* signals, size_t occurrenceCount, PatternCandidate* out, size_t maxOut) {
    if (signals == nullptr || occurrenceCount == 0 || out == nullptr || maxOut == 0) {
        return 0;
    }

    const size_t accepted = acceptSignals(signals, occurrenceCount);
    size_t written = 0;
    while (written < maxOut && popPatternCandidate(out[written])) {
        ++written;
    }

    (void)accepted;
    return written;
}

bool PatternAssembler::popPatternCandidate(PatternCandidate& out) {
    if (_count == 0) {
        return false;
    }

    out = _queue[_readIndex];
    _readIndex = (_readIndex + 1) % kQueueCapacity;
    --_count;
    return true;
}

size_t PatternAssembler::popPatternCandidates(PatternCandidate* out, size_t maxOut) {
    if (out == nullptr || maxOut == 0) {
        return 0;
    }

    size_t written = 0;
    while (written < maxOut && popPatternCandidate(out[written])) {
        ++written;
    }
    return written;
}

void PatternAssembler::pushRecentSignal(const InspectedOccurrence& occurrence) {
    const size_t writeIndex = (_recentSignalReadIndex + _recentOccurrenceCount) % kRecentSignalCapacity;
    _recentSignals[writeIndex] = occurrence;
    if (_recentOccurrenceCount < kRecentSignalCapacity) {
        ++_recentOccurrenceCount;
    } else {
        _recentSignalReadIndex = (_recentSignalReadIndex + 1) % kRecentSignalCapacity;
    }
}

bool PatternAssembler::pushPatternCandidate(const PatternCandidate& candidate) {
    if (_count == kQueueCapacity) {
        return false;
    }

    const size_t writeIndex = (_readIndex + _count) % kQueueCapacity;
    _queue[writeIndex] = candidate;
    ++_count;
    return true;
}

} // namespace detection

