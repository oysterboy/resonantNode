#include "PatternAssembler.h"

// PatternAssembler converts inspected occurrences into queued PatternCandidates.
namespace {

using PatternCandidate = detection::PatternCandidate;
using PatternCandidateKind = detection::PatternCandidateKind;

detection::TransientEvidence scalarTransientEvidenceFromOccurrence(const detection::Occurrence& source) {
    detection::TransientEvidence transient = {};
    transient.present = source.scalar.present;
    transient.onsetSample = source.startSample;
    transient.peakSample = source.peakSample;
    transient.releaseSample = source.releaseSample;
    transient.startMs = source.startMs;
    transient.heardAtMs = source.releaseMs != 0 ? source.releaseMs : source.peakMs;
    transient.acceptedMs = transient.heardAtMs;
    transient.durationMs = source.durationMs;
    transient.onsetStrength = source.scalar.onsetStrength;
    transient.peakStrength = source.scalar.peakStrength;
    transient.releaseStrength = source.scalar.releaseStrength;
    transient.ambientBaseline = source.scalar.baseline;
    transient.audioOverflowDuringCandidate = source.scalar.audioOverflowDuringCandidate;
    return transient;
}

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
    switch (source.occurrenceType) {
        case detection::OccurrenceType::Frequency:
            candidate.onsetSample = source.startSample;
            candidate.peakSample = source.peakSample;
            candidate.releaseSample = source.releaseSample;
            candidate.startMs = source.startMs;
            candidate.heardAtMs = source.releaseMs != 0 ? source.releaseMs : source.peakMs;
            candidate.acceptedMs = candidate.heardAtMs;
            candidate.durationMs = source.durationMs;
            candidate.onsetStrength = source.frequency.contrast;
            candidate.peakStrength = source.frequency.score;
            candidate.releaseStrength = source.frequency.contrast;
            candidate.ambientBaseline = 0.0f;
            candidate.ampStrength = source.scalar.strengthClass;
            candidate.scalarEvidence = source.scalar.evidence;
            candidate.frequencyScoreStrength = source.frequency.scoreStrength;
            candidate.frequencyContrastQuality = source.frequency.contrastQuality;
            candidate.targetBandStrength = source.frequency.targetBandStrength;
            candidate.firstPulseMs = candidate.acceptedMs;
            candidate.lastPulseMs = candidate.acceptedMs;
            candidate.occurrenceSlots[0].kindTag = static_cast<uint8_t>(source.occurrenceType);
            candidate.occurrenceSlots[0].sourceTag = static_cast<uint8_t>(source.detectorId);
            candidate.occurrenceSlots[0].onsetSample = source.startSample;
            candidate.occurrenceSlots[0].peakSample = source.peakSample;
            candidate.occurrenceSlots[0].releaseSample = source.releaseSample;
            candidate.occurrenceSlots[0].startMs = source.startMs;
            candidate.occurrenceSlots[0].peakMs = source.peakMs;
            candidate.occurrenceSlots[0].releaseMs = source.releaseMs;
            candidate.occurrenceSlots[0].strength = source.frequency.score;
            // Frequency score/contrast are mapped into the candidate strength fields used by reporting.
            candidate.frequency = source.frequency.measurement;
            candidate.frequencyFull = source.frequency.measurement;
            candidate.transient.present = false;
            break;

        case detection::OccurrenceType::Scalar:
            candidate.onsetSample = source.startSample;
            candidate.peakSample = source.peakSample;
            candidate.releaseSample = source.releaseSample;
            candidate.startMs = source.startMs;
            candidate.heardAtMs = source.releaseMs != 0 ? source.releaseMs : source.startMs;
            candidate.acceptedMs = candidate.heardAtMs;
            candidate.durationMs = source.durationMs;
            candidate.onsetStrength = source.scalar.onsetStrength;
            candidate.peakStrength = source.strength;
            candidate.releaseStrength = source.scalar.releaseStrength;
            candidate.ambientBaseline = source.scalar.baseline;
            candidate.ampStrength = source.scalar.strengthClass;
            candidate.scalarEvidence = source.scalar.evidence;
            candidate.frequencyScoreStrength = source.frequency.scoreStrength;
            candidate.frequencyContrastQuality = source.frequency.contrastQuality;
            candidate.targetBandStrength = source.frequency.targetBandStrength;
            candidate.firstPulseMs = candidate.acceptedMs;
            candidate.lastPulseMs = candidate.acceptedMs;
            candidate.occurrenceSlots[0].kindTag = static_cast<uint8_t>(source.occurrenceType);
            candidate.occurrenceSlots[0].sourceTag = static_cast<uint8_t>(source.detectorId);
            candidate.occurrenceSlots[0].onsetSample = source.startSample;
            candidate.occurrenceSlots[0].peakSample = source.peakSample;
            candidate.occurrenceSlots[0].releaseSample = source.releaseSample;
            candidate.occurrenceSlots[0].startMs = source.startMs;
            candidate.occurrenceSlots[0].peakMs = source.peakMs;
            candidate.occurrenceSlots[0].releaseMs = source.releaseMs;
            candidate.occurrenceSlots[0].strength = source.strength;
            candidate.transient = scalarTransientEvidenceFromOccurrence(source);
            candidate.frequency = source.frequency.measurement;
            candidate.frequencyFull = source.frequency.measurement;
            break;

        case detection::OccurrenceType::None:
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
    _recentOccurrenceReadIndex = 0;
    _recentOccurrenceCount = 0;
}

void PatternAssembler::acceptOccurrence(const InspectedOccurrence& occurrence) {
    acceptOccurrences(&occurrence, 1);
}

size_t PatternAssembler::acceptOccurrences(const InspectedOccurrence* occurrences, size_t count) {
    if (occurrences == nullptr || count == 0) {
        return 0;
    }

    size_t accepted = 0;
    for (size_t i = 0; i < count; ++i) {
        const InspectedOccurrence& occurrence = occurrences[i];
        pushRecentOccurrence(occurrence);

        if (occurrence.decision != OccurrenceDecision::Accepted || !occurrence.occurrence.present) {
            continue;
        }

        switch (occurrence.occurrence.occurrenceType) {
            case OccurrenceType::Scalar:
            case OccurrenceType::Frequency:
                if (occurrence.occurrence.valid) {
                    const PatternCandidate candidate = makePatternCandidateFromSignal(occurrence);
                    if (candidate.transient.present || candidate.frequency.present || candidate.durationMs > 0 || candidate.peakStrength != 0.0f || candidate.onsetStrength != 0.0f) {
                        if (pushPatternCandidate(candidate)) {
                            ++accepted;
                        }
                    }

                }
                break;

            case OccurrenceType::None:
            default:
                // Unsupported kinds are ignored in this pass.
                break;
        }
    }

    return accepted;
}

size_t PatternAssembler::assemble(const InspectedOccurrence* occurrences, size_t occurrenceCount, PatternCandidate* out, size_t maxOut) {
    if (occurrences == nullptr || occurrenceCount == 0 || out == nullptr || maxOut == 0) {
        return 0;
    }

    const size_t accepted = acceptOccurrences(occurrences, occurrenceCount);
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

void PatternAssembler::pushRecentOccurrence(const InspectedOccurrence& occurrence) {
    const size_t writeIndex = (_recentOccurrenceReadIndex + _recentOccurrenceCount) % kRecentOccurrenceCapacity;
    _recentOccurrences[writeIndex] = occurrence;
    if (_recentOccurrenceCount < kRecentOccurrenceCapacity) {
        ++_recentOccurrenceCount;
    } else {
        _recentOccurrenceReadIndex = (_recentOccurrenceReadIndex + 1) % kRecentOccurrenceCapacity;
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

