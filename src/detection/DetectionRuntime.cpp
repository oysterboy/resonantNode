#include "DetectionRuntime.h"

#include <string.h>

// DetectionRuntime pipeline execution in source order.
namespace detection {

DetectionRuntime::DetectionRuntime() = default;

namespace {

OccurrenceSource occurrenceSourceForStream(FeatureStreamId stream) {
    switch (stream) {
        case FeatureStreamId::FrequencyScore:
        case FeatureStreamId::FrequencyContrast:
            return OccurrenceSource::Frequency;
        case FeatureStreamId::AmpEnvelope:
        case FeatureStreamId::Unknown:
        default:
            return OccurrenceSource::Amp;
    }
}

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

const char* detectorSelectionNameForRuntime(DetectorSelection kind) {
    switch (kind) {
        case DetectorSelection::FrequencyMatch:
            return "frequency_match";
        case DetectorSelection::ScalarTransient:
            return "scalar_transient";
        default:
            return "unknown";
    }
}

bool reasonIsNone(const char* reason) {
    return reason == nullptr || strcmp(reason, "none") == 0;
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

const char* scalarRejectReasonOrFallback(const char* transientRejectReason, const char* onsetRejectReason) {
    return !reasonIsNone(transientRejectReason) ? transientRejectReason : onsetRejectReason;
}

void populateScalarLegacyDiagnosticsFromReport(DetectionDiagnostics& diagnostics, const DetectorReport& report) {
    diagnostics.scalarRejectReason = report.scalar.inspect.rejectReason;
    diagnostics.scalarNoEmitReason = report.scalar.inspect.noEmitReason;
    diagnostics.scalarGateReason = report.scalar.inspect.gateReason;
    diagnostics.scalarOpened = report.scalar.inspect.opened;
    diagnostics.scalarReleased = report.scalar.inspect.released;
    diagnostics.scalarValidRelease = report.scalar.inspect.validRelease;
    diagnostics.scalarEmitAllowed = report.scalar.inspect.emitAllowed;
    diagnostics.scalarOpenMs = report.scalar.inspect.openMs;
    diagnostics.scalarPeakMs = report.scalar.inspect.peakMs;
    diagnostics.scalarReleaseMs = report.scalar.inspect.releaseMs;
    diagnostics.scalarDurationMs = report.scalar.inspect.durationMs;
    diagnostics.scalarMinDurationMs = report.thresholds.minDurationMs;
    diagnostics.scalarMaxDurationMs = report.thresholds.maxDurationMs;
    diagnostics.scalarPeakStrength = report.scalar.inspect.peakStrength;
}

void populateFrequencyLegacyDiagnosticsFromReport(DetectionDiagnostics& diagnostics, const DetectorReport& report) {
    diagnostics.acceptedPresent = report.accepted.present;
    diagnostics.acceptedStartMs = report.accepted.startMs;
    diagnostics.acceptedPeakMs = report.accepted.peakMs;
    diagnostics.acceptedReleaseMs = report.accepted.endMs;
    diagnostics.acceptedDurationMs = report.accepted.durationMs;
    diagnostics.acceptedStrength = report.accepted.strength;
    diagnostics.acceptedScore = report.frequency.accepted.score;
    diagnostics.acceptedContrast = report.frequency.accepted.contrast;
    diagnostics.frequencyScoreOkFrames = report.frequency.aggregates.scoreOkCount;
    diagnostics.frequencyContrastOkFrames = report.frequency.aggregates.contrastOkCount;
    diagnostics.frequencyBothOkFrames = report.frequency.aggregates.bothOkCount;
    diagnostics.frequencyMatchFrames = report.frequency.aggregates.matchCount;
    diagnostics.frequencyScoreThreshold = report.frequency.thresholds.scoreThreshold;
    diagnostics.frequencyContrastThreshold = report.frequency.thresholds.contrastThreshold;
    diagnostics.frequencyRejectReason = report.frequency.inspect.rejectReason;
    diagnostics.frequencyNoEmitReason = report.frequency.inspect.noEmitReason;
    diagnostics.frequencyGateReason = report.frequency.inspect.gateReason;
    diagnostics.frequencyCandidateState = report.frequency.inspect.candidateState;
    diagnostics.frequencyReadyOk = report.frequency.inspect.readyOk;
    diagnostics.frequencyGateOpen = report.frequency.inspect.gateOpen;
    diagnostics.frequencyOpened = report.frequency.inspect.opened;
    diagnostics.frequencyReleased = report.frequency.inspect.released;
    diagnostics.frequencyEmitted = report.frequency.inspect.emitted;
    diagnostics.frequencyValidRelease = report.frequency.inspect.validRelease;
    diagnostics.frequencyEmitAllowed = report.frequency.inspect.emitAllowed;
    diagnostics.frequencyOpenMs = report.frequency.inspect.openMs;
    diagnostics.frequencyPeakMs = report.frequency.inspect.peakMs;
    diagnostics.frequencyReleaseMs = report.frequency.inspect.releaseMs;
    diagnostics.frequencyDurationMs = report.frequency.inspect.durationMs;
    diagnostics.frequencyMinDurationMs = report.thresholds.minDurationMs;
    diagnostics.frequencyMaxDurationMs = report.thresholds.maxDurationMs;
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
    _diagnostics = {};
    _detectorReport = {};
    _resultQueueOverflowCount = 0;
    _frequencyDetector.resetDiagnosticsSummary();
}

void DetectionRuntime::resetOccurrenceSources() {
    _frequencyDetector.resetState();
    _scalarDetector.resetState();
}

void DetectionRuntime::resetSourceRejectSummaries() {
    _frequencyDetector.resetRejectSummary();
    _scalarDetector.resetAcceptedOccurrenceSummary();
    _scalarDetector.resetSelectedRejectSummary();
    _scalarDetector.resetLegacyRejectSummary();
}

void DetectionRuntime::resetDetectionState() {
    resetOccurrenceSources();
    _occurrenceInspector.reset();
    _patternMatcher.reset();
    _fieldStateTracker.reset();
    _featureHistory.reset();
    _resultQueue[0] = {};
    _resultReadIndex = 0;
    _resultCount = 0;
    _resultQueueOverflowCount = 0;
    _latestPipelineResult = {};
    _hasLatestPipelineResult = false;
    _lastOccurrence = {};
    _lastInspectedOccurrence = {};
    _detectorReport = {};
}

void DetectionRuntime::setDiagnosticsEnabled(bool enabled) {
    _diagnosticsEnabled = enabled;
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

void DetectionRuntime::captureDiagnostics() {
    refreshDetectorReports(_latestPipelineResult.timestampMs);

    if (!_diagnosticsEnabled) {
        _diagnostics = {};
        return;
    }

    _diagnostics = {};
    _diagnostics.observedAtMs = _latestPipelineResult.timestampMs;
    _diagnostics.occurrenceSource = detectorSelectionNameForRuntime(_detectorSelection);
    const bool acceptedPresent = _lastOccurrence.present && _lastOccurrence.source == OccurrenceSource::Frequency;
    _diagnostics.acceptedPresent = acceptedPresent;
    if (acceptedPresent) {
        _diagnostics.acceptedStartMs = _lastOccurrence.startMs;
        _diagnostics.acceptedPeakMs = _lastOccurrence.peakMs;
        _diagnostics.acceptedReleaseMs = _lastOccurrence.releaseMs;
        _diagnostics.acceptedDurationMs = _lastOccurrence.durationMs;
        _diagnostics.acceptedStrength = _lastOccurrence.strength;
        _diagnostics.acceptedScore = _lastOccurrence.score;
        _diagnostics.acceptedContrast = _lastOccurrence.contrast;
    }

    _diagnostics.ampCenteredMagnitude = _lastOccurrence.ampLevel;
    _diagnostics.ampLevel = _lastOccurrence.ampLevel;
    _diagnostics.ampBaseline = _lastOccurrence.ampBaseline;
    _diagnostics.ampLift = _lastOccurrence.ampLevel - _lastOccurrence.ampBaseline;

    _diagnostics.scalarOnsetRejectReason = _scalarDetector.lastOnsetRejectReasonName();
    _diagnostics.scalarTransientRejectReason = _scalarDetector.lastTransientRejectReasonName();
    if (_detectorSelection == DetectorSelection::ScalarTransient) {
        populateScalarLegacyDiagnosticsFromReport(_diagnostics, _detectorReport);
    } else {
        const auto& scalarReportDetail = _scalarDetector.reportDetail();
        const auto& scalarDetail = scalarReportDetail.inspect;
        _diagnostics.scalarRejectReason = scalarRejectReasonOrFallback(
            _diagnostics.scalarTransientRejectReason,
            _diagnostics.scalarOnsetRejectReason
        );
        _diagnostics.scalarNoEmitReason = _diagnostics.scalarRejectReason;
        _diagnostics.scalarGateReason = _diagnostics.scalarRejectReason;
        _diagnostics.scalarOpened = scalarDetail.opened;
        _diagnostics.scalarReleased = scalarDetail.released;
        _diagnostics.scalarValidRelease = _diagnostics.scalarReleased && reasonIsNone(_diagnostics.scalarRejectReason);
        _diagnostics.scalarEmitAllowed = _diagnostics.scalarValidRelease;
        _diagnostics.scalarOpenMs = scalarDetail.openMs;
        _diagnostics.scalarPeakMs = scalarDetail.peakMs;
        _diagnostics.scalarReleaseMs = scalarDetail.releaseMs;
        _diagnostics.scalarDurationMs = scalarDetail.durationMs;
        _diagnostics.scalarMinDurationMs = _scalarDetector.minTransientDurationMs();
        _diagnostics.scalarMaxDurationMs = _scalarDetector.maxTransientDurationMs();
        _diagnostics.scalarPeakStrength = scalarDetail.peakStrength;
    }
    _diagnostics.scalarTransientRejectedDurationMs = _scalarDetector.lastTransientRejectedDurationMs();
    _diagnostics.scalarTransientRejectedStrength = _scalarDetector.lastTransientRejectedStrength();
    _diagnostics.sourceSummary = {};
    _diagnostics.sourceLastCandidate = {};

    if (_detectorSelection == DetectorSelection::FrequencyMatch) {
        const auto& detector = _frequencyDetector;
        populateFrequencyLegacyDiagnosticsFromReport(_diagnostics, _detectorReport);
        _diagnostics.frequencyPresent = detector.diagnosticsObservedCount > 0;
        _diagnostics.frequencyValidWindow = detector.evidenceOk;
        _diagnostics.frequencyMatched = detector.diagnosticsMatchedCount > 0;
        _diagnostics.frequencyScoreOk = detector.attackScoreOk;
        _diagnostics.frequencyContrastOk = detector.attackContrastOk;
        _diagnostics.frequencyFrames = detector.diagnosticsObservedCount;
        _diagnostics.frequencyValidFrames = detector.diagnosticsValidCount;
        _diagnostics.frequencyScoreOkFrames = detector.diagnosticsScoreOkCount;
        _diagnostics.frequencyContrastOkFrames = detector.diagnosticsContrastOkCount;
        _diagnostics.frequencyBothOkFrames = detector.diagnosticsBothOkCount;
        _diagnostics.frequencyMatchFrames = detector.diagnosticsMatchedCount;
        _diagnostics.frequencyRejectFrames = detector.diagnosticsRejectedCount;
        _diagnostics.frequencyReleaseScoreOkFrames = detector.diagnosticsReleaseScoreOkCount;
        _diagnostics.frequencyReleaseContrastOkFrames = detector.diagnosticsReleaseContrastOkCount;
        _diagnostics.frequencyReleaseBothOkFrames = detector.diagnosticsReleaseBothOkCount;
        _diagnostics.frequencyReleaseScoreTooLowFrames = detector.diagnosticsReleaseScoreTooLowCount;
        _diagnostics.frequencyReleaseContrastTooLowFrames = detector.diagnosticsReleaseContrastTooLowCount;
        _diagnostics.frequencyReleaseScoreAndContrastTooLowFrames = detector.diagnosticsReleaseScoreAndContrastTooLowCount;
        _diagnostics.frequencyReleaseNoEvidenceFrames = detector.diagnosticsReleaseNoEvidenceCount;
        _diagnostics.frequencyDiagLongestMatchStreakFrames = detector.diagLongestMatchStreakFrames;
        _diagnostics.frequencyDiagLongestMatchStreakStartMs = detector.diagLongestMatchStreakStartMs;
        _diagnostics.frequencyDiagLongestMatchStreakEndMs = detector.diagLongestMatchStreakEndMs;
        _diagnostics.frequencyScoreMean = detector.diagnosticsScoreMean();
        _diagnostics.frequencyContrastMean = detector.diagnosticsContrastMean();
        _diagnostics.frequencyScoreMin = detector.diagnosticsScoreMin;
        _diagnostics.frequencyContrastMin = detector.diagnosticsContrastMin;
        _diagnostics.frequencyScoreMax = detector.diagnosticsScoreMax;
        _diagnostics.frequencyContrastMax = detector.diagnosticsContrastMax;
        _diagnostics.frequencyScoreMaxMs = detector.diagnosticsScoreMaxMs;
        _diagnostics.frequencyContrastMaxMs = detector.diagnosticsContrastMaxMs;
        _diagnostics.frequencyTargetPowerMean = detector.diagnosticsTargetPowerMean();
        _diagnostics.frequencyLowerPowerMean = detector.diagnosticsLowerPowerMean();
        _diagnostics.frequencyUpperPowerMean = detector.diagnosticsUpperPowerMean();
        _diagnostics.frequencyNeighborPowerMean = detector.diagnosticsNeighborPowerMeanValue();
        _diagnostics.frequencyNeighborPowerMaxMean = detector.diagnosticsNeighborPowerMaxMean();
        _diagnostics.frequencyTargetPowerMax = detector.diagnosticsTargetPower.max;
        _diagnostics.frequencyLowerPowerMax = detector.diagnosticsLowerPower.max;
        _diagnostics.frequencyUpperPowerMax = detector.diagnosticsUpperPower.max;
        _diagnostics.frequencyNeighborPowerMeanMax = detector.diagnosticsNeighborPowerMean.max;
        _diagnostics.frequencyNeighborPowerMaxMax = detector.diagnosticsNeighborPowerMax.max;
        _diagnostics.frequencyTargetPowerMaxMs = detector.diagnosticsTargetPower.maxMs;
        _diagnostics.frequencyLowerPowerMaxMs = detector.diagnosticsLowerPower.maxMs;
        _diagnostics.frequencyUpperPowerMaxMs = detector.diagnosticsUpperPower.maxMs;
        _diagnostics.frequencyNeighborPowerMeanMaxMs = detector.diagnosticsNeighborPowerMean.maxMs;
        _diagnostics.frequencyNeighborPowerMaxMaxMs = detector.diagnosticsNeighborPowerMax.maxMs;
        _diagnostics.frequencyLowerScoreMean = detector.diagnosticsLowerScoreMean();
        _diagnostics.frequencyUpperScoreMean = detector.diagnosticsUpperScoreMean();
        _diagnostics.frequencyLowerScoreMax = detector.diagnosticsLowerScore.max;
        _diagnostics.frequencyUpperScoreMax = detector.diagnosticsUpperScore.max;
        _diagnostics.frequencyLowerScoreMaxMs = detector.diagnosticsLowerScore.maxMs;
        _diagnostics.frequencyUpperScoreMaxMs = detector.diagnosticsUpperScore.maxMs;
        _diagnostics.frequencyPeakScore = detector.candidatePeakScore;
        _diagnostics.frequencyPeakContrast = detector.candidatePeakContrast;
        _diagnostics.frequencyPeakSampleCount = detector.candidatePeakSampleCount;
        _diagnostics.frequencyScoreThreshold = detector.attackScoreThreshold;
        _diagnostics.frequencyContrastThreshold = detector.attackContrastThreshold;
        const bool scoreNear = _diagnostics.frequencyScoreThreshold > 0.0f
            && _diagnostics.frequencyScoreMax >= (_diagnostics.frequencyScoreThreshold * 0.75f);
        const bool contrastNear = _diagnostics.frequencyContrastThreshold > 0.0f
            && _diagnostics.frequencyContrastMax >= (_diagnostics.frequencyContrastThreshold * 0.75f);
        _diagnostics.frequencyNearMiss = !acceptedPresent && (scoreNear || contrastNear
            || _diagnostics.frequencyScoreOkFrames > 0
            || _diagnostics.frequencyContrastOkFrames > 0);
        if (_diagnostics.frequencyNearMiss) {
            if (_diagnostics.frequencyScoreOkFrames > 0 && _diagnostics.frequencyContrastOkFrames == 0) {
                _diagnostics.frequencyNearMissReason = "score_ok_contrast_low";
            } else if (_diagnostics.frequencyContrastOkFrames > 0 && _diagnostics.frequencyScoreOkFrames == 0) {
                _diagnostics.frequencyNearMissReason = "contrast_ok_score_low";
            } else if (scoreNear && contrastNear) {
                _diagnostics.frequencyNearMissReason = "near_threshold";
            } else if (scoreNear) {
                _diagnostics.frequencyNearMissReason = "score_near_threshold";
            } else if (contrastNear) {
                _diagnostics.frequencyNearMissReason = "contrast_near_threshold";
            } else if (_diagnostics.frequencyScoreOkFrames > 0 && _diagnostics.frequencyContrastOkFrames > 0) {
                _diagnostics.frequencyNearMissReason = "both_ok_no_emission";
            } else {
                _diagnostics.frequencyNearMissReason = "near_miss";
            }
        } else {
            _diagnostics.frequencyNearMissReason = "none";
        }

        _diagnostics.sourceSummary.present = detector.rejectedCount > 0;
        _diagnostics.sourceSummary.candidateCount = detector.rejectedCount;
        _diagnostics.sourceSummary.rejectCount = detector.rejectedCount;
        _diagnostics.sourceSummary.bestDurationMs = detector.bestDurationMs;
        _diagnostics.sourceSummary.secondBestDurationMs = detector.secondBestDurationMs;
        _diagnostics.sourceSummary.bestOpenMs = detector.bestOpenMs;
        _diagnostics.sourceSummary.bestPeakMs = detector.bestPeakMs;
        _diagnostics.sourceSummary.bestLastMatchMs = detector.bestLastMatchMs;
        _diagnostics.sourceSummary.bestCloseMs = detector.bestCloseMs;
        _diagnostics.sourceSummary.bestPeakPrimary = detector.bestPeakScore;
        _diagnostics.sourceSummary.bestPeakSecondary = detector.bestPeakContrast;
        _diagnostics.sourceSummary.bestRejectReason = detector.bestRejectReason;
        _diagnostics.sourceSummary.bestGateReason = detector.bestGateReason;
        _diagnostics.sourceSummary.closeCause = frequencyReleaseFailCauseName(detector.candidateCloseCause);
        _diagnostics.sourceSummary.scoreTooLowFrames = detector.diagnosticsScoreTooLowCount;
        _diagnostics.sourceSummary.contrastTooLowFrames = detector.diagnosticsContrastTooLowCount;
        _diagnostics.sourceSummary.scoreAndContrastTooLowFrames = detector.diagnosticsScoreAndContrastTooLowCount;
        _diagnostics.sourceSummary.maxPeakPrimary = detector.diagnosticsScoreMax;
        _diagnostics.sourceSummary.maxPeakPrimaryMs = detector.diagnosticsScoreMaxMs;
        _diagnostics.sourceSummary.maxPeakSecondary = detector.diagnosticsContrastMax;
        _diagnostics.sourceSummary.maxPeakSecondaryMs = detector.diagnosticsContrastMaxMs;
        _diagnostics.sourceSummary.totalMatchMs = detector.totalMatchMs;
        _diagnostics.sourceSummary.totalGapMs = detector.totalGapMs;
        _diagnostics.sourceSummary.maxGapMs = detector.maxGapMs;
        _diagnostics.sourceSummary.islandCount = detector.islandCount;
        _diagnostics.sourceLastCandidate.present = detector.candidateActive || detector.candidateClosed || detector.candidateEmitted || detector.candidateOpenMs > 0;
        _diagnostics.sourceLastCandidate.peakMs = detector.candidatePeakMs;
        _diagnostics.sourceLastCandidate.durationMs = detector.candidateDurationMs;
        _diagnostics.sourceLastCandidate.sampleCount = detector.candidatePeakSampleCount;
        _diagnostics.sourceLastCandidate.peakPrimary = detector.candidatePeakScore;
        _diagnostics.sourceLastCandidate.peakSecondary = detector.candidatePeakContrast;
        _diagnostics.sourceLastCandidate.reason = detector.candidateEmitted
            ? "none"
            : (detector.candidateClosed
                ? (detector.noEmitReason[0] != '\0' ? detector.noEmitReason : "unknown")
                : (detector.candidateActive
                    ? "open"
                    : (detector.gateReason[0] != '\0' ? detector.gateReason : "unknown")));
        _diagnostics.sourceLastCandidate.gateReason = detector.gateReason;
        _diagnostics.sourceLastCandidate.scope = "unknown";
        _diagnostics.frequencyRejectReason = detector.candidateEmitted
            ? "none"
            : (detector.candidateClosed
                ? (detector.noEmitReason[0] != '\0' ? detector.noEmitReason : "unknown")
                : (detector.gateReason[0] != '\0' ? detector.gateReason : "unknown"));
        _diagnostics.frequencyNoEmitReason = detector.noEmitReason;
        _diagnostics.frequencyGateReason = detector.gateReason;
        _diagnostics.frequencyWouldCandidateReason = detector.wouldCandidateReason;
        _diagnostics.frequencyCandidateState = detector.candidateState;
        _diagnostics.frequencyReadyOk = detector.evidenceOk;
        _diagnostics.frequencyGateOpen = detector.attackOk;
        _diagnostics.frequencyOpened = detector.candidateActive
            || detector.candidateClosed
            || detector.candidateEmitted
            || detector.candidateOpenMs > 0;
        _diagnostics.frequencyReleased = detector.candidateClosed || detector.candidateCloseMs > 0;
        _diagnostics.frequencyEmitted = detector.candidateEmitted;
        _diagnostics.frequencyValidRelease = detector.validRelease;
        _diagnostics.frequencyEmitAllowed = detector.emitAllowed;
        _diagnostics.frequencyAcceptedCandidateId = detector.acceptedCandidateId;
        _diagnostics.frequencySelectedRejectCandidateId = detector.selectedRejectCandidateId;
        _diagnostics.frequencyLastCandidateId = detector.lastCandidateId;
        _diagnostics.frequencyLifecycleCandidateId = detector.candidateLifecycleId;
        _diagnostics.frequencyLastMatchMs = detector.candidateLastMatchedMs;
        _diagnostics.frequencyDurationUsedMs = detector.candidateDecisionDurationMs;
        _diagnostics.frequencyDurationPrintedMs = detector.candidateDurationMs;
        _diagnostics.frequencyMinDurationUsedMs = detector.candidateDecisionMinDurationMs;
        _diagnostics.frequencyMinDurationReportedMs = detector.candidateMinDurationMs;
        _diagnostics.frequencyDurationOk = detector.candidateDecisionDurationOk;
        _diagnostics.frequencyDurationInconsistent = detector.candidateDurationInconsistent;
        _diagnostics.frequencyPrintedDurationInconsistent = detector.candidateDurationMs >= detector.candidateMinDurationMs && !detector.candidateEmitted;
        _diagnostics.frequencyOpenMs = detector.candidateOpenMs;
        _diagnostics.frequencyPeakMs = detector.candidatePeakMs;
        _diagnostics.frequencyReleaseMs = detector.candidateCloseMs;
        _diagnostics.frequencyDurationMs = detector.candidateDurationMs;
        _diagnostics.frequencyMinDurationMs = detector.candidateMinDurationMs;
        _diagnostics.frequencyMaxDurationMs = detector.candidateMaxDurationMs;
        _diagnostics.frequencyScoreMax = detector.diagnosticsScoreMax;
        _diagnostics.frequencyContrastMax = detector.diagnosticsContrastMax;
        _diagnostics.frequencyScoreMaxMs = detector.diagnosticsScoreMaxMs;
        _diagnostics.frequencyContrastMaxMs = detector.diagnosticsContrastMaxMs;
        _diagnostics.frequencyPeakScore = detector.candidatePeakScore;
        _diagnostics.frequencyPeakContrast = detector.candidatePeakContrast;
        _diagnostics.frequencyPeakSampleCount = detector.candidatePeakSampleCount;
    } else {
        const auto& scalarLegacySummary = _scalarDetector.legacyRejectSummary();
        const bool scalarSelectedRejectPresent = _detectorReport.selectedReject.present;
        const auto& scalarSelectedReject = _detectorReport.selectedReject;
        const auto& scalarDetail = _detectorReport.scalar.inspect;
        populateScalarLegacyDiagnosticsFromReport(_diagnostics, _detectorReport);
        _diagnostics.sourceSummary.present = scalarLegacySummary.rejectedCandidateCount > 0;
        _diagnostics.sourceSummary.candidateCount = scalarLegacySummary.rejectedCandidateCount;
        _diagnostics.sourceSummary.rejectCount = scalarLegacySummary.rejectedCandidateCount;
        _diagnostics.sourceSummary.bestDurationMs = scalarLegacySummary.bestRejectedDurationMs;
        _diagnostics.sourceSummary.secondBestDurationMs = scalarLegacySummary.secondBestRejectedDurationMs;
        _diagnostics.sourceSummary.bestOpenMs = scalarLegacySummary.bestRejectedOpenMs;
        _diagnostics.sourceSummary.bestPeakMs = scalarLegacySummary.bestRejectedPeakMs;
        _diagnostics.sourceSummary.bestLastMatchMs = scalarLegacySummary.bestRejectedLastMatchMs;
        _diagnostics.sourceSummary.bestCloseMs = scalarLegacySummary.bestRejectedCloseMs;
        _diagnostics.sourceSummary.bestPeakPrimary = scalarLegacySummary.bestRejectedPeakStrength;
        _diagnostics.sourceSummary.bestPeakSecondary = 0.0f;
        _diagnostics.sourceSummary.bestRejectReason = scalarLegacySummary.bestRejectedReason;
        _diagnostics.sourceSummary.bestGateReason = scalarLegacySummary.bestRejectedGateReason;
        _diagnostics.sourceSummary.scoreTooLowFrames = 0;
        _diagnostics.sourceSummary.contrastTooLowFrames = 0;
        _diagnostics.sourceSummary.scoreAndContrastTooLowFrames = 0;
        _diagnostics.sourceSummary.maxPeakPrimary = scalarLegacySummary.maxRejectedPeakStrength;
        _diagnostics.sourceSummary.maxPeakPrimaryMs = scalarLegacySummary.maxRejectedPeakStrengthMs;
        _diagnostics.sourceSummary.maxPeakSecondary = 0.0f;
        _diagnostics.sourceSummary.maxPeakSecondaryMs = 0UL;
        _diagnostics.sourceSummary.totalMatchMs = scalarLegacySummary.totalRejectedMatchMs;
        _diagnostics.sourceSummary.totalGapMs = scalarLegacySummary.totalRejectedGapMs;
        _diagnostics.sourceSummary.maxGapMs = scalarLegacySummary.maxRejectedGapMs;
        _diagnostics.sourceSummary.islandCount = scalarLegacySummary.rejectedIslandCount;
        _diagnostics.sourceLastCandidate.present = scalarSelectedRejectPresent
            || scalarDetail.opened
            || scalarDetail.released;
        _diagnostics.sourceLastCandidate.peakMs = scalarSelectedRejectPresent
            ? scalarSelectedReject.peakMs
            : scalarDetail.peakMs;
        _diagnostics.sourceLastCandidate.durationMs = scalarSelectedRejectPresent
            ? scalarSelectedReject.durationMs
            : scalarDetail.durationMs;
        _diagnostics.sourceLastCandidate.sampleCount = 0;
        _diagnostics.sourceLastCandidate.peakPrimary = scalarSelectedRejectPresent
            ? scalarSelectedReject.strength
            : scalarDetail.peakStrength;
        _diagnostics.sourceLastCandidate.peakSecondary = 0.0f;
        _diagnostics.sourceLastCandidate.reason = scalarSelectedRejectPresent
            ? scalarSelectedReject.detectorReason
            : scalarDetail.rejectReason;
        _diagnostics.sourceLastCandidate.gateReason = scalarDetail.gateReason;
        _diagnostics.sourceLastCandidate.scope = "unknown";
        _diagnostics.scalarTransientRejectedDurationMs = _scalarDetector.lastTransientRejectedDurationMs();
        _diagnostics.scalarTransientRejectedStrength = _scalarDetector.lastTransientRejectedStrength();
        _diagnostics.frequencyPresent = false;
        _diagnostics.frequencyValidWindow = false;
        _diagnostics.frequencyMatched = false;
        _diagnostics.frequencyScoreOk = false;
        _diagnostics.frequencyContrastOk = false;
        _diagnostics.frequencyFrames = 0;
        _diagnostics.frequencyValidFrames = 0;
        _diagnostics.frequencyScoreOkFrames = 0;
        _diagnostics.frequencyContrastOkFrames = 0;
        _diagnostics.frequencyBothOkFrames = 0;
        _diagnostics.frequencyMatchFrames = 0;
        _diagnostics.frequencyRejectFrames = 0;
        _diagnostics.frequencyReleaseScoreOkFrames = 0;
        _diagnostics.frequencyReleaseContrastOkFrames = 0;
        _diagnostics.frequencyReleaseBothOkFrames = 0;
        _diagnostics.frequencyReleaseScoreTooLowFrames = 0;
        _diagnostics.frequencyReleaseContrastTooLowFrames = 0;
        _diagnostics.frequencyReleaseScoreAndContrastTooLowFrames = 0;
        _diagnostics.frequencyReleaseNoEvidenceFrames = 0;
        _diagnostics.frequencyDiagLongestMatchStreakFrames = 0;
        _diagnostics.frequencyDiagLongestMatchStreakStartMs = 0;
        _diagnostics.frequencyDiagLongestMatchStreakEndMs = 0;
        _diagnostics.frequencyScoreMean = 0.0f;
        _diagnostics.frequencyContrastMean = 0.0f;
        _diagnostics.frequencyScoreMin = 0.0f;
        _diagnostics.frequencyContrastMin = 0.0f;
        _diagnostics.frequencyScoreMax = 0.0f;
        _diagnostics.frequencyContrastMax = 0.0f;
        _diagnostics.frequencyScoreMaxMs = 0;
        _diagnostics.frequencyContrastMaxMs = 0;
        _diagnostics.frequencyTargetPowerMean = 0.0f;
        _diagnostics.frequencyLowerPowerMean = 0.0f;
        _diagnostics.frequencyUpperPowerMean = 0.0f;
        _diagnostics.frequencyNeighborPowerMean = 0.0f;
        _diagnostics.frequencyNeighborPowerMaxMean = 0.0f;
        _diagnostics.frequencyTargetPowerMax = 0.0f;
        _diagnostics.frequencyLowerPowerMax = 0.0f;
        _diagnostics.frequencyUpperPowerMax = 0.0f;
        _diagnostics.frequencyNeighborPowerMeanMax = 0.0f;
        _diagnostics.frequencyNeighborPowerMaxMax = 0.0f;
        _diagnostics.frequencyTargetPowerMaxMs = 0;
        _diagnostics.frequencyLowerPowerMaxMs = 0;
        _diagnostics.frequencyUpperPowerMaxMs = 0;
        _diagnostics.frequencyNeighborPowerMeanMaxMs = 0;
        _diagnostics.frequencyNeighborPowerMaxMaxMs = 0;
        _diagnostics.frequencyLowerScoreMean = 0.0f;
        _diagnostics.frequencyUpperScoreMean = 0.0f;
        _diagnostics.frequencyLowerScoreMax = 0.0f;
        _diagnostics.frequencyUpperScoreMax = 0.0f;
        _diagnostics.frequencyLowerScoreMaxMs = 0;
        _diagnostics.frequencyUpperScoreMaxMs = 0;
        _diagnostics.frequencyPeakScore = 0.0f;
        _diagnostics.frequencyPeakContrast = 0.0f;
        _diagnostics.frequencyPeakSampleCount = 0;
        _diagnostics.frequencyScoreThreshold = 0.0f;
        _diagnostics.frequencyContrastThreshold = 0.0f;
        _diagnostics.frequencyNearMiss = false;
        _diagnostics.frequencyNearMissReason = "none";
        _diagnostics.frequencyRejectReason = _diagnostics.scalarRejectReason;
        _diagnostics.frequencyNoEmitReason = _diagnostics.scalarNoEmitReason;
        _diagnostics.frequencyGateReason = _diagnostics.scalarGateReason;
        _diagnostics.frequencyWouldCandidateReason = _diagnostics.scalarGateReason;
        _diagnostics.frequencyCandidateState = _diagnostics.scalarOpened ? "active" : "idle";
        _diagnostics.frequencyReadyOk = _diagnostics.scalarOpened;
        _diagnostics.frequencyGateOpen = _diagnostics.scalarEmitAllowed;
        _diagnostics.frequencyOpened = _diagnostics.scalarOpened;
        _diagnostics.frequencyReleased = _diagnostics.scalarReleased;
        _diagnostics.frequencyEmitted = _diagnostics.scalarEmitAllowed;
        _diagnostics.frequencyValidRelease = _diagnostics.scalarValidRelease;
        _diagnostics.frequencyEmitAllowed = _diagnostics.scalarEmitAllowed;
        _diagnostics.frequencyOpenMs = _diagnostics.scalarOpenMs;
        _diagnostics.frequencyPeakMs = _diagnostics.scalarPeakMs;
        _diagnostics.frequencyReleaseMs = _diagnostics.scalarReleaseMs;
        _diagnostics.frequencyDurationMs = _diagnostics.scalarDurationMs;
        _diagnostics.frequencyMinDurationMs = _diagnostics.scalarMinDurationMs;
        _diagnostics.frequencyMaxDurationMs = _diagnostics.scalarMaxDurationMs;
        _diagnostics.frequencyScoreMax = 0.0f;
        _diagnostics.frequencyContrastMax = 0.0f;
        _diagnostics.frequencyScoreMaxMs = 0;
        _diagnostics.frequencyContrastMaxMs = 0;
        _diagnostics.frequencyPeakScore = _diagnostics.scalarPeakStrength;
        _diagnostics.frequencyPeakContrast = 0.0f;
        _diagnostics.frequencyPeakSampleCount = 0;
    }
    _diagnostics.patternResultQueueOverflowCount = _resultQueueOverflowCount;
    _diagnostics.detectorKind = _detectorSelection == DetectorSelection::FrequencyMatch
        ? "frequency_match"
        : "scalar_transient";
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
    resetOccurrenceSources();
    applyScalarTransientConfig(_scalarDetector, _scalarTransientConfig);
    _detectorReport = {};
}

void DetectionRuntime::setInspectionPlan(const InspectionPlan& plan) {
    _inspectionPlan = plan;
    _occurrenceInspector.configure(_inspectionPlan);
}

void DetectionRuntime::setPatternRulesConfig(const PatternRulesConfig& config) {
    _patternRulesConfig = config;
    _patternMatcher.configure(_patternRulesConfig);
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
                selectedScalarValue(audioSamplePacket, frequencyEvidence, _scalarTransientConfig.observedStream),
                OccurrenceKind::AmpTransient,
                occurrenceSourceForStream(_scalarTransientConfig.observedStream)
            );
            break;
    }

    drainOccurrenceSources(nowMs);
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

const DetectionDiagnostics& DetectionRuntime::diagnostics() const {
    return _diagnostics;
}

const DetectorReport* DetectionRuntime::detectorReport(DetectorId id) const {
    return _detectorReport.detectorId == id ? &_detectorReport : nullptr;
}

const DetectorReport& DetectionRuntime::activeDetectorReport() const {
    return _detectorReport;
}

const DetectorReport& DetectionRuntime::scalarDetectorReport() const {
    static const DetectorReport kEmptyReport = {};
    const DetectorReport* report = detectorReport(DetectorId::ScalarTransient);
    return report != nullptr ? *report : kEmptyReport;
}

const DetectorReport& DetectionRuntime::frequencyDetectorReport() const {
    static const DetectorReport kEmptyReport = {};
    const DetectorReport* report = detectorReport(DetectorId::FrequencyMatch);
    return report != nullptr ? *report : kEmptyReport;
}

const FrequencyMatchDetector& DetectionRuntime::frequencyDetector() const {
    return _frequencyDetector;
}

const FieldState& DetectionRuntime::fieldState() const {
    return _fieldStateTracker.state();
}

const FeatureHistory& DetectionRuntime::featureHistory() const {
    return _featureHistory;
}

void DetectionRuntime::drainOccurrenceSources(unsigned long nowMs) {
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
        ++_resultQueueOverflowCount;
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


