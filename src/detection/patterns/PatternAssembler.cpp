#include "PatternAssembler.h"

namespace {

constexpr unsigned long kSequenceGapMs = 250UL;

using PatternCandidate = detection::PatternCandidate;
using PatternCandidateKind = detection::PatternCandidateKind;

PatternCandidate makePatternCandidateFromSignal(const detection::InspectedSignal& signal) {
    PatternCandidate candidate = {};
    candidate.transient = {};

    const detection::SignalCandidate& source = signal.signal;
    candidate.kind = PatternCandidateKind::SinglePulse;
    candidate.lineageId = static_cast<uint32_t>(source.startSample & 0xFFFFFFFFu);
    candidate.primarySlotIndex = 0;
    candidate.signalCount = 1;
    candidate.pulseCount = 1;
    candidate.signalSlotCount = 1;
    switch (source.kind) {
        case detection::SignalKind::FrequencyMatch:
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
            candidate.signalConfidence = source.signalConfidence;
            candidate.frequencyConfidence = source.frequencyConfidence;
            candidate.ampSupport = source.ampSupport;
            candidate.locality = source.locality;
            candidate.ampWindow = source.ampWindow;
            candidate.duplicateRisk = source.duplicateRisk;
            candidate.duplicateRiskScore = source.duplicateRiskScore;
            candidate.firstPulseMs = candidate.acceptedMs;
            candidate.lastPulseMs = candidate.acceptedMs;
            candidate.signalSlots[0].kindTag = static_cast<uint8_t>(source.kind);
            candidate.signalSlots[0].sourceTag = static_cast<uint8_t>(source.source);
            candidate.signalSlots[0].onsetSample = source.startSample;
            candidate.signalSlots[0].peakSample = source.peakSample;
            candidate.signalSlots[0].releaseSample = source.releaseSample;
            candidate.signalSlots[0].startMs = source.startMs;
            candidate.signalSlots[0].peakMs = source.peakMs;
            candidate.signalSlots[0].releaseMs = source.releaseMs;
            candidate.signalSlots[0].strength = source.score;
            candidate.signalSlots[0].confidence = source.frequencyConfidence;
            // Frequency score/contrast are temporarily mapped into legacy strength fields for compatibility.
            candidate.frequency = source.frequency;
            candidate.frequencyFull = source.frequency;
            candidate.transient.present = false;
            break;

        case detection::SignalKind::AmpTransient:
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
            candidate.signalConfidence = source.signalConfidence;
            candidate.frequencyConfidence = source.frequencyConfidence;
            candidate.ampSupport = source.ampSupport;
            candidate.locality = source.locality;
            candidate.ampWindow = source.ampWindow;
            candidate.duplicateRisk = source.duplicateRisk;
            candidate.duplicateRiskScore = source.duplicateRiskScore;
            candidate.firstPulseMs = candidate.acceptedMs;
            candidate.lastPulseMs = candidate.acceptedMs;
            candidate.signalSlots[0].kindTag = static_cast<uint8_t>(source.kind);
            candidate.signalSlots[0].sourceTag = static_cast<uint8_t>(source.source);
            candidate.signalSlots[0].onsetSample = source.startSample;
            candidate.signalSlots[0].peakSample = source.peakSample;
            candidate.signalSlots[0].releaseSample = source.releaseSample;
            candidate.signalSlots[0].startMs = source.startMs;
            candidate.signalSlots[0].peakMs = source.peakMs;
            candidate.signalSlots[0].releaseMs = source.releaseMs;
            candidate.signalSlots[0].strength = source.strength;
            candidate.signalSlots[0].confidence = source.signalConfidence;
            candidate.transient = source.transient;
            candidate.frequency = source.frequency;
            candidate.frequencyFull = source.frequency;
            break;

        case detection::SignalKind::None:
        default:
            candidate.kind = PatternCandidateKind::Unknown;
            break;
    }

    return candidate;
}

PatternCandidate makePulseSequenceCandidate(
    const detection::InspectedSignal& first,
    const detection::InspectedSignal& second
) {
    PatternCandidate candidate = {};
    candidate.kind = PatternCandidateKind::PulseSequence;
    candidate.lineageId = static_cast<uint32_t>(first.signal.startSample & 0xFFFFFFFFu) ^
                           (static_cast<uint32_t>(second.signal.startSample & 0xFFFFFFFFu) << 1);
    candidate.primarySlotIndex = 0;
    candidate.signalCount = 2;
    candidate.pulseCount = 2;
    candidate.signalSlotCount = 2;

    const detection::SignalCandidate* sources[2] = { &first.signal, &second.signal };
    candidate.firstPulseMs = sources[0]->releaseMs != 0 ? sources[0]->releaseMs : sources[0]->peakMs;
    candidate.lastPulseMs = sources[1]->releaseMs != 0 ? sources[1]->releaseMs : sources[1]->peakMs;
    candidate.minGapMs = candidate.lastPulseMs > candidate.firstPulseMs ? (candidate.lastPulseMs - candidate.firstPulseMs) : 0;
    candidate.maxGapMs = candidate.minGapMs;
    candidate.startMs = sources[0]->startMs;
    candidate.heardAtMs = candidate.lastPulseMs;
    candidate.acceptedMs = candidate.heardAtMs;
    candidate.durationMs = candidate.lastPulseMs > candidate.startMs ? (candidate.lastPulseMs - candidate.startMs) : 0;
    candidate.signalConfidence = (first.signalConfidence + second.signalConfidence) * 0.5f;
    candidate.frequencyConfidence = (first.frequencyConfidence + second.frequencyConfidence) * 0.5f;
    candidate.ampSupport = first.ampSupport;
    candidate.locality = first.locality;
    candidate.ampWindow = first.ampWindow;
    candidate.duplicateRisk = first.duplicateRisk || second.duplicateRisk;
    candidate.duplicateRiskScore = (first.duplicateRiskScore + second.duplicateRiskScore) * 0.5f;
    candidate.canOverlap = true;

    for (size_t i = 0; i < 2; ++i) {
        const detection::SignalCandidate& source = *sources[i];
        candidate.signalSlots[i].kindTag = static_cast<uint8_t>(source.kind);
        candidate.signalSlots[i].sourceTag = static_cast<uint8_t>(source.source);
        candidate.signalSlots[i].onsetSample = source.startSample;
        candidate.signalSlots[i].peakSample = source.peakSample;
        candidate.signalSlots[i].releaseSample = source.releaseSample;
        candidate.signalSlots[i].startMs = source.startMs;
        candidate.signalSlots[i].peakMs = source.peakMs;
        candidate.signalSlots[i].releaseMs = source.releaseMs;
        candidate.signalSlots[i].strength = source.kind == detection::SignalKind::FrequencyMatch ? source.score : source.strength;
        candidate.signalSlots[i].confidence = source.kind == detection::SignalKind::FrequencyMatch ? source.frequencyConfidence : source.signalConfidence;
    }

    candidate.onsetSample = sources[0]->startSample;
    candidate.peakSample = sources[1]->peakSample;
    candidate.releaseSample = sources[1]->releaseSample;
    candidate.onsetStrength = sources[0]->kind == detection::SignalKind::FrequencyMatch ? sources[0]->score : sources[0]->strength;
    candidate.peakStrength = sources[1]->kind == detection::SignalKind::FrequencyMatch ? sources[1]->score : sources[1]->strength;
    candidate.releaseStrength = candidate.peakStrength;
    candidate.ambientBaseline = sources[0]->kind == detection::SignalKind::AmpTransient ? sources[0]->ampBaseline : 0.0f;
    candidate.transient = first.signal.transient;
    candidate.frequency = first.signal.frequency;
    candidate.frequencyFull = second.signal.frequency;
    return candidate;
}

} // namespace

namespace detection {

void PatternAssembler::reset() {
    _readIndex = 0;
    _count = 0;
    _recentSignalReadIndex = 0;
    _recentSignalCount = 0;
}

void PatternAssembler::acceptSignal(const InspectedSignal& signal) {
    acceptSignals(&signal, 1);
}

size_t PatternAssembler::acceptSignals(const InspectedSignal* signals, size_t count) {
    if (signals == nullptr || count == 0) {
        return 0;
    }

    size_t accepted = 0;
    for (size_t i = 0; i < count; ++i) {
        const InspectedSignal& signal = signals[i];
        pushRecentSignal(signal);

        if (!signal.accepted || !signal.signal.present) {
            continue;
        }

        switch (signal.signal.kind) {
            case SignalKind::AmpTransient:
            case SignalKind::FrequencyMatch:
                if (signal.signal.valid) {
                    const PatternCandidate candidate = makePatternCandidateFromSignal(signal);
                    if (candidate.transient.present || candidate.frequency.present || candidate.durationMs > 0 || candidate.peakStrength != 0.0f || candidate.onsetStrength != 0.0f) {
                        if (pushPatternCandidate(candidate)) {
                            ++accepted;
                        }
                    }

                    if (signal.signal.kind == SignalKind::FrequencyMatch && _recentSignalCount >= 2) {
                        const size_t prevIndex = (_recentSignalReadIndex + _recentSignalCount - 2) % kRecentSignalCapacity;
                        const InspectedSignal& prev = _recentSignals[prevIndex];
                        if (prev.accepted && prev.signal.present && prev.signal.kind == SignalKind::FrequencyMatch && prev.signal.valid) {
                            const unsigned long prevMs = prev.signal.releaseMs != 0 ? prev.signal.releaseMs : prev.signal.peakMs;
                            const unsigned long currMs = signal.signal.releaseMs != 0 ? signal.signal.releaseMs : signal.signal.peakMs;
                            const unsigned long gapMs = currMs > prevMs ? (currMs - prevMs) : 0;
                            if (gapMs > 0 && gapMs <= kSequenceGapMs) {
                                PatternCandidate sequenceCandidate = makePulseSequenceCandidate(prev, signal);
                                sequenceCandidate.minGapMs = gapMs;
                                sequenceCandidate.maxGapMs = gapMs;
                                sequenceCandidate.signalCount = 2;
                                sequenceCandidate.pulseCount = 2;
                                if (pushPatternCandidate(sequenceCandidate)) {
                                    ++accepted;
                                }
                            }
                        }
                    }
                }
                break;

            case SignalKind::None:
            default:
                // Unsupported kinds are ignored in this pass.
                break;
        }
    }

    return accepted;
}

size_t PatternAssembler::assemble(const InspectedSignal* signals, size_t signalCount, PatternCandidate* out, size_t maxOut) {
    if (signals == nullptr || signalCount == 0 || out == nullptr || maxOut == 0) {
        return 0;
    }

    const size_t accepted = acceptSignals(signals, signalCount);
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

void PatternAssembler::pushRecentSignal(const InspectedSignal& signal) {
    const size_t writeIndex = (_recentSignalReadIndex + _recentSignalCount) % kRecentSignalCapacity;
    _recentSignals[writeIndex] = signal;
    if (_recentSignalCount < kRecentSignalCapacity) {
        ++_recentSignalCount;
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
