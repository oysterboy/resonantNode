#include "DetectionRuntime.h"

#include <string.h>

// DetectionRuntime pipeline execution in source order.
namespace detection {

DetectionRuntime::DetectionRuntime() = default;

namespace {

float selectedScalarValue(const AudioSamplePacket& audioSamplePacket, const FrequencyBandMeasurementPacket& frequencyEvidence, FeatureStreamId stream) {
    switch (stream) {
        case FeatureStreamId::AmpEnvelope:
            return audioSamplePacket.audioMagnitudeValue;
        case FeatureStreamId::FrequencyScore:
            return frequencyEvidence.targetBandScoreValue;
        case FeatureStreamId::FrequencyContrast:
            return frequencyEvidence.targetBandContrastValue;
        case FeatureStreamId::Unknown:
        default:
            return static_cast<float>(audioSamplePacket.level);
    }
}

void applyScalarTransientConfig(ScalarTransientDetector& detector, const ScalarTransientConfig& config) {
    detector.setOnsetDetectionThreshold(config.onsetDetectionThreshold);
    detector.setOnsetReleaseThreshold(config.onsetReleaseThreshold);
    detector.setCooldownAfterOnsetMs(config.cooldownAfterOnsetMs);
    detector.setMinTransientDurationMs(config.minTransientDurationMs);
    detector.setMaxTransientDurationMs(config.maxTransientDurationMs);
    detector.setMinTransientPeakStrength(config.minTransientPeakStrength);
    detector.setReleaseDebounceMs(config.releaseDebounceMs);
}

} // namespace

void DetectionRuntime::resetState() {
    resetDetectionState();
    resetDiagnosticsCounters();
}

void DetectionRuntime::resetDiagnostics() {
    resetDiagnosticsCounters();
}

void DetectionRuntime::resetDiagnosticsCounters() {
    _detectorReport = {};
    _frequencyDetector.resetDiagnosticsSummary();
}

void DetectionRuntime::resetDetectors() {
    _frequencyDetector.resetState();
    _scalarDetector.resetState();
}

void DetectionRuntime::resetSourceRejectSummaries() {
    _frequencyDetector.resetRejectSummary();
    _scalarDetector.resetAcceptedOccurrenceSummary();
    _scalarDetector.resetSelectedRejectSummary();
}

void DetectionRuntime::resetDetectionState() {
    resetDetectors();
    _occurrenceInspector.reset();
    _patternMatcher.reset();
    _fieldStateTracker.reset();
    _featureHistory.reset();
    _resultQueue[0] = {};
    _resultReadIndex = 0;
    _resultCount = 0;
    _latestPipelineResult = {};
    _hasLatestPipelineResult = false;
    _lastOccurrence = {};
    _lastInspectedOccurrence = {};
    _detectorReport = {};
}

void DetectionRuntime::setDiagnosticsEnabled(bool enabled) {
    _frequencyDetector.setDiagnosticsEnabled(enabled);
    _scalarDetector.setDiagnosticsEnabled(enabled);
}

void DetectionRuntime::refreshDetectorReports(unsigned long nowMs) {
    _detectorReport = {};

    // DetectionRuntime coordinates detector-owned report snapshots. It must
    // not become the permanent home of detector-specific report assembly.
    switch (_detectorSelection) {
        case DetectorSelection::FrequencyMatch:
            _frequencyDetector.buildReport(_detectorReport, nowMs);
            break;
        case DetectorSelection::ScalarTransient:
            _scalarDetector.buildReport(_detectorReport, nowMs);
            break;
    }
}

void DetectionRuntime::setFrequencyMatchConfig(const FrequencyMatchConfig& config) {
    _frequencyMatchConfig = config;
}

void DetectionRuntime::setScalarTransientConfig(const ScalarTransientConfig& config) {
    _scalarTransientConfig = config;
    applyScalarTransientConfig(_scalarDetector, _scalarTransientConfig);
}

void DetectionRuntime::setDetectorSelection(DetectorSelection selection) {
    _detectorSelection = selection;
    resetDetectors();
    applyScalarTransientConfig(_scalarDetector, _scalarTransientConfig);
    _detectorReport = {};
}

void DetectionRuntime::setInspectionPlan(const InspectionPlan& plan) {
    _inspectionPlan = plan;
    _occurrenceInspector.configure(_inspectionPlan);
}

void DetectionRuntime::setPatternMatcherConfig(const PatternMatcherConfig& config) {
    _patternMatcherConfig = config;
    _patternMatcher.configure(_patternMatcherConfig);
}

void DetectionRuntime::setFieldStateConfig(const FieldStateConfig& config) {
    _fieldStateTracker.setConfig(config);
}

void DetectionRuntime::setProfileName(const char* profileName) {
    _profileName = profileName != nullptr ? profileName : "unknown";
}

void DetectionRuntime::observeFrame(
    const AudioSamplePacket& audioSamplePacket,
    const FrequencyBandMeasurementPacket& frequencyEvidence,
    unsigned long nowMs
) {
    _fieldStateTracker.update(nowMs);
    if (!audioSamplePacket.valid) {
        return;
    }

    FeatureExtractor::observeFrame(audioSamplePacket, _featureHistory);
    FeatureExtractor::observeFrequencyMeasurementPacket(frequencyEvidence, nowMs, _featureHistory);

    switch (_detectorSelection) {
        case DetectorSelection::FrequencyMatch:
            if (!frequencyEvidence.present || !frequencyEvidence.fresh) {
                break;
            }
            {
                FrequencyMatchEvaluation::Values frequencyTuning = {};
                frequencyTuning.attackScoreMin = _frequencyMatchConfig.attackScoreMin;
                frequencyTuning.releaseScoreMin = _frequencyMatchConfig.releaseScoreMin;
                frequencyTuning.attackContrastMin = _frequencyMatchConfig.attackContrastMin;
                frequencyTuning.releaseContrastMin = _frequencyMatchConfig.releaseContrastMin;
                _frequencyDetector.update(
                    frequencyEvidence,
                    audioSamplePacket,
                    audioSamplePacket.timeMs,
                    audioSamplePacket.sampleIndex,
                    frequencyTuning,
                    _frequencyMatchConfig.releaseDebounceMs,
                    _frequencyMatchConfig.cooldownAfterReleaseMs,
                    _frequencyMatchConfig.minDurationMs);
            }
            break;
        case DetectorSelection::ScalarTransient:
            if (streamRequiresFreshFrequency(_scalarTransientConfig.observedStream) && !frequencyEvidence.fresh) {
                break;
            }
            _scalarDetector.update(
                audioSamplePacket,
                selectedScalarValue(audioSamplePacket, frequencyEvidence, _scalarTransientConfig.observedStream)
            );
            break;
    }

    drainDetectors(nowMs);
    drainPatternMatcher(nowMs);
    refreshDetectorReports(nowMs);
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

const DetectorReport& DetectionRuntime::activeDetectorReport() const {
    return _detectorReport;
}

const PatternMatcherReport& DetectionRuntime::activePatternMatcherReport() const {
    return _patternMatcher.report();
}

const FieldState& DetectionRuntime::fieldState() const {
    return _fieldStateTracker.state();
}

const FeatureHistory& DetectionRuntime::featureHistory() const {
    return _featureHistory;
}

void DetectionRuntime::drainDetectors(unsigned long nowMs) {
    Occurrence candidate;

    switch (_detectorSelection) {
        case DetectorSelection::FrequencyMatch:
            while (_frequencyDetector.popOccurrence(candidate)) {
                _fieldStateTracker.observeOccurrence(candidate, nowMs);
                const InspectedOccurrence inspected = _occurrenceInspector.inspectWithHistory(candidate, &_featureHistory);
                _fieldStateTracker.observeInspectedOccurrence(inspected, nowMs);
                _lastOccurrence = candidate;
                _lastInspectedOccurrence = inspected;
                _patternMatcher.acceptOccurrence(inspected);
            }
            break;
        case DetectorSelection::ScalarTransient:
            while (_scalarDetector.popOccurrence(candidate)) {
                _fieldStateTracker.observeOccurrence(candidate, nowMs);
                const InspectedOccurrence inspected = _occurrenceInspector.inspectWithHistory(candidate, &_featureHistory);
                _fieldStateTracker.observeInspectedOccurrence(inspected, nowMs);
                _lastOccurrence = candidate;
                _lastInspectedOccurrence = inspected;
                _patternMatcher.acceptOccurrence(inspected);
            }
            break;
    }

    (void)nowMs;
}

void DetectionRuntime::drainPatternMatcher(unsigned long nowMs) {
    PatternResult result = {};
    while (_patternMatcher.popPatternResult(nowMs, result)) {
        if (_lastInspectedOccurrence.occurrence.present) {
            result.inspectedOccurrence = _lastInspectedOccurrence;
        }
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
    _latestPipelineResult.hasPatternReport = true;
    _latestPipelineResult.patternReport = _patternMatcher.report();
    _latestPipelineResult.hasOccurrence = occurrence != nullptr && occurrence->present;
    if (_latestPipelineResult.hasOccurrence && occurrence != nullptr) {
        _latestPipelineResult.occurrence = *occurrence;
    }
    if (inspectedOccurrence != nullptr && inspectedOccurrence->occurrence.present) {
        _latestPipelineResult.inspectedOccurrence = *inspectedOccurrence;
    } else if (result.inspectedOccurrence.occurrence.present) {
        _latestPipelineResult.inspectedOccurrence = result.inspectedOccurrence;
    }
    if (_latestPipelineResult.inspectedOccurrence.occurrence.present) {
        _latestPipelineResult.pattern.inspectedOccurrence = _latestPipelineResult.inspectedOccurrence;
    }
    _latestPipelineResult.hasField = true;
    _latestPipelineResult.field = _fieldStateTracker.state();
    _latestPipelineResult.profileName = _profileName;
    _latestPipelineResult.timestampMs = nowMs;
    _hasLatestPipelineResult = true;
}

} // namespace detection


