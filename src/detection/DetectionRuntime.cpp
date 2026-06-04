#include "DetectionRuntime.h"

#include <string.h>

#include "detectors/FrequencyMatchDetector.h"

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

float selectedScalarValue(const AudioSamplePacket& frame, const FrequencyFeatureFrame& frequencyEvidence, FeatureStreamId stream) {
    switch (stream) {
        case FeatureStreamId::AmpEnvelope:
            return frame.audioMagnitudeValue;
        case FeatureStreamId::FrequencyScore:
            return frequencyEvidence.score;
        case FeatureStreamId::FrequencyContrast:
            return frequencyEvidence.spectralContrast;
        case FeatureStreamId::Unknown:
        default:
            return static_cast<float>(frame.level);
    }
}

const char* occurrenceSourceName(OccurrenceSourceKind kind) {
    switch (kind) {
        case OccurrenceSourceKind::FrequencyMatch:
            return "frequency_match";
        case OccurrenceSourceKind::ScalarTransient:
            return "scalar_transient";
        default:
            return "unknown";
    }
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
    _frequencyEmitter.detector().resetDiagnosticsSummary();
}

void DetectionRuntime::resetOccurrenceSources() {
    _frequencyEmitter.reset();
    _scalarEmitter.reset();
}

void DetectionRuntime::resetSourceRejectSummaries() {
    _frequencyEmitter.detector().resetRejectSummary();
    _scalarEmitter.resetRejectSummary();
}

void DetectionRuntime::resetDetectionState() {
    resetOccurrenceSources();
    _occurrenceInspector.reset();
    _patternAssembler.reset();
    _fieldStateTracker.reset();
    _featureHistory.reset();
    _resultQueue[0] = {};
    _resultReadIndex = 0;
    _resultCount = 0;
    _latestPipelineResult = {};
    _hasLatestPipelineResult = false;
    _lastOccurrence = {};
    _lastInspectedOccurrence = {};
}

void DetectionRuntime::setDiagnosticsEnabled(bool enabled) {
    _diagnosticsEnabled = enabled;
    _frequencyEmitter.setDiagnosticsEnabled(enabled);
    _scalarEmitter.setDiagnosticsEnabled(enabled);
}

void DetectionRuntime::captureDiagnostics() {
    if (!_diagnosticsEnabled) {
        _diagnostics = {};
        return;
    }

    _diagnostics = {};
    _diagnostics.observedAtMs = _latestPipelineResult.timestampMs;
    _diagnostics.occurrenceSource = occurrenceSourceName(_occurrenceSourceKind);
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

    _diagnostics.scalarOnsetRejectReason = _scalarEmitter.lastOnsetRejectReasonName();
    _diagnostics.scalarTransientRejectReason = _scalarEmitter.lastTransientRejectReasonName();
    _diagnostics.scalarRejectReason = _diagnostics.scalarTransientRejectReason != nullptr && strcmp(_diagnostics.scalarTransientRejectReason, "none") != 0
        ? _diagnostics.scalarTransientRejectReason
        : _diagnostics.scalarOnsetRejectReason;
    _diagnostics.scalarNoEmitReason = _diagnostics.scalarRejectReason;
    _diagnostics.scalarGateReason = _diagnostics.scalarRejectReason;
    _diagnostics.scalarOpened = _scalarEmitter.candidateActive()
        || _scalarEmitter.releaseObserved()
        || _scalarEmitter.candidateFirstSeenMs() > 0;
    _diagnostics.scalarReleased = _scalarEmitter.releaseObserved()
        || _scalarEmitter.candidateReleaseObservedMs() > 0;
    _diagnostics.scalarValidRelease = _diagnostics.scalarReleased
        && _diagnostics.scalarRejectReason != nullptr
        && strcmp(_diagnostics.scalarRejectReason, "none") == 0;
    _diagnostics.scalarEmitAllowed = _diagnostics.scalarValidRelease;
    _diagnostics.scalarOpenMs = _scalarEmitter.candidateFirstSeenMs();
    _diagnostics.scalarPeakMs = _scalarEmitter.candidatePeakMs();
    _diagnostics.scalarReleaseMs = _scalarEmitter.candidateReleaseObservedMs();
    _diagnostics.scalarDurationMs = _diagnostics.scalarReleased && _diagnostics.scalarReleaseMs >= _diagnostics.scalarOpenMs
        ? _diagnostics.scalarReleaseMs - _diagnostics.scalarOpenMs
        : 0UL;
    _diagnostics.scalarMinDurationMs = _scalarTransientConfig.minTransientDurationMs;
    _diagnostics.scalarMaxDurationMs = _scalarTransientConfig.maxTransientDurationMs;
    _diagnostics.scalarPeakStrength = _scalarEmitter.candidatePeakStrength();
    _diagnostics.scalarTransientRejectedDurationMs = _scalarEmitter.lastTransientRejectedDurationMs();
    _diagnostics.scalarTransientRejectedStrength = _scalarEmitter.lastTransientRejectedStrength();
    _diagnostics.sourceSummary = {};
    _diagnostics.sourceLastCandidate = {};

    if (_occurrenceSourceKind == OccurrenceSourceKind::FrequencyMatch) {
        const auto& detector = _frequencyEmitter.detector();
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
        _diagnostics.frequencyPeakScore = detector.candidatePeakScore;
        _diagnostics.frequencyPeakContrast = detector.candidatePeakContrast;
        _diagnostics.frequencyPeakWindowSampleCount = detector.candidatePeakWindowSampleCount;
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
        _diagnostics.sourceLastCandidate.windowSamples = detector.candidatePeakWindowSampleCount;
        _diagnostics.sourceLastCandidate.peakPrimary = detector.candidatePeakScore;
        _diagnostics.sourceLastCandidate.peakSecondary = detector.candidatePeakContrast;
        _diagnostics.sourceLastCandidate.reason = detector.noEmitReason;
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
        _diagnostics.frequencyPeakWindowSampleCount = detector.candidatePeakWindowSampleCount;
    } else {
        _diagnostics.scalarRejectReason = _scalarEmitter.lastTransientRejectReasonName();
        _diagnostics.scalarNoEmitReason = _diagnostics.scalarRejectReason;
        _diagnostics.scalarGateReason = _diagnostics.scalarRejectReason;
        _diagnostics.sourceSummary.present = _scalarEmitter.rejectedCandidateCount() > 0;
        _diagnostics.sourceSummary.candidateCount = _scalarEmitter.rejectedCandidateCount();
        _diagnostics.sourceSummary.rejectCount = _scalarEmitter.rejectedCandidateCount();
        _diagnostics.sourceSummary.bestDurationMs = _scalarEmitter.bestRejectedDurationMs();
        _diagnostics.sourceSummary.secondBestDurationMs = _scalarEmitter.secondBestRejectedDurationMs();
        _diagnostics.sourceSummary.bestOpenMs = _scalarEmitter.bestRejectedOpenMs();
        _diagnostics.sourceSummary.bestPeakMs = _scalarEmitter.bestRejectedPeakMs();
        _diagnostics.sourceSummary.bestLastMatchMs = _scalarEmitter.bestRejectedLastMatchMs();
        _diagnostics.sourceSummary.bestCloseMs = _scalarEmitter.bestRejectedCloseMs();
        _diagnostics.sourceSummary.bestPeakPrimary = _scalarEmitter.bestRejectedPeakStrength();
        _diagnostics.sourceSummary.bestPeakSecondary = 0.0f;
        _diagnostics.sourceSummary.bestRejectReason = _scalarEmitter.bestRejectedReasonName();
        _diagnostics.sourceSummary.bestGateReason = _scalarEmitter.bestRejectedGateReasonName();
        _diagnostics.sourceSummary.scoreTooLowFrames = 0;
        _diagnostics.sourceSummary.contrastTooLowFrames = 0;
        _diagnostics.sourceSummary.scoreAndContrastTooLowFrames = 0;
        _diagnostics.sourceSummary.maxPeakPrimary = _scalarEmitter.maxRejectedPeakStrength();
        _diagnostics.sourceSummary.maxPeakPrimaryMs = _scalarEmitter.maxRejectedPeakStrengthMs();
        _diagnostics.sourceSummary.maxPeakSecondary = 0.0f;
        _diagnostics.sourceSummary.maxPeakSecondaryMs = 0UL;
        _diagnostics.sourceSummary.totalMatchMs = _scalarEmitter.totalRejectedMatchMs();
        _diagnostics.sourceSummary.totalGapMs = _scalarEmitter.totalRejectedGapMs();
        _diagnostics.sourceSummary.maxGapMs = _scalarEmitter.maxRejectedGapMs();
        _diagnostics.sourceSummary.islandCount = _scalarEmitter.rejectedIslandCount();
        _diagnostics.sourceLastCandidate.present = _scalarEmitter.candidateActive()
            || _scalarEmitter.releaseObserved()
            || _scalarEmitter.candidateFirstSeenMs() > 0;
        _diagnostics.sourceLastCandidate.peakMs = _scalarEmitter.candidatePeakMs();
        _diagnostics.sourceLastCandidate.durationMs = _scalarEmitter.transientDurationMs();
        _diagnostics.sourceLastCandidate.windowSamples = 0;
        _diagnostics.sourceLastCandidate.peakPrimary = _scalarEmitter.candidatePeakStrength();
        _diagnostics.sourceLastCandidate.peakSecondary = 0.0f;
        _diagnostics.sourceLastCandidate.reason = _scalarEmitter.lastTransientRejectReasonName();
        _diagnostics.sourceLastCandidate.gateReason = _scalarEmitter.lastTransientRejectReasonName();
        _diagnostics.sourceLastCandidate.scope = "unknown";
        _diagnostics.scalarOpened = _scalarEmitter.candidateActive()
            || _scalarEmitter.releaseObserved()
            || _scalarEmitter.candidateFirstSeenMs() > 0;
        _diagnostics.scalarReleased = _scalarEmitter.releaseObserved()
            || _scalarEmitter.candidateReleaseObservedMs() > 0;
        _diagnostics.scalarValidRelease = _diagnostics.scalarReleased
            && _diagnostics.scalarRejectReason != nullptr
            && strcmp(_diagnostics.scalarRejectReason, "none") == 0;
        _diagnostics.scalarEmitAllowed = _diagnostics.scalarValidRelease;
        _diagnostics.scalarOpenMs = _scalarEmitter.candidateFirstSeenMs();
        _diagnostics.scalarPeakMs = _scalarEmitter.candidatePeakMs();
        _diagnostics.scalarReleaseMs = _scalarEmitter.candidateReleaseObservedMs();
        _diagnostics.scalarDurationMs = _diagnostics.scalarReleased && _diagnostics.scalarReleaseMs >= _diagnostics.scalarOpenMs
            ? _diagnostics.scalarReleaseMs - _diagnostics.scalarOpenMs
            : 0UL;
        _diagnostics.scalarMinDurationMs = _scalarTransientConfig.minTransientDurationMs;
        _diagnostics.scalarMaxDurationMs = _scalarTransientConfig.maxTransientDurationMs;
        _diagnostics.scalarPeakStrength = _scalarEmitter.candidatePeakStrength();
        _diagnostics.scalarTransientRejectedDurationMs = _scalarEmitter.lastTransientRejectedDurationMs();
        _diagnostics.scalarTransientRejectedStrength = _scalarEmitter.lastTransientRejectedStrength();
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
        _diagnostics.frequencyPeakScore = 0.0f;
        _diagnostics.frequencyPeakContrast = 0.0f;
        _diagnostics.frequencyPeakWindowSampleCount = 0;
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
        _diagnostics.frequencyPeakWindowSampleCount = 0;
    }
    _diagnostics.detectorKind = _occurrenceSourceKind == OccurrenceSourceKind::FrequencyMatch
        ? "frequency_match"
        : "scalar_transient";
}

void DetectionRuntime::setFrequencyMatchConfig(const FrequencyMatchConfig& config) {
    _frequencyMatchConfig = config;
    _frequencyEmitter.setConfig(_frequencyMatchConfig);
}

void DetectionRuntime::setScalarTransientConfig(const ScalarTransientConfig& config) {
    _scalarTransientConfig = config;
    _scalarEmitter.setConfig(_scalarTransientConfig);
}

void DetectionRuntime::setOccurrenceSource(OccurrenceSourceKind kind) {
    _occurrenceSourceKind = kind;
    resetOccurrenceSources();
    _frequencyEmitter.setConfig(_frequencyMatchConfig);
    _scalarEmitter.setConfig(_scalarTransientConfig);
}

void DetectionRuntime::setInspectionPlan(const InspectionPlan& plan) {
    _inspectionPlan = plan;
    _occurrenceInspector.configure(_inspectionPlan);
}

void DetectionRuntime::setPatternRulesConfig(const PatternRulesConfig& config) {
    _patternRulesConfig = config;
    _patternRules.configure(_patternRulesConfig);
}

void DetectionRuntime::setFieldStateConfig(const FieldStateConfig& config) {
    _fieldStateTracker.setConfig(config);
}

void DetectionRuntime::setProfileName(const char* profileName) {
    _profileName = profileName != nullptr ? profileName : "unknown";
}

void DetectionRuntime::observeFrame(
    const AudioSamplePacket& frame,
    const FrequencyFeatureFrame& frequencyEvidence,
    unsigned long nowMs
) {
    _fieldStateTracker.update(nowMs);
    if (!frame.valid) {
        return;
    }

    FeatureExtractor::observeFrame(frame, _featureHistory);
    FeatureExtractor::observeFrequencyFeatureFrame(frequencyEvidence, nowMs, _featureHistory);

    switch (_occurrenceSourceKind) {
        case OccurrenceSourceKind::FrequencyMatch:
            _frequencyEmitter.observeFrame(frame, frequencyEvidence);
            break;
        case OccurrenceSourceKind::ScalarTransient:
            _scalarEmitter.observeFrame(
                frame,
                selectedScalarValue(frame, frequencyEvidence, _scalarTransientConfig.observedStream),
                OccurrenceKind::AmpTransient,
                occurrenceSourceForStream(_scalarTransientConfig.observedStream)
            );
            break;
    }

    drainOccurrenceSources(nowMs);
    drainPatternAssembler(nowMs);
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

const FrequencyOccurrenceSource& DetectionRuntime::frequencyEmitter() const {
    return _frequencyEmitter;
}

const FieldState& DetectionRuntime::fieldState() const {
    return _fieldStateTracker.state();
}

const FeatureHistory& DetectionRuntime::featureHistory() const {
    return _featureHistory;
}

void DetectionRuntime::drainOccurrenceSources(unsigned long nowMs) {
    Occurrence candidate;

    switch (_occurrenceSourceKind) {
        case OccurrenceSourceKind::FrequencyMatch:
            while (_frequencyEmitter.popOccurrence(candidate)) {
                _fieldStateTracker.observeOccurrence(candidate, nowMs);
                const InspectedOccurrence inspected = _occurrenceInspector.inspectWithHistory(candidate, &_featureHistory);
                _fieldStateTracker.observeInspectedOccurrence(inspected, nowMs);
                _lastOccurrence = candidate;
                _lastInspectedOccurrence = inspected;
                _patternAssembler.acceptOccurrence(inspected);
            }
            break;
        case OccurrenceSourceKind::ScalarTransient:
            while (_scalarEmitter.popOccurrence(candidate)) {
                _fieldStateTracker.observeOccurrence(candidate, nowMs);
                const InspectedOccurrence inspected = _occurrenceInspector.inspectWithHistory(candidate, &_featureHistory);
                _fieldStateTracker.observeInspectedOccurrence(inspected, nowMs);
                _lastOccurrence = candidate;
                _lastInspectedOccurrence = inspected;
                _patternAssembler.acceptOccurrence(inspected);
            }
            break;
    }

    (void)nowMs;
}

void DetectionRuntime::drainPatternAssembler(unsigned long nowMs) {
    PatternCandidate candidate;
    while (_patternAssembler.popPatternCandidate(candidate)) {
        PatternResult result = _patternRules.evaluate(candidate, nowMs);
        if (_lastInspectedOccurrence.occurrence.present) {
            result.inspectedOccurrence = &_lastInspectedOccurrence;
        }
        _fieldStateTracker.observePatternResult(result, nowMs);
        capturePipelineResult(result, &_lastOccurrence, &_lastInspectedOccurrence, nowMs);
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
    } else if (result.inspectedOccurrence != nullptr && result.inspectedOccurrence->occurrence.present) {
        _latestPipelineResult.inspectedOccurrence = *result.inspectedOccurrence;
    }
    if (_latestPipelineResult.inspectedOccurrence.occurrence.present) {
        _latestPipelineResult.pattern.inspectedOccurrence = &_latestPipelineResult.inspectedOccurrence;
    }
    _latestPipelineResult.hasField = true;
    _latestPipelineResult.field = _fieldStateTracker.state();
    _latestPipelineResult.profileName = _profileName;
    _latestPipelineResult.timestampMs = nowMs;
    _hasLatestPipelineResult = true;
}

} // namespace detection


