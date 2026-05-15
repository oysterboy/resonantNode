#include "PatternAssembler.h"

namespace {

DetectionPipeline::PatternCandidate makePatternCandidateFromSignal(const detection::InspectedSignal& signal) {
    DetectionPipeline::PatternCandidate candidate = {};
    candidate.transient = {};

    const detection::SignalCandidate& source = signal.signal;
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
            candidate.transient = source.transient;
            candidate.frequency = source.frequency;
            candidate.frequencyFull = source.frequency;
            break;

        case detection::SignalKind::None:
        default:
            break;
    }

    return candidate;
}

} // namespace

namespace detection {

void PatternAssembler::reset() {
    _queue[0] = {};
    _readIndex = 0;
    _count = 0;
}

void PatternAssembler::acceptSignal(const InspectedSignal& signal) {
    if (!signal.accepted) {
        return;
    }

    if (!signal.signal.present) {
        return;
    }

    switch (signal.signal.kind) {
        case SignalKind::AmpTransient:
        case SignalKind::FrequencyMatch:
            if (signal.signal.valid) {
                const PatternCandidate candidate = makePatternCandidateFromSignal(signal);
                if (candidate.transient.present || candidate.frequency.present || candidate.durationMs > 0 || candidate.peakStrength != 0.0f || candidate.onsetStrength != 0.0f) {
                    pushPatternCandidate(candidate);
                }
            }
            break;

        case SignalKind::None:
        default:
            // Unsupported kinds are ignored in this pass.
            break;
    }
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
