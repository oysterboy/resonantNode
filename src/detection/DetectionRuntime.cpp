#include "DetectionRuntime.h"

namespace detection {

DetectionRuntime::DetectionRuntime() = default;

void DetectionRuntime::reset() {
    _ampEmitter.reset();
    _frequencyEmitter.reset();
    _signalInspector.reset();
    _patternAssembler.reset();
    _fieldStateTracker.reset();
    _featureHistory.reset();
    _signalEmitterKind = ProfileSignalEmitterKind::Frequency;
    _inspectionRulesKind = ProfileInspectionRulesKind::FreqAmp;
    _signalInspector.configure(_inspectionConfig);
    _signalInspector.setInspectionRules(_inspectionRulesKind);
    _profileName = "unknown";
    _resultQueue[0] = {};
    _resultReadIndex = 0;
    _resultCount = 0;
    _latestPipelineResult = {};
    _hasLatestPipelineResult = false;
    _lastSignalCandidate = {};
    _lastInspectedSignal = {};
}

void DetectionRuntime::setFrequencyTuning(const FrequencyMatchEvaluation::Values& tuning) {
    _frequencyTuning = tuning;
}

void DetectionRuntime::setSignalEmitter(ProfileSignalEmitterKind kind) {
    _signalEmitterKind = kind;
    _frequencyEmitter.reset();
    _ampEmitter.reset();
}

void DetectionRuntime::setInspectionRules(ProfileInspectionRulesKind kind) {
    _inspectionRulesKind = kind;
    _signalInspector.setInspectionRules(_inspectionRulesKind);
}

void DetectionRuntime::setInspectionConfig(const InspectionConfig& config) {
    _inspectionConfig = config;
    _signalInspector.configure(_inspectionConfig);
}

void DetectionRuntime::setRequireSupportForAcceptance(bool value) {
    _patternRules.setRequireSupportForAcceptance(value);
}

void DetectionRuntime::setFieldStateConfig(const FieldStateConfig& config) {
    _fieldStateTracker.setConfig(config);
}

void DetectionRuntime::setProfileName(const char* profileName) {
    _profileName = profileName != nullptr ? profileName : "unknown";
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

    FeatureExtractor::observeFrame(frame, _featureHistory);
    FeatureExtractor::observeFrequencyEvidence(frequencyEvidence, nowMs, _featureHistory);

    switch (_signalEmitterKind) {
        case ProfileSignalEmitterKind::Frequency:
            _frequencyEmitter.observeFrame(frame, frequencyEvidence, _frequencyTuning);
            break;
        case ProfileSignalEmitterKind::Amp:
            _ampEmitter.observeFrame(frame);
            break;
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

bool DetectionRuntime::hasLatestPipelineResult() const {
    return _hasLatestPipelineResult;
}

const DetectionPipelineResult& DetectionRuntime::latestPipelineResult() const {
    return _latestPipelineResult;
}

const FrequencySignalEmitter& DetectionRuntime::frequencyEmitter() const {
    return _frequencyEmitter;
}

const FieldState& DetectionRuntime::fieldState() const {
    return _fieldStateTracker.state();
}

const FeatureHistory& DetectionRuntime::featureHistory() const {
    return _featureHistory;
}

void DetectionRuntime::drainSignalEmitters(unsigned long nowMs) {
    SignalCandidate candidate;

    switch (_signalEmitterKind) {
        case ProfileSignalEmitterKind::Frequency:
            while (_frequencyEmitter.popSignalCandidate(candidate)) {
                _fieldStateTracker.observeSignalCandidate(candidate, nowMs);
                const InspectedSignal inspected = _signalInspector.inspectWithHistory(candidate, &_featureHistory);
                _fieldStateTracker.observeInspectedSignal(inspected, nowMs);
                _lastSignalCandidate = candidate;
                _lastInspectedSignal = inspected;
                _patternAssembler.acceptSignal(inspected);
            }
            break;
        case ProfileSignalEmitterKind::Amp:
            while (_ampEmitter.popSignalCandidate(candidate)) {
                _fieldStateTracker.observeSignalCandidate(candidate, nowMs);
                const InspectedSignal inspected = _signalInspector.inspectWithHistory(candidate, &_featureHistory);
                _fieldStateTracker.observeInspectedSignal(inspected, nowMs);
                _lastSignalCandidate = candidate;
                _lastInspectedSignal = inspected;
                _patternAssembler.acceptSignal(inspected);
            }
            break;
    }

    (void)nowMs;
}

void DetectionRuntime::drainPatternAssembler(unsigned long nowMs) {
    PatternCandidate candidate;
    while (_patternAssembler.popPatternCandidate(candidate)) {
        const PatternResult result = _patternRules.evaluate(candidate, nowMs);
        _fieldStateTracker.observePatternResult(result, nowMs);
        capturePipelineResult(result, &_lastSignalCandidate, &_lastInspectedSignal, nowMs);
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

void DetectionRuntime::capturePipelineResult(
    const PatternResult& result,
    const SignalCandidate* signal,
    const InspectedSignal* inspectedSignal,
    unsigned long nowMs
) {
    _latestPipelineResult = {};
    _latestPipelineResult.hasPattern = true;
    _latestPipelineResult.pattern = result;
    _latestPipelineResult.hasSignal = signal != nullptr && signal->present;
    if (_latestPipelineResult.hasSignal && signal != nullptr) {
        _latestPipelineResult.signal = *signal;
    }
    _latestPipelineResult.hasInspectedSignal = inspectedSignal != nullptr && inspectedSignal->signal.present;
    if (_latestPipelineResult.hasInspectedSignal && inspectedSignal != nullptr) {
        _latestPipelineResult.inspectedSignal = *inspectedSignal;
    }
    _latestPipelineResult.hasField = true;
    _latestPipelineResult.field = _fieldStateTracker.state();
    _latestPipelineResult.profileName = _profileName;
    _latestPipelineResult.timestampMs = nowMs;
    _hasLatestPipelineResult = true;
}

} // namespace detection
