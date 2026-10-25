#include "DetectionRuntime.h"
#include "../app/TimingUtils.h"

#include <string.h>

#include <Arduino.h>

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
#ifdef ANALYZER_MODE
    resetDiagnosticsCounters();
#endif
}

void DetectionRuntime::resetDetectors() {
    _scalarDetector.resetState();
    _pendingInspectionReadIndex = 0;
    _pendingInspectionCount = 0;
}

void DetectionRuntime::resetDetectionState() {
    resetDetectors();
    _occurrenceInspector.reset();
    _fieldStateTracker.reset();
    _featureHistory.reset();
    resetDetectionQueues();
#ifdef ANALYZER_MODE
    resetDetectionBookkeeping();
#endif
}

void DetectionRuntime::resetDetectionQueues() {
    memset(&_resultQueue[0], 0, sizeof(_resultQueue[0]));
    _resultReadIndex = 0;
    _resultCount = 0;
    _verdictQueueOverflowCount = 0;
#ifdef ANALYZER_MODE
    memset(&_pipelineEventQueue[0], 0, sizeof(_pipelineEventQueue[0]));
    _pipelineEventReadIndex = 0;
    _pipelineEventCount = 0;
#endif
}

#ifdef ANALYZER_MODE

void DetectionRuntime::resetDiagnostics() {
    resetDiagnosticsCounters();
}

void DetectionRuntime::resetDiagnosticsCounters() {
    _detectorReport = {};
    _pipelineEventOverflowCount = 0;
    _verdictQueueOverflowCount = 0;
    _detectorReportMismatchCount = 0;
    _historyIncomplete = {};
    _observeFrameCount = 0;
    _freshDetectorInputCount = 0;
    _detectorDrainCount = 0;
    _detectorReportRefreshCount = 0;
    _noFreshFrequencySkipCount = 0;
    _detectorOccurrencePoppedCount = 0;
    _detectorValidOccurrencePoppedCount = 0;
    _verdictProducedCount = 0;
    _verdictEventPushedCount = 0;
    _verdictEventDroppedCount = 0;
}

void DetectionRuntime::resetSourceRejectSummaries() {
    _scalarDetector.resetAcceptedOccurrenceSummary();
    _scalarDetector.resetSelectedRejectSummary();
}

void DetectionRuntime::resetDetectionBookkeeping() {
    memset(&_latestPipelineResult, 0, sizeof(_latestPipelineResult));
    _hasLatestPipelineResult = false;
    _pipelineEventOverflowCount = 0;
    _pipelineEventSequenceId = 0;
    _lastEmittedAcceptedOccurrenceId = 0;
    _lastEmittedAcceptedReportGeneration = 0;
    _lastEmittedSelectedRejectOccurrenceId = 0;
    _lastEmittedSelectedRejectReportGeneration = 0;
    memset(&_detectorReport, 0, sizeof(_detectorReport));
    _lastObservedScalarReportGeneration = 0;
}

void DetectionRuntime::setDiagnosticsEnabled(bool enabled) {
    _scalarDetector.setDiagnosticsEnabled(enabled);
}

bool DetectionRuntime::captureLatestDetectorReportIfChanged() {
    if (_lastObservedScalarReportGeneration == _scalarDetector.reportGeneration()) {
        return false;
    }
    _detectorReport = _scalarDetector.latestReport();
    _lastObservedScalarReportGeneration = _scalarDetector.reportGeneration();
    return true;
}

void DetectionRuntime::drainDetectorReportEvents(unsigned long nowMs) {
    (void)nowMs;
    if (!captureLatestDetectorReportIfChanged()) {
        return;
    }

    ++_detectorReportRefreshCount;
    // One reject event per new selected reject, not per refreeze (a weaker
    // reject refreezes for rejected_count; decision 2026-10-13).
    const uint32_t currentReportGeneration = _scalarDetector.reportGeneration();
    const uint32_t currentRejectGeneration = _scalarDetector.selectedRejectGeneration();

    if (_detectorReport.selectedReject.present &&
        _detectorReport.selectedReject.occurrenceId != 0 &&
        (_detectorReport.selectedReject.occurrenceId != _lastEmittedSelectedRejectOccurrenceId ||
         _lastEmittedSelectedRejectReportGeneration != currentRejectGeneration)) {
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
        event.integrity.verdictPresent = false;
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
            _lastEmittedSelectedRejectReportGeneration = currentRejectGeneration;
        }
    }
}

#endif // ANALYZER_MODE

void DetectionRuntime::setScalarTransientConfig(const ScalarTransientConfig& config) {
    _scalarTransientConfig = config;
    applyScalarTransientConfig(_scalarDetector, _scalarTransientConfig);
}

void DetectionRuntime::setDetectorSelection(DetectorSelection selection) {
    _detectorSelection = selection;
    resetDetectors();
    applyScalarTransientConfig(_scalarDetector, _scalarTransientConfig);
#ifdef ANALYZER_MODE
    _pipelineEventQueue[0] = {};
    _pipelineEventReadIndex = 0;
    _pipelineEventCount = 0;
    _lastEmittedAcceptedOccurrenceId = 0;
    _lastEmittedAcceptedReportGeneration = 0;
    _lastEmittedSelectedRejectOccurrenceId = 0;
    _lastEmittedSelectedRejectReportGeneration = 0;
    _detectorReport = {};
    _lastObservedScalarReportGeneration = 0;
#endif
}

// FeatureHistory keeps one buffer per inspection module rather than one per
// known stream; the plan is the only reader of history, so it is also the
// authority on which streams are worth recording. The header can't depend
// on InspectorTypes.h, so the bound is enforced here, where both are visible.
static_assert(FeatureHistory::kMaxActiveStreams >= kMaxInspectionModules,
    "FeatureHistory must have a slot for every stream an InspectionPlan can name");

void DetectionRuntime::setInspectionPlan(const InspectionPlan& plan) {
    _inspectionPlan = plan;
    _occurrenceInspector.configure(_inspectionPlan);

    FeatureStreamId streams[kMaxInspectionModules] = {};
    size_t streamCount = 0;
    for (size_t i = 0; i < _inspectionPlan.count && i < kMaxInspectionModules; ++i) {
        const InspectionModuleConfig& module = _inspectionPlan.modules[i];
        if (module.kind == InspectionModuleKind::MagnitudeFeatureStrength) {
            streams[streamCount++] = module.magnitude.stream;
        }
    }
    // Rebinding the streams clears every buffer. Only do it when the set of
    // streams changes: a threshold-only plan change (a live PARAM SET, step 10
    // item 7) keeps the history, so inspections stay complete.
    bool sameStreams = _featureHistory.activeStreamCount() == streamCount;
    for (size_t i = 0; sameStreams && i < streamCount; ++i) {
        sameStreams = _featureHistory.activeStream(i) == streams[i];
    }
    if (!sameStreams) {
        _featureHistory.setActiveStreams(streams, streamCount);
    }
}

void DetectionRuntime::setFieldStateConfig(const FieldStateConfig& config) {
    _fieldStateTracker.setConfig(config);
}

void DetectionRuntime::setProfileName(const char* profileName) {
    _profileName = profileName != nullptr ? profileName : "unknown";
}

void DetectionRuntime::setVerdictQueueEnabled(bool enabled) {
    _verdictQueueEnabled = enabled;
    if (!enabled) {
        _resultQueue[0] = {};
        _resultReadIndex = 0;
        _resultCount = 0;
    }
}

void IRAM_ATTR DetectionRuntime::observeFrame(
    const AudioSamplePacket& audioSamplePacket,
    const FrequencyBandMeasurementPacket& frequencyEvidence,
    unsigned long nowMs
) {
#ifdef ANALYZER_MODE
    ++_observeFrameCount;
#endif
    _fieldStateTracker.update(nowMs);
    if (!audioSamplePacket.valid) {
        return;
    }

    FeatureExtractor::observeFrame(audioSamplePacket, _featureHistory);
    FeatureExtractor::observeFrequencyMeasurementPacket(frequencyEvidence, nowMs, _featureHistory);

    bool detectorInputProcessed = false;

    if (streamRequiresFreshFrequency(_scalarTransientConfig.observedStream) && !frequencyEvidence.fresh) {
#ifdef ANALYZER_MODE
        ++_noFreshFrequencySkipCount;
#endif
    } else {
        _scalarDetector.update(
            audioSamplePacket,
            selectedScalarValue(audioSamplePacket, frequencyEvidence, _scalarTransientConfig.observedStream)
        );
        detectorInputProcessed = true;
#ifdef ANALYZER_MODE
        ++_freshDetectorInputCount;
#endif
    }

    if (detectorInputProcessed || hasPendingDetectorOutput() || _pendingInspectionCount > 0) {
#ifdef ANALYZER_MODE
        ++_detectorDrainCount;
#endif
        drainDetectors(nowMs);
#ifdef ANALYZER_MODE
        drainDetectorReportEvents(nowMs);
#endif
    }
}

bool DetectionRuntime::popOccurrenceVerdict(OccurrenceVerdict& out) {
    if (_resultCount == 0) {
        return false;
    }

    out = _resultQueue[_resultReadIndex];
    _resultReadIndex = (_resultReadIndex + 1) % kResultQueueCapacity;
    --_resultCount;
    return true;
}

unsigned long DetectionRuntime::verdictQueueOverflowCount() const {
    return _verdictQueueOverflowCount;
}

unsigned long DetectionRuntime::pendingInspectionOverflowCount() const {
    return _pendingInspectionOverflowCount;
}

const FieldState& DetectionRuntime::fieldState() const {
    return _fieldStateTracker.state();
}

#ifdef ANALYZER_MODE

bool DetectionRuntime::popPipelineEvent(DetectionPipelineEvent& out) {
    if (_pipelineEventCount == 0) {
        return false;
    }

    out = _pipelineEventQueue[_pipelineEventReadIndex];
    _pipelineEventReadIndex = (_pipelineEventReadIndex + 1) % kPipelineEventQueueCapacity;
    --_pipelineEventCount;
    return true;
}

unsigned long DetectionRuntime::detectorReportMismatchCount() const {
    return _detectorReportMismatchCount;
}

const DetectionRuntime::HistoryIncompleteSnapshot& DetectionRuntime::historyIncompleteSnapshot() const {
    return _historyIncomplete;
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

uint32_t DetectionRuntime::verdictProducedCount() const {
    return _verdictProducedCount;
}

uint32_t DetectionRuntime::verdictEventPushedCount() const {
    return _verdictEventPushedCount;
}

uint32_t DetectionRuntime::verdictEventDroppedCount() const {
    return _verdictEventDroppedCount;
}

uint32_t DetectionRuntime::scalarReportGeneration() const {
    return _scalarDetector.reportGeneration();
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

const FeatureHistory& DetectionRuntime::featureHistory() const {
    return _featureHistory;
}

#endif // ANALYZER_MODE

bool IRAM_ATTR DetectionRuntime::hasPendingDetectorOutput() const {
    return _scalarDetector.hasPendingOccurrence();
}

void DetectionRuntime::drainDetectors(unsigned long nowMs) {
    Occurrence occurrence;
    while (_scalarDetector.popOccurrence(occurrence)) {
#ifdef ANALYZER_MODE
        ++_detectorOccurrencePoppedCount;
        if (occurrence.present && occurrence.valid) {
            ++_detectorValidOccurrencePoppedCount;
        }
        const DetectorReport& poppedReport = _scalarDetector.latestReport();
        if (poppedReport.detectorId != occurrence.detectorId || !poppedReport.accepted.present ||
            poppedReport.accepted.occurrenceId != occurrence.occurrenceId) {
            ++_detectorReportMismatchCount;
        }
#endif
        _fieldStateTracker.observeOccurrence(occurrence, nowMs);

        // Step 8b item 1 (decisions/2026-10-22-inspect-when-window-complete.md):
        // an occurrence whose inspection window ends after now waits until it
        // has; one that is ready goes straight through, unless an earlier one is
        // still waiting (order is kept).
        const unsigned long readyAtMs = _occurrenceInspector.windowCompleteAtMs(occurrence);
        const bool ready = !timing::after(readyAtMs, nowMs);
        if (ready && _pendingInspectionCount == 0) {
            inspectAndRender(occurrence, nowMs
#ifdef ANALYZER_MODE
                , _scalarDetector.latestReport()
#endif
            );
            continue;
        }
        if (_pendingInspectionCount == kPendingInspectionCapacity) {
            // Cannot happen on the shipped values (one occurrence per >= 85 ms,
            // a wait of <= 15 ms); if it does, inspect the oldest now, early.
            ++_pendingInspectionOverflowCount;
            PendingInspection& oldest = _pendingInspections[_pendingInspectionReadIndex];
            inspectAndRender(oldest.occurrence, nowMs
#ifdef ANALYZER_MODE
                , oldest.detectorReport
#endif
            );
            _pendingInspectionReadIndex = (_pendingInspectionReadIndex + 1) % kPendingInspectionCapacity;
            --_pendingInspectionCount;
        }
        PendingInspection& slot =
            _pendingInspections[(_pendingInspectionReadIndex + _pendingInspectionCount) % kPendingInspectionCapacity];
        slot.occurrence = occurrence;
        slot.readyAtMs = readyAtMs;
#ifdef ANALYZER_MODE
        slot.detectorReport = _scalarDetector.latestReport();
#endif
        ++_pendingInspectionCount;
    }

    while (_pendingInspectionCount > 0) {
        PendingInspection& next = _pendingInspections[_pendingInspectionReadIndex];
        if (timing::after(next.readyAtMs, nowMs)) {
            break;
        }
        inspectAndRender(next.occurrence, nowMs
#ifdef ANALYZER_MODE
            , next.detectorReport
#endif
        );
        _pendingInspectionReadIndex = (_pendingInspectionReadIndex + 1) % kPendingInspectionCapacity;
        --_pendingInspectionCount;
    }
}

void DetectionRuntime::inspectAndRender(const Occurrence& occurrence, unsigned long nowMs
#ifdef ANALYZER_MODE
    , const DetectorReport& detectorReport
#endif
) {
    const InspectedOccurrence inspected = _occurrenceInspector.inspectWithHistory(occurrence, &_featureHistory, nowMs);
    _fieldStateTracker.observeInspectedOccurrence(inspected, nowMs);
#ifdef ANALYZER_MODE
    for (size_t i = 0; i < inspected.magnitudeObservationCount; ++i) {
        const MagnitudeInspectionObservation& obs = inspected.magnitudeObservations[i];
        if (obs.note != MagnitudeInspectionNote::HistoryWindowIncomplete) {
            continue;
        }
        ++_historyIncomplete.count;
        _historyIncomplete.stream = obs.stream;
        _historyIncomplete.requestedStartMs = obs.requestedStartMs;
        _historyIncomplete.requestedEndMs = obs.requestedEndMs;
        _historyIncomplete.inspectionNowMs = obs.inspectionNowMs;
        _historyIncomplete.ringOldestMs = _featureHistory.oldestBinTimeMs(obs.stream);
        _historyIncomplete.ringNewestMs = _featureHistory.latestTimeMs(obs.stream);
        _historyIncomplete.ringBins = _featureHistory.binCount(obs.stream);
        _historyIncomplete.outOfOrderRecords = _featureHistory.outOfOrderRecordCount();
        break;
    }
#endif
    // The verdict is the last step of inspection (makeVerdict); FieldState,
    // the verdict queue and the Analyzer's pipeline event all take it here,
    // with the inspected occurrence and the detector report in scope.
    OccurrenceVerdict verdict;
    if (!makeVerdict(inspected, _inspectionPlan, verdict)) {
        return;
    }
#ifdef ANALYZER_MODE
    ++_verdictProducedCount;
#endif
    _fieldStateTracker.observeOccurrenceVerdict(verdict, nowMs);
#ifdef ANALYZER_MODE
    capturePipelineResult(verdict, inspected, detectorReport, nowMs);
#endif
    if (_verdictQueueEnabled) {
        pushOccurrenceVerdict(verdict);
    }
}

bool DetectionRuntime::pushOccurrenceVerdict(const OccurrenceVerdict& result) {
    if (!_verdictQueueEnabled) {
        return true;
    }
    if (_resultCount == kResultQueueCapacity) {
        ++_verdictQueueOverflowCount;
        return false;
    }

    const size_t writeIndex = (_resultReadIndex + _resultCount) % kResultQueueCapacity;
    _resultQueue[writeIndex] = result;
    ++_resultCount;
    return true;
}

#ifdef ANALYZER_MODE

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

bool DetectionRuntime::capturePipelineResult(
    const OccurrenceVerdict& result,
    const InspectedOccurrence& inspected,
    const DetectorReport& detectorReport,
    unsigned long nowMs
) {
    const InspectedOccurrence* matchedInspectedOccurrence = &inspected;
    const DetectorReport* matchedDetectorReport = &detectorReport;
    _latestPipelineResult = {};
    _latestPipelineResult.hasVerdict = true;
    _latestPipelineResult.verdict = result;
    if (matchedInspectedOccurrence != nullptr && matchedInspectedOccurrence->occurrence.present) {
        _latestPipelineResult.hasVerdictInspectedOccurrence = true;
        _latestPipelineResult.verdictInspectedOccurrence = *matchedInspectedOccurrence;
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
    event.integrity.verdictPresent = true;
    event.integrity.occurrenceMatched = event.detectorReportMatched;
    event.integrity.correlationComplete = event.integrity.detectorReportPresent && event.integrity.inspectionPresent && event.integrity.verdictPresent;
    event.integrity.reason = event.integrity.correlationComplete
        ? PipelineIntegrityReason::None
        : (event.integrity.detectorReportPresent
            ? PipelineIntegrityReason::MissingVerdict
            : PipelineIntegrityReason::MissingDetectorReport);
    event.hasVerdict = true;
    event.verdict = result;
    event.hasSourceRecord = true;
    event.sourceRecord.detectorReport = matchedDetectorReport != nullptr ? *matchedDetectorReport : DetectorReport{};
    event.sourceRecord.sourceSelection = event.sourceSelection;
    event.sourceRecord.eventId = event.eventId;
    event.sourceRecord.reportGeneration = matchedDetectorReport != nullptr
        ? _scalarDetector.reportGeneration()
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
        ++_verdictEventPushedCount;
    } else {
        ++_verdictEventDroppedCount;
    }
    return eventPushed;
}

#endif // ANALYZER_MODE

} // namespace detection


