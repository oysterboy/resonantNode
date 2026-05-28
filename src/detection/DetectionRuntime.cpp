#include "DetectionRuntime.h"

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
        case FeatureStreamId::AmbientFloor:
        case FeatureStreamId::Unknown:
        default:
            return OccurrenceSource::Amp;
    }
}

float selectedScalarValue(const AudioSignalFrame& frame, FeatureStreamId stream, const FeatureHistory& history) {
    switch (stream) {
        case FeatureStreamId::AmpEnvelope:
            return frame.centeredMagnitude;
        case FeatureStreamId::AmbientFloor:
            return frame.baseline;
        case FeatureStreamId::FrequencyScore:
        case FeatureStreamId::FrequencyContrast:
            return history.latestValue(stream);
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

const char* frequencyDiagReasonLabel(FrequencyDiagReason reason) {
    switch (reason) {
        case FrequencyDiagReason::None:
            return "none";
        case FrequencyDiagReason::NoFrames:
            return "no_frames";
        case FrequencyDiagReason::NoValidFrames:
            return "no_valid_frames";
        case FrequencyDiagReason::ScoreTooLow:
            return "score_too_low";
        case FrequencyDiagReason::ContrastTooLow:
            return "contrast_too_low";
        case FrequencyDiagReason::ScoreAndContrastTooLow:
            return "score_and_contrast_too_low";
        case FrequencyDiagReason::MatchTooShort:
            return "match_too_short";
        case FrequencyDiagReason::Suppressed:
            return "suppressed";
        case FrequencyDiagReason::NotReady:
            return "not_ready";
        case FrequencyDiagReason::GateClosed:
            return "gate_closed";
        case FrequencyDiagReason::TimingOutsideWindow:
            return "timing_outside_window";
        case FrequencyDiagReason::OccurrenceEmitted:
            return "occurrence_emitted";
        case FrequencyDiagReason::Unknown:
        default:
            return "unknown";
    }
}

FrequencyDiagReason classifyFrequencyDiagReason(const DetectionDiagnostics& diag,
                                                const FrequencyMatchDetector& detector,
                                                const bool acceptedPresent) {
    if (acceptedPresent) {
        return FrequencyDiagReason::OccurrenceEmitted;
    }

    if (diag.frequencyFrames == 0) {
        return FrequencyDiagReason::NoFrames;
    }

    if (diag.frequencyValidFrames == 0) {
        return FrequencyDiagReason::NoValidFrames;
    }

    if (diag.frequencyScoreOkFrames == 0 && diag.frequencyContrastOkFrames == 0) {
        return FrequencyDiagReason::ScoreAndContrastTooLow;
    }

    if (diag.frequencyScoreOkFrames == 0) {
        return FrequencyDiagReason::ScoreTooLow;
    }

    if (diag.frequencyContrastOkFrames == 0) {
        return FrequencyDiagReason::ContrastTooLow;
    }

    if (detector.suppressReason[0] != '\0' && strcmp(detector.suppressReason, "refractory") == 0) {
        return FrequencyDiagReason::Suppressed;
    }

    if (!detector.readyOk) {
        return FrequencyDiagReason::NotReady;
    }

    if (!detector.gateOpen) {
        if (detector.candidateActive) {
            return FrequencyDiagReason::TimingOutsideWindow;
        }
        if (detector.candidateClosed && !detector.candidateEmitted) {
            return FrequencyDiagReason::MatchTooShort;
        }
        return FrequencyDiagReason::GateClosed;
    }

    if (detector.candidateClosed && !detector.candidateEmitted) {
        return FrequencyDiagReason::MatchTooShort;
    }

    return FrequencyDiagReason::Unknown;
}

} // namespace

const char* frequencyDiagReasonName(FrequencyDiagReason reason) {
    return frequencyDiagReasonLabel(reason);
}

void DetectionRuntime::resetState() {
    _frequencyEmitter.reset();
    _scalarEmitter.reset();
    _occurrenceInspector.reset();
    _patternAssembler.reset();
    _fieldStateTracker.reset();
    _featureHistory.reset();
    _resultQueue[0] = {};
    _resultReadIndex = 0;
    _resultCount = 0;
    _latestPipelineResult = {};
    _hasLatestPipelineResult = false;
    resetDiagnostics();
    _lastOccurrence = {};
    _lastInspectedOccurrence = {};
}

void DetectionRuntime::resetDiagnostics() {
    _diagnostics = {};
    _frequencyEmitter.detector().resetDiagnosticsSummary();
}

void DetectionRuntime::setDiagnosticsEnabled(bool enabled) {
    _diagnosticsEnabled = enabled;
    _frequencyEmitter.setDiagnosticsEnabled(enabled);
    _scalarEmitter.setDiagnosticsEnabled(enabled);
    if (!enabled) {
        resetDiagnostics();
    }
}

void DetectionRuntime::captureDiagnostics() {
    if (!_diagnosticsEnabled) {
        resetDiagnostics();
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
    _diagnostics.scalarTransientRejectedDurationMs = _scalarEmitter.lastTransientRejectedDurationMs();
    _diagnostics.scalarTransientRejectedStrength = _scalarEmitter.lastTransientRejectedStrength();

    const auto& detector = _frequencyEmitter.detector();
    _diagnostics.frequencyPresent = detector.diagnosticsObservedCount > 0;
    _diagnostics.frequencyValidWindow = detector.readyOk;
    _diagnostics.frequencyMatched = detector.diagnosticsMatchedCount > 0;
    _diagnostics.frequencyScoreOk = detector.bestScoreOk;
    _diagnostics.frequencyContrastOk = detector.bestContrastOk;
    _diagnostics.frequencyFrames = detector.diagnosticsObservedCount;
    _diagnostics.frequencyValidFrames = detector.diagnosticsValidCount;
    _diagnostics.frequencyScoreOkFrames = detector.diagnosticsScoreOkCount;
    _diagnostics.frequencyContrastOkFrames = detector.diagnosticsContrastOkCount;
    _diagnostics.frequencyBothOkFrames = detector.diagnosticsBothOkCount;
    _diagnostics.frequencyMatchFrames = detector.diagnosticsMatchedCount;
    _diagnostics.frequencyRejectFrames = detector.diagnosticsRejectedCount;
    _diagnostics.frequencyScoreMean = detector.diagnosticsScoreMean();
    _diagnostics.frequencyContrastMean = detector.diagnosticsContrastMean();
    _diagnostics.frequencyScoreMin = detector.diagnosticsScoreMin;
    _diagnostics.frequencyContrastMin = detector.diagnosticsContrastMin;
    _diagnostics.frequencySuppressReason = detector.suppressReason;
    _diagnostics.frequencyWouldCandidateReason = detector.wouldCandidateReason;
    _diagnostics.frequencyCandidateState = detector.candidateState;
    _diagnostics.frequencyReadyOk = detector.readyOk;
    _diagnostics.frequencyGateOpen = detector.gateOpen;
    _diagnostics.frequencyScoreMax = detector.diagnosticsScoreMax;
    _diagnostics.frequencyContrastMax = detector.diagnosticsContrastMax;
    _diagnostics.frequencyScoreMaxMs = detector.diagnosticsScoreMaxMs;
    _diagnostics.frequencyContrastMaxMs = detector.diagnosticsContrastMaxMs;
    _diagnostics.frequencyScoreThreshold = detector.thresholdScore;
    _diagnostics.frequencyContrastThreshold = detector.thresholdContrast;
    _diagnostics.frequencyBestRejectReason = classifyFrequencyDiagReason(_diagnostics, detector, acceptedPresent);
    _diagnostics.frequencyReason = frequencyDiagReasonLabel(_diagnostics.frequencyBestRejectReason);
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
    _frequencyEmitter.reset();
    _scalarEmitter.reset();
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
    const AudioSignalFrame& frame,
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
                selectedScalarValue(frame, _scalarTransientConfig.observedStream, _featureHistory),
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
        const PatternResult result = _patternRules.evaluate(candidate, nowMs);
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
    _latestPipelineResult.hasInspectedOccurrence = inspectedOccurrence != nullptr && inspectedOccurrence->occurrence.present;
    if (_latestPipelineResult.hasInspectedOccurrence && inspectedOccurrence != nullptr) {
        _latestPipelineResult.inspectedOccurrence = *inspectedOccurrence;
    }
    _latestPipelineResult.hasField = true;
    _latestPipelineResult.field = _fieldStateTracker.state();
    _latestPipelineResult.profileName = _profileName;
    _latestPipelineResult.timestampMs = nowMs;
    _hasLatestPipelineResult = true;
}

} // namespace detection


