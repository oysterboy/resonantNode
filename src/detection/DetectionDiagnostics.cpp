#include "DetectionDiagnostics.h"

// The Analyzer's variant only; the Node's NullDetectionDiagnostics is all
// inline in the header. Moved out of DetectionRuntime.cpp in step 11e item 7,
// bodies unchanged except for the member names.
#ifdef ANALYZER_MODE

namespace detection {

DetectionDiagnostics::DetectionDiagnostics(ScalarTransientDetector& detector, const FeatureHistory& history)
    : _detector(detector), _history(history) {}

void DetectionDiagnostics::resetCounters() {
    _detectorReport = {};
    _pipelineEventOverflowCount = 0;
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

void DetectionDiagnostics::resetBookkeeping() {
    _pipelineEvents.clear();
    _pipelineEventOverflowCount = 0;
    _pipelineEventSequenceId = 0;
    _lastEmittedAcceptedOccurrenceId = 0;
    _lastEmittedAcceptedReportGeneration = 0;
    _lastEmittedSelectedRejectOccurrenceId = 0;
    _lastEmittedSelectedRejectReportGeneration = 0;
    _detectorReport = {};
    _lastObservedScalarReportGeneration = 0;
}

void DetectionDiagnostics::onOccurrencePopped(const Occurrence& occurrence) {
    ++_detectorOccurrencePoppedCount;
    if (occurrence.present && occurrence.valid) {
        ++_detectorValidOccurrencePoppedCount;
    }
    const DetectorReport& poppedReport = _detector.latestReport();
    if (poppedReport.detectorId != occurrence.detectorId || !poppedReport.accepted.present ||
        poppedReport.accepted.occurrenceId != occurrence.occurrenceId) {
        ++_detectorReportMismatchCount;
    }
}

void DetectionDiagnostics::onInspected(const InspectedOccurrence& inspected) {
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
        _historyIncomplete.ringOldestMs = _history.oldestBinTimeMs(obs.stream);
        _historyIncomplete.ringNewestMs = _history.latestTimeMs(obs.stream);
        _historyIncomplete.ringBins = _history.binCount(obs.stream);
        _historyIncomplete.outOfOrderRecords = _history.outOfOrderRecordCount();
        break;
    }
}

bool DetectionDiagnostics::captureLatestDetectorReportIfChanged() {
    if (_lastObservedScalarReportGeneration == _detector.reportGeneration()) {
        return false;
    }
    _detectorReport = _detector.latestReport();
    _lastObservedScalarReportGeneration = _detector.reportGeneration();
    return true;
}

const DetectorReport& DetectionDiagnostics::activeDetectorReport() {
    captureLatestDetectorReportIfChanged();
    return _detectorReport;
}

bool DetectionDiagnostics::pushPipelineEvent(const DetectionPipelineEvent& event) {
    if (!_pipelineEvents.push(event)) {
        ++_pipelineEventOverflowCount;
        return false;
    }
    return true;
}

void DetectionDiagnostics::afterDrain() {
    if (!captureLatestDetectorReportIfChanged()) {
        return;
    }

    ++_detectorReportRefreshCount;
    // One reject event per new selected reject, not per refreeze (a weaker
    // reject refreezes for rejected_count; decision 2026-10-13).
    const uint32_t currentReportGeneration = _detector.reportGeneration();
    const uint32_t currentRejectGeneration = _detector.selectedRejectGeneration();

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

void DetectionDiagnostics::onVerdict(const OccurrenceVerdict& result, const InspectedOccurrence& inspected,
                                     const ReportSlot& slot, unsigned long nowMs) {
    (void)nowMs;
    ++_verdictProducedCount;
    const DetectorReport& detectorReport = slot.report;

    DetectionPipelineEvent event = {};
    event.kind = DetectionEventKind::AcceptedPipelineResult;
    event.eventId = ++_pipelineEventSequenceId;
    event.hasOccurrenceId = true;
    event.occurrenceId = static_cast<uint32_t>(result.occurrenceId);
    event.hasCandidateId = false;
    event.candidateId = 0;
    event.detectorReportPresent = detectorReport.detectorId != DetectorId::Unknown;
    event.detectorReportMatched =
        event.detectorReportPresent &&
        inspected.occurrence.present &&
        detectorReport.accepted.present &&
        detectorReport.accepted.occurrenceId == result.occurrenceId &&
        inspected.occurrence.occurrenceId == result.occurrenceId;
    event.sourceSelection = inspected.occurrence.present && inspected.decision == OccurrenceDecision::Rejected
        ? "selected_reject"
        : "selected_occurrence";
    event.sourceOccurrenceId = inspected.occurrence.present ? inspected.occurrence.occurrenceId : 0UL;
    event.sourceCandidateId = 0;
    event.integrity.detectorReportPresent = event.detectorReportPresent;
    event.integrity.inspectionPresent = event.detectorReportMatched;
    event.integrity.verdictPresent = true;
    event.integrity.occurrenceMatched = event.detectorReportMatched;
    event.integrity.correlationComplete =
        event.integrity.detectorReportPresent && event.integrity.inspectionPresent && event.integrity.verdictPresent;
    event.integrity.reason = event.integrity.correlationComplete
        ? PipelineIntegrityReason::None
        : (event.integrity.detectorReportPresent ? PipelineIntegrityReason::MissingVerdict
                                                 : PipelineIntegrityReason::MissingDetectorReport);
    event.hasVerdict = true;
    event.verdict = result;
    event.hasSourceRecord = true;
    event.sourceRecord.detectorReport = detectorReport;
    event.sourceRecord.sourceSelection = event.sourceSelection;
    event.sourceRecord.eventId = event.eventId;
    event.sourceRecord.reportGeneration = _detector.reportGeneration();
    event.sourceRecord.eventTrialAttribution = 0;
    event.sourceRecord.sourceOccurrenceId = event.sourceOccurrenceId;
    event.sourceRecord.sourceCandidateId = event.sourceCandidateId;
    event.sourceRecord.sourceReportMatched = event.detectorReportMatched;
    if (!event.detectorReportMatched) {
        ++_detectorReportMismatchCount;
    }
    if (inspected.occurrence.present) {
        event.hasInspectedOccurrence = true;
        event.inspectedOccurrence = inspected;
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
}

}  // namespace detection

#endif  // ANALYZER_MODE
