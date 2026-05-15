#include "DetectionRuntime.h"

namespace detection {

DetectionRuntime::DetectionRuntime() = default;

void DetectionRuntime::reset() {
    _ampEmitter.reset();
    _frequencyEmitter.reset();
    _signalInspector.reset();
    _patternAssembler.reset();
    _fieldStateTracker.reset();
    _ampEnabled = true;
    _resultQueue[0] = {};
    _resultReadIndex = 0;
    _resultCount = 0;
}

void DetectionRuntime::setFrequencyTuning(const FrequencyEvidenceEvaluation::Values& tuning) {
    _frequencyTuning = tuning;
}

void DetectionRuntime::setAmpEnabled(bool enabled) {
    _ampEnabled = enabled;
    if (!_ampEnabled) {
        _ampEmitter.reset();
    }
}

void DetectionRuntime::observeFrame(
    const AudioSignalFrame& frame,
    const FrequencyEvidence& frequencyEvidence,
    unsigned long nowMs
) {
    _fieldStateTracker.update(nowMs);
    if (!frame.valid) {
        return;
    }

    _frequencyEmitter.observeFrame(frame, frequencyEvidence, _frequencyTuning);
    if (_ampEnabled) {
        _ampEmitter.observeFrame(frame);
    }

    drainSignalEmitters(nowMs);
    drainPatternAssembler(nowMs);
}

bool DetectionRuntime::popPatternResult(PatternResult& out) {
    if (_resultCount == 0) {
        return false;
    }

    out = _resultQueue[_resultReadIndex];
    _resultReadIndex = (_resultReadIndex + 1) % kResultQueueCapacity;
    --_resultCount;
    return true;
}

const FrequencySignalEmitter& DetectionRuntime::frequencyEmitter() const {
    return _frequencyEmitter;
}

const FieldState& DetectionRuntime::fieldState() const {
    return _fieldStateTracker.state();
}

void DetectionRuntime::drainSignalEmitters(unsigned long nowMs) {
    SignalCandidate candidate;

    while (_frequencyEmitter.popSignalCandidate(candidate)) {
        _fieldStateTracker.observeSignalCandidate(candidate, nowMs);
        const InspectedSignal inspected = _signalInspector.inspect(candidate, _frequencyTuning);
        _fieldStateTracker.observeInspectedSignal(inspected, nowMs);
        _patternAssembler.acceptSignal(inspected);
    }

    while (_ampEmitter.popSignalCandidate(candidate)) {
        _fieldStateTracker.observeSignalCandidate(candidate, nowMs);
        const InspectedSignal inspected = _signalInspector.inspect(candidate, _frequencyTuning);
        _fieldStateTracker.observeInspectedSignal(inspected, nowMs);
        _patternAssembler.acceptSignal(inspected);
    }

    (void)nowMs;
}

void DetectionRuntime::drainPatternAssembler(unsigned long nowMs) {
    PatternCandidate candidate;
    while (_patternAssembler.popPatternCandidate(candidate)) {
        const PatternResult result = _patternRules.evaluate(candidate, nowMs, _frequencyTuning);
        _fieldStateTracker.observePatternResult(result, nowMs);
        pushPatternResult(result);
    }
}

bool DetectionRuntime::pushPatternResult(const PatternResult& result) {
    if (_resultCount == kResultQueueCapacity) {
        return false;
    }

    const size_t writeIndex = (_resultReadIndex + _resultCount) % kResultQueueCapacity;
    _resultQueue[writeIndex] = result;
    ++_resultCount;
    return true;
}

} // namespace detection
