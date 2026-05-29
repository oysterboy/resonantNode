#include "AnalyzerApp.h"

#include <Arduino.h>
#include <stdlib.h>
#include <string.h>
#include <new>

#include "../../RuntimeDefaults.h"
#include "../../AudioDebugConfig.h"
#include "../../TimingUtils.h"
#include "AnalyzerTextUtils.h"
#include "../../detection/detectors/FrequencyMatchDetector.h"
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

const char* inspectionPlanName(const detection::InspectionPlan& plan) {
    if (plan.count == 1 &&
        plan.modules[0].kind == detection::InspectionModuleKind::ScalarFeatureStrength) {
        switch (plan.modules[0].target) {
            case detection::EvidenceTarget::FrequencyScoreStrength:
                return "frequency_score";
            case detection::EvidenceTarget::TargetBandStrength:
                return "target_band";
            case detection::EvidenceTarget::AmpStrength:
            default:
                return "amp_strength";
        }
    }

    return "custom";
}

const char* inspectionModulesName(const detection::InspectionPlan& plan) {
    if (plan.count == 1 &&
        plan.modules[0].kind == detection::InspectionModuleKind::ScalarFeatureStrength) {
        return "ScalarFeatureStrength";
    }

    return "custom";
}

const char* inspectionEvidenceTargetsName(const detection::InspectionPlan& plan) {
    if (plan.count > 0 && plan.modules[0].kind == detection::InspectionModuleKind::ScalarFeatureStrength) {
        return evidenceTargetName(plan.modules[0].target);
    }

    return "none";
}

const char* seqOutputModeName(AnalyzerApp::SeqOutputMode mode) {
    switch (mode) {
        case AnalyzerApp::SeqOutputMode::Quiet:
            return "quiet";
        case AnalyzerApp::SeqOutputMode::Compact:
            return "compact";
        case AnalyzerApp::SeqOutputMode::Full:
            return "full";
        case AnalyzerApp::SeqOutputMode::Source:
            return "source";
        case AnalyzerApp::SeqOutputMode::Inspect:
            return "inspect";
        case AnalyzerApp::SeqOutputMode::Pattern:
            return "pattern";
        case AnalyzerApp::SeqOutputMode::Explain:
            return "dump";
        default:
            return "compact";
    }
}

const char* seqOutputWhenName(AnalyzerApp::SeqOutputWhen value) {
    switch (value) {
        case AnalyzerApp::SeqOutputWhen::Off:
            return "off";
        case AnalyzerApp::SeqOutputWhen::All:
            return "all";
        case AnalyzerApp::SeqOutputWhen::Miss:
        default:
            return "miss";
    }
}

bool seqOutputWhenEnabled(AnalyzerApp::SeqOutputWhen configured, AnalyzerResult result) {
    switch (configured) {
        case AnalyzerApp::SeqOutputWhen::All:
            return true;
        case AnalyzerApp::SeqOutputWhen::Off:
            return false;
        case AnalyzerApp::SeqOutputWhen::Miss:
        default:
            switch (result) {
                case AnalyzerResult::Miss:
                case AnalyzerResult::Late:
                case AnalyzerResult::Duplicate:
                case AnalyzerResult::Unexpected:
                case AnalyzerResult::Rejected:
                case AnalyzerResult::Ambiguous:
                case AnalyzerResult::TooDense:
                case AnalyzerResult::InvalidAudio:
                    return true;
                case AnalyzerResult::Expected:
                case AnalyzerResult::Early:
                case AnalyzerResult::Unknown:
                default:
                    return false;
            }
    }
}

AnalyzerApp::SeqOutputMode seqOutputModeFromToken(const char* token, bool* valid) {
    if (valid != nullptr) {
        *valid = true;
    }
    if (token == nullptr || *token == '\0') {
        if (valid != nullptr) {
            *valid = false;
        }
        return AnalyzerApp::SeqOutputMode::Compact;
    }
    if (equalsIgnoreCase(token, "compact") || equalsIgnoreCase(token, "trial")) {
        return AnalyzerApp::SeqOutputMode::Compact;
    }
    if (equalsIgnoreCase(token, "full")) {
        return AnalyzerApp::SeqOutputMode::Full;
    }
    if (equalsIgnoreCase(token, "source")) {
        return AnalyzerApp::SeqOutputMode::Source;
    }
    if (equalsIgnoreCase(token, "inspect")) {
        return AnalyzerApp::SeqOutputMode::Inspect;
    }
    if (equalsIgnoreCase(token, "pattern")) {
        return AnalyzerApp::SeqOutputMode::Pattern;
    }
    if (equalsIgnoreCase(token, "dump")) {
        return AnalyzerApp::SeqOutputMode::Explain;
    }
    if (equalsIgnoreCase(token, "quiet")) {
        return AnalyzerApp::SeqOutputMode::Quiet;
    }
    if (valid != nullptr) {
        *valid = false;
    }
    return AnalyzerApp::SeqOutputMode::Compact;
}

AnalyzerApp::SeqOutputWhen seqOutputWhenFromToken(const char* token, bool* valid) {
    if (valid != nullptr) {
        *valid = true;
    }
    if (token == nullptr || *token == '\0') {
        if (valid != nullptr) {
            *valid = false;
        }
        return AnalyzerApp::SeqOutputWhen::Miss;
    }
    if (equalsIgnoreCase(token, "off")) {
        return AnalyzerApp::SeqOutputWhen::Off;
    }
    if (equalsIgnoreCase(token, "miss")) {
        return AnalyzerApp::SeqOutputWhen::Miss;
    }
    if (equalsIgnoreCase(token, "all")) {
        return AnalyzerApp::SeqOutputWhen::All;
    }
    if (valid != nullptr) {
        *valid = false;
    }
    return AnalyzerApp::SeqOutputWhen::Miss;
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
    _lastPrintMs = 0;
    _usbLineLength = 0;
    _usbLineBuffer[0] = '\0';
    _emitterLineLength = 0;
    _emitterLineBuffer[0] = '\0';
    _controlClaimPending = false;
    _controlClaimSent = false;
    _controlClaimAtMs = 0;

    Serial.println("EVT analyzer_ready");
    Serial.println("EVT analyzer_help type='HELP', 'BASE', 'PARAM freqScore=10000 freqContrast=50.0', 'TEST', 'RAW trigger f=3200 dur=100 post=1000 dump=bin', 'SEQ MODE quiet|compact|full|source|inspect|pattern|dump WHEN off|miss|all VERBOSE 0|1|2 STATUS', 'CAP', 'DET AMP', 'VAL', 'VAL OFF'");
}

void AnalyzerApp::configureParameters() {
    configureI2SParameters();
}

void AnalyzerApp::configureI2SParameters() {
    _audioSignal.setSmoothingFactor(0.5f);
    _audioSignal.setBaselineUpdateFactor(0.005f);
    _audioSignal.setBaselineTrackingQuietThreshold(20);
}

const char* AnalyzerApp::sequenceOutputModeName(SeqOutputMode mode) {
    return seqOutputModeName(mode);
}

const char* AnalyzerApp::sequenceOutputWhenName(SeqOutputWhen value) {
    return seqOutputWhenName(value);
}

bool AnalyzerApp::sequenceOutputModeEnabled(SeqOutputMode configured, SeqOutputMode requested) {
    if (configured == SeqOutputMode::Quiet) {
        return false;
    }
    if (configured == SeqOutputMode::Explain || configured == SeqOutputMode::Full) {
        return true;
    }
    return configured == requested;
}

bool AnalyzerApp::sequenceOutputWhenEnabled(SeqOutputWhen configured, AnalyzerResult result) {
    return seqOutputWhenEnabled(configured, result);
}

AnalyzerApp::SeqOutputMode AnalyzerApp::sequenceOutputModeFromToken(const char* token, bool* valid) {
    return seqOutputModeFromToken(token, valid);
}

AnalyzerApp::SeqOutputWhen AnalyzerApp::sequenceOutputWhenFromToken(const char* token, bool* valid) {
    return seqOutputWhenFromToken(token, valid);
}

AnalyzerApp::SequenceDiagMode AnalyzerApp::sequenceDiagModeFromOutputWhen(SeqOutputWhen when) {
    switch (when) {
        case SeqOutputWhen::Off:
            return SequenceDiagMode::Off;
        case SeqOutputWhen::All:
            return SequenceDiagMode::Trial;
        case SeqOutputWhen::Miss:
        default:
            return SequenceDiagMode::Miss;
    }
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
            _freqBandStream.observeCenteredSample(frame.centeredSample);
            if (_sequenceTest.active && _sequenceTest.currentTrial > 0) {
                const detection::FrequencyFeatureFrame runtimeFrequencyFrame = captureFrequencyFeatureFrame(frame.sampleTimeMs);
                _detection->observeFrame(frame, runtimeFrequencyFrame, frame.sampleTimeMs);
                while (_detection->popPatternResult(_sequenceTest.currentTrialDiagnostics.runtimePatternResult)) {
                    _sequenceTest.currentTrialDiagnostics.runtimePatternCaptured = true;
                    _sequenceTest.currentTrialDiagnostics.runtimeFieldState = _detection->fieldState();
                    handleSequenceCandidate(_sequenceTest.currentTrialDiagnostics.runtimePatternResult, &runtimeFrequencyFrame);
                }
            }
        }
        updateSequenceAmbientStats();

        if (_sequenceTest.active &&
            _sequenceTest.profileKind == detection::DetectionProfileKind::TonalPulse2 &&
            !_sequenceTest.quiet &&
            timing::elapsedSince(now, _sequenceTest.lastStatusPrintMs, 500UL)) {
            Serial.print("SEQ_FREQDBG profile=TonalPulse2 score=");
            Serial.print(_freqBandStream.lastFrequencyScore(), 1);
            Serial.print(" contrast=");
            Serial.print(_freqBandStream.lastSpectralContrast(), 2);
            Serial.print(" target_power=");
            Serial.print(_freqBandStream.lastTargetPower(), 1);
            Serial.print(" neighbor_power=");
            Serial.print(_freqBandStream.lastNeighborPower(), 1);
            Serial.print(" total_energy=");
            Serial.print(_freqBandStream.lastTotalEnergy(), 1);
            Serial.print(" ready=");
            Serial.print(_freqBandStream.windowReady() ? 1 : 0);
            Serial.print(" samples=");
            Serial.println(_freqBandStream.sampleCount());
            _sequenceTest.lastStatusPrintMs = now;
        }

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
        pending.diagMode,
        pending.setupLabel,
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
        case detection::DetectionProfileKind::TonalPulse2:
            return "tonal_pulse_2";
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
        case detection::DetectionProfileKind::TonalPulse2:
            return "tonal_pulse_2 profile view";
        case detection::DetectionProfileKind::ChirpExperimental:
            return "chirp_experimental profile view";
        case detection::DetectionProfileKind::TonalPulse:
        default:
            return "generic tonal pulse profile view";
    }
}

AnalyzerReport* AnalyzerApp::sequenceReportScratch() {
    if (_sequenceReportScratch == nullptr) {
        _sequenceReportScratch = new (std::nothrow) AnalyzerReport();
        if (_sequenceReportScratch != nullptr) {
            *_sequenceReportScratch = makeEmptyAnalyzerReport();
        }
    }

    return _sequenceReportScratch;
}

void AnalyzerApp::buildSequenceAnalyzerReport(AnalyzerReport& report,
                                              unsigned long trialNumber,
                                              AnalyzerResult result,
                                              long dtMs,
                                              long durMs,
                                              float strength,
                                              bool audioOverflow,
                                              unsigned long duplicateCount,
                                              const SequenceTest::TrialDiagnostics& diagnostics) const {
    report = makeEmptyAnalyzerReport();

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
    const bool runtimeReceivedOccurrence = pipelineResult != nullptr && pipelineResult->hasOccurrence;
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
        report.occurrences.score = occurrence.score;
        report.occurrences.contrast = occurrence.contrast;
        report.occurrences.strength = occurrence.strength;
        report.occurrences.confidence = occurrence.confidence;
        report.occurrences.mainRejectReason = runtimeInspectedOccurrence->rejected
            ? occurrenceRejectReasonName(runtimeInspectedOccurrence->rejectReason)
            : "none";
        report.occurrences.rejectReason = report.occurrences.mainRejectReason;
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
        report.occurrences.score = runtimePatternResult != nullptr ? runtimePatternResult->freq.score : 0.0f;
        report.occurrences.contrast = runtimePatternResult != nullptr ? runtimePatternResult->freq.spectralContrast : 0.0f;
        report.occurrences.strength = strength;
        report.occurrences.confidence = trialHasPipelineEvidence ? runtimePatternResult->confidence : 0.0f;
        report.occurrences.mainRejectReason = analyzerReasonName(report.classification.reason);
        report.occurrences.rejectReason = report.occurrences.mainRejectReason;
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
    report.profileDetail.inspectionPlan = inspectionPlanName(selectedProfile.inspectionPlan);
    report.profileDetail.inspectionModules = inspectionModulesName(selectedProfile.inspectionPlan);
    report.profileDetail.evidenceTargets = inspectionEvidenceTargetsName(selectedProfile.inspectionPlan);
    report.profileDetail.requiredSupportTarget = evidenceTargetName(selectedProfile.patternRulesConfig.requiredSupportTarget);
    report.profileDetail.ampStrength = selectedProfile.patternRulesConfig.requireSupportForAcceptance ? "enabled" : "disabled";
    report.profileDetail.ampStrengthMin = strengthClassName(selectedProfile.patternRulesConfig.minimumSupportStrength);
    report.profileDetail.requireSupportForAcceptance = selectedProfile.patternRulesConfig.requireSupportForAcceptance;
    report.profileDetail.freqScore = trialHasPipelineEvidence ? runtimePatternResult->freq.score : 0.0f;
    report.profileDetail.freqContrast = trialHasPipelineEvidence ? runtimePatternResult->freq.spectralContrast : 0.0f;
    report.profileDetail.freqScoreMin = selectedProfile.frequencyMatch.scoreMin;
    report.profileDetail.freqContrastMin = selectedProfile.frequencyMatch.contrastMin;
    report.profileDetail.ampCenteredMagnitude = report.occurrences.primaryStrength;
    report.profileDetail.ampLevel = report.profileDetail.ampCenteredMagnitude;
    report.profileDetail.ampBase = diagnostics.acceptedAmbientBaseline;
    report.profileDetail.ampLift = report.profileDetail.ampCenteredMagnitude - report.profileDetail.ampBase;
    const detection::AmpStrengthEvidence ampStrengthEvidence = trialHasPipelineEvidence && runtimeInspectedOccurrence != nullptr
        ? runtimeInspectedOccurrence->ampStrengthEvidence
        : detection::AmpStrengthEvidence{};
    report.profileDetail.ampStrengthObservation.available = ampStrengthEvidence.available;
    report.profileDetail.ampStrengthObservation.observedOnly = ampStrengthEvidence.observedOnly;
    report.profileDetail.ampStrengthObservation.mode = detection::scalarInspectionModeName(ampStrengthEvidence.mode);
    report.profileDetail.ampStrengthObservation.note = ampStrengthEvidence.available
        ? "amp_strength_seen"
        : (trialHasPipelineEvidence ? "inspector_no_amp_strength" : "missing_pipeline_result");
    report.profileDetail.ampStrengthObservation.windowStartMs = ampStrengthEvidence.windowStartMs;
    report.profileDetail.ampStrengthObservation.windowEndMs = ampStrengthEvidence.windowEndMs;
    report.profileDetail.ampStrengthObservation.classificationValue = ampStrengthEvidence.classificationValue;
    report.profileDetail.ampStrengthObservation.centeredMagnitude = ampStrengthEvidence.peak;
    report.profileDetail.ampStrengthObservation.peak = ampStrengthEvidence.peak;
    report.profileDetail.ampStrengthObservation.mean = ampStrengthEvidence.mean;
    report.profileDetail.ampStrengthObservation.last = ampStrengthEvidence.last;
    report.profileDetail.ampStrengthObservation.baseline = ampStrengthEvidence.baseline;
    report.profileDetail.ampStrengthObservation.lift = ampStrengthEvidence.lift;
    report.profileDetail.ampStrengthObservation.sampleCount = ampStrengthEvidence.sampleCount;
    report.profileDetail.ampStrengthObservation.sustainedCount = ampStrengthEvidence.sustainedCount;
    report.profileDetail.ampStrengthObservation.sustainedMs = ampStrengthEvidence.sustainedMs;
    report.profileDetail.ampStrengthObservation.sustainedThreshold = ampStrengthEvidence.sustainedThreshold;
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

    const bool diagnosticsRequested = _sequenceTest.outputConfig.when != AnalyzerApp::SeqOutputWhen::Off;
    const detection::DetectionDiagnostics* runtimeDiag = nullptr;
    const FrequencyMatchDetector* frequencyDetector = nullptr;
    if (diagnosticsRequested && _detection != nullptr) {
        _detection->captureDiagnostics();
        runtimeDiag = &_detection->diagnostics();
        frequencyDetector = &_detection->frequencyEmitter().detector();
    }

    report.frequency.currentTrialId = report.context.trial;
    report.frequency.windowStartMs = _sequenceTest.currentTrialStartMs;
    report.frequency.windowEndMs = _sequenceTest.currentTrialEndMs;
    report.frequency.expectedWindowMs = _sequenceTest.currentTrialEndMs >= _sequenceTest.currentTrialStartMs
        ? _sequenceTest.currentTrialEndMs - _sequenceTest.currentTrialStartMs
        : 0UL;
    report.frequency.expectedFrameCountEstimate =
        static_cast<unsigned long>((report.frequency.expectedWindowMs
            * static_cast<unsigned long>(_audioSource.sampleRateHz() > 0 ? _audioSource.sampleRateHz() : 16000UL)) / 1000UL);

    report.frequency.acceptedPresent = report.occurrences.present
        && report.occurrences.valid
        && report.primaryPattern.accepted;
    report.frequency.acceptedTrialId = report.frequency.acceptedPresent ? report.context.trial : 0UL;
    report.frequency.acceptedSource = report.frequency.acceptedPresent
        ? (report.occurrences.primarySource != nullptr ? report.occurrences.primarySource : "unknown")
        : "none";
    report.frequency.acceptedDtMs = report.frequency.acceptedPresent ? report.occurrences.primaryDtMs : -1;
    report.frequency.acceptedStartMs = report.frequency.acceptedPresent ? report.occurrences.startMs : 0UL;
    report.frequency.acceptedPeakMs = report.frequency.acceptedPresent ? report.occurrences.peakMs : 0UL;
    report.frequency.acceptedReleaseMs = report.frequency.acceptedPresent ? report.occurrences.releaseMs : 0UL;
    report.frequency.acceptedDurationMs = report.frequency.acceptedPresent ? report.occurrences.primaryDurationMs : 0UL;
    report.frequency.acceptedStrength = report.frequency.acceptedPresent ? report.occurrences.primaryStrength : 0.0f;
    report.frequency.acceptedScore = report.frequency.acceptedPresent ? report.occurrences.score : 0.0f;
    report.frequency.acceptedContrast = report.frequency.acceptedPresent ? report.occurrences.contrast : 0.0f;

    if (runtimeDiag != nullptr) {
        report.frequency.frames = runtimeDiag->frequencyFrames;
        report.frequency.validFrames = runtimeDiag->frequencyValidFrames;
        report.frequency.scoreOkFrames = runtimeDiag->frequencyScoreOkFrames;
        report.frequency.contrastOkFrames = runtimeDiag->frequencyContrastOkFrames;
        report.frequency.bothOkFrames = runtimeDiag->frequencyBothOkFrames;
        report.frequency.matchFrames = runtimeDiag->frequencyMatchFrames;
        report.frequency.rejectFrames = runtimeDiag->frequencyRejectFrames;
        report.frequency.meanScore = runtimeDiag->frequencyScoreMean;
        report.frequency.meanContrast = runtimeDiag->frequencyContrastMean;
        report.frequency.sumScore = report.frequency.meanScore * static_cast<float>(report.frequency.frames);
        report.frequency.sumContrast = report.frequency.meanContrast * static_cast<float>(report.frequency.frames);
        report.frequency.scoreThreshold = runtimeDiag->frequencyScoreThreshold;
        report.frequency.contrastThreshold = runtimeDiag->frequencyContrastThreshold;
        report.frequency.maxScore = runtimeDiag->frequencyScoreMax;
        report.frequency.maxScoreMs = runtimeDiag->frequencyScoreMaxMs;
        report.frequency.maxContrast = runtimeDiag->frequencyContrastMax;
        report.frequency.maxContrastMs = runtimeDiag->frequencyContrastMaxMs;
        report.frequency.minScore = runtimeDiag->frequencyScoreMin;
        report.frequency.minContrast = runtimeDiag->frequencyContrastMin;
        report.frequency.peakScore = runtimeDiag->frequencyPeakScore;
        report.frequency.peakContrast = runtimeDiag->frequencyPeakContrast;
        report.frequency.peakWindowSampleCount = runtimeDiag->frequencyPeakWindowSampleCount;
        report.frequency.liveFreqReason = runtimeDiag->frequencyRejectReason != nullptr ? runtimeDiag->frequencyRejectReason : "none";
        report.frequency.liveFreqWould = runtimeDiag->frequencyWouldCandidateReason != nullptr ? runtimeDiag->frequencyWouldCandidateReason : "none";
        report.frequency.liveFreqState = runtimeDiag->frequencyCandidateState != nullptr ? runtimeDiag->frequencyCandidateState : "none";
        report.frequency.liveFreqReady = runtimeDiag->frequencyReadyOk;
        report.frequency.liveFreqGate = runtimeDiag->frequencyGateOpen;
        report.frequency.liveFreqPresent = runtimeDiag->frequencyPresent;
        report.frequency.liveFreqValid = runtimeDiag->frequencyValidWindow;
        report.frequency.liveFreqMatch = runtimeDiag->frequencyMatched;
        report.frequency.trialMissReason = runtimeDiag->frequencyRejectReason != nullptr ? runtimeDiag->frequencyRejectReason : "unknown";
        report.frequency.nearMiss = runtimeDiag->frequencyNearMiss;
        report.frequency.nearMissReason = runtimeDiag->frequencyNearMissReason != nullptr ? runtimeDiag->frequencyNearMissReason : "none";
    }

    if (report.frequency.longestMatchRunFrames == 0 && report.frequency.matchFrames > 0) {
        report.frequency.longestMatchRunFrames = report.frequency.matchFrames;
        report.frequency.longestMatchRunMs = report.frequency.fmDurationMs;
    }

    if (frequencyDetector != nullptr) {
        report.frequency.sourceOccurrenceEmitted = frequencyDetector->candidateEmitted;
        report.frequency.runtimeEvidenceSeen = runtimeDiag != nullptr ? runtimeDiag->frequencyPresent : false;
        report.frequency.runtimeOccurrenceReceived = report.frequency.sourceOccurrenceEmitted && runtimeReceivedOccurrence;
        report.frequency.fmRejectReason = runtimeDiag != nullptr && runtimeDiag->frequencyRejectReason != nullptr
            ? runtimeDiag->frequencyRejectReason
            : "unknown";
        report.frequency.fmNoEmitReason = runtimeDiag != nullptr && runtimeDiag->frequencyNoEmitReason != nullptr
            ? runtimeDiag->frequencyNoEmitReason
            : "none";
        report.frequency.fmGateReason = runtimeDiag != nullptr && runtimeDiag->frequencyGateReason != nullptr
            ? runtimeDiag->frequencyGateReason
            : "none";
        report.frequency.fmOpened = runtimeDiag != nullptr ? runtimeDiag->frequencyOpened : false;
        report.frequency.fmReleased = runtimeDiag != nullptr ? runtimeDiag->frequencyReleased : false;
        report.frequency.fmEmitted = runtimeDiag != nullptr ? runtimeDiag->frequencyEmitted : false;
        report.frequency.fmDurationOk = runtimeDiag != nullptr ? runtimeDiag->frequencyValidRelease : false;
        report.frequency.fmValidRelease = runtimeDiag != nullptr ? runtimeDiag->frequencyValidRelease : false;
        report.frequency.fmEmitAllowed = runtimeDiag != nullptr ? runtimeDiag->frequencyEmitAllowed : false;
        report.frequency.fmOpenMs = runtimeDiag != nullptr ? runtimeDiag->frequencyOpenMs : 0UL;
        report.frequency.fmPeakMs = runtimeDiag != nullptr ? runtimeDiag->frequencyPeakMs : 0UL;
        report.frequency.fmReleaseMs = runtimeDiag != nullptr ? runtimeDiag->frequencyReleaseMs : 0UL;
        report.frequency.fmDurationMs = runtimeDiag != nullptr ? runtimeDiag->frequencyDurationMs : 0UL;
        report.frequency.fmMinDurationMs = runtimeDiag != nullptr ? runtimeDiag->frequencyMinDurationMs : 0UL;
        report.frequency.fmMaxDurationMs = runtimeDiag != nullptr ? runtimeDiag->frequencyMaxDurationMs : 0UL;
        report.frequency.diagFirstFrameMs = report.frequency.fmOpenMs;
        report.frequency.diagLastFrameMs = report.frequency.fmReleaseMs;
        report.frequency.diagFrameCountOk = report.frequency.expectedFrameCountEstimate == 0
            ? report.frequency.frames == 0
            : report.frequency.frames > 0;
        report.frequency.detectionGateBlocked = !runtimeDiag->frequencyGateOpen || !runtimeDiag->frequencyReadyOk;
        if (!runtimeDiag->frequencyReadyOk) {
            report.frequency.detectionGateReason = "not_ready";
        } else if (!runtimeDiag->frequencyGateOpen) {
            report.frequency.detectionGateReason = report.frequency.fmGateReason != nullptr && report.frequency.fmGateReason[0] != '\0'
                ? report.frequency.fmGateReason
                : "unknown";
        } else {
            report.frequency.detectionGateReason = "none";
        }
    }
    report.frequency.inconsistent = report.classification.result == AnalyzerResult::Miss && report.frequency.acceptedPresent;
    if (report.frequency.acceptedPresent) {
        report.frequency.freqEvidenceClass = "accepted";
    } else if (report.frequency.fmOpened && report.frequency.fmReleased && !report.frequency.fmEmitted) {
        report.frequency.freqEvidenceClass = "strong_no_occurrence";
    } else if (report.frequency.scoreOkFrames > 0 || report.frequency.contrastOkFrames > 0) {
        report.frequency.freqEvidenceClass = "partial";
    } else if (report.frequency.maxScore > 0.0f) {
        report.frequency.freqEvidenceClass = "weak";
    } else {
        report.frequency.freqEvidenceClass = "none";
    }
    if (report.classification.result == AnalyzerResult::Miss && !report.frequency.acceptedPresent && report.frequency.trialMissReason != nullptr && strcmp(report.frequency.trialMissReason, "occurrence_emitted") == 0) {
        report.frequency.trialMissReason = "unknown_or_stale_reason";
        report.frequency.inconsistent = true;
    }
    report.frequency.analyzerSeenOccurrence = report.frequency.acceptedPresent;
    if (!report.frequency.sourceOccurrenceEmitted) {
        report.frequency.runtimeOccurrenceReceived = false;
    }

    const bool scalarProfile = selectedProfile.occurrenceSource == detection::OccurrenceSourceKind::ScalarTransient;
    if (scalarProfile) {
        report.scalar.currentTrialId = report.context.trial;
        report.scalar.windowStartMs = _sequenceTest.currentTrialStartMs;
        report.scalar.windowEndMs = _sequenceTest.currentTrialEndMs;
        report.scalar.expectedWindowMs = report.scalar.windowEndMs >= report.scalar.windowStartMs
            ? report.scalar.windowEndMs - report.scalar.windowStartMs
            : 0UL;
        report.scalar.expectedFrameCountEstimate =
            static_cast<unsigned long>((report.scalar.expectedWindowMs
                * static_cast<unsigned long>(_audioSource.sampleRateHz() > 0 ? _audioSource.sampleRateHz() : 16000UL)) / 1000UL);
        report.scalar.diagFrameCountOk = report.scalar.expectedWindowMs > 0 && report.scalar.expectedFrameCountEstimate > 0;

        report.scalar.acceptedPresent = report.occurrences.present
            && report.occurrences.valid
            && report.primaryPattern.accepted
            && report.occurrences.primarySource != nullptr
            && strcmp(report.occurrences.primarySource, "amp") == 0;
        report.scalar.acceptedTrialId = report.scalar.acceptedPresent ? report.context.trial : 0UL;
        report.scalar.acceptedSource = report.scalar.acceptedPresent
            ? (report.occurrences.primarySource != nullptr ? report.occurrences.primarySource : "unknown")
            : "none";
        report.scalar.acceptedDtMs = report.scalar.acceptedPresent ? report.occurrences.primaryDtMs : -1;
        report.scalar.acceptedStartMs = report.scalar.acceptedPresent ? report.occurrences.startMs : 0UL;
        report.scalar.acceptedPeakMs = report.scalar.acceptedPresent ? report.occurrences.peakMs : 0UL;
        report.scalar.acceptedReleaseMs = report.scalar.acceptedPresent ? report.occurrences.releaseMs : 0UL;
        report.scalar.acceptedDurationMs = report.scalar.acceptedPresent ? report.occurrences.primaryDurationMs : 0UL;
        report.scalar.acceptedStrength = report.scalar.acceptedPresent ? report.occurrences.primaryStrength : 0.0f;
        report.scalar.acceptedScore = report.scalar.acceptedPresent ? report.occurrences.score : 0.0f;
        report.scalar.acceptedContrast = report.scalar.acceptedPresent ? report.occurrences.contrast : 0.0f;

        if (runtimeDiag != nullptr) {
            report.scalar.scalarRejectReason = runtimeDiag->scalarRejectReason != nullptr ? runtimeDiag->scalarRejectReason : "unknown";
            report.scalar.scalarNoEmitReason = runtimeDiag->scalarNoEmitReason != nullptr ? runtimeDiag->scalarNoEmitReason : "none";
            report.scalar.scalarGateReason = runtimeDiag->scalarGateReason != nullptr ? runtimeDiag->scalarGateReason : "none";
            report.scalar.scalarOpened = runtimeDiag->scalarOpened;
            report.scalar.scalarReleased = runtimeDiag->scalarReleased;
            report.scalar.scalarValidRelease = runtimeDiag->scalarValidRelease;
            report.scalar.scalarEmitAllowed = runtimeDiag->scalarEmitAllowed;
        report.scalar.scalarOpenMs = runtimeDiag->scalarOpenMs;
        report.scalar.scalarPeakMs = runtimeDiag->scalarPeakMs;
        report.scalar.scalarReleaseMs = runtimeDiag->scalarReleaseMs;
        report.scalar.scalarDurationMs = runtimeDiag->scalarDurationMs;
        report.scalar.scalarMinDurationMs = runtimeDiag->scalarMinDurationMs;
        report.scalar.scalarMaxDurationMs = runtimeDiag->scalarMaxDurationMs;
        report.scalar.scalarPeakStrength = runtimeDiag->scalarPeakStrength;
        report.scalar.sourceOccurrenceEmitted = report.occurrences.present;
            report.scalar.runtimeEvidenceSeen = runtimeDiag->scalarOpened
                || runtimeDiag->scalarReleased
                || (runtimeDiag->scalarRejectReason != nullptr && strcmp(runtimeDiag->scalarRejectReason, "none") != 0);
            report.scalar.runtimeOccurrenceReceived = report.scalar.sourceOccurrenceEmitted;
            report.scalar.analyzerSeenOccurrence = report.scalar.acceptedPresent;
            report.scalar.liveScalarReason = runtimeDiag->scalarRejectReason != nullptr ? runtimeDiag->scalarRejectReason : "none";
            report.scalar.liveScalarWould = runtimeDiag->scalarNoEmitReason != nullptr ? runtimeDiag->scalarNoEmitReason : "none";
            report.scalar.liveScalarReady = runtimeDiag->scalarOpened;
            report.scalar.liveScalarGate = runtimeDiag->scalarEmitAllowed;
            report.scalar.liveScalarPresent = report.occurrences.present;
            report.scalar.liveScalarValid = report.occurrences.valid;
            report.scalar.liveScalarMatch = report.primaryPattern.accepted;
            report.scalar.liveScalarState = runtimeDiag->scalarOpened
                ? (runtimeDiag->scalarReleased ? "released" : "active")
                : "idle";
            report.scalar.detectionGateBlocked = !report.scalar.acceptedPresent
                && (report.scalar.scalarOpened
                    || report.scalar.scalarReleased
                    || (report.scalar.scalarRejectReason != nullptr && strcmp(report.scalar.scalarRejectReason, "none") != 0));
            if (!report.scalar.acceptedPresent) {
                if (report.scalar.scalarRejectReason != nullptr && strcmp(report.scalar.scalarRejectReason, "none") != 0) {
                    report.scalar.detectionGateReason = report.scalar.scalarRejectReason;
                } else if (report.scalar.scalarOpened && !report.scalar.scalarReleased) {
                    report.scalar.detectionGateReason = "opened_not_released";
                } else if (!report.scalar.scalarOpened) {
                    report.scalar.detectionGateReason = "no_evidence";
                } else {
                    report.scalar.detectionGateReason = "none";
                }
            } else {
                report.scalar.detectionGateReason = "none";
            }
        }

        if (report.scalar.acceptedPresent) {
            report.scalar.scalarRejectReason = "none";
            report.scalar.scalarNoEmitReason = "none";
            report.scalar.scalarGateReason = "none";
            report.scalar.scalarOpened = true;
            report.scalar.scalarReleased = true;
            report.scalar.scalarValidRelease = true;
            report.scalar.scalarEmitAllowed = true;
            report.scalar.scalarOpenMs = report.scalar.acceptedStartMs;
            report.scalar.scalarPeakMs = report.scalar.acceptedPeakMs;
            report.scalar.scalarReleaseMs = report.scalar.acceptedReleaseMs;
            report.scalar.scalarDurationMs = report.scalar.acceptedDurationMs;
            report.scalar.sourceOccurrenceEmitted = true;
            report.scalar.runtimeEvidenceSeen = true;
            report.scalar.runtimeOccurrenceReceived = true;
            report.scalar.analyzerSeenOccurrence = true;
            report.scalar.liveScalarReason = "none";
            report.scalar.liveScalarWould = "none";
            report.scalar.liveScalarReady = true;
            report.scalar.liveScalarGate = true;
            report.scalar.liveScalarPresent = true;
            report.scalar.liveScalarValid = true;
            report.scalar.liveScalarMatch = true;
            report.scalar.liveScalarState = "released";
            report.scalar.detectionGateBlocked = false;
            report.scalar.detectionGateReason = "none";
        }

        report.scalar.inconsistent = report.classification.result == AnalyzerResult::Miss && report.scalar.acceptedPresent;
    }

}









