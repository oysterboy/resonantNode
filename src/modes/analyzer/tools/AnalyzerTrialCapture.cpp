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

void AnalyzerApp::handleSequencePending(
    const detection::DetectionPipelineEvent& event,
    const detection::FrequencyBandMeasurementPacket* liveFrequencyMeasurementPacket
) {
    if (!_sequenceTest.active || _sequenceTest.currentTrial == 0) {
        return;
    }

    auto& diagnostics = _sequenceTest.currentTrialDiagnostics;
    const bool rejectHasIdentity =
        event.kind == detection::DetectionEventKind::RejectedSourceCandidate &&
        event.hasSourceRecord &&
        event.sourceRecord.eventId != 0 &&
        event.sourceRecord.reportGeneration != 0;
    const bool rejectAlreadyConsumed =
        rejectHasIdentity &&
        event.sourceRecord.eventId == _sequenceTest.consumedSourceRejectEventId &&
        event.sourceRecord.reportGeneration == _sequenceTest.consumedSourceRejectReportGeneration;
    if (rejectAlreadyConsumed) {
        return;
    }

    ++_sequenceTest.sourceCandidateCount;
    if (event.hasVerdict) {
        ++_sequenceTest.verdictCount;
    }
    if (event.hasInspectedOccurrence && event.inspectedOccurrence.occurrence.present) {
        ++_sequenceTest.inspectedOccurrenceCount;
    }
    if (event.kind == detection::DetectionEventKind::AcceptedPipelineResult) {
        ++_sequenceTest.sourceAcceptedCount;
    } else if (event.kind == detection::DetectionEventKind::RejectedSourceCandidate) {
        ++_sequenceTest.sourceRejectedCount;
        if (rejectHasIdentity) {
            _sequenceTest.consumedSourceRejectEventId = event.sourceRecord.eventId;
            _sequenceTest.consumedSourceRejectReportGeneration = event.sourceRecord.reportGeneration;
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
            _sequenceTest.selectedSourceRejectCaptured = true;
            _sequenceTest.selectedSourceReject = event.sourceRecord;
            _sequenceTest.selectedSourceReject.eventTrialAttribution = _sequenceTest.currentTrial;
            _sequenceTest.consumedSourceRejectEventId = event.sourceRecord.eventId;
            _sequenceTest.consumedSourceRejectReportGeneration = event.sourceRecord.reportGeneration;
        }
        const unsigned long sourceOnsetMs = event.hasSourceRecord && event.sourceRecord.detectorReport.selectedReject.present
            ? event.sourceRecord.detectorReport.selectedReject.startMs
            : 0UL;
        const long dtFromTriggerMs = sourceOnsetMs > 0
            ? static_cast<long>(timing::delta(sourceOnsetMs, _sequenceTest.currentTrialScheduledAtMs))
            : 0L;
        const bool preWindow = sourceOnsetMs > 0 && timing::beforeDeadline(sourceOnsetMs, _sequenceTest.currentTrialStartMs + _sequenceTest.windowStartOffsetMs);
        const bool postWindow = timing::after(sourceOnsetMs, _sequenceTest.currentTrialEndMs);
        const bool inWindow = sourceOnsetMs > 0 && !preWindow && !postWindow;
        if (inWindow) {
            _sequenceTest.rejectedInWindowCount++;
            _sequenceTest.currentTrialRejected++;
        }
        diagnostics.runtimePatternCaptured = diagnostics.runtimePatternCaptured || event.hasVerdict;
        (void)dtFromTriggerMs;
        if (liveFrequencyMeasurementPacket != nullptr) {
            (void)liveFrequencyMeasurementPacket;
        }
        return;
    }

    if (verdict == nullptr) {
        return;
    }

    const unsigned long onsetMs = verdict->primaryStartMs;
    const long dtFromTriggerMs = static_cast<long>(timing::delta(onsetMs, _sequenceTest.currentTrialScheduledAtMs));

    const bool bufferOverrunSeenNow = verdict->primaryAudioOverflow
                                      || _audioSource.stats().overflowCount != _sequenceTest.trialOverflowCountAtStart;
    if (bufferOverrunSeenNow) {
        _sequenceTest.bufferOverrun = true;
    }

    const char* selectedSourceSelection = selectedInspectedOccurrence != nullptr &&
        selectedInspectedOccurrence->decision == detection::OccurrenceDecision::Rejected
        ? "selected_reject"
        : "selected_occurrence";

    const bool preWindow = timing::beforeDeadline(onsetMs, _sequenceTest.currentTrialStartMs + _sequenceTest.windowStartOffsetMs);
    const bool postWindow = timing::after(onsetMs, _sequenceTest.currentTrialEndMs);
    const bool inWindow = !preWindow && !postWindow;

    if (!inWindow) {
        if (!_sequenceTest.bufferOverrun) {
            _sequenceTest.currentTrialUnexpected++;
        }
        return;
    }

    if (selectedInspectedOccurrence != nullptr
        && selectedInspectedOccurrence->occurrence.present
        && selectedInspectedOccurrence->decision == detection::OccurrenceDecision::Accepted
        && !_sequenceTest.primaryAcceptedOccurrenceCaptured) {
        _sequenceTest.primaryAcceptedOccurrenceCaptured = true;
        _sequenceTest.primaryAcceptedInspectedOccurrence = *selectedInspectedOccurrence;
        if (selectedDetectorReportAvailable) {
            _sequenceTest.primaryAcceptedDetectorReport = *selectedDetectorReport;
        }
        if (event.hasSourceRecord) {
            _sequenceTest.primaryAcceptedSourceRecord = event.sourceRecord;
            _sequenceTest.primaryAcceptedSourceRecord.eventTrialAttribution = _sequenceTest.currentTrial;
        }
        _sequenceTest.currentTrialDiagnostics.onsetSeen = true;
        if (_sequenceTest.currentTrialDiagnostics.firstOnsetMs == 0) {
            _sequenceTest.currentTrialDiagnostics.firstOnsetMs = onsetMs;
        }
        if (_sequenceTest.currentTrialOnsetDetectedMs == 0) {
            _sequenceTest.currentTrialOnsetDetectedMs = onsetMs;
        }
        _sequenceTest.currentTrialDiagnostics.accepted = true;
        _sequenceTest.currentTrialDiagnostics.acceptedPatternMs = onsetMs;
        _sequenceTest.currentTrialDiagnostics.acceptedPatternStrength = verdict->primaryStrength;
        _sequenceTest.currentTrialDiagnostics.acceptedPatternDurationMs = verdict->primaryDurationMs;
        _sequenceTest.currentTrialDiagnostics.acceptedAmbientBaseline = verdict->primaryAmbientBaseline;
        _sequenceTest.currentTrialPatternDetectedMs = onsetMs;
    }

    if (!verdict->valid) {
        const bool shouldUpdateBestRejected = !_sequenceTest.bestRejectedPatternCaptured
            || verdict->primaryStrength > _sequenceTest.bestRejectedInWindow.primaryStrength;
        if (shouldUpdateBestRejected) {
            _sequenceTest.bestRejectedPatternCaptured = true;
            _sequenceTest.bestRejectedInWindow = *verdict;
            _sequenceTest.bestRejectedInspectedOccurrence = {};
            _sequenceTest.bestRejectedDetectorReport = {};
            if (selectedInspectedOccurrence != nullptr && selectedInspectedOccurrence->occurrence.present) {
                _sequenceTest.bestRejectedInspectedOccurrence = *selectedInspectedOccurrence;
                if (selectedDetectorReportAvailable) {
                    _sequenceTest.bestRejectedDetectorReport = *selectedDetectorReport;
                }
            }
        }
        if (!verdict->valid) {
            _sequenceTest.rejectedInWindowCount++;
            _sequenceTest.currentTrialRejected++;
        }
        return;
    }

    const bool hadPrimaryBeforePending = _sequenceTest.primaryValidPatternCaptured;

    if (!hadPrimaryBeforePending) {
        _sequenceTest.primaryValidPatternCaptured = true;
        _sequenceTest.primaryValidPattern = *verdict;
        if (selectedInspectedOccurrence != nullptr && selectedInspectedOccurrence->occurrence.present) {
            _sequenceTest.primaryValidInspectedOccurrence = *selectedInspectedOccurrence;
            if (selectedDetectorReportAvailable) {
                _sequenceTest.primaryValidDetectorReport = *selectedDetectorReport;
            }
        }
    }

    _sequenceTest.currentTrialDiagnostics.onsetSeen = true;
    if (_sequenceTest.currentTrialDiagnostics.firstOnsetMs == 0) {
        _sequenceTest.currentTrialDiagnostics.firstOnsetMs = onsetMs;
    }
    if (_sequenceTest.currentTrialOnsetDetectedMs == 0) {
        _sequenceTest.currentTrialOnsetDetectedMs = onsetMs;
    }

    if (hadPrimaryBeforePending) {
        if (diagnostics.duplicateCount == 0) {
            diagnostics.duplicatePatternMs = onsetMs;
            diagnostics.duplicatePatternStrength = verdict->primaryStrength;
            diagnostics.duplicatePatternDurationMs = verdict->primaryDurationMs;
            strncpy(diagnostics.duplicateReason, "duplicate_after_primary", sizeof(diagnostics.duplicateReason) - 1);
            diagnostics.duplicateReason[sizeof(diagnostics.duplicateReason) - 1] = '\0';
        }
        _sequenceTest.currentTrialDiagnostics.duplicateCount++;
        return;
    }

    _sequenceTest.currentTrialDiagnostics.accepted = true;
    _sequenceTest.currentTrialDiagnostics.acceptedPatternMs = onsetMs;
    _sequenceTest.currentTrialDiagnostics.acceptedPatternStrength = verdict->primaryStrength;
    _sequenceTest.currentTrialDiagnostics.acceptedPatternDurationMs = verdict->primaryDurationMs;
    _sequenceTest.currentTrialDiagnostics.acceptedAmbientBaseline = verdict->primaryAmbientBaseline;
    _sequenceTest.currentTrialPatternDetectedMs = onsetMs;
}
