#include "DetectionRuntime.h"

// DetectionRuntime pipeline execution in source order.
namespace detection {

DetectionRuntime::DetectionRuntime() = default;

namespace {

OccurrenceSource occurrenceSourceForStream(FeatureStreamId stream) {
    switch (stream) {
        case FeatureStreamId::FrequencyScore:
        case FeatureStreamId::FrequencyContrast:
            return OccurrenceSource::Frequency;
        case FeatureStreamId::AmpEnvelope:
        case FeatureStreamId::AmbientFloor:
        case FeatureStreamId::Unknown:
        default:
            return OccurrenceSource::Amp;
    }
}

float selectedScalarValue(const AudioSignalFrame& frame, FeatureStreamId stream, const FeatureHistory& history) {
    switch (stream) {
        case FeatureStreamId::AmpEnvelope:
            return frame.centeredMagnitude;
        case FeatureStreamId::AmbientFloor:
            return frame.baseline;
        case FeatureStreamId::FrequencyScore:
        case FeatureStreamId::FrequencyContrast:
            return history.latestValue(stream);
        case FeatureStreamId::Unknown:
        default:
            return static_cast<float>(frame.level);
    }
}

const char* occurrenceSourceName(OccurrenceSourceKind kind) {
    switch (kind) {
        case OccurrenceSourceKind::FrequencyMatch:
            return "frequency_match";
        case OccurrenceSourceKind::ScalarTransient:
            return "scalar_transient";
        default:
            return "unknown";
    }
}

} // namespace

void DetectionRuntime::resetState() {
    _frequencyEmitter.reset();
    _scalarEmitter.reset();
    _occurrenceInspector.reset();
    _patternAssembler.reset();
    _fieldStateTracker.reset();
    _featureHistory.reset();
    _resultQueue[0] = {};
    _resultReadIndex = 0;
    _resultCount = 0;
    _latestPipelineResult = {};
    _hasLatestPipelineResult = false;
    resetDiagnostics();
    _lastOccurrence = {};
    _lastInspectedOccurrence = {};
}

void DetectionRuntime::resetDiagnostics() {
    _diagnostics = {};
}

void DetectionRuntime::setFrequencyMatchConfig(const FrequencyMatchConfig& config) {
    _frequencyMatchConfig = config;
    _frequencyEmitter.setConfig(_frequencyMatchConfig);
}

void DetectionRuntime::setScalarTransientConfig(const ScalarTransientConfig& config) {
    _scalarTransientConfig = config;
    _scalarEmitter.setConfig(_scalarTransientConfig);
}

void DetectionRuntime::setOccurrenceSource(OccurrenceSourceKind kind) {
    _occurrenceSourceKind = kind;
    _frequencyEmitter.reset();
    _scalarEmitter.reset();
    _frequencyEmitter.setConfig(_frequencyMatchConfig);
    _scalarEmitter.setConfig(_scalarTransientConfig);
}

void DetectionRuntime::setInspectionPlan(const InspectionPlan& plan) {
    _inspectionPlan = plan;
    _occurrenceInspector.configure(_inspectionPlan);
}

void DetectionRuntime::setPatternRulesConfig(const PatternRulesConfig& config) {
    _patternRulesConfig = config;
    _patternRules.configure(_patternRulesConfig);
}

void DetectionRuntime::setFieldStateConfig(const FieldStateConfig& config) {
    _fieldStateTracker.setConfig(config);
}

void DetectionRuntime::setProfileName(const char* profileName) {
    _profileName = profileName != nullptr ? profileName : "unknown";
}

void DetectionRuntime::observeFrame(
    const AudioSignalFrame& frame,
    const FrequencyFeatureFrame& frequencyEvidence,
    unsigned long nowMs
) {
    _fieldStateTracker.update(nowMs);
    if (!frame.valid) {
        return;
    }

    FeatureExtractor::observeFrame(frame, _featureHistory);
    FeatureExtractor::observeFrequencyFeatureFrame(frequencyEvidence, nowMs, _featureHistory);

    switch (_occurrenceSourceKind) {
        case OccurrenceSourceKind::FrequencyMatch:
            _frequencyEmitter.observeFrame(frame, frequencyEvidence);
            break;
        case OccurrenceSourceKind::ScalarTransient:
            _scalarEmitter.observeFrame(
                frame,
                selectedScalarValue(frame, _scalarTransientConfig.observedStream, _featureHistory),
                OccurrenceKind::AmpTransient,
                occurrenceSourceForStream(_scalarTransientConfig.observedStream)
            );
            break;
    }

    updateDiagnostics(frame, frequencyEvidence, nowMs);
    drainOccurrenceSources(nowMs);
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

const DetectionDiagnostics& DetectionRuntime::diagnostics() const {
    return _diagnostics;
}

const FrequencyOccurrenceSource& DetectionRuntime::frequencyEmitter() const {
    return _frequencyEmitter;
}

const FieldState& DetectionRuntime::fieldState() const {
    return _fieldStateTracker.state();
}

const FeatureHistory& DetectionRuntime::featureHistory() const {
    return _featureHistory;
}

void DetectionRuntime::drainOccurrenceSources(unsigned long nowMs) {
    Occurrence candidate;

    switch (_occurrenceSourceKind) {
        case OccurrenceSourceKind::FrequencyMatch:
            while (_frequencyEmitter.popOccurrence(candidate)) {
                _fieldStateTracker.observeOccurrence(candidate, nowMs);
                const InspectedOccurrence inspected = _occurrenceInspector.inspectWithHistory(candidate, &_featureHistory);
                _fieldStateTracker.observeInspectedOccurrence(inspected, nowMs);
                _lastOccurrence = candidate;
                _lastInspectedOccurrence = inspected;
                _patternAssembler.acceptOccurrence(inspected);
            }
            break;
        case OccurrenceSourceKind::ScalarTransient:
            while (_scalarEmitter.popOccurrence(candidate)) {
                _fieldStateTracker.observeOccurrence(candidate, nowMs);
                const InspectedOccurrence inspected = _occurrenceInspector.inspectWithHistory(candidate, &_featureHistory);
                _fieldStateTracker.observeInspectedOccurrence(inspected, nowMs);
                _lastOccurrence = candidate;
                _lastInspectedOccurrence = inspected;
                _patternAssembler.acceptOccurrence(inspected);
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
        capturePipelineResult(result, &_lastOccurrence, &_lastInspectedOccurrence, nowMs);
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
    const Occurrence* occurrence,
    const InspectedOccurrence* inspectedOccurrence,
    unsigned long nowMs
) {
    _latestPipelineResult = {};
    _latestPipelineResult.hasPattern = true;
    _latestPipelineResult.pattern = result;
    _latestPipelineResult.hasOccurrence = occurrence != nullptr && occurrence->present;
    if (_latestPipelineResult.hasOccurrence && occurrence != nullptr) {
        _latestPipelineResult.occurrence = *occurrence;
    }
    _latestPipelineResult.hasInspectedOccurrence = inspectedOccurrence != nullptr && inspectedOccurrence->occurrence.present;
    if (_latestPipelineResult.hasInspectedOccurrence && inspectedOccurrence != nullptr) {
        _latestPipelineResult.inspectedOccurrence = *inspectedOccurrence;
    }
    _latestPipelineResult.hasField = true;
    _latestPipelineResult.field = _fieldStateTracker.state();
    _latestPipelineResult.profileName = _profileName;
    _latestPipelineResult.timestampMs = nowMs;
    _hasLatestPipelineResult = true;
}

void DetectionRuntime::updateDiagnostics(
    const AudioSignalFrame& frame,
    const FrequencyFeatureFrame& frequencyEvidence,
    unsigned long nowMs
) {
    _diagnostics.present = frame.valid;
    _diagnostics.observedAtMs = nowMs;
    _diagnostics.occurrenceSource = occurrenceSourceName(_occurrenceSourceKind);
    _diagnostics.ampCenteredMagnitude = frame.centeredMagnitude;
    _diagnostics.ampLevel = static_cast<float>(frame.level);
    _diagnostics.ampBaseline = frame.baseline;
    _diagnostics.ampLift = frame.centeredMagnitude - frame.baseline;

    _diagnostics.scalarOnsetRejectReason = _scalarEmitter.lastOnsetRejectReasonName();
    _diagnostics.scalarTransientRejectReason = _scalarEmitter.lastTransientRejectReasonName();
    _diagnostics.scalarTransientRejectedDurationMs = _scalarEmitter.lastTransientRejectedDurationMs();
    _diagnostics.scalarTransientRejectedStrength = _scalarEmitter.lastTransientRejectedStrength();

    FrequencyMatchEvaluation::Values tuning = {};
    tuning.scoreMin = _frequencyMatchConfig.scoreMin;
    tuning.contrastMin = _frequencyMatchConfig.contrastMin;
    const auto frequencyEval = FrequencyMatchEvaluation::evaluate(frequencyEvidence, tuning);
    const auto& detector = _frequencyEmitter.detector();
    _diagnostics.detectorKind = _occurrenceSourceKind == OccurrenceSourceKind::FrequencyMatch
        ? "frequency_match"
        : "scalar_transient";
    _diagnostics.frequencyPresent = frequencyEval.present;
    _diagnostics.frequencyValidWindow = frequencyEval.validWindow;
    _diagnostics.frequencyMatched = frequencyEval.matched;
    _diagnostics.frequencyScoreOk = frequencyEval.scoreOk;
    _diagnostics.frequencyContrastOk = frequencyEval.contrastOk;
    _diagnostics.frequencyScore = frequencyEval.score;
    _diagnostics.frequencyContrast = frequencyEval.contrast;
    _diagnostics.frequencyScoreMin = frequencyEval.scoreMin;
    _diagnostics.frequencyContrastMin = frequencyEval.contrastMin;
    _diagnostics.frequencyReason = FrequencyMatchEvaluation::reasonName(frequencyEval.reason);
    _diagnostics.frequencySuppressReason = detector.suppressReason;
    _diagnostics.frequencyWouldCandidateReason = detector.wouldCandidateReason;
    _diagnostics.frequencyCandidateState = detector.candidateState;
    _diagnostics.frequencyReadyOk = detector.readyOk;
    _diagnostics.frequencyGateOpen = detector.gateOpen;
}

} // namespace detection


