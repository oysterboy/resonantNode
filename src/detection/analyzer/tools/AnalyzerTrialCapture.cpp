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
    const detection::PatternResult& patternResult,
    const detection::InspectedOccurrence* selectedInspectedOccurrence,
    const detection::FrequencyBandMeasurementPacket* liveFrequencyMeasurementPacket
) {
    if (!_sequenceTest.active || _sequenceTest.currentTrial == 0) {
        return;
    }

    auto& diagnostics = _sequenceTest.currentTrialDiagnostics;
    diagnostics.rawPendingCount++;

    const unsigned long onsetMs = patternResult.primaryStartMs;
    const long dtFromTriggerMs = static_cast<long>(onsetMs) - static_cast<long>(_sequenceTest.currentTrialScheduledAtMs);
    const long dtFromTrialStartMs = static_cast<long>(onsetMs) - static_cast<long>(_sequenceTest.currentTrialStartMs);

    const bool bufferOverrunSeenNow = patternResult.primaryAudioOverflow
                                      || _audioSource.stats().overflowCount != _sequenceTest.trialOverflowCountAtStart;
    if (bufferOverrunSeenNow) {
        _sequenceTest.bufferOverrun = true;
    }

    const detection::DetectorReport selectedDetectorReport = _detection.activeDetectorReport();
    const bool selectedDetectorReportAvailable = selectedDetectorReport.detectorId != detection::DetectorId::Unknown;

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
        entry.durationMs = patternResult.primaryDurationMs;
        entry.strength = patternResult.primaryStrength;
        entry.origin = origin;
        entry.peakMs = patternResult.primaryPeakMs;
        entry.endDtMs = dtFromTriggerMs >= 0 ? dtFromTriggerMs + static_cast<long>(patternResult.primaryDurationMs) : -1;
        entry.patternValid = patternResult.valid;
        entry.patternAccepted = patternResult.patternAccepted;
        entry.patternMatched = patternResult.patternMatched;
        entry.supportMatched = patternResult.supportMatched;
        entry.behaviorEligible = patternResult.valid;
        entry.duplicatePending = duplicatePending;
        entry.pendingClass = pendingClass;
        entry.patternType = patternResult.type;
        entry.reasonCode = patternResult.reasonCode;
        entry.rejectReasonCode = patternResult.rejectReason;
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

    if (!diagnostics.bestPendingAccepted || patternResult.primaryStrength > diagnostics.bestPendingStrength) {
        diagnostics.bestPendingAccepted = true;
        diagnostics.bestPendingDtFromTriggerMs = dtFromTriggerMs;
        diagnostics.bestPendingDurationMs = patternResult.primaryDurationMs;
        diagnostics.bestPendingStrength = patternResult.primaryStrength;
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
        && !patternResult.uncertain
        && !_sequenceTest.primaryValidPatternCaptured
        && !_sequenceTest.primaryAcceptedOccurrenceCaptured) {
        _sequenceTest.primaryAcceptedOccurrenceCaptured = true;
        _sequenceTest.primaryAcceptedInspectedOccurrence = *selectedInspectedOccurrence;
        if (selectedDetectorReportAvailable) {
            _sequenceTest.primaryAcceptedDetectorReport = selectedDetectorReport;
            _sequenceTest.primaryAcceptedDetectorReport.sourceSelection = "selected_occurrence";
            _sequenceTest.primaryAcceptedDetectorReport.sourceOccurrenceId = selectedInspectedOccurrence->occurrence.occurrenceId;
            _sequenceTest.primaryAcceptedDetectorReport.sourceCandidateId = selectedInspectedOccurrence->occurrence.occurrenceId;
            _sequenceTest.primaryAcceptedDetectorReport.sourceReportMatched = true;
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
        _sequenceTest.currentTrialDiagnostics.patternAccepted = true;
        _sequenceTest.currentTrialDiagnostics.acceptedPatternMs = onsetMs;
        _sequenceTest.currentTrialDiagnostics.acceptedPatternOnsetStrength = patternResult.primaryOnsetStrength;
        _sequenceTest.currentTrialDiagnostics.acceptedPatternStrength = patternResult.primaryStrength;
        _sequenceTest.currentTrialDiagnostics.acceptedPatternDurationMs = patternResult.primaryDurationMs;
        _sequenceTest.currentTrialDiagnostics.acceptedPatternReleaseStrength = patternResult.primaryReleaseStrength;
        _sequenceTest.currentTrialDiagnostics.acceptedPatternPeakMs = patternResult.primaryPeakMs;
        _sequenceTest.currentTrialDiagnostics.acceptedPatternReleaseMs = patternResult.primaryStartMs + patternResult.primaryDurationMs;
        _sequenceTest.currentTrialDiagnostics.acceptedAmbientBaseline = patternResult.primaryAmbientBaseline;
        _sequenceTest.currentTrialDiagnostics.lastRejectStrength = 0.0f;
        _sequenceTest.currentTrialDiagnostics.lastRejectDurationMs = 0;
        _sequenceTest.currentTrialPatternDetectedMs = onsetMs;
    }

    if (!patternResult.valid) {
        const bool shouldUpdateBestRejected = !_sequenceTest.bestRejectedPatternCaptured
            || patternResult.primaryStrength > _sequenceTest.bestRejectedInWindow.primaryStrength;
        if (shouldUpdateBestRejected) {
            _sequenceTest.bestRejectedPatternCaptured = true;
            _sequenceTest.bestRejectedInWindow = patternResult;
            _sequenceTest.bestRejectedInspectedOccurrence = {};
            _sequenceTest.bestRejectedDetectorReport = {};
            if (selectedInspectedOccurrence != nullptr && selectedInspectedOccurrence->occurrence.present) {
                _sequenceTest.bestRejectedInspectedOccurrence = *selectedInspectedOccurrence;
                if (selectedDetectorReportAvailable) {
                    _sequenceTest.bestRejectedDetectorReport = selectedDetectorReport;
                    _sequenceTest.bestRejectedDetectorReport.sourceSelection = "selected_reject";
                    _sequenceTest.bestRejectedDetectorReport.sourceOccurrenceId = selectedInspectedOccurrence->occurrence.occurrenceId;
                    _sequenceTest.bestRejectedDetectorReport.sourceCandidateId = selectedInspectedOccurrence->occurrence.occurrenceId;
                    _sequenceTest.bestRejectedDetectorReport.sourceReportMatched = true;
                }
            }
        }
        if (!patternResult.uncertain) {
            _sequenceTest.rejectedInWindowCount++;
            _sequenceTest.currentTrialRejected++;
        }
        return;
    }

    const bool hadPrimaryBeforePending = _sequenceTest.primaryValidPatternCaptured;

    if (!hadPrimaryBeforePending) {
        _sequenceTest.primaryValidPatternCaptured = true;
        _sequenceTest.primaryValidPattern = patternResult;
        if (selectedInspectedOccurrence != nullptr && selectedInspectedOccurrence->occurrence.present) {
            _sequenceTest.primaryValidInspectedOccurrence = *selectedInspectedOccurrence;
            if (selectedDetectorReportAvailable) {
                _sequenceTest.primaryValidDetectorReport = selectedDetectorReport;
                _sequenceTest.primaryValidDetectorReport.sourceSelection = "selected_occurrence";
                _sequenceTest.primaryValidDetectorReport.sourceOccurrenceId = selectedInspectedOccurrence->occurrence.occurrenceId;
                _sequenceTest.primaryValidDetectorReport.sourceCandidateId = selectedInspectedOccurrence->occurrence.occurrenceId;
                _sequenceTest.primaryValidDetectorReport.sourceReportMatched = true;
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
            diagnostics.duplicatePatternStrength = patternResult.primaryStrength;
            diagnostics.duplicatePatternDurationMs = patternResult.primaryDurationMs;
            diagnostics.duplicatePatternPeakMs = patternResult.primaryPeakMs;
            diagnostics.duplicatePatternReleaseMs = patternResult.primaryStartMs + patternResult.primaryDurationMs;
            diagnostics.duplicateDeltaFromPrimaryMs = diagnostics.patternAccepted
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

    _sequenceTest.currentTrialDiagnostics.patternAccepted = true;
    _sequenceTest.currentTrialDiagnostics.acceptedPatternMs = onsetMs;
    _sequenceTest.currentTrialDiagnostics.acceptedPatternOnsetStrength = patternResult.primaryOnsetStrength;
    _sequenceTest.currentTrialDiagnostics.acceptedPatternStrength = patternResult.primaryStrength;
    _sequenceTest.currentTrialDiagnostics.acceptedPatternDurationMs = patternResult.primaryDurationMs;
    _sequenceTest.currentTrialDiagnostics.acceptedPatternReleaseStrength = patternResult.primaryReleaseStrength;
    _sequenceTest.currentTrialDiagnostics.acceptedPatternPeakMs = patternResult.primaryPeakMs;
    _sequenceTest.currentTrialDiagnostics.acceptedPatternReleaseMs = patternResult.primaryStartMs + patternResult.primaryDurationMs;
    _sequenceTest.currentTrialDiagnostics.acceptedAmbientBaseline = patternResult.primaryAmbientBaseline;
    _sequenceTest.currentTrialDiagnostics.lastRejectStrength = 0.0f;
    _sequenceTest.currentTrialDiagnostics.lastRejectDurationMs = 0;
    _sequenceTest.currentTrialPatternDetectedMs = onsetMs;
}
