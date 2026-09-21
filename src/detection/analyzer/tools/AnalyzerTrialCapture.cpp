#include "../../../modes/analyzer/AnalyzerModeApp.h"

#include <Arduino.h>
#include <string.h>

#include "../../features/FrequencyMeasurementPacketBuilder.h"

namespace {

constexpr long kLateOnsetMinMs = 200L;

enum class SequencePendingClass : uint8_t {
    Unknown = 0,
    ExpectedPrimary,
    Late,
    UnexpectedNoise,
    Duplicate,
};

uint8_t sequencePendingClass(bool duplicatePending, bool inWindow, long dtFromTriggerMs) {
    if (duplicatePending) {
        return static_cast<uint8_t>(SequencePendingClass::Duplicate);
    }
    if (!inWindow) {
        return static_cast<uint8_t>(SequencePendingClass::UnexpectedNoise);
    }
    if (dtFromTriggerMs >= kLateOnsetMinMs) {
        return static_cast<uint8_t>(SequencePendingClass::Late);
    }
    return static_cast<uint8_t>(SequencePendingClass::ExpectedPrimary);
}

} // namespace

detection::FrequencyBandMeasurementPacket AnalyzerApp::captureFrequencyMeasurementPacket(const AudioSamplePacket& audioSamplePacket) const {
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

    diagnostics.rawPendingCount++;
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
        : &_detection.activeDetectorReport();
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
            ? static_cast<long>(sourceOnsetMs) - static_cast<long>(_sequenceTest.currentTrialScheduledAtMs)
            : 0L;
        const bool preWindow = sourceOnsetMs > 0 && sourceOnsetMs < _sequenceTest.currentTrialStartMs + _sequenceTest.windowStartOffsetMs;
        const bool postWindow = sourceOnsetMs > _sequenceTest.currentTrialEndMs;
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
    const long dtFromTriggerMs = static_cast<long>(onsetMs) - static_cast<long>(_sequenceTest.currentTrialScheduledAtMs);
    const long dtFromTrialStartMs = static_cast<long>(onsetMs) - static_cast<long>(_sequenceTest.currentTrialStartMs);

    const bool bufferOverrunSeenNow = verdict->primaryAudioOverflow
                                      || _audioSource.stats().overflowCount != _sequenceTest.trialOverflowCountAtStart;
    if (bufferOverrunSeenNow) {
        _sequenceTest.bufferOverrun = true;
    }

    const char* selectedSourceSelection = selectedInspectedOccurrence != nullptr &&
        selectedInspectedOccurrence->decision == detection::OccurrenceDecision::Rejected
        ? "selected_reject"
        : "selected_occurrence";

    const bool preWindow = onsetMs < _sequenceTest.currentTrialStartMs + _sequenceTest.windowStartOffsetMs;
    const bool postWindow = onsetMs > _sequenceTest.currentTrialEndMs;
    const bool inWindow = !preWindow && !postWindow;
    const bool duplicatePending = _sequenceTest.primaryValidPatternCaptured && inWindow;
    const auto pendingClass = sequencePendingClass(duplicatePending, inWindow, dtFromTriggerMs);

    const SequenceTest::PendingOrigin origin = preWindow
        ? SequenceTest::PendingOrigin::PreWindow
        : postWindow
            ? SequenceTest::PendingOrigin::PostWindow
            : SequenceTest::PendingOrigin::InWindow;

    if (diagnostics.firstPendingMs == 0) {
        diagnostics.firstPendingMs = onsetMs;
    }

    if (diagnostics.pendingCount < SequenceTest::kMaxTrialPending) {
        auto& entry = diagnostics.pendingSamples[diagnostics.pendingCount++];
        entry.pendingMs = onsetMs;
        entry.dtFromTriggerMs = dtFromTriggerMs;
        entry.dtFromTrialStartMs = dtFromTrialStartMs;
        entry.durationMs = verdict->primaryDurationMs;
        entry.strength = verdict->primaryStrength;
        entry.origin = origin;
        entry.peakMs = verdict->primaryPeakMs;
        entry.endDtMs = dtFromTriggerMs >= 0 ? dtFromTriggerMs + static_cast<long>(verdict->primaryDurationMs) : -1;
        entry.valid = verdict->valid;
        entry.accepted = verdict->accepted;
        entry.proposalMatched = verdict->proposalMatched;
        entry.supportMatched = verdict->supportMatched;
        entry.behaviorEligible = verdict->valid;
        entry.duplicatePending = duplicatePending;
        entry.pendingClass = pendingClass;
        entry.verdictType = verdict->type;
        entry.reasonCode = verdict->reasonCode;
        entry.rejectReasonCode = verdict->rejectReason;
    } else {
        diagnostics.pendingOverflowCount++;
    }

    if (origin == SequenceTest::PendingOrigin::PreWindow) {
        diagnostics.pendingPreWindowCount++;
    } else if (origin == SequenceTest::PendingOrigin::InWindow) {
        diagnostics.pendingInWindowCount++;
    } else {
        diagnostics.pendingPostWindowCount++;
    }

    if (!diagnostics.bestPendingAccepted || verdict->primaryStrength > diagnostics.bestPendingStrength) {
        diagnostics.bestPendingAccepted = true;
        diagnostics.bestPendingDtFromTriggerMs = dtFromTriggerMs;
        diagnostics.bestPendingDurationMs = verdict->primaryDurationMs;
        diagnostics.bestPendingStrength = verdict->primaryStrength;
        diagnostics.bestPendingOrigin = origin;
    }

    if (!inWindow) {
        if (!_sequenceTest.bufferOverrun) {
            _sequenceTest.unexpected++;
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
        _sequenceTest.primaryAcceptedOccurrenceDtMs = dtFromTriggerMs;
        _sequenceTest.currentTrialDiagnostics.onsetSeen = true;
        if (_sequenceTest.currentTrialDiagnostics.firstOnsetMs == 0) {
            _sequenceTest.currentTrialDiagnostics.firstOnsetMs = onsetMs;
        }
        _sequenceTest.currentTrialDiagnostics.lastOnsetMs = onsetMs;
        if (_sequenceTest.currentTrialOnsetDetectedMs == 0) {
            _sequenceTest.currentTrialOnsetDetectedMs = onsetMs;
        }
        _sequenceTest.currentTrialDiagnostics.accepted = true;
        _sequenceTest.currentTrialDiagnostics.acceptedPatternMs = onsetMs;
        _sequenceTest.currentTrialDiagnostics.acceptedPatternOnsetStrength = verdict->primaryOnsetStrength;
        _sequenceTest.currentTrialDiagnostics.acceptedPatternStrength = verdict->primaryStrength;
        _sequenceTest.currentTrialDiagnostics.acceptedPatternDurationMs = verdict->primaryDurationMs;
        _sequenceTest.currentTrialDiagnostics.acceptedPatternReleaseStrength = verdict->primaryReleaseStrength;
        _sequenceTest.currentTrialDiagnostics.acceptedPatternPeakMs = verdict->primaryPeakMs;
        _sequenceTest.currentTrialDiagnostics.acceptedPatternReleaseMs = verdict->primaryStartMs + verdict->primaryDurationMs;
        _sequenceTest.currentTrialDiagnostics.acceptedAmbientBaseline = verdict->primaryAmbientBaseline;
        _sequenceTest.currentTrialDiagnostics.lastRejectStrength = 0.0f;
        _sequenceTest.currentTrialDiagnostics.lastRejectDurationMs = 0;
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
        _sequenceTest.primaryValidPatternDtMs = dtFromTriggerMs;
    }

    _sequenceTest.currentTrialDiagnostics.onsetSeen = true;
    if (_sequenceTest.currentTrialDiagnostics.firstOnsetMs == 0) {
        _sequenceTest.currentTrialDiagnostics.firstOnsetMs = onsetMs;
    }
    _sequenceTest.currentTrialDiagnostics.lastOnsetMs = onsetMs;
    if (_sequenceTest.currentTrialOnsetDetectedMs == 0) {
        _sequenceTest.currentTrialOnsetDetectedMs = onsetMs;
    }

    if (hadPrimaryBeforePending) {
        if (diagnostics.duplicateCount == 0) {
            diagnostics.duplicatePatternMs = onsetMs;
            diagnostics.duplicatePatternStrength = verdict->primaryStrength;
            diagnostics.duplicatePatternDurationMs = verdict->primaryDurationMs;
            diagnostics.duplicatePatternPeakMs = verdict->primaryPeakMs;
            diagnostics.duplicatePatternReleaseMs = verdict->primaryStartMs + verdict->primaryDurationMs;
            diagnostics.duplicateDeltaFromPrimaryMs = diagnostics.accepted
                ? static_cast<long>(onsetMs) - static_cast<long>(diagnostics.acceptedPatternMs)
                : 0;
            strncpy(diagnostics.duplicateReason, "duplicate_after_primary", sizeof(diagnostics.duplicateReason) - 1);
            diagnostics.duplicateReason[sizeof(diagnostics.duplicateReason) - 1] = '\0';
        }
        _sequenceTest.currentTrialDiagnostics.duplicateCount++;
        if (_sequenceTest.currentTrialDiagnostics.duplicateDtCount < SequenceTest::kMaxDuplicateDts) {
            _sequenceTest.currentTrialDiagnostics.duplicateDts[_sequenceTest.currentTrialDiagnostics.duplicateDtCount++] = onsetMs >= _sequenceTest.currentTrialPatternDetectedMs
                ? onsetMs - _sequenceTest.currentTrialPatternDetectedMs
                : 0;
        }
        return;
    }

    _sequenceTest.currentTrialDiagnostics.accepted = true;
    _sequenceTest.currentTrialDiagnostics.acceptedPatternMs = onsetMs;
    _sequenceTest.currentTrialDiagnostics.acceptedPatternOnsetStrength = verdict->primaryOnsetStrength;
    _sequenceTest.currentTrialDiagnostics.acceptedPatternStrength = verdict->primaryStrength;
    _sequenceTest.currentTrialDiagnostics.acceptedPatternDurationMs = verdict->primaryDurationMs;
    _sequenceTest.currentTrialDiagnostics.acceptedPatternReleaseStrength = verdict->primaryReleaseStrength;
    _sequenceTest.currentTrialDiagnostics.acceptedPatternPeakMs = verdict->primaryPeakMs;
    _sequenceTest.currentTrialDiagnostics.acceptedPatternReleaseMs = verdict->primaryStartMs + verdict->primaryDurationMs;
    _sequenceTest.currentTrialDiagnostics.acceptedAmbientBaseline = verdict->primaryAmbientBaseline;
    _sequenceTest.currentTrialDiagnostics.lastRejectStrength = 0.0f;
    _sequenceTest.currentTrialDiagnostics.lastRejectDurationMs = 0;
    _sequenceTest.currentTrialPatternDetectedMs = onsetMs;
}
