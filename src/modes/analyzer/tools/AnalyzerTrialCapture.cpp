#include "../../../detection/analyzer/AnalyzerTrialClassifier.h"
#include "../AnalyzerModeApp.h"

#include <Arduino.h>
#include <string.h>

#include "../../../app/TimingUtils.h"
#include "../../../detection/features/FrequencyMeasurementPacketBuilder.h"

namespace {


} // namespace

detection::FrequencyBandMeasurementPacket IRAM_ATTR AnalyzerApp::captureFrequencyMeasurementPacket(const AudioSamplePacket& audioSamplePacket) const {
    return detection::buildFrequencyMeasurementPacket(_freqBandStream, audioSamplePacket);
}

void AnalyzerApp::handleSequencePending(const detection::DetectionPipelineEvent& event) {
    if (!_sequenceTest.active || _sequenceTest.trial.number == 0) {
        return;
    }

    auto& diagnostics = _sequenceTest.trial.diagnostics;
    const bool rejectHasIdentity =
        event.kind == detection::DetectionEventKind::RejectedSourceCandidate &&
        event.hasSourceRecord &&
        event.sourceRecord.eventId != 0 &&
        event.sourceRecord.reportGeneration != 0;
    const bool rejectAlreadyConsumed =
        rejectHasIdentity &&
        event.sourceRecord.eventId == _sequenceTest.totals.consumedSourceRejectEventId &&
        event.sourceRecord.reportGeneration == _sequenceTest.totals.consumedSourceRejectReportGeneration;
    if (rejectAlreadyConsumed) {
        return;
    }

    ++_sequenceTest.trial.sourceCandidateCount;
    if (event.hasVerdict) {
        ++_sequenceTest.trial.verdictCount;
    }
    if (event.hasInspectedOccurrence && event.inspectedOccurrence.occurrence.present) {
        ++_sequenceTest.trial.inspectedOccurrenceCount;
    }
    if (event.kind == detection::DetectionEventKind::AcceptedPipelineResult) {
        ++_sequenceTest.trial.sourceAcceptedCount;
    } else if (event.kind == detection::DetectionEventKind::RejectedSourceCandidate) {
        ++_sequenceTest.trial.sourceRejectedCount;
        if (rejectHasIdentity) {
            _sequenceTest.totals.consumedSourceRejectEventId = event.sourceRecord.eventId;
            _sequenceTest.totals.consumedSourceRejectReportGeneration = event.sourceRecord.reportGeneration;
        }
    }

    const detection::DetectorReport* selectedDetectorReport = event.hasSourceRecord
        ? &event.sourceRecord.detectorReport
        : &_detection.diagnostics().activeDetectorReport();
    const bool selectedDetectorReportAvailable = selectedDetectorReport != nullptr &&
        selectedDetectorReport->detectorId != detection::DetectorId::Unknown;
    const detection::InspectedOccurrence* selectedInspectedOccurrence =
        event.hasInspectedOccurrence && event.inspectedOccurrence.occurrence.present
            ? &event.inspectedOccurrence
            : nullptr;
    const detection::OccurrenceVerdict* verdict = event.hasVerdict ? &event.verdict : nullptr;

    if (event.kind == detection::DetectionEventKind::RejectedSourceCandidate) {
        if (event.hasSourceRecord) {
            _sequenceTest.trial.selectedSourceRejectCaptured = true;
            _sequenceTest.trial.selectedSourceReject = event.sourceRecord;
            _sequenceTest.trial.selectedSourceReject.eventTrialAttribution = _sequenceTest.trial.number;
            _sequenceTest.totals.consumedSourceRejectEventId = event.sourceRecord.eventId;
            _sequenceTest.totals.consumedSourceRejectReportGeneration = event.sourceRecord.reportGeneration;
        }
        const unsigned long sourceOnsetMs = event.hasSourceRecord && event.sourceRecord.detectorReport.selectedReject.present
            ? event.sourceRecord.detectorReport.selectedReject.startMs
            : 0UL;
        const bool preWindow = sourceOnsetMs > 0 && timing::beforeDeadline(sourceOnsetMs, _sequenceTest.trial.startMs);
        const bool postWindow = timing::after(sourceOnsetMs, _sequenceTest.trial.endMs);
        const bool inWindow = sourceOnsetMs > 0 && !preWindow && !postWindow;
        if (inWindow) {
            _sequenceTest.trial.rejectedInWindowCount++;
            _sequenceTest.trial.rejected++;
        }
        diagnostics.runtimePatternCaptured = diagnostics.runtimePatternCaptured || event.hasVerdict;
        return;
    }

    if (verdict == nullptr) {
        return;
    }

    const unsigned long onsetMs = verdict->primaryStartMs;

    const bool bufferOverrunSeenNow = verdict->primaryAudioOverflow
                                      || _audioSource.stats().overflowCount != _sequenceTest.trial.overflowCountAtStart;
    if (bufferOverrunSeenNow) {
        _sequenceTest.trial.bufferOverrun = true;
    }

    const char* selectedSourceSelection = selectedInspectedOccurrence != nullptr &&
        selectedInspectedOccurrence->decision == detection::OccurrenceDecision::Rejected
        ? "selected_reject"
        : "selected_occurrence";

    const bool preWindow = timing::beforeDeadline(onsetMs, _sequenceTest.trial.startMs);
    const bool postWindow = timing::after(onsetMs, _sequenceTest.trial.endMs);
    const bool inWindow = !preWindow && !postWindow;

    if (!inWindow) {
        if (!_sequenceTest.trial.bufferOverrun) {
            _sequenceTest.trial.unexpected++;
        }
        return;
    }

    if (selectedInspectedOccurrence != nullptr
        && selectedInspectedOccurrence->occurrence.present
        && selectedInspectedOccurrence->decision == detection::OccurrenceDecision::Accepted
        && !_sequenceTest.trial.primaryAcceptedOccurrenceCaptured) {
        _sequenceTest.trial.primaryAcceptedOccurrenceCaptured = true;
        _sequenceTest.trial.primaryAcceptedInspectedOccurrence = *selectedInspectedOccurrence;
        if (selectedDetectorReportAvailable) {
            _sequenceTest.trial.primaryAcceptedDetectorReport = *selectedDetectorReport;
        }
        if (event.hasSourceRecord) {
            _sequenceTest.trial.primaryAcceptedSourceRecord = event.sourceRecord;
            _sequenceTest.trial.primaryAcceptedSourceRecord.eventTrialAttribution = _sequenceTest.trial.number;
        }
        if (_sequenceTest.trial.diagnostics.firstOnsetMs == 0) {
            _sequenceTest.trial.diagnostics.firstOnsetMs = onsetMs;
        }
        if (_sequenceTest.trial.onsetDetectedMs == 0) {
            _sequenceTest.trial.onsetDetectedMs = onsetMs;
        }
        _sequenceTest.trial.diagnostics.accepted = true;
        _sequenceTest.trial.diagnostics.acceptedPatternMs = onsetMs;
        _sequenceTest.trial.diagnostics.acceptedPatternDurationMs = verdict->primaryDurationMs;
        _sequenceTest.trial.diagnostics.acceptedAmbientBaseline = verdict->primaryAmbientBaseline;
        _sequenceTest.trial.patternDetectedMs = onsetMs;
    }

    if (!verdict->valid) {
        const bool shouldUpdateBestRejected = !_sequenceTest.trial.bestRejectedPatternCaptured
            || verdict->primaryStrength > _sequenceTest.trial.bestRejectedInWindow.primaryStrength;
        if (shouldUpdateBestRejected) {
            _sequenceTest.trial.bestRejectedPatternCaptured = true;
            _sequenceTest.trial.bestRejectedInWindow = *verdict;
            _sequenceTest.trial.bestRejectedInspectedOccurrence = {};
            _sequenceTest.trial.bestRejectedDetectorReport = {};
            if (selectedInspectedOccurrence != nullptr && selectedInspectedOccurrence->occurrence.present) {
                _sequenceTest.trial.bestRejectedInspectedOccurrence = *selectedInspectedOccurrence;
                if (selectedDetectorReportAvailable) {
                    _sequenceTest.trial.bestRejectedDetectorReport = *selectedDetectorReport;
                }
            }
        }
        if (!verdict->valid) {
            _sequenceTest.trial.rejectedInWindowCount++;
            _sequenceTest.trial.rejected++;
        }
        return;
    }

    const bool hadPrimaryBeforePending = _sequenceTest.trial.primaryValidPatternCaptured;

    if (!hadPrimaryBeforePending) {
        _sequenceTest.trial.primaryValidPatternCaptured = true;
        _sequenceTest.trial.primaryValidPattern = *verdict;
        if (selectedInspectedOccurrence != nullptr && selectedInspectedOccurrence->occurrence.present) {
            _sequenceTest.trial.primaryValidInspectedOccurrence = *selectedInspectedOccurrence;
            if (selectedDetectorReportAvailable) {
                _sequenceTest.trial.primaryValidDetectorReport = *selectedDetectorReport;
            }
        }
    }

    if (_sequenceTest.trial.diagnostics.firstOnsetMs == 0) {
        _sequenceTest.trial.diagnostics.firstOnsetMs = onsetMs;
    }
    if (_sequenceTest.trial.onsetDetectedMs == 0) {
        _sequenceTest.trial.onsetDetectedMs = onsetMs;
    }

    if (hadPrimaryBeforePending) {
        _sequenceTest.trial.diagnostics.duplicateCount++;
        return;
    }

    _sequenceTest.trial.diagnostics.accepted = true;
    _sequenceTest.trial.diagnostics.acceptedPatternMs = onsetMs;
    _sequenceTest.trial.diagnostics.acceptedPatternDurationMs = verdict->primaryDurationMs;
    _sequenceTest.trial.diagnostics.acceptedAmbientBaseline = verdict->primaryAmbientBaseline;
    _sequenceTest.trial.patternDetectedMs = onsetMs;
}
