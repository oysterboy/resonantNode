#include "DetectionRuntime.h"

#include <Arduino.h>
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

const char* occurrenceDecisionName(OccurrenceDecision decision) {
    switch (decision) {
        case OccurrenceDecision::Accepted:
            return "accepted";
        case OccurrenceDecision::Rejected:
            return "rejected";
        case OccurrenceDecision::None:
        default:
            return "missing";
    }
}

void printPatternPathLine(
    unsigned long trial,
    unsigned long occurrenceId,
    bool attempt,
    bool accepted,
    PatternInputRejectReason rejectReason,
    OccurrenceDecision inspectionDecision,
    size_t matcherPending,
    size_t correlationPending,
    bool resultProduced,
    bool eventPushed
) {
    if (trial == 0) {
        return;
    }

    Serial.print("SEQ_PATTERN_PATH trial=");
    Serial.print(trial);
    Serial.print(" occurrence_id=");
    Serial.print(occurrenceId);
    Serial.print(" attempt=");
    Serial.print(attempt ? 1 : 0);
    Serial.print(" accepted=");
    Serial.print(accepted ? 1 : 0);
    Serial.print(" reject_reason=");
    Serial.print(patternInputRejectReasonName(rejectReason));
    Serial.print(" inspection_decision=");
    Serial.print(occurrenceDecisionName(inspectionDecision));
    Serial.print(" matcher_pending=");
    Serial.print(static_cast<unsigned long>(matcherPending));
    Serial.print(" correlation_pending=");
    Serial.print(static_cast<unsigned long>(correlationPending));
    Serial.print(" result_produced=");
    Serial.print(resultProduced ? 1 : 0);
    Serial.print(" event_pushed=");
    Serial.println(eventPushed ? 1 : 0);
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
    _patternResultQueueOverflowCount = 0;
    _patternInspectedQueueOverflowCount = 0;
    _detectorReportMismatchCount = 0;
    _observeFrameCount = 0;
    _freshDetectorInputCount = 0;
    _detectorDrainCount = 0;
    _patternDrainCount = 0;
    _detectorReportRefreshCount = 0;
    _noFreshFrequencySkipCount = 0;
    _detectorOccurrencePoppedCount = 0;
    _detectorValidOccurrencePoppedCount = 0;
    _latestPatternInputRejectReason = PatternInputRejectReason::None;
    _patternAcceptAttemptCount = 0;
    _patternAcceptSuccessCount = 0;
    _patternAcceptRejectCount = 0;
    _patternResultProducedCount = 0;
    _patternEventPushedCount = 0;
    _patternEventDroppedCount = 0;
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
    _patternResultQueueOverflowCount = 0;
    _pipelineEventQueue[0] = {};
    _pipelineEventReadIndex = 0;
    _pipelineEventCount = 0;
    _pipelineEventSequenceId = 0;
    _lastEmittedAcceptedOccurrenceId = 0;
    _lastEmittedAcceptedReportGeneration = 0;
    _lastEmittedSelectedRejectOccurrenceId = 0;
    _lastEmittedSelectedRejectReportGeneration = 0;
    _patternInspectedQueue[0] = {};
    _patternInspectedReadIndex = 0;
    _patternInspectedCount = 0;
    _patternInspectedQueueOverflowCount = 0;
    _patternCorrelationFailureCount = 0;
    _detectorReport = {};
    _lastObservedScalarReportGeneration = 0;
    _lastObservedFrequencyReportGeneration = 0;
}

void DetectionRuntime::setDiagnosticsEnabled(bool enabled) {
    _frequencyDetector.setDiagnosticsEnabled(enabled);
    _scalarDetector.setDiagnosticsEnabled(enabled);
}

bool DetectionRuntime::captureLatestDetectorReportIfChanged() {
    switch (_detectorSelection) {
        case DetectorSelection::FrequencyMatch:
            if (_lastObservedFrequencyReportGeneration == _frequencyDetector.reportGeneration()) {
                return false;
            }
            _detectorReport = _frequencyDetector.latestReport();
            _lastObservedFrequencyReportGeneration = _frequencyDetector.reportGeneration();
            _lastObservedScalarReportGeneration = 0;
            return true;
        case DetectorSelection::ScalarTransient:
            if (_lastObservedScalarReportGeneration == _scalarDetector.reportGeneration()) {
                return false;
            }
            _detectorReport = _scalarDetector.latestReport();
            _lastObservedScalarReportGeneration = _scalarDetector.reportGeneration();
            _lastObservedFrequencyReportGeneration = 0;
            return true;
    }

    return false;
}

void DetectionRuntime::drainDetectorReportEvents(unsigned long nowMs) {
    (void)nowMs;
    if (!captureLatestDetectorReportIfChanged()) {
        return;
    }

    ++_detectorReportRefreshCount;
    const uint32_t currentReportGeneration = _detectorReport.detectorId == DetectorId::ScalarTransient
        ? _scalarDetector.reportGeneration()
        : _frequencyDetector.reportGeneration();

    if (_detectorReport.selectedReject.present &&
        _detectorReport.selectedReject.occurrenceId != 0 &&
        (_detectorReport.selectedReject.occurrenceId != _lastEmittedSelectedRejectOccurrenceId ||
         _lastEmittedSelectedRejectReportGeneration != currentReportGeneration)) {
        DetectionPipelineEvent event = {};
        event.kind = DetectionEventKind::RejectedSourceCandidate;
        event.eventId = ++_pipelineEventSequenceId;
        event.hasCandidateId = true;
        event.candidateId = static_cast<uint32_t>(_detectorReport.selectedReject.occurrenceId);
        event.hasOccurrenceId = false;
        event.occurrenceId = 0;
        event.detectorReportPresent = true;
        event.detectorReportMatched = true;
        event.sourceSelection = "selected_reject";
        event.sourceCandidateId = static_cast<unsigned long>(_detectorReport.selectedReject.occurrenceId);
        event.sourceOccurrenceId = 0;
        event.integrity.detectorReportPresent = true;
        event.integrity.inspectionPresent = true;
        event.integrity.patternReportPresent = false;
        event.integrity.patternResultPresent = false;
        event.integrity.occurrenceMatched = true;
        event.integrity.correlationComplete = true;
        event.hasSourceRecord = true;
        event.sourceRecord.detectorReport = _detectorReport;
        event.sourceRecord.sourceSelection = event.sourceSelection;
        event.sourceRecord.eventId = event.eventId;
        event.sourceRecord.reportGeneration = currentReportGeneration;
        event.sourceRecord.eventTrialAttribution = 0;
        event.sourceRecord.sourceOccurrenceId = event.sourceOccurrenceId;
        event.sourceRecord.sourceCandidateId = event.sourceCandidateId;
        event.sourceRecord.sourceReportMatched = true;
        if (pushPipelineEvent(event)) {
            _lastEmittedSelectedRejectOccurrenceId = event.candidateId;
            _lastEmittedSelectedRejectReportGeneration = currentReportGeneration;
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
    _lastEmittedAcceptedReportGeneration = 0;
    _lastEmittedSelectedRejectOccurrenceId = 0;
    _lastEmittedSelectedRejectReportGeneration = 0;
    _patternInspectedQueue[0] = {};
    _patternInspectedReadIndex = 0;
    _patternInspectedCount = 0;
    _detectorReport = {};
    _lastObservedScalarReportGeneration = 0;
    _lastObservedFrequencyReportGeneration = 0;
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

void DetectionRuntime::setPatternDiagnosticsTrial(unsigned long trial) {
    _patternDiagnosticsTrial = trial;
}

void DetectionRuntime::setPatternResultQueueEnabled(bool enabled) {
    _patternResultQueueEnabled = enabled;
    if (!enabled) {
        _resultQueue[0] = {};
        _resultReadIndex = 0;
        _resultCount = 0;
    }
}

void DetectionRuntime::observeFrame(
    const AudioSamplePacket& audioSamplePacket,
    const FrequencyBandMeasurementPacket& frequencyEvidence,
    unsigned long nowMs
) {
    ++_observeFrameCount;
    _fieldStateTracker.update(nowMs);
    if (!audioSamplePacket.valid) {
        return;
    }

    FeatureExtractor::observeFrame(audioSamplePacket, _featureHistory);
    FeatureExtractor::observeFrequencyMeasurementPacket(frequencyEvidence, nowMs, _featureHistory);

    bool detectorInputProcessed = false;

    switch (_detectorSelection) {
        case DetectorSelection::FrequencyMatch:
            if (!frequencyEvidence.present || !frequencyEvidence.fresh) {
                ++_noFreshFrequencySkipCount;
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
                detectorInputProcessed = true;
                ++_freshDetectorInputCount;
            }
            break;
        case DetectorSelection::ScalarTransient:
            if (streamRequiresFreshFrequency(_scalarTransientConfig.observedStream) && !frequencyEvidence.fresh) {
                ++_noFreshFrequencySkipCount;
                break;
            }
            _scalarDetector.update(
                audioSamplePacket,
                selectedScalarValue(audioSamplePacket, frequencyEvidence, _scalarTransientConfig.observedStream)
            );
            detectorInputProcessed = true;
            ++_freshDetectorInputCount;
            break;
    }

    const bool detectorHadPendingOutput = hasPendingDetectorOutput();
    const bool patternHadPendingWork = hasPendingPatternWork();

    if (detectorInputProcessed || detectorHadPendingOutput) {
        ++_detectorDrainCount;
        drainDetectors(nowMs);
        drainDetectorReportEvents(nowMs);
    }

    if (detectorInputProcessed || detectorHadPendingOutput || patternHadPendingWork) {
        ++_patternDrainCount;
        drainPatternMatcher(nowMs);
    }
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

unsigned long DetectionRuntime::patternResultQueueOverflowCount() const {
    return _patternResultQueueOverflowCount;
}

unsigned long DetectionRuntime::patternInspectedQueueOverflowCount() const {
    return _patternInspectedQueueOverflowCount;
}

unsigned long DetectionRuntime::detectorReportMismatchCount() const {
    return _detectorReportMismatchCount;
}

uint32_t DetectionRuntime::observeFrameCount() const {
    return _observeFrameCount;
}

uint32_t DetectionRuntime::freshDetectorInputCount() const {
    return _freshDetectorInputCount;
}

uint32_t DetectionRuntime::detectorDrainCount() const {
    return _detectorDrainCount;
}

uint32_t DetectionRuntime::patternDrainCount() const {
    return _patternDrainCount;
}

uint32_t DetectionRuntime::detectorReportRefreshCount() const {
    return _detectorReportRefreshCount;
}

uint32_t DetectionRuntime::noFreshFrequencySkipCount() const {
    return _noFreshFrequencySkipCount;
}

uint32_t DetectionRuntime::detectorOccurrencePoppedCount() const {
    return _detectorOccurrencePoppedCount;
}

uint32_t DetectionRuntime::detectorValidOccurrencePoppedCount() const {
    return _detectorValidOccurrencePoppedCount;
}

uint32_t DetectionRuntime::patternAcceptAttemptCount() const {
    return _patternAcceptAttemptCount;
}

uint32_t DetectionRuntime::patternAcceptSuccessCount() const {
    return _patternAcceptSuccessCount;
}

uint32_t DetectionRuntime::patternAcceptRejectCount() const {
    return _patternAcceptRejectCount;
}

uint32_t DetectionRuntime::patternResultProducedCount() const {
    return _patternResultProducedCount;
}

uint32_t DetectionRuntime::patternEventPushedCount() const {
    return _patternEventPushedCount;
}

uint32_t DetectionRuntime::patternEventDroppedCount() const {
    return _patternEventDroppedCount;
}

PatternInputRejectReason DetectionRuntime::latestPatternInputRejectReason() const {
    return _latestPatternInputRejectReason;
}

uint32_t DetectionRuntime::scalarReportGeneration() const {
    return _scalarDetector.reportGeneration();
}

uint32_t DetectionRuntime::frequencyReportGeneration() const {
    return _frequencyDetector.reportGeneration();
}

bool DetectionRuntime::hasLatestPipelineResult() const {
    return _hasLatestPipelineResult;
}

const DetectionPipelineResult& DetectionRuntime::latestPipelineResult() const {
    return _latestPipelineResult;
}

const DetectorReport& DetectionRuntime::activeDetectorReport() const {
    const_cast<DetectionRuntime*>(this)->captureLatestDetectorReportIfChanged();
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

bool DetectionRuntime::hasPendingDetectorOutput() const {
    switch (_detectorSelection) {
        case DetectorSelection::FrequencyMatch:
            return _frequencyDetector.hasPendingOccurrence();
        case DetectorSelection::ScalarTransient:
            return _scalarDetector.hasPendingOccurrence();
    }

    return false;
}

bool DetectionRuntime::hasPendingPatternWork() const {
    return _patternInspectedCount > 0;
}

void DetectionRuntime::drainDetectors(unsigned long nowMs) {
    Occurrence occurrence;

    switch (_detectorSelection) {
        case DetectorSelection::FrequencyMatch:
            while (_frequencyDetector.popOccurrence(occurrence)) {
                ++_detectorOccurrencePoppedCount;
                if (occurrence.present && occurrence.valid) {
                    ++_detectorValidOccurrencePoppedCount;
                }
                _fieldStateTracker.observeOccurrence(occurrence, nowMs);
                const InspectedOccurrence inspected = _occurrenceInspector.inspectWithHistory(occurrence, &_featureHistory, nowMs);
                _fieldStateTracker.observeInspectedOccurrence(inspected, nowMs);
                PendingPatternObservation observation = {};
                observation.inspected = inspected;
                observation.detectorReport = _frequencyDetector.latestReport();
                if (observation.detectorReport.detectorId != occurrence.detectorId ||
                    !observation.detectorReport.accepted.present ||
                    observation.detectorReport.accepted.occurrenceId != occurrence.occurrenceId) {
                    ++_detectorReportMismatchCount;
                }
                ++_patternAcceptAttemptCount;
                const bool acceptedByMatcher = _patternMatcher.acceptOccurrence(inspected);
                PatternInputRejectReason rejectReason = _patternMatcher.lastInputRejectReason();
                if (acceptedByMatcher) {
                    ++_patternAcceptSuccessCount;
                    if (!pushPatternObservation(observation)) {
                        rejectReason = PatternInputRejectReason::CorrelationQueueFull;
                        _latestPatternInputRejectReason = rejectReason;
                    }
                } else {
                    ++_patternAcceptRejectCount;
                    _latestPatternInputRejectReason = rejectReason;
                }
                printPatternPathLine(
                    _patternDiagnosticsTrial,
                    inspected.occurrence.occurrenceId,
                    true,
                    acceptedByMatcher,
                    rejectReason,
                    inspected.decision,
                    _patternMatcher.pendingInputCount(),
                    _patternInspectedCount,
                    false,
                    false
                );
            }
            break;
        case DetectorSelection::ScalarTransient:
            while (_scalarDetector.popOccurrence(occurrence)) {
                ++_detectorOccurrencePoppedCount;
                if (occurrence.present && occurrence.valid) {
                    ++_detectorValidOccurrencePoppedCount;
                }
                _fieldStateTracker.observeOccurrence(occurrence, nowMs);
                const InspectedOccurrence inspected = _occurrenceInspector.inspectWithHistory(occurrence, &_featureHistory, nowMs);
                _fieldStateTracker.observeInspectedOccurrence(inspected, nowMs);
                PendingPatternObservation observation = {};
                observation.inspected = inspected;
                observation.detectorReport = _scalarDetector.latestReport();
                if (observation.detectorReport.detectorId != occurrence.detectorId ||
                    !observation.detectorReport.accepted.present ||
                    observation.detectorReport.accepted.occurrenceId != occurrence.occurrenceId) {
                    ++_detectorReportMismatchCount;
                }
                ++_patternAcceptAttemptCount;
                const bool acceptedByMatcher = _patternMatcher.acceptOccurrence(inspected);
                PatternInputRejectReason rejectReason = _patternMatcher.lastInputRejectReason();
                if (acceptedByMatcher) {
                    ++_patternAcceptSuccessCount;
                    if (!pushPatternObservation(observation)) {
                        rejectReason = PatternInputRejectReason::CorrelationQueueFull;
                        _latestPatternInputRejectReason = rejectReason;
                    }
                } else {
                    ++_patternAcceptRejectCount;
                    _latestPatternInputRejectReason = rejectReason;
                }
                printPatternPathLine(
                    _patternDiagnosticsTrial,
                    inspected.occurrence.occurrenceId,
                    true,
                    acceptedByMatcher,
                    rejectReason,
                    inspected.decision,
                    _patternMatcher.pendingInputCount(),
                    _patternInspectedCount,
                    false,
                    false
                );
            }
            break;
    }

    (void)nowMs;
}

void DetectionRuntime::drainPatternMatcher(unsigned long nowMs) {
    PatternResult result = {};
    while (_patternMatcher.popPatternResult(nowMs, result)) {
        ++_patternResultProducedCount;
        PendingPatternObservation matchedObservation = {};
        const bool hasMatchedInspectedOccurrence = popPatternObservation(result.occurrenceId, matchedObservation);
        _fieldStateTracker.observePatternResult(result, nowMs);
        const bool eventPushed = capturePipelineResult(
            result,
            hasMatchedInspectedOccurrence ? &matchedObservation.inspected : nullptr,
            hasMatchedInspectedOccurrence ? &matchedObservation.detectorReport : nullptr,
            nowMs
        );
        printPatternPathLine(
            _patternDiagnosticsTrial,
            result.occurrenceId,
            false,
            false,
            PatternInputRejectReason::None,
            hasMatchedInspectedOccurrence ? matchedObservation.inspected.decision : OccurrenceDecision::None,
            _patternMatcher.pendingInputCount(),
            _patternInspectedCount,
            true,
            eventPushed
        );
        if (_patternResultQueueEnabled) {
            pushPatternResult(result);
        }
    }
}

bool DetectionRuntime::pushPatternResult(const PatternResult& result) {
    if (!_patternResultQueueEnabled) {
        return true;
    }
    if (_resultCount == kResultQueueCapacity) {
        ++_patternResultQueueOverflowCount;
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

unsigned long DetectionRuntime::patternCorrelationFailureCount() const {
    return _patternCorrelationFailureCount;
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

bool DetectionRuntime::capturePipelineResult(
    const PatternResult& result,
    const InspectedOccurrence* matchedInspectedOccurrence,
    const DetectorReport* matchedDetectorReport,
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
    event.eventId = ++_pipelineEventSequenceId;
    event.hasOccurrenceId = true;
    event.occurrenceId = static_cast<uint32_t>(result.occurrenceId);
    event.hasCandidateId = false;
    event.candidateId = 0;
    event.detectorReportPresent = matchedDetectorReport != nullptr && matchedDetectorReport->detectorId != DetectorId::Unknown;
    event.detectorReportMatched =
        event.detectorReportPresent &&
        matchedInspectedOccurrence != nullptr &&
        matchedInspectedOccurrence->occurrence.present &&
        matchedDetectorReport->accepted.present &&
        matchedDetectorReport->accepted.occurrenceId == result.occurrenceId &&
        matchedInspectedOccurrence->occurrence.occurrenceId == result.occurrenceId;
    event.sourceSelection = matchedInspectedOccurrence != nullptr && matchedInspectedOccurrence->occurrence.present &&
        matchedInspectedOccurrence->decision == OccurrenceDecision::Rejected
        ? "selected_reject"
        : "selected_occurrence";
    event.sourceOccurrenceId = matchedInspectedOccurrence != nullptr && matchedInspectedOccurrence->occurrence.present
        ? matchedInspectedOccurrence->occurrence.occurrenceId
        : 0UL;
    event.sourceCandidateId = 0;
    event.integrity.detectorReportPresent = event.detectorReportPresent;
    event.integrity.inspectionPresent = event.detectorReportMatched;
    event.integrity.patternReportPresent = true;
    event.integrity.patternResultPresent = true;
    event.integrity.occurrenceMatched = event.detectorReportMatched;
    event.integrity.correlationComplete = event.integrity.detectorReportPresent && event.integrity.inspectionPresent && event.integrity.patternResultPresent;
    event.integrity.reason = event.integrity.correlationComplete
        ? PipelineIntegrityReason::None
        : (event.integrity.detectorReportPresent
            ? (event.integrity.inspectionPresent ? PipelineIntegrityReason::MissingPatternResult : PipelineIntegrityReason::MissingInspectedOccurrence)
            : PipelineIntegrityReason::MissingDetectorReport);
    event.hasPatternResult = true;
    event.patternResult = result;
    event.hasSourceRecord = true;
    event.sourceRecord.detectorReport = matchedDetectorReport != nullptr ? *matchedDetectorReport : DetectorReport{};
    event.sourceRecord.sourceSelection = event.sourceSelection;
    event.sourceRecord.eventId = event.eventId;
    event.sourceRecord.reportGeneration = matchedDetectorReport != nullptr
        ? (matchedDetectorReport->detectorId == DetectorId::ScalarTransient
            ? _scalarDetector.reportGeneration()
            : _frequencyDetector.reportGeneration())
        : 0U;
    event.sourceRecord.eventTrialAttribution = 0;
    event.sourceRecord.sourceOccurrenceId = event.sourceOccurrenceId;
    event.sourceRecord.sourceCandidateId = event.sourceCandidateId;
    event.sourceRecord.sourceReportMatched = event.detectorReportMatched;
    if (matchedDetectorReport == nullptr || !event.detectorReportMatched) {
        ++_detectorReportMismatchCount;
    }
    if (matchedInspectedOccurrence != nullptr && matchedInspectedOccurrence->occurrence.present) {
        event.hasInspectedOccurrence = true;
        event.inspectedOccurrence = *matchedInspectedOccurrence;
    }
    bool eventPushed = false;
    if (_lastEmittedAcceptedOccurrenceId != event.occurrenceId ||
        _lastEmittedAcceptedReportGeneration != event.sourceRecord.reportGeneration) {
        eventPushed = pushPipelineEvent(event);
        if (eventPushed) {
            _lastEmittedAcceptedOccurrenceId = event.occurrenceId;
            _lastEmittedAcceptedReportGeneration = event.sourceRecord.reportGeneration;
        }
    }
    if (eventPushed) {
        ++_patternEventPushedCount;
    } else {
        ++_patternEventDroppedCount;
    }
    return eventPushed;
}

bool DetectionRuntime::pushPatternObservation(const PendingPatternObservation& observation) {
    if (_patternInspectedCount == kResultQueueCapacity) {
        ++_patternInspectedQueueOverflowCount;
        return false;
    }

    const size_t writeIndex = (_patternInspectedReadIndex + _patternInspectedCount) % kResultQueueCapacity;
    _patternInspectedQueue[writeIndex] = observation;
    ++_patternInspectedCount;
    return true;
}

bool DetectionRuntime::popPatternObservation(unsigned long occurrenceId, PendingPatternObservation& out) {
    if (_patternInspectedCount == 0) {
        return false;
    }

    size_t matchOffset = 0;
    for (; matchOffset < _patternInspectedCount; ++matchOffset) {
        const size_t index = (_patternInspectedReadIndex + matchOffset) % kResultQueueCapacity;
        if (_patternInspectedQueue[index].inspected.occurrence.occurrenceId == occurrenceId) {
            out = _patternInspectedQueue[index];
            break;
        }
    }

    if (matchOffset >= _patternInspectedCount) {
        ++_patternCorrelationFailureCount;
        return false;
    }

    for (size_t i = matchOffset; i + 1 < _patternInspectedCount; ++i) {
        const size_t from = (_patternInspectedReadIndex + i + 1) % kResultQueueCapacity;
        const size_t to = (_patternInspectedReadIndex + i) % kResultQueueCapacity;
        _patternInspectedQueue[to] = _patternInspectedQueue[from];
    }
    _patternInspectedReadIndex = (_patternInspectedReadIndex + _patternInspectedCount - 1) % kResultQueueCapacity;
    --_patternInspectedCount;
    return true;
}

} // namespace detection


