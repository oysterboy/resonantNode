#include "DetectionRuntime.h"

#include <string.h>

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

// Narrow view over whichever detector is currently selected, used only to
// collapse drainDetectors()'s duplicated per-branch drain loop into one. Not
// a general IDetector interface: it deliberately exposes only the one call
// this loop needs, with a signature and meaning already identical on both
// detectors. update() stays switch-based in observeFrame() (genuinely
// different signatures per detector), and latestReport()/reportGeneration()
// stay switch-based wherever they're already used (see
// captureLatestDetectorReportIfChanged()), since neither OccurrenceVerdict nor
// FieldState is ever built from DetectorReport, that's a diagnostics-only
// concern, not part of the drain path this adapter unifies.
class ActiveDetectorAdapter {
public:
    ActiveDetectorAdapter(DetectorSelection selection,
                           FrequencyMatchDetector& frequencyDetector,
                           ScalarTransientDetector& scalarDetector)
        : _selection(selection), _frequencyDetector(frequencyDetector), _scalarDetector(scalarDetector) {}

    bool popOccurrence(Occurrence& out) {
        switch (_selection) {
            case DetectorSelection::FrequencyMatch:
                return _frequencyDetector.popOccurrence(out);
            case DetectorSelection::ScalarTransient:
                return _scalarDetector.popOccurrence(out);
        }
        return false;
    }

private:
    DetectorSelection _selection;
    FrequencyMatchDetector& _frequencyDetector;
    ScalarTransientDetector& _scalarDetector;
};

} // namespace

void DetectionRuntime::resetState() {
    resetDetectionState();
#ifdef ANALYZER_MODE
    resetDiagnosticsCounters();
#endif
}

void DetectionRuntime::resetDetectors() {
    _frequencyDetector.resetState();
    _scalarDetector.resetState();
}

void DetectionRuntime::resetDetectionState() {
    resetDetectors();
    _occurrenceInspector.reset();
    _occurrenceEvaluator.reset();
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
#ifdef ANALYZER_MODE
    _verdictQueueOverflowCount = 0;
    memset(&_pipelineEventQueue[0], 0, sizeof(_pipelineEventQueue[0]));
    _pipelineEventReadIndex = 0;
    _pipelineEventCount = 0;
    memset(&_verdictCorrelationQueue[0], 0, sizeof(_verdictCorrelationQueue[0]));
    _verdictCorrelationReadIndex = 0;
    _verdictCorrelationCount = 0;
    _verdictCorrelationQueueOverflowCount = 0;
#endif
}

#ifdef ANALYZER_MODE

void DetectionRuntime::resetDiagnostics() {
    resetDiagnosticsCounters();
}

void DetectionRuntime::resetDiagnosticsCounters() {
    _detectorReport = {};
    _frequencyDetector.resetDiagnosticsSummary();
    _pipelineEventOverflowCount = 0;
    _verdictQueueOverflowCount = 0;
    _verdictCorrelationQueueOverflowCount = 0;
    _detectorReportMismatchCount = 0;
    _observeFrameCount = 0;
    _freshDetectorInputCount = 0;
    _detectorDrainCount = 0;
    _evaluatorDrainCount = 0;
    _detectorReportRefreshCount = 0;
    _noFreshFrequencySkipCount = 0;
    _detectorOccurrencePoppedCount = 0;
    _detectorValidOccurrencePoppedCount = 0;
    _latestEvaluatorInputRejectReason = EvaluatorInputRejectReason::None;
    _evaluatorAcceptAttemptCount = 0;
    _evaluatorAcceptSuccessCount = 0;
    _evaluatorAcceptRejectCount = 0;
    _verdictProducedCount = 0;
    _verdictEventPushedCount = 0;
    _verdictEventDroppedCount = 0;
}

void DetectionRuntime::resetSourceRejectSummaries() {
    _frequencyDetector.resetRejectSummary();
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
    _verdictCorrelationFailureCount = 0;
    memset(&_detectorReport, 0, sizeof(_detectorReport));
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
        event.integrity.evaluatorReportPresent = false;
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
            _lastEmittedSelectedRejectReportGeneration = currentReportGeneration;
        }
    }
}

#endif // ANALYZER_MODE

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
#ifdef ANALYZER_MODE
    _pipelineEventQueue[0] = {};
    _pipelineEventReadIndex = 0;
    _pipelineEventCount = 0;
    _lastEmittedAcceptedOccurrenceId = 0;
    _lastEmittedAcceptedReportGeneration = 0;
    _lastEmittedSelectedRejectOccurrenceId = 0;
    _lastEmittedSelectedRejectReportGeneration = 0;
    _verdictCorrelationQueue[0] = {};
    _verdictCorrelationReadIndex = 0;
    _verdictCorrelationCount = 0;
    _detectorReport = {};
    _lastObservedScalarReportGeneration = 0;
    _lastObservedFrequencyReportGeneration = 0;
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
    _occurrenceEvaluator.configure(_inspectionPlan);

    FeatureStreamId streams[kMaxInspectionModules] = {};
    size_t streamCount = 0;
    for (size_t i = 0; i < _inspectionPlan.count && i < kMaxInspectionModules; ++i) {
        const InspectionModuleConfig& module = _inspectionPlan.modules[i];
        if (module.kind == InspectionModuleKind::MagnitudeFeatureStrength) {
            streams[streamCount++] = module.magnitude.stream;
        }
    }
    _featureHistory.setActiveStreams(streams, streamCount);
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

void DetectionRuntime::observeFrame(
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

    switch (_detectorSelection) {
        case DetectorSelection::FrequencyMatch:
            if (!frequencyEvidence.present || !frequencyEvidence.fresh) {
#ifdef ANALYZER_MODE
                ++_noFreshFrequencySkipCount;
#endif
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
#ifdef ANALYZER_MODE
                ++_freshDetectorInputCount;
#endif
            }
            break;
        case DetectorSelection::ScalarTransient:
            if (streamRequiresFreshFrequency(_scalarTransientConfig.observedStream) && !frequencyEvidence.fresh) {
#ifdef ANALYZER_MODE
                ++_noFreshFrequencySkipCount;
#endif
                break;
            }
            _scalarDetector.update(
                audioSamplePacket,
                selectedScalarValue(audioSamplePacket, frequencyEvidence, _scalarTransientConfig.observedStream)
            );
            detectorInputProcessed = true;
#ifdef ANALYZER_MODE
            ++_freshDetectorInputCount;
#endif
            break;
    }

    const bool detectorHadPendingOutput = hasPendingDetectorOutput();
    const bool evaluatorHadPendingWork = hasPendingEvaluatorWork();

    if (detectorInputProcessed || detectorHadPendingOutput) {
#ifdef ANALYZER_MODE
        ++_detectorDrainCount;
#endif
        drainDetectors(nowMs);
#ifdef ANALYZER_MODE
        drainDetectorReportEvents(nowMs);
#endif
    }

    if (detectorInputProcessed || detectorHadPendingOutput || evaluatorHadPendingWork) {
#ifdef ANALYZER_MODE
        ++_evaluatorDrainCount;
#endif
        drainOccurrenceEvaluator(nowMs);
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

unsigned long DetectionRuntime::verdictQueueOverflowCount() const {
    return _verdictQueueOverflowCount;
}

unsigned long DetectionRuntime::verdictCorrelationQueueOverflowCount() const {
    return _verdictCorrelationQueueOverflowCount;
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

uint32_t DetectionRuntime::evaluatorDrainCount() const {
    return _evaluatorDrainCount;
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

uint32_t DetectionRuntime::evaluatorAcceptAttemptCount() const {
    return _evaluatorAcceptAttemptCount;
}

uint32_t DetectionRuntime::evaluatorAcceptSuccessCount() const {
    return _evaluatorAcceptSuccessCount;
}

uint32_t DetectionRuntime::evaluatorAcceptRejectCount() const {
    return _evaluatorAcceptRejectCount;
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

EvaluatorInputRejectReason DetectionRuntime::latestEvaluatorInputRejectReason() const {
    return _latestEvaluatorInputRejectReason;
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

const OccurrenceEvaluatorReport& DetectionRuntime::activeEvaluatorReport() const {
    return _occurrenceEvaluator.report();
}

const FeatureHistory& DetectionRuntime::featureHistory() const {
    return _featureHistory;
}

#endif // ANALYZER_MODE

bool DetectionRuntime::hasPendingDetectorOutput() const {
    switch (_detectorSelection) {
        case DetectorSelection::FrequencyMatch:
            return _frequencyDetector.hasPendingOccurrence();
        case DetectorSelection::ScalarTransient:
            return _scalarDetector.hasPendingOccurrence();
    }

    return false;
}

bool DetectionRuntime::hasPendingEvaluatorWork() const {
    // Query the matcher's own pending-input state directly. The correlation
    // queue (_verdictCorrelationCount) is diagnostics-only bookkeeping and can
    // legitimately diverge from what the matcher itself still has queued
    // (for example, when pushVerdictObservation() fails while
    // acceptOccurrence() already succeeded), so it must not be the authority
    // for whether pattern work is pending.
    return _occurrenceEvaluator.hasPendingInput();
}

void DetectionRuntime::drainDetectors(unsigned long nowMs) {
    Occurrence occurrence;
    ActiveDetectorAdapter activeDetector(_detectorSelection, _frequencyDetector, _scalarDetector);

    while (activeDetector.popOccurrence(occurrence)) {
#ifdef ANALYZER_MODE
        ++_detectorOccurrencePoppedCount;
        if (occurrence.present && occurrence.valid) {
            ++_detectorValidOccurrencePoppedCount;
        }
#endif
        _fieldStateTracker.observeOccurrence(occurrence, nowMs);
        const InspectedOccurrence inspected = _occurrenceInspector.inspectWithHistory(occurrence, &_featureHistory, nowMs);
        _fieldStateTracker.observeInspectedOccurrence(inspected, nowMs);
#ifdef ANALYZER_MODE
        // The correlation observation exists only to attach a matching
        // DetectorReport/InspectedOccurrence to the diagnostic
        // DetectionPipelineEvent; neither OccurrenceVerdict nor FieldState is
        // ever built from it. latestReport() stays switch-based rather than
        // going through the adapter, see the adapter's own comment for why.
        PendingVerdictObservation observation = {};
        observation.inspected = inspected;
        observation.detectorReport = _detectorSelection == DetectorSelection::FrequencyMatch
            ? _frequencyDetector.latestReport()
            : _scalarDetector.latestReport();
        if (observation.detectorReport.detectorId != occurrence.detectorId ||
            !observation.detectorReport.accepted.present ||
            observation.detectorReport.accepted.occurrenceId != occurrence.occurrenceId) {
            ++_detectorReportMismatchCount;
        }
        ++_evaluatorAcceptAttemptCount;
#endif
        // Core: feeding the matcher is what eventually produces OccurrenceVerdict.
        const bool acceptedByMatcher = _occurrenceEvaluator.acceptOccurrence(inspected);
#ifdef ANALYZER_MODE
        EvaluatorInputRejectReason rejectReason = _occurrenceEvaluator.lastInputRejectReason();
        if (acceptedByMatcher) {
            ++_evaluatorAcceptSuccessCount;
            if (!pushVerdictObservation(observation)) {
                rejectReason = EvaluatorInputRejectReason::CorrelationQueueFull;
                _latestEvaluatorInputRejectReason = rejectReason;
            }
        } else {
            ++_evaluatorAcceptRejectCount;
            _latestEvaluatorInputRejectReason = rejectReason;
        }
#else
        (void)acceptedByMatcher;
#endif
    }

    (void)nowMs;
}

void DetectionRuntime::drainOccurrenceEvaluator(unsigned long nowMs) {
    OccurrenceVerdict result = {};
    while (_occurrenceEvaluator.popOccurrenceVerdict(nowMs, result)) {
#ifdef ANALYZER_MODE
        ++_verdictProducedCount;
        PendingVerdictObservation matchedObservation = {};
        const bool hasMatchedInspectedOccurrence = popVerdictObservation(result.occurrenceId, matchedObservation);
#endif
        // Core: FieldState and the OccurrenceVerdict queue are built from the
        // bare OccurrenceVerdict, independent of any correlation bookkeeping.
        _fieldStateTracker.observeOccurrenceVerdict(result, nowMs);
#ifdef ANALYZER_MODE
        capturePipelineResult(
            result,
            hasMatchedInspectedOccurrence ? &matchedObservation.inspected : nullptr,
            hasMatchedInspectedOccurrence ? &matchedObservation.detectorReport : nullptr,
            nowMs
        );
#endif
        if (_verdictQueueEnabled) {
            pushOccurrenceVerdict(result);
        }
    }
}

bool DetectionRuntime::pushOccurrenceVerdict(const OccurrenceVerdict& result) {
    if (!_verdictQueueEnabled) {
        return true;
    }
    if (_resultCount == kResultQueueCapacity) {
#ifdef ANALYZER_MODE
        ++_verdictQueueOverflowCount;
#endif
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

unsigned long DetectionRuntime::verdictCorrelationFailureCount() const {
    return _verdictCorrelationFailureCount;
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
    const InspectedOccurrence* matchedInspectedOccurrence,
    const DetectorReport* matchedDetectorReport,
    unsigned long nowMs
) {
    _latestPipelineResult = {};
    _latestPipelineResult.hasVerdict = true;
    _latestPipelineResult.verdict = result;
    _latestPipelineResult.hasEvaluatorReport = true;
    _latestPipelineResult.evaluatorReport = _occurrenceEvaluator.report();
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
    event.integrity.evaluatorReportPresent = true;
    event.integrity.verdictPresent = true;
    event.integrity.occurrenceMatched = event.detectorReportMatched;
    event.integrity.correlationComplete = event.integrity.detectorReportPresent && event.integrity.inspectionPresent && event.integrity.verdictPresent;
    event.integrity.reason = event.integrity.correlationComplete
        ? PipelineIntegrityReason::None
        : (event.integrity.detectorReportPresent
            ? (event.integrity.inspectionPresent ? PipelineIntegrityReason::MissingVerdict : PipelineIntegrityReason::MissingInspectedOccurrence)
            : PipelineIntegrityReason::MissingDetectorReport);
    event.hasVerdict = true;
    event.verdict = result;
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
        ++_verdictEventPushedCount;
    } else {
        ++_verdictEventDroppedCount;
    }
    return eventPushed;
}

bool DetectionRuntime::pushVerdictObservation(const PendingVerdictObservation& observation) {
    if (_verdictCorrelationCount == kResultQueueCapacity) {
        ++_verdictCorrelationQueueOverflowCount;
        return false;
    }

    const size_t writeIndex = (_verdictCorrelationReadIndex + _verdictCorrelationCount) % kResultQueueCapacity;
    _verdictCorrelationQueue[writeIndex] = observation;
    ++_verdictCorrelationCount;
    return true;
}

bool DetectionRuntime::popVerdictObservation(unsigned long occurrenceId, PendingVerdictObservation& out) {
    if (_verdictCorrelationCount == 0) {
        return false;
    }

    size_t matchOffset = 0;
    for (; matchOffset < _verdictCorrelationCount; ++matchOffset) {
        const size_t index = (_verdictCorrelationReadIndex + matchOffset) % kResultQueueCapacity;
        if (_verdictCorrelationQueue[index].inspected.occurrence.occurrenceId == occurrenceId) {
            out = _verdictCorrelationQueue[index];
            break;
        }
    }

    if (matchOffset >= _verdictCorrelationCount) {
        ++_verdictCorrelationFailureCount;
        return false;
    }

    for (size_t i = matchOffset; i + 1 < _verdictCorrelationCount; ++i) {
        const size_t from = (_verdictCorrelationReadIndex + i + 1) % kResultQueueCapacity;
        const size_t to = (_verdictCorrelationReadIndex + i) % kResultQueueCapacity;
        _verdictCorrelationQueue[to] = _verdictCorrelationQueue[from];
    }
    _verdictCorrelationReadIndex = (_verdictCorrelationReadIndex + _verdictCorrelationCount - 1) % kResultQueueCapacity;
    --_verdictCorrelationCount;
    return true;
}

#endif // ANALYZER_MODE

} // namespace detection


