#include "DetectionRuntime.h"

#include <string.h>

// DetectionRuntime pipeline execution in source order.
namespace detection {

DetectionRuntime::DetectionRuntime() = default;

namespace {

float selectedScalarValue(const AudioSamplePacket& audioSamplePacket, const FrequencyBandMeasurementPacket& frequencyEvidence, FeatureStreamId stream) {
    switch (stream) {
        case FeatureStreamId::AmpMagnitude:
            return static_cast<float>(audioSamplePacket.audioMagnitudeValue);
        case FeatureStreamId::AmpEnvelope:
            return static_cast<float>(audioSamplePacket.smoothedLevel);
        case FeatureStreamId::FrequencyTarget:
            return frequencyEvidence.targetBandValue;
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
    detector.setRequireCarrierQuality(config.requireCarrierQuality);
    detector.setRequireMinStrength(config.requireMinStrength);
    detector.setMinMatchedMeanStrength(config.minMatchedMeanStrength);
    detector.setMinCoverageAboveReleaseMs(config.minCoverageAboveReleaseMs);
    detector.setMinLongestIslandMs(config.minLongestIslandMs);
    detector.setMaxGapMs(config.maxGapMs);
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
    _pipelineEventOverflowCount = 0;
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
    _pipelineEventOverflowCount = 0;
    _pipelineEventQueue[0] = {};
    _pipelineEventReadIndex = 0;
    _pipelineEventCount = 0;
    _lastEmittedAcceptedOccurrenceId = 0;
    _lastEmittedSelectedRejectOccurrenceId = 0;
    _patternInspectedQueue[0] = {};
    _patternInspectedReadIndex = 0;
    _patternInspectedCount = 0;
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

    if (_detectorReport.selectedReject.present &&
        _detectorReport.selectedReject.occurrenceId != 0 &&
        _detectorReport.selectedReject.occurrenceId != _lastEmittedSelectedRejectOccurrenceId) {
        DetectionPipelineEvent event = {};
        event.kind = DetectionEventKind::RejectedSourceCandidate;
        event.candidateId = static_cast<uint32_t>(_detectorReport.selectedReject.occurrenceId);
        event.hasSourceRecord = true;
        event.sourceRecord.detectorReport = _detectorReport;
        if (pushPipelineEvent(event)) {
            _lastEmittedSelectedRejectOccurrenceId = event.candidateId;
        }
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
    _pipelineEventQueue[0] = {};
    _pipelineEventReadIndex = 0;
    _pipelineEventCount = 0;
    _lastEmittedAcceptedOccurrenceId = 0;
    _lastEmittedSelectedRejectOccurrenceId = 0;
    _patternInspectedQueue[0] = {};
    _patternInspectedReadIndex = 0;
    _patternInspectedCount = 0;
    _detectorReport = {};
}

void DetectionRuntime::setInspectionPlan(const InspectionPlan& plan) {
    _inspectionPlan = plan;
    _occurrenceInspector.configure(_inspectionPlan);
    _patternMatcher.configure(_inspectionPlan);
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
                FrequencyMatchCriteria::Values frequencyTuning = {};
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
    refreshDetectorReports(nowMs);
    drainPatternMatcher(nowMs);
    refreshDetectorReports(nowMs);
}

bool DetectionRuntime::popPipelineEvent(DetectionPipelineEvent& out) {
    if (_pipelineEventCount == 0) {
        return false;
    }

    out = _pipelineEventQueue[_pipelineEventReadIndex];
    _pipelineEventReadIndex = (_pipelineEventReadIndex + 1) % kPipelineEventQueueCapacity;
    --_pipelineEventCount;
    return true;
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
    Occurrence occurrence;

    switch (_detectorSelection) {
        case DetectorSelection::FrequencyMatch:
            while (_frequencyDetector.popOccurrence(occurrence)) {
                _fieldStateTracker.observeOccurrence(occurrence, nowMs);
                const InspectedOccurrence inspected = _occurrenceInspector.inspectWithHistory(occurrence, &_featureHistory, nowMs);
                _fieldStateTracker.observeInspectedOccurrence(inspected, nowMs);
                if (_patternMatcher.acceptOccurrence(inspected)) {
                    pushPatternInspectedOccurrence(inspected);
                }
            }
            break;
        case DetectorSelection::ScalarTransient:
            while (_scalarDetector.popOccurrence(occurrence)) {
                _fieldStateTracker.observeOccurrence(occurrence, nowMs);
                const InspectedOccurrence inspected = _occurrenceInspector.inspectWithHistory(occurrence, &_featureHistory, nowMs);
                _fieldStateTracker.observeInspectedOccurrence(inspected, nowMs);
                if (_patternMatcher.acceptOccurrence(inspected)) {
                    pushPatternInspectedOccurrence(inspected);
                }
            }
            break;
    }

    (void)nowMs;
}

void DetectionRuntime::drainPatternMatcher(unsigned long nowMs) {
    PatternResult result = {};
    while (_patternMatcher.popPatternResult(nowMs, result)) {
        InspectedOccurrence matchedInspectedOccurrence = {};
        const bool hasMatchedInspectedOccurrence = popPatternInspectedOccurrence(matchedInspectedOccurrence);
        _fieldStateTracker.observePatternResult(result, nowMs);
        capturePipelineResult(
            result,
            hasMatchedInspectedOccurrence ? &matchedInspectedOccurrence : nullptr,
            nowMs
        );
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

unsigned long DetectionRuntime::pipelineEventOverflowCount() const {
    return _pipelineEventOverflowCount;
}

bool DetectionRuntime::pushPipelineEvent(const DetectionPipelineEvent& event) {
    if (_pipelineEventCount == kPipelineEventQueueCapacity) {
        ++_pipelineEventOverflowCount;
        return false;
    }

    const size_t writeIndex = (_pipelineEventReadIndex + _pipelineEventCount) % kPipelineEventQueueCapacity;
    _pipelineEventQueue[writeIndex] = event;
    ++_pipelineEventCount;
    return true;
}

void DetectionRuntime::capturePipelineResult(
    const PatternResult& result,
    const InspectedOccurrence* matchedInspectedOccurrence,
    unsigned long nowMs
) {
    _latestPipelineResult = {};
    _latestPipelineResult.hasPattern = true;
    _latestPipelineResult.pattern = result;
    _latestPipelineResult.hasPatternReport = true;
    _latestPipelineResult.patternReport = _patternMatcher.report();
    if (matchedInspectedOccurrence != nullptr && matchedInspectedOccurrence->occurrence.present) {
        _latestPipelineResult.hasPatternInspectedOccurrence = true;
        _latestPipelineResult.patternInspectedOccurrence = *matchedInspectedOccurrence;
        _latestPipelineResult.hasOccurrence = true;
        _latestPipelineResult.occurrence = matchedInspectedOccurrence->occurrence;
    }
    _latestPipelineResult.hasField = true;
    _latestPipelineResult.field = _fieldStateTracker.state();
    _latestPipelineResult.profileName = _profileName;
    _latestPipelineResult.timestampMs = nowMs;
    _hasLatestPipelineResult = true;

    DetectionPipelineEvent event = {};
    event.kind = DetectionEventKind::AcceptedPipelineResult;
    event.occurrenceId = static_cast<uint32_t>(result.occurrenceId);
    event.candidateId = static_cast<uint32_t>(result.occurrenceId);
    event.hasPatternResult = true;
    event.patternResult = result;
    event.hasSourceRecord = true;
    event.sourceRecord.detectorReport = _detectorReport;
    if (matchedInspectedOccurrence != nullptr && matchedInspectedOccurrence->occurrence.present) {
        event.hasInspectedOccurrence = true;
        event.inspectedOccurrence = *matchedInspectedOccurrence;
    }
    if (_lastEmittedAcceptedOccurrenceId != event.occurrenceId) {
        if (pushPipelineEvent(event)) {
            _lastEmittedAcceptedOccurrenceId = event.occurrenceId;
        }
    }
}

bool DetectionRuntime::pushPatternInspectedOccurrence(const InspectedOccurrence& occurrence) {
    if (_patternInspectedCount == kResultQueueCapacity) {
        return false;
    }

    const size_t writeIndex = (_patternInspectedReadIndex + _patternInspectedCount) % kResultQueueCapacity;
    _patternInspectedQueue[writeIndex] = occurrence;
    ++_patternInspectedCount;
    return true;
}

bool DetectionRuntime::popPatternInspectedOccurrence(InspectedOccurrence& out) {
    if (_patternInspectedCount == 0) {
        return false;
    }

    out = _patternInspectedQueue[_patternInspectedReadIndex];
    _patternInspectedReadIndex = (_patternInspectedReadIndex + 1) % kResultQueueCapacity;
    --_patternInspectedCount;
    return true;
}

} // namespace detection


