#include "AnalyzerApp.h"

#include <Arduino.h>
#include <stdlib.h>
#include <string.h>

#include "../../RuntimeDefaults.h"
#include "../../AudioDebugConfig.h"
#include "../../TimingUtils.h"
#include "../../detection/features/FrequencyMatchEvaluation.h"
#include "AnalyzerTextUtils.h"
#include "../../detection/features/FeatureExtractor.h"
#include "../../detection/features/FeatureHistory.h"
#include "../../detection/patterns/PatternAssembler.h"
#include "../../detection/patterns/PatternNames.h"
#include "../../detection/patterns/PatternRules.h"
#include "../../detection/occurrences/Occurrence.h"
#include "AnalyzerClassifier.h"
#include "../../detection/inspector/OccurrenceInspector.h"

/*
AnalyzerApp

This file owns analyzer-mode orchestration, not the detector internals.

File structure:
- local utility helpers
- construction and setup
- runtime loop and diagnostic probe state
- console and emitter control
- raw-trigger and value-mode helpers
- sequence, capture, and base sessions
- diagnostics and summary output
*/
constexpr int kMaxSamplesPerLoop = 128;
constexpr long kLateOnsetMinMs = 200L;
constexpr long kCleanDurationMinMs = 80L;
constexpr long kCleanDurationMaxMs = 180L;
constexpr long kSmearedDurationMinMs = 181L;
constexpr long kSmearedDurationMaxMs = 240L;
constexpr long kTooLongDurationMinMs = 241L;
constexpr long kNearMaxDurationMinMs = 220L;


uint32_t sampleOffsetUs(uint32_t sampleOffset, uint32_t sampleRateHz) {
    if (sampleRateHz == 0) {
        return 0;
    }

    return static_cast<uint32_t>((static_cast<uint64_t>(sampleOffset) * 1000000ULL) / static_cast<uint64_t>(sampleRateHz));
}

bool analyzerLogEnabled(uint32_t flags, AnalyzerApp::AnalyzerLogFlags flag) {
    return (flags & static_cast<uint32_t>(flag)) != 0;
}

void buildFrequencyFailReason(const detection::FrequencyFeatureFrame& evidence,
                              const FrequencyMatchEvaluation::Values& tuning,
                              char* out,
                              size_t outSize) {
    FrequencyMatchEvaluation::buildFailReason(evidence, tuning, out, outSize);
}

const char* occurrenceKindName(detection::OccurrenceKind kind) {
    switch (kind) {
        case detection::OccurrenceKind::AmpTransient:
            return "amp_transient";
        case detection::OccurrenceKind::FrequencyMatch:
            return "frequency_match";
        case detection::OccurrenceKind::BroadbandTransient:
            return "broadband_transient";
        case detection::OccurrenceKind::None:
        default:
            return "none";
    }
}

const char* occurrenceSourceName(detection::OccurrenceSource source) {
    switch (source) {
        case detection::OccurrenceSource::Amp:
            return "amp";
        case detection::OccurrenceSource::Frequency:
            return "frequency";
        case detection::OccurrenceSource::Broadband:
            return "broadband";
        case detection::OccurrenceSource::None:
        default:
            return "none";
    }
}

const char* occurrenceDetectorKindName(detection::OccurrenceDetectorKind kind) {
    switch (kind) {
        case detection::OccurrenceDetectorKind::Transient:
            return "transient";
        case detection::OccurrenceDetectorKind::FrequencyMatch:
            return "frequency_match";
        case detection::OccurrenceDetectorKind::Dip:
            return "dip";
        case detection::OccurrenceDetectorKind::Plateau:
            return "plateau";
        case detection::OccurrenceDetectorKind::ThresholdCrossing:
            return "threshold_crossing";
        case detection::OccurrenceDetectorKind::Unknown:
        default:
            return "unknown";
    }
}

const char* occurrenceRejectReasonName(detection::OccurrenceRejectReason reason) {
    switch (reason) {
        case detection::OccurrenceRejectReason::None:
            return "none";
        case detection::OccurrenceRejectReason::TooShort:
            return "too_short";
        case detection::OccurrenceRejectReason::TooLong:
            return "too_long";
        case detection::OccurrenceRejectReason::TooWeak:
            return "too_weak";
        case detection::OccurrenceRejectReason::BelowThreshold:
            return "below_threshold";
        case detection::OccurrenceRejectReason::DuplicateRisk:
            return "duplicate_risk";
        case detection::OccurrenceRejectReason::Cooldown:
            return "cooldown";
        case detection::OccurrenceRejectReason::MissingFrequencyEvidence:
            return "missing_frequency_evidence";
        case detection::OccurrenceRejectReason::MissingAmpSupport:
            return "missing_amp_support";
        case detection::OccurrenceRejectReason::InvalidTiming:
            return "invalid_timing";
        case detection::OccurrenceRejectReason::UnsupportedKind:
            return "unsupported_kind";
        case detection::OccurrenceRejectReason::Unknown:
        default:
            return "unknown";
    }
}

const char* strengthClassName(detection::StrengthClass value) {
    switch (value) {
        case detection::StrengthClass::None:
            return "none";
        case detection::StrengthClass::Weak:
            return "weak";
        case detection::StrengthClass::Medium:
            return "medium";
        case detection::StrengthClass::Strong:
            return "strong";
        case detection::StrengthClass::Unknown:
        default:
            return "unknown";
    }
}

const char* evidenceTargetName(detection::EvidenceTarget value) {
    switch (value) {
        case detection::EvidenceTarget::AmpStrength:
            return "AmpStrength";
        case detection::EvidenceTarget::FrequencyScoreStrength:
            return "FrequencyScoreStrength";
        case detection::EvidenceTarget::FrequencyContrastQuality:
            return "FrequencyContrastQuality";
        case detection::EvidenceTarget::TargetBandStrength:
            return "TargetBandStrength";
        case detection::EvidenceTarget::None:
        default:
            return "None";
    }
}

// -----------------------------------------------------------------------------
// Construction and setup
// -----------------------------------------------------------------------------

AnalyzerApp::AnalyzerApp(int inputPin)
    : _inputPin(inputPin),
      _i2sSource(14, 27, 33, 16000, 32),
      _audioSource(_i2sSource),
      _audioSignal(_audioSource),
      _freqBandStream() {
    _frequencyEvidenceTuning.scoreMin = detection::detectionProfileForKind(detection::DetectionProfileKind::TonalPulse).frequencyMatch.scoreMin;
    _frequencyEvidenceTuning.contrastMin = detection::detectionProfileForKind(detection::DetectionProfileKind::TonalPulse).frequencyMatch.contrastMin;
}

void AnalyzerApp::begin() {
    beginEmitterControl();

    configureParameters();
    _audioSource.begin();
    _audioSignal.begin();
    _audioSignal.setCurveSampleCallback(&AnalyzerApp::sequenceCurveSampleCallback, this);
    _freqBandStream.resetState();
    _freqBandStream.setSampleRateHz(_audioSource.sampleRateHz());
    _freqBandStream.setTargetFrequencyHz(runtime::kDefaultChirpFrequencyHz);
    if (_detection == nullptr) {
        _detection = new detection::DetectionRuntime();
    }
    if (_sequenceFeatureHistory == nullptr) {
        _sequenceFeatureHistory = new detection::FeatureHistory();
    }
    _sequenceFeatureHistory->reset();
    _lastPrintMs = 0;
    _usbLineLength = 0;
    _usbLineBuffer[0] = '\0';
    _emitterLineLength = 0;
    _emitterLineBuffer[0] = '\0';
    _controlClaimPending = false;
    _controlClaimSent = false;
    _controlClaimAtMs = 0;

    Serial.println("EVT analyzer_ready");
    Serial.println("EVT analyzer_help type='HELP', 'BASE', 'PARAM freqScore=10000 freqContrast=50.0', 'TEST', 'RAW trigger f=3200 dur=100 post=1000 dump=bin', 'SEQ log=default|summary|summary+trial|trial|candidate|explain|custom|full dumpSamples=1 curveFormat=samples', 'CAP', 'DET AMP', 'VAL', 'VAL OFF'");
}

void AnalyzerApp::configureParameters() {
    configureI2SParameters();
}

void AnalyzerApp::configureI2SParameters() {
    _audioSignal.setSmoothingFactor(0.5f);
    _audioSignal.setBaselineUpdateFactor(0.005f);
    _audioSignal.setBaselineTrackingQuietThreshold(20);
}

// -----------------------------------------------------------------------------
// Runtime loop and diagnostic probe state
// -----------------------------------------------------------------------------

void AnalyzerApp::update() {
    const unsigned long now = millis();

    int processedSamples = 0;
    AudioBlock block;
    while (processedSamples < kMaxSamplesPerLoop && _i2sSource.readBlock(block)) {
        if (block.sampleCount == 0 || block.samples == nullptr) {
            break;
        }

        const uint32_t sampleRateHz = _audioSource.sampleRateHz() > 0 ? _audioSource.sampleRateHz() : 16000UL;
        for (uint16_t i = 0; i < block.sampleCount; ++i) {
            const uint32_t sampleTimeUs = block.approxStartMicros + sampleOffsetUs(static_cast<uint32_t>(i), sampleRateHz);
            AudioSignalFrame frame;
            _audioSignal.update(static_cast<int>(block.samples[i]), sampleTimeUs, frame);
            if (_sequenceFeatureHistory != nullptr) {
                detection::FeatureExtractor::observeFrame(frame, *_sequenceFeatureHistory);
            }
            _freqBandStream.observeCenteredSample(frame.centeredSample);
            if (_sequenceTest.active && _sequenceTest.currentTrial > 0) {
                const detection::FrequencyFeatureFrame runtimeFrequencyFrame = captureFrequencyFeatureFrame(frame.sampleTimeMs);
                _detection->observeFrame(frame, runtimeFrequencyFrame, frame.sampleTimeMs);
                detection::PatternResult runtimePatternResult;
                while (_detection->popPatternResult(runtimePatternResult)) {
                    _sequenceTest.currentTrialDiagnostics.runtimePatternResult = runtimePatternResult;
                    _sequenceTest.currentTrialDiagnostics.runtimePatternCaptured = true;
                    _sequenceTest.currentTrialDiagnostics.runtimeFieldState = _detection->fieldState();
                handleSequenceCandidate(runtimePatternResult, &runtimeFrequencyFrame);
                }
            }
        }
        updateSequenceAmbientStats();

        processedSamples += static_cast<int>(block.sampleCount);
        if (processedSamples > kMaxSamplesPerLoop) {
            processedSamples = kMaxSamplesPerLoop;
        }
    }

    _sequenceTest.samplesProcessed += static_cast<unsigned long>(processedSamples);
    if (static_cast<unsigned long>(processedSamples) > _sequenceTest.maxSamplesPerLoop) {
        _sequenceTest.maxSamplesPerLoop = static_cast<unsigned long>(processedSamples);
    }

    updateBaseSession(now);
    if (_controlClaimPending && !_controlClaimSent && timing::atOrAfter(now, _controlClaimAtMs)) {
        sendEmitterCommand("MODE REMOTE");
        _controlClaimSent = true;
        _controlClaimPending = false;
    }
    processPendingSequenceStart();
    updateSequenceTest(now);
    updateCaptureSession(now);
    pollUsbConsole();
    pollEmitterSerial();
    if (_valMode) {
        printValueFrame(now);
    }

#if TEST_LOG_STRESS
    Serial.println("LOG_STRESS");
#endif
}

unsigned long AnalyzerApp::loopDelayMs() const {
    return TEST_LOOP_DELAY_MS;
}

void AnalyzerApp::resetDetectorState() {
    _audioSignal.resetSignalState();
    if (_sequenceFeatureHistory != nullptr) {
        _sequenceFeatureHistory->reset();
    }
}

void AnalyzerApp::startBaseSession(unsigned long durationMs, bool quiet) {
    if (durationMs == 0) {
        durationMs = 1;
    }

    stopSequenceTest();
    stopCaptureSession();
    _baseSession.active = true;
    _baseSession.quiet = quiet;
    _baseSession.durationMs = durationMs;
    _baseSession.startedAtMs = millis();
    _baseSession.lastStatusPrintMs = _baseSession.startedAtMs;
    _baseSession.ignoredRawSamples = 0;
    _baseSession.samples = 0;
    _baseSession.rawSum = 0;
    _baseSession.rawMin = 0;
    _baseSession.rawMax = 0;
    _baseSession.deltaSum = 0.0f;
    _baseSession.deltaMin = 0.0f;
    _baseSession.deltaMax = 0.0f;
    _baseSession.baselineSum = 0.0f;
    _baseSession.baselineMin = 0.0f;
    _baseSession.baselineMax = 0.0f;

    sendEmitterCommand("MODE REMOTE");
    delay(100);
    _audioSignal.rebase();
    resetDetectorState();
    _audioSignal.resetStats();
    _audioSource.resetStats();
    Serial.println("AUDIO stats reset");

    Serial.print("BASE start dur_ms=");
    Serial.println(durationMs);
    if (!_baseSession.quiet) {
        Serial.println("BASE running");
    }
}

void AnalyzerApp::stopBaseSession() {
    _baseSession.active = false;
}

void AnalyzerApp::updateBaseSession(unsigned long now) {
    if (_valMode) {
        return;
    }
    if (!_baseSession.active) {
        return;
    }

    const int raw = _audioSignal.rawSignal();
    const float delta = static_cast<float>(_audioSignal.centeredSignal());
    const float baseline = _audioSignal.baseline();

    if (raw <= 0) {
        _baseSession.ignoredRawSamples++;
        return;
    }

    if (_baseSession.samples == 0) {
        _baseSession.rawMin = raw;
        _baseSession.rawMax = raw;
        _baseSession.deltaMin = delta;
        _baseSession.deltaMax = delta;
        _baseSession.baselineMin = baseline;
        _baseSession.baselineMax = baseline;
    } else {
        if (raw < _baseSession.rawMin) {
            _baseSession.rawMin = raw;
        }
        if (raw > _baseSession.rawMax) {
            _baseSession.rawMax = raw;
        }
        if (delta < _baseSession.deltaMin) {
            _baseSession.deltaMin = delta;
        }
        if (delta > _baseSession.deltaMax) {
            _baseSession.deltaMax = delta;
        }
        if (baseline < _baseSession.baselineMin) {
            _baseSession.baselineMin = baseline;
        }
        if (baseline > _baseSession.baselineMax) {
            _baseSession.baselineMax = baseline;
        }
    }

    _baseSession.samples++;
    _baseSession.rawSum += static_cast<unsigned long>(raw);
    _baseSession.deltaSum += delta;
    _baseSession.baselineSum += baseline;

    if (AUDIO_VERBOSE_DEBUG && !_baseSession.quiet && timing::elapsedSince(now, _baseSession.lastStatusPrintMs, 5000UL)) {
        const unsigned long avgRaw = _baseSession.samples > 0 ? _baseSession.rawSum / _baseSession.samples : 0;
        const float avgDelta = _baseSession.samples > 0 ? _baseSession.deltaSum / static_cast<float>(_baseSession.samples) : 0.0f;
        const float avgBaseline = _baseSession.samples > 0 ? _baseSession.baselineSum / static_cast<float>(_baseSession.samples) : 0.0f;
        const float baselineDrift = _baseSession.baselineMax - _baseSession.baselineMin;

        Serial.print("BASE status t=");
        Serial.print(now);
        Serial.print(" elapsed_ms=");
        Serial.print(now - _baseSession.startedAtMs);
        Serial.print(" samples=");
        Serial.print(_baseSession.samples);
        Serial.print(" rawSample_avg=");
        Serial.print(avgRaw);
        Serial.print(" rawSample_peak=");
        Serial.print(_baseSession.rawMax);
        Serial.print(" centeredSample_avg=");
        Serial.print(avgDelta, 1);
        Serial.print(" centeredSample_max=");
        Serial.print(_baseSession.deltaMax, 1);
        Serial.print(" baseline_avg=");
        Serial.print(avgBaseline, 1);
        Serial.print(" baseline_drift=");
        Serial.println(baselineDrift, 1);
        _baseSession.lastStatusPrintMs = now;
    }

    if (timing::elapsedSince(now, _baseSession.startedAtMs, _baseSession.durationMs)) {
        printBaseSummary();
        stopBaseSession();
        Serial.println("BASE stopped");
    }
}


// -----------------------------------------------------------------------------
// Raw trigger and value-mode helpers
// -----------------------------------------------------------------------------

void AnalyzerApp::printValueModeBanner() const {
    if (_valMode) {
        return;
    }
    Serial.print("EVT analyzer_val on source=");
    Serial.print("I2S");
    Serial.println(" probe=AMP");
    printDetectionParameters();
}

// -----------------------------------------------------------------------------
// Sequence, capture, and base sessions
// -----------------------------------------------------------------------------

void AnalyzerApp::processPendingSequenceStart() {
    if (!_pendingSequenceStart.active) {
        return;
    }

    PendingSequenceStart pending = _pendingSequenceStart;
    _pendingSequenceStart.active = false;

    startSequenceTest(
        pending.totalTrials,
        pending.periodMs,
        pending.windowEndOffsetMs,
        pending.toneHz,
        pending.durationMs,
        pending.quiet,
        pending.showDetails,
        pending.setupLabel,
        pending.logFlags,
        pending.sampleDumpEnabled,
        pending.sampleDumpFirstTrials,
        pending.sampleDumpEveryNth,
        pending.sampleDumpLeadMs,
        pending.sampleDumpTailMs,
        pending.sampleDumpStepMs,
        pending.sampleDumpMaxRows,
        pending.profileKind,
        pending.externalEmitter);
}

const char* AnalyzerApp::activeAnalyzerProfileName() const {
    return detection::detectionProfileName(_sequenceTest.profileKind);
}

const char* analyzerProfileDetailNamespace(detection::DetectionProfileKind profileKind) {
    switch (profileKind) {
        case detection::DetectionProfileKind::Amp:
            return "amp";
        case detection::DetectionProfileKind::ChirpExperimental:
            return "chirp_experimental";
        case detection::DetectionProfileKind::TonalPulse:
        default:
            return "tonal_pulse";
    }
}

const char* analyzerProfileDetailSummary(detection::DetectionProfileKind profileKind) {
    switch (profileKind) {
        case detection::DetectionProfileKind::Amp:
            return "amp scalar profile view";
        case detection::DetectionProfileKind::ChirpExperimental:
            return "chirp_experimental profile view";
        case detection::DetectionProfileKind::TonalPulse:
        default:
            return "generic tonal pulse profile view";
    }
}

AnalyzerReport AnalyzerApp::buildSequenceAnalyzerReport(unsigned long trialNumber,
                                                        AnalyzerResult result,
                                                        long dtMs,
                                                        long durMs,
                                                     float strength,
                                                     bool audioOverflow,
                                                     unsigned long duplicateCount,
                                                     const SequenceTest::TrialDiagnostics& diagnostics) const {
    AnalyzerReport report = makeEmptyAnalyzerReport();

    report.context.profile = activeAnalyzerProfileName();
    report.context.mode = _sequenceTest.externalEmitter ? "OBS" : "SEQ";
    report.context.trial = trialNumber;
    report.context.trigger = _sequenceTest.externalEmitter ? "observe" : "chirp";
    report.context.target = "tone";
    report.context.timestampMs = _sequenceTest.currentTrialEndMs;
    report.context.build = "pass-c";

    report.expected.triggerMs = _sequenceTest.currentTrialStartMs;
    report.expected.windowStartMs = _sequenceTest.currentTrialStartMs;
    report.expected.windowEndMs = _sequenceTest.currentTrialEndMs;
    report.expected.patternType = "sequence_trial";
    report.expected.expectedSource = _sequenceTest.externalEmitter ? "external" : "local";

    const detection::DetectionPipelineResult* pipelineResult = _detection != nullptr && _detection->hasLatestPipelineResult()
        ? &_detection->latestPipelineResult()
        : nullptr;
    const bool actualPipelineAvailable = pipelineResult != nullptr && pipelineResult->hasPattern;
    const detection::PatternResult* runtimePatternResult = actualPipelineAvailable ? &pipelineResult->pattern : nullptr;
    const detection::InspectedOccurrence* runtimeInspectedOccurrence = actualPipelineAvailable && pipelineResult->hasInspectedOccurrence
        ? &pipelineResult->inspectedOccurrence
        : nullptr;
    const detection::FieldState* runtimeFieldState = actualPipelineAvailable && pipelineResult->hasField
        ? &pipelineResult->field
        : nullptr;
    const detection::DetectionProfile& selectedProfile = detection::detectionProfileForKind(_sequenceTest.profileKind);
    const bool trialHasPipelineEvidence = actualPipelineAvailable
        && runtimePatternResult != nullptr
        && diagnostics.rawCandidateCount > 0;
    const auto artifactReason = [&]() -> const char* {
        if (actualPipelineAvailable) {
            return "captured_from_runtime_pipeline";
        }
        return "missing_pipeline_result";
    }();

    AnalyzerSequenceClassificationInput classificationInput;
    classificationInput.result = result;
    classificationInput.dtMs = dtMs;
    classificationInput.rawCandidateCount = diagnostics.rawCandidateCount;
    classificationInput.audioOverflow = audioOverflow;
    classificationInput.patternAvailable = actualPipelineAvailable && runtimePatternResult != nullptr;
    report.classification = classifySequenceTrial(classificationInput);
    {
        // Analyzer consumes the PatternResult produced by DetectionRuntime.
        // Analyzer does not re-run occurrence inspection or pattern interpretation.
        AnalyzerPatternObservation pattern = {};
        pattern.type = trialHasPipelineEvidence ? detection::patternTypeName(runtimePatternResult->type) : "unknown";
        pattern.accepted = trialHasPipelineEvidence
            ? runtimePatternResult->valid
            : false;
        pattern.candidateAccepted = trialHasPipelineEvidence ? runtimePatternResult->patternCandidateAccepted : false;
        pattern.patternMatched = trialHasPipelineEvidence ? runtimePatternResult->patternMatched : false;
        pattern.supportMatched = trialHasPipelineEvidence ? runtimePatternResult->supportMatched : false;
        pattern.behaviorEligible = pattern.accepted;
        pattern.confidence = trialHasPipelineEvidence ? runtimePatternResult->confidence : 0.0f;
        pattern.dtMs = report.classification.dtMs;
        pattern.ampStrength = trialHasPipelineEvidence ? strengthClassName(runtimePatternResult->ampStrength) : "unknown";
        pattern.reason = trialHasPipelineEvidence ? detection::patternReasonName(runtimePatternResult->reasonCode) : analyzerReasonName(report.classification.reason);
        pattern.rejectReason = trialHasPipelineEvidence ? detection::patternRejectReasonName(runtimePatternResult->rejectReason) : analyzerReasonName(report.classification.reason);
        pattern.involvedOccurrences = trialHasPipelineEvidence ? runtimePatternResult->occurrenceCount : 0U;
        report.primaryPattern = pattern;
    }

    report.occurrences.total = diagnostics.rawCandidateCount;
    report.occurrences.accepted = trialHasPipelineEvidence && runtimePatternResult->valid ? 1U : 0U;
    report.occurrences.rejected = diagnostics.rawCandidateCount > report.occurrences.accepted ? diagnostics.rawCandidateCount - report.occurrences.accepted : 0U;
    if (trialHasPipelineEvidence && runtimeInspectedOccurrence != nullptr && runtimeInspectedOccurrence->occurrence.present) {
        const detection::Occurrence& occurrence = runtimeInspectedOccurrence->occurrence;
        report.occurrences.kind = occurrenceKindName(occurrence.kind);
        report.occurrences.primarySource = occurrenceSourceName(occurrence.source);
        report.occurrences.detectorKind = occurrenceDetectorKindName(occurrence.detectorKind);
        report.occurrences.present = occurrence.present;
        report.occurrences.valid = occurrence.valid;
        report.occurrences.startMs = occurrence.startMs;
        report.occurrences.peakMs = occurrence.peakMs;
        report.occurrences.releaseMs = occurrence.releaseMs;
        report.occurrences.primaryDtMs = static_cast<long>(occurrence.startMs) - static_cast<long>(_sequenceTest.currentTrialScheduledAtMs);
        report.occurrences.primaryDurationMs = occurrence.durationMs;
        report.occurrences.primaryStrength = occurrence.strength;
        report.occurrences.strength = occurrence.strength;
        report.occurrences.confidence = occurrence.confidence;
        report.occurrences.mainRejectReason = runtimeInspectedOccurrence->rejected
            ? occurrenceRejectReasonName(runtimeInspectedOccurrence->rejectReason)
            : "none";
        report.occurrences.rejectReason = report.occurrences.mainRejectReason;
        report.occurrences.duplicateRisk = occurrence.duplicateRisk;
    } else {
        report.occurrences.kind = "none";
        report.occurrences.primarySource = "unknown";
        report.occurrences.detectorKind = "unknown";
        report.occurrences.present = false;
        report.occurrences.valid = false;
        report.occurrences.startMs = 0;
        report.occurrences.peakMs = 0;
        report.occurrences.releaseMs = 0;
        report.occurrences.primaryDtMs = dtMs;
        report.occurrences.primaryDurationMs = durMs >= 0 ? static_cast<unsigned long>(durMs) : 0UL;
        report.occurrences.primaryStrength = strength;
        report.occurrences.strength = strength;
        report.occurrences.confidence = trialHasPipelineEvidence ? runtimePatternResult->confidence : 0.0f;
        report.occurrences.mainRejectReason = analyzerReasonName(report.classification.reason);
        report.occurrences.rejectReason = report.occurrences.mainRejectReason;
        report.occurrences.duplicateRisk = duplicateCount > 0;
    }

    report.inspection.inspected = diagnostics.rawCandidateCount;
    report.inspection.accepted = report.occurrences.accepted;
    report.inspection.rejected = diagnostics.rawCandidateCount > report.inspection.accepted ? diagnostics.rawCandidateCount - report.inspection.accepted : 0U;
    if (trialHasPipelineEvidence && runtimeInspectedOccurrence != nullptr && runtimeInspectedOccurrence->occurrence.present) {
        report.inspection.primaryEvidence = occurrenceSourceName(runtimeInspectedOccurrence->occurrence.source);
        switch (selectedProfile.patternRulesConfig.requiredSupportTarget) {
            case detection::EvidenceTarget::FrequencyScoreStrength:
                report.inspection.moduleTarget = "frequency_score";
                report.inspection.moduleStrengthClass = strengthClassName(runtimeInspectedOccurrence->frequencyScoreStrength);
                break;
            case detection::EvidenceTarget::TargetBandStrength:
                report.inspection.moduleTarget = "target_band";
                report.inspection.moduleStrengthClass = strengthClassName(runtimeInspectedOccurrence->targetBandStrength);
                break;
            case detection::EvidenceTarget::AmpStrength:
            default:
                report.inspection.moduleTarget = "amp_strength";
                report.inspection.moduleStrengthClass = strengthClassName(runtimeInspectedOccurrence->ampStrength);
                break;
        }
        report.inspection.mainRejectReason = runtimeInspectedOccurrence->rejected ? occurrenceRejectReasonName(runtimeInspectedOccurrence->rejectReason) : "none";
    } else {
        report.inspection.primaryEvidence = "none";
        report.inspection.moduleTarget = "unknown";
        report.inspection.moduleStrengthClass = "unsupported";
        report.inspection.mainRejectReason = analyzerReasonName(report.classification.reason);
    }

    if (actualPipelineAvailable && runtimeFieldState != nullptr) {
        report.field.state = runtimeFieldState->dense ? "dense" : (runtimeFieldState->active ? (runtimeFieldState->quiet ? "quiet" : "active") : "unknown");
        report.field.rawActivity = runtimeFieldState->activity;
        report.field.validPatternActivity = runtimeFieldState->density;
        report.field.recentValidPatterns = runtimeFieldState->recentPatternCount;
        report.field.recentRejects = runtimeFieldState->recentOccurrenceCount > runtimeFieldState->recentPatternCount
            ? runtimeFieldState->recentOccurrenceCount - runtimeFieldState->recentPatternCount
            : 0U;
    } else {
        report.field.state = "unknown";
        report.field.rawActivity = 0.0f;
        report.field.validPatternActivity = 0.0f;
        report.field.recentValidPatterns = 0U;
        report.field.recentRejects = diagnostics.rawCandidateCount;
    }

    report.profileDetail.namespaceName = analyzerProfileDetailNamespace(_sequenceTest.profileKind);
    report.profileDetail.summary = analyzerProfileDetailSummary(_sequenceTest.profileKind);
    report.profileDetail.emitter = detection::occurrenceSourceKindName(selectedProfile.occurrenceSource);
    report.profileDetail.inspectionAcceptance = detection::occurrenceSourceKindName(selectedProfile.occurrenceSource);
    report.profileDetail.inspectionPlan = selectedProfile.patternRulesConfig.requireSupportForAcceptance
        ? "scalar_feature_strength+duplicate_risk"
        : "duplicate_risk";
    report.profileDetail.inspectionModules = selectedProfile.patternRulesConfig.requireSupportForAcceptance
        ? "ScalarFeatureStrength,DuplicateRisk"
        : "DuplicateRisk";
    report.profileDetail.evidenceTargets = selectedProfile.patternRulesConfig.requireSupportForAcceptance
        ? evidenceTargetName(selectedProfile.patternRulesConfig.requiredSupportTarget)
        : "none";
    report.profileDetail.requiredSupportTarget = evidenceTargetName(selectedProfile.patternRulesConfig.requiredSupportTarget);
    report.profileDetail.ampStrength = selectedProfile.patternRulesConfig.requireSupportForAcceptance ? "enabled" : "disabled";
    report.profileDetail.ampStrengthMin = strengthClassName(selectedProfile.patternRulesConfig.minimumSupportStrength);
    report.profileDetail.requireSupportForAcceptance = selectedProfile.patternRulesConfig.requireSupportForAcceptance;
    report.profileDetail.freqScore = trialHasPipelineEvidence ? runtimePatternResult->freq.score : 0.0f;
    report.profileDetail.freqContrast = trialHasPipelineEvidence ? runtimePatternResult->freq.spectralContrast : 0.0f;
    report.profileDetail.freqScoreMin = selectedProfile.frequencyMatch.scoreMin;
    report.profileDetail.freqContrastMin = selectedProfile.frequencyMatch.contrastMin;
    report.profileDetail.ampLevel = report.occurrences.primaryStrength;
    report.profileDetail.ampBase = diagnostics.acceptedAmbientBaseline;
    report.profileDetail.ampLift = report.profileDetail.ampLevel - report.profileDetail.ampBase;
    const detection::AmpStrengthEvidence ampStrengthEvidence = trialHasPipelineEvidence && runtimeInspectedOccurrence != nullptr
        ? runtimeInspectedOccurrence->ampStrengthEvidence
        : detection::AmpStrengthEvidence{};
    report.profileDetail.ampStrengthObservation.available = ampStrengthEvidence.available;
    report.profileDetail.ampStrengthObservation.observedOnly = ampStrengthEvidence.observedOnly;
    report.profileDetail.ampStrengthObservation.note = ampStrengthEvidence.available
        ? "amp_strength_seen"
        : (trialHasPipelineEvidence ? "inspector_no_amp_strength" : "missing_pipeline_result");
    report.profileDetail.ampStrengthObservation.windowStartMs = ampStrengthEvidence.windowStartMs;
    report.profileDetail.ampStrengthObservation.windowEndMs = ampStrengthEvidence.windowEndMs;
    report.profileDetail.ampStrengthObservation.peak = ampStrengthEvidence.peak;
    report.profileDetail.ampStrengthObservation.baseline = ampStrengthEvidence.baseline;
    report.profileDetail.ampStrengthObservation.lift = ampStrengthEvidence.lift;
    report.profileDetail.ampStrengthObservation.strength = strengthClassName(ampStrengthEvidence.strength);

    report.debug.occurrences = diagnostics.rawCandidateCount;
    report.debug.inspected = diagnostics.rawCandidateCount;
    report.debug.patterns = diagnostics.patternAccepted ? 1U : 0U;
    report.debug.rejects = report.occurrences.rejected;
    report.debug.duplicates = duplicateCount;
    report.debug.unexpected = result == AnalyzerResult::Unexpected ? 1U : 0U;
    report.debug.artifactCaptured = trialHasPipelineEvidence;
    report.debug.artifactFallback = !trialHasPipelineEvidence;
    report.debug.artifactState = trialHasPipelineEvidence ? "CAPTURED" : "MISSING_PIPELINE";
    report.debug.artifactReason = artifactReason;
    report.debug.pipelineSource = trialHasPipelineEvidence ? "actual_pipeline" : "missing_runtime_pipeline";
    report.debug.pipelineFallback = !trialHasPipelineEvidence;
    report.debug.mainRejectReason = trialHasPipelineEvidence && runtimeInspectedOccurrence != nullptr
        ? (runtimeInspectedOccurrence->rejected ? occurrenceRejectReasonName(runtimeInspectedOccurrence->rejectReason) : "none")
        : analyzerReasonName(report.classification.reason);

    return report;
}









