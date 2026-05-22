#include "AnalyzerApp.h"

#include <Arduino.h>
#include <stdlib.h>
#include <string.h>

#include "../../RuntimeDefaults.h"
#include "../../AudioDebugConfig.h"
#include "../../detection/DetectorParameters.h"
#include "../../detection/features/FrequencyMatchEvaluation.h"
#include "../../detection/inspector/FrequencyWindowProbe.h"
#include "../../detection/features/FeatureExtractor.h"
#include "../../detection/features/FeatureHistory.h"
#include "../../detection/signals/RawWindow.h"
#include "../../detection/patterns/PatternAssembler.h"
#include "../../detection/patterns/PatternNames.h"
#include "../../detection/patterns/PatternRules.h"
#include "../../detection/signals/SignalCandidate.h"
#include "../../detection/inspector/SignalInspector.h"

/*
AnalyzerApp

This file owns analyzer-mode orchestration, not the detector internals.

File structure:
- local utility helpers
- construction and setup
- runtime loop and detector state
- console and emitter control
- raw-trigger and value-mode helpers
- sequence, capture, and base sessions
- diagnostics and summary output
*/
constexpr int kMaxSamplesPerLoop = 128;
constexpr unsigned long kSequenceWarmupMs = 500;
constexpr unsigned long kRawCaptureFlushSamples = 256;
constexpr unsigned long kRawCaptureTimeoutSlackMs = 2000;
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

bool startsWithToken(const char* line, const char* token) {
    return strncmp(line, token, strlen(token)) == 0;
}

bool equalsIgnoreCase(const char* a, const char* b) {
    while (*a != '\0' && *b != '\0') {
        if (toupper(static_cast<unsigned char>(*a)) != toupper(static_cast<unsigned char>(*b))) {
            return false;
        }
        ++a;
        ++b;
    }

    return *a == '\0' && *b == '\0';
}

bool startsWithTokenIgnoreCase(const char* line, const char* token) {
    while (*token != '\0') {
        if (*line == '\0') {
            return false;
        }
        if (toupper(static_cast<unsigned char>(*line)) != toupper(static_cast<unsigned char>(*token))) {
            return false;
        }
        ++line;
        ++token;
    }

    return true;
}

bool analyzerLogEnabled(uint32_t flags, AnalyzerApp::AnalyzerLogFlags flag) {
    return (flags & static_cast<uint32_t>(flag)) != 0;
}

void buildFrequencyFailReason(const detection::FrequencyEvidence& evidence,
                              const FrequencyMatchEvaluation::Values& tuning,
                              char* out,
                              size_t outSize) {
    FrequencyMatchEvaluation::buildFailReason(evidence, tuning, out, outSize);
}

const char* signalKindName(detection::SignalKind kind) {
    switch (kind) {
        case detection::SignalKind::AmpTransient:
            return "amp_transient";
        case detection::SignalKind::FrequencyMatch:
            return "frequency_match";
        case detection::SignalKind::BroadbandTransient:
            return "broadband_transient";
        case detection::SignalKind::None:
        default:
            return "none";
    }
}

const char* signalSourceName(detection::SignalSource source) {
    switch (source) {
        case detection::SignalSource::Amp:
            return "amp";
        case detection::SignalSource::Frequency:
            return "frequency";
        case detection::SignalSource::Broadband:
            return "broadband";
        case detection::SignalSource::None:
        default:
            return "none";
    }
}

const char* signalRejectReasonName(detection::SignalRejectReason reason) {
    switch (reason) {
        case detection::SignalRejectReason::None:
            return "none";
        case detection::SignalRejectReason::TooShort:
            return "too_short";
        case detection::SignalRejectReason::TooLong:
            return "too_long";
        case detection::SignalRejectReason::TooWeak:
            return "too_weak";
        case detection::SignalRejectReason::BelowThreshold:
            return "below_threshold";
        case detection::SignalRejectReason::DuplicateRisk:
            return "duplicate_risk";
        case detection::SignalRejectReason::Cooldown:
            return "cooldown";
        case detection::SignalRejectReason::MissingFrequencyEvidence:
            return "missing_frequency_evidence";
        case detection::SignalRejectReason::MissingAmpSupport:
            return "missing_amp_support";
        case detection::SignalRejectReason::InvalidTiming:
            return "invalid_timing";
        case detection::SignalRejectReason::UnsupportedKind:
            return "unsupported_kind";
        case detection::SignalRejectReason::Unknown:
        default:
            return "unknown";
    }
}

const char* ampSupportName(detection::AmpSupportLevel value) {
    switch (value) {
        case detection::AmpSupportLevel::None:
            return "none";
        case detection::AmpSupportLevel::Weak:
            return "weak";
        case detection::AmpSupportLevel::Medium:
            return "medium";
        case detection::AmpSupportLevel::Strong:
            return "strong";
        case detection::AmpSupportLevel::Unknown:
        default:
            return "unknown";
    }
}

int16_t rawCaptureSampleToInt16(int sample) {
    const int32_t shifted = static_cast<int32_t>(sample) >> 16;
    if (shifted > 32767) {
        return 32767;
    }
    if (shifted < -32768) {
        return -32768;
    }
    return static_cast<int16_t>(shifted);
}

unsigned long rawCaptureChunkSize(unsigned long sampleRateHz, unsigned long decim) {
    const unsigned long baseChunk = sampleRateHz / 20UL;
    const unsigned long decimatedChunk = decim > 0 ? baseChunk / decim : baseChunk;
    return decimatedChunk > 0 ? decimatedChunk : 1UL;
}

unsigned long countSelectedSampleDumpTrials(unsigned long totalTrials, unsigned long firstTrials, unsigned long everyNth) {
    unsigned long selected = 0;
    for (unsigned long trial = 1; trial <= totalTrials; ++trial) {
        const bool firstSelected = firstTrials > 0 && trial <= firstTrials;
        const bool everySelected = everyNth > 0 && trial % everyNth == 0;
        if (firstSelected || everySelected) {
            ++selected;
        }
    }
    return selected;
}

uint32_t analyzerLogFlagsFromLevel(unsigned long level) {
    if (level == 0) {
        return AnalyzerApp::ANALYZER_LOG_NONE;
    }
    if (level == 1) {
        return AnalyzerApp::DEFAULT_ANALYZER_LOG_FLAGS;
    }
    return AnalyzerApp::ANALYZER_LOG_SUMMARY |
           AnalyzerApp::ANALYZER_LOG_TRIAL |
           AnalyzerApp::ANALYZER_LOG_CANDIDATE |
           AnalyzerApp::ANALYZER_LOG_EXPLAIN;
}

uint32_t analyzerLogFlagsFromToken(const char* token) {
    if (token == nullptr || *token == '\0') {
        return AnalyzerApp::DEFAULT_ANALYZER_LOG_FLAGS;
    }

    if (equalsIgnoreCase(token, "default")) {
        return AnalyzerApp::DEFAULT_ANALYZER_LOG_FLAGS;
    }
    if (equalsIgnoreCase(token, "quiet") || equalsIgnoreCase(token, "none")) {
        return AnalyzerApp::ANALYZER_LOG_NONE;
    }
    if (equalsIgnoreCase(token, "full")) {
        return AnalyzerApp::ANALYZER_LOG_SUMMARY |
               AnalyzerApp::ANALYZER_LOG_TRIAL |
               AnalyzerApp::ANALYZER_LOG_CANDIDATE |
               AnalyzerApp::ANALYZER_LOG_EXPLAIN;
    }

    char buffer[64];
    strncpy(buffer, token, sizeof(buffer));
    buffer[sizeof(buffer) - 1] = '\0';

    uint32_t flags = AnalyzerApp::ANALYZER_LOG_NONE;
    char* savePtr = nullptr;
    char* part = strtok_r(buffer, ",+|", &savePtr);
    while (part != nullptr) {
        if (equalsIgnoreCase(part, "summary")) {
            flags |= AnalyzerApp::ANALYZER_LOG_SUMMARY;
        } else if (equalsIgnoreCase(part, "trial")) {
            flags |= AnalyzerApp::ANALYZER_LOG_TRIAL;
        } else if (equalsIgnoreCase(part, "candidate")) {
            flags |= AnalyzerApp::ANALYZER_LOG_CANDIDATE;
        } else if (equalsIgnoreCase(part, "explain")) {
            flags |= AnalyzerApp::ANALYZER_LOG_EXPLAIN;
        } else if (equalsIgnoreCase(part, "custom")) {
            flags |= AnalyzerApp::ANALYZER_LOG_CUSTOM;
        } else if (equalsIgnoreCase(part, "default")) {
            flags |= AnalyzerApp::DEFAULT_ANALYZER_LOG_FLAGS;
        } else if (equalsIgnoreCase(part, "full")) {
            flags |= AnalyzerApp::ANALYZER_LOG_SUMMARY |
                     AnalyzerApp::ANALYZER_LOG_TRIAL |
                     AnalyzerApp::ANALYZER_LOG_CANDIDATE |
                     AnalyzerApp::ANALYZER_LOG_EXPLAIN;
        } else if (equalsIgnoreCase(part, "quiet") || equalsIgnoreCase(part, "none")) {
            flags = AnalyzerApp::ANALYZER_LOG_NONE;
        }
        part = strtok_r(nullptr, ",+|", &savePtr);
    }

    return flags;
}

void printSequenceHelp() {
    Serial.println("CMD: SEQ help");
    Serial.println("CMD: SEQ");
    Serial.println("CMD: SEQ stop");
    Serial.println("SEQ IN: start [tries=N] [period=MS] [window=MS] [freq=HZ] [dur=MS] [test=LABEL]");
    Serial.println("SEQ IN: OBS start [tries=N] [period=2000] [window=1800] [freq=HZ] [dur=MS] [test=LABEL]");
    Serial.println("SEQ IN: [profile=freqamp|chirp]");
    Serial.println("SEQ IN: [log=default|none|quiet|summary|summary+trial|trial|candidate|explain|custom|full]");
    Serial.println("SEQ IN: stable summary=log=summary");
    Serial.println("SEQ IN: [debug=0|1|2] [dumpSamples=0|1] [curveFormat=off|samples]");
    Serial.println("SEQ IN: [sampleFirst=N] [sampleEvery=N] [sampleLead=MS] [sampleTail=MS] [sampleStep=MS] [sampleMax=N]");
    Serial.println("SEQ OUT: SEQ start / SEQ running / SEQ_CAND / SEQ_TRIAL / SEQ_EXPLAIN / SEQ_CUSTOM / SEQ_SUMMARY");
    Serial.println("SEQ OUT: candidate fields include onset_sample peak_sample release_sample peak_ms dur end_dt_ms freq_*");
    Serial.println("SEQ OBS: passive observe mode for an already-running external emitter");
    Serial.println("SEQ PROFILE: profile=freqamp|chirp");
    Serial.println("SEQ PARAM: freqScore=50000 freqContrast=20.0");
}

bool waitForEmitterAck(const char* expectedPrefix, unsigned long timeoutMs) {
    const unsigned long startMs = millis();
    char line[96];
    size_t lineLength = 0;

    // Wait synchronously for the emitter to acknowledge remote mode.
    while (millis() - startMs < timeoutMs) {
        while (Serial2.available() > 0) {
            const char c = static_cast<char>(Serial2.read());
            if (c == '\r') {
                continue;
            }

            if (c == '\n') {
                line[lineLength] = '\0';
                if (lineLength > 0 && strncmp(line, expectedPrefix, strlen(expectedPrefix)) == 0) {
                    return true;
                }
                lineLength = 0;
                continue;
            }

            if (lineLength < sizeof(line) - 1) {
                line[lineLength++] = c;
            }
        }
        delay(1);
    }

    return false;
}

const char* h3SequenceCandidateClassFromResult(const char* result) {
    if (strcmp(result, "expected") == 0) {
        return "expected_primary";
    }
    if (strcmp(result, "duplicate") == 0) {
        return "duplicate";
    }
    if (strcmp(result, "late") == 0) {
        return "late";
    }
    if (strcmp(result, "self_suppressed") == 0) {
        return "self_suppressed";
    }
    return "unexpected_noise";
}

const char* h3SequenceCandidateClass(bool duplicateCandidate, bool inWindow, long dtFromTriggerMs) {
    if (duplicateCandidate) {
        return "duplicate";
    }
    if (!inWindow) {
        return "unexpected_noise";
    }
    if (dtFromTriggerMs >= kLateOnsetMinMs) {
        return "late";
    }
    return "expected_primary";
}

const char* sequenceTrialDurationClass(long durMs) {
    if (durMs < 0) {
        return "-";
    }
    if (durMs <= kCleanDurationMaxMs) {
        return "normal";
    }
    if (durMs >= kNearMaxDurationMinMs) {
        return "near_max";
    }
    return "long";
}

void printH3FrequencyEvidenceFields(const detection::PatternResult& patternResult,
                                    const detection::FrequencyEvidence& frequencyEvidence,
                                    const detection::FrequencyEvidence* liveFrequencyEvidence,
                                    const FrequencyMatchEvaluation::Values& tuning,
                                    const char* candidateClass,
                                    long transientAgeOrDtMs,
                                    unsigned long referenceMs) {
        const auto frequencyEval = FrequencyMatchEvaluation::evaluate(frequencyEvidence, tuning);
        Serial.print(" candidate_class=");
        Serial.print(candidateClass);
    Serial.print(" pattern_valid=");
    Serial.print(patternResult.valid ? 1 : 0);
    Serial.print(" pattern_type=");
    Serial.print(detection::patternTypeName(patternResult.type));
    Serial.print(" pattern_reason=");
    Serial.print(detection::patternReasonName(patternResult.reasonCode));
    Serial.print(" candidateAccepted=");
    Serial.print(patternResult.patternCandidateAccepted ? 1 : 0);
    Serial.print(" patternMatched=");
    Serial.print(patternResult.patternMatched ? 1 : 0);
    Serial.print(" supportMatched=");
    Serial.print(patternResult.supportMatched ? 1 : 0);
    Serial.print(" behaviorEligible=");
    Serial.print(patternResult.patternMatched && patternResult.supportMatched ? 1 : 0);
    Serial.print(" reject_reason=");
    Serial.print(detection::patternRejectReasonName(patternResult.rejectReason));
    Serial.print(" transient_duration_ms=");
    Serial.print(patternResult.candidate.durationMs);
    Serial.print(" transient_peak_strength=");
    Serial.print(patternResult.candidate.peakStrength, 1);
    Serial.print(" transient_age_or_dt_ms=");
    if (transientAgeOrDtMs >= 0) {
        Serial.print(transientAgeOrDtMs);
        Serial.print("ms");
    } else {
        Serial.print("-");
    }
    Serial.print(" freq_present=");
    Serial.print(patternResult.freq.present ? 1 : 0);
    Serial.print(" freq_matched=");
    Serial.print(patternResult.freq.matched ? 1 : 0);
    Serial.print(" freq_score_ok=");
    Serial.print(frequencyEval.scoreOk ? 1 : 0);
    Serial.print(" freq_contrast_ok=");
    Serial.print(frequencyEval.contrastOk ? 1 : 0);
    Serial.print(" freq_score=");
    Serial.print(patternResult.freq.score, 1);
    Serial.print(" freq_conf=");
    Serial.print(patternResult.freq.confidence, 1);
    Serial.print(" freq_target_hz=");
    Serial.print(frequencyEvidence.targetHz);
    Serial.print(" freq_target_power=");
    Serial.print(frequencyEvidence.targetPower, 1);
    Serial.print(" freq_neighbor_power=");
    Serial.print(frequencyEvidence.neighborPower, 1);
    Serial.print(" freq_total_energy=");
    Serial.print(frequencyEvidence.totalEnergy, 1);
    Serial.print(" freq_contrast=");
    Serial.print(patternResult.freq.spectralContrast, 2);
    Serial.print(" freq_observed_at_ms=");
    Serial.print(patternResult.freq.observedAtMs);
    Serial.print(" freq_age_ms=");
    if (patternResult.freq.observedAtMs > 0 && referenceMs >= patternResult.freq.observedAtMs) {
        Serial.print(referenceMs - patternResult.freq.observedAtMs);
        Serial.print("ms");
    } else {
        Serial.print("-");
    }
    Serial.print(" freq_valid_window=");
    Serial.print(patternResult.freq.validWindow ? 1 : 0);
    Serial.print(" freq_eval_reason=");
    Serial.print(FrequencyMatchEvaluation::reasonName(frequencyEval.reason));
    if (liveFrequencyEvidence != nullptr) {
        Serial.print(" liveFreq[avail=");
        Serial.print(liveFrequencyEvidence->present ? 1 : 0);
        Serial.print(" ready=");
        Serial.print(liveFrequencyEvidence->windowAvailable ? 1 : 0);
        Serial.print(" samples=");
        Serial.print(liveFrequencyEvidence->windowSampleCount);
        Serial.print(" score=");
        Serial.print(liveFrequencyEvidence->score, 1);
        Serial.print(" target=");
        Serial.print(liveFrequencyEvidence->targetHz);
        Serial.print(" contrast=");
        Serial.print(liveFrequencyEvidence->spectralContrast, 2);
        Serial.print(" obs=");
        Serial.print(liveFrequencyEvidence->observedAtMs);
        Serial.print("]");
    }
}

// -----------------------------------------------------------------------------
// Construction and setup
// -----------------------------------------------------------------------------

AnalyzerApp::AnalyzerApp(int inputPin)
    : _inputPin(inputPin),
      _i2sSource(14, 27, 33, 16000, 32),
      _audioSource(_i2sSource),
      _audioOnsetDetector(),
      _audioSignal(_audioSource),
      _freqBandStream() {
    _frequencyEvidenceTuning.scoreMin = 10000.0f;
    _frequencyEvidenceTuning.contrastMin = 20.0f;
}

void AnalyzerApp::begin() {
    beginEmitterControl();

    configureParameters();
    _audioSource.begin();
    _audioSignal.begin();
    _audioSignal.setCurveSampleCallback(&AnalyzerApp::sequenceCurveSampleCallback, this);
    _audioOnsetDetector.begin();
    _freqBandStream.resetState();
    _freqBandStream.setSampleRateHz(_audioSource.sampleRateHz());
    _freqBandStream.setTargetFrequencyHz(runtime::kDefaultChirpFrequencyHz);
    if (_sequenceFeatureHistory == nullptr) {
        _sequenceFeatureHistory = new detection::FeatureHistory();
    }
    _sequenceFeatureHistory->reset();
    _audioOnsetDetector.setDiagnosticsEnabled(AUDIO_VERBOSE_DEBUG);
    _lastPrintMs = 0;
    _usbLineLength = 0;
    _usbLineBuffer[0] = '\0';
    _emitterLineLength = 0;
    _emitterLineBuffer[0] = '\0';
    _controlClaimPending = false;
    _controlClaimSent = false;
    _controlClaimAtMs = 0;

    Serial.println("EVT analyzer_ready");
    Serial.println("EVT analyzer_help type='HELP', 'BASE', 'PARAM onset=23.0 release=20.0 cooldown=50 releaseDebounce=10 minMs=90 maxMs=240 minStrength=40.0 freqScore=10000 freqContrast=20.0', 'TEST', 'RAW trigger f=3200 dur=100 post=1000 dump=bin', 'SEQ log=default|summary|summary+trial|trial|candidate|explain|custom|full dumpSamples=1 curveFormat=samples', 'CAP', 'DET AMP', 'VAL', 'VAL OFF'");
}

void AnalyzerApp::configureParameters() {
    configureSharedParameters();
    configureI2SParameters();
}

void AnalyzerApp::configureSharedParameters() {
    _audioSignal.setSmoothingFactor(0.5f);
    _audioSignal.setBaselineUpdateFactor(0.005f);
}

void AnalyzerApp::configureI2SParameters() {
    _audioSignal.setBaselineTrackingQuietThreshold(20);
    setDetectorOnsetDetectionThreshold(30.0f);
    setDetectorOnsetReleaseThreshold(20.0f);
    setDetectorCooldownAfterOnsetMs(50);
    setDetectorReleaseDebounceMs(10);
    setDetectorMinTransientDurationMs(90);
    setDetectorMaxTransientDurationMs(240);
    setDetectorMinTransientPeakStrength(40.0f);
}

// -----------------------------------------------------------------------------
// Runtime loop and detector state
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
            _audioOnsetDetector.update(static_cast<float>(frame.level), frame.sampleTimeUs);
            _freqBandStream.observeCenteredSample(frame.centeredSample);
            if (_sequenceTest.active && _sequenceTest.currentTrial > 0 && _detection != nullptr) {
                const detection::FrequencyEvidence runtimeFrequencyEvidence = captureFrequencyEvidence(frame.sampleTimeMs);
                _detection->observeFrame(frame, runtimeFrequencyEvidence, frame.sampleTimeMs);
                detection::PatternResult runtimePatternResult;
                while (_detection->popPatternResult(runtimePatternResult)) {
                    _sequenceTest.currentTrialDiagnostics.runtimePatternResult = runtimePatternResult;
                    _sequenceTest.currentTrialDiagnostics.runtimePatternCaptured = true;
                    _sequenceTest.currentTrialDiagnostics.runtimeFieldState = _detection->fieldState();
                handleSequenceCandidate(runtimePatternResult, &runtimeFrequencyEvidence);
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
    if (_controlClaimPending && !_controlClaimSent && now >= _controlClaimAtMs) {
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

bool AnalyzerApp::detectorOnsetDetected() const {
    return _audioOnsetDetector.onsetDetected();
}

float AnalyzerApp::detectorOnsetStrength() const {
    return _audioOnsetDetector.onsetStrength();
}

bool AnalyzerApp::detectorTransientDetected() const {
    return _audioOnsetDetector.transientDetected();
}

float AnalyzerApp::detectorTransientStrength() const {
    return _audioOnsetDetector.transientStrength();
}

unsigned long AnalyzerApp::detectorTransientDurationMs() const {
    return _audioOnsetDetector.transientDurationMs();
}

bool AnalyzerApp::detectorTransientPeakActive() const {
    return _audioOnsetDetector.peakActive();
}

const char* AnalyzerApp::detectorOnsetRejectReasonName() const {
    return _audioOnsetDetector.lastOnsetRejectReasonName();
}

const char* AnalyzerApp::detectorTransientRejectReasonName() const {
    return _audioOnsetDetector.lastTransientRejectReasonName();
}

unsigned long AnalyzerApp::detectorTransientRejectedDurationMs() const {
    return _audioOnsetDetector.lastTransientRejectedDurationMs();
}

float AnalyzerApp::detectorTransientRejectedStrength() const {
    return _audioOnsetDetector.lastTransientRejectedStrength();
}

float AnalyzerApp::detectorOnsetDetectionThreshold() const {
    return _audioOnsetDetector.onsetDetectionThreshold();
}

float AnalyzerApp::detectorOnsetReleaseThreshold() const {
    return _audioOnsetDetector.onsetReleaseThreshold();
}

unsigned long AnalyzerApp::detectorCooldownAfterOnsetMs() const {
    return _audioOnsetDetector.cooldownAfterOnsetMs();
}

unsigned long AnalyzerApp::detectorMinTransientDurationMs() const {
    return _audioOnsetDetector.minTransientDurationMs();
}

unsigned long AnalyzerApp::detectorMaxTransientDurationMs() const {
    return _audioOnsetDetector.maxTransientDurationMs();
}

float AnalyzerApp::detectorMinTransientPeakStrength() const {
    return _audioOnsetDetector.minTransientPeakStrength();
}

unsigned long AnalyzerApp::detectorReleaseDebounceMs() const {
    return _audioOnsetDetector.releaseDebounceMs();
}

void AnalyzerApp::setDetectorOnsetDetectionThreshold(float value) {
    _audioOnsetDetector.setOnsetDetectionThreshold(value);
}

void AnalyzerApp::setDetectorOnsetReleaseThreshold(float value) {
    _audioOnsetDetector.setOnsetReleaseThreshold(value);
}

void AnalyzerApp::setDetectorCooldownAfterOnsetMs(unsigned long value) {
    _audioOnsetDetector.setCooldownAfterOnsetMs(value);
}

void AnalyzerApp::setDetectorMinTransientDurationMs(unsigned long value) {
    _audioOnsetDetector.setMinTransientDurationMs(value);
}

void AnalyzerApp::setDetectorMaxTransientDurationMs(unsigned long value) {
    _audioOnsetDetector.setMaxTransientDurationMs(value);
}

void AnalyzerApp::setDetectorMinTransientPeakStrength(float value) {
    _audioOnsetDetector.setMinTransientPeakStrength(value);
}

void AnalyzerApp::setDetectorReleaseDebounceMs(unsigned long value) {
    _audioOnsetDetector.setReleaseDebounceMs(value);
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

    if (AUDIO_VERBOSE_DEBUG && !_baseSession.quiet && now - _baseSession.lastStatusPrintMs >= 5000UL) {
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

    if (now - _baseSession.startedAtMs >= _baseSession.durationMs) {
        printBaseSummary();
        stopBaseSession();
        Serial.println("BASE stopped");
    }
}

void AnalyzerApp::beginEmitterControl() {
    Serial2.begin(_controlBaudRate, SERIAL_8N1, _controlRxPin, _controlTxPin);
    Serial.print("EVT analyzer_control rx=");
    Serial.print(_controlRxPin);
    Serial.print(" tx=");
    Serial.println(_controlTxPin);
    Serial.println("EVT analyzer_control_claim scheduled");
}

// -----------------------------------------------------------------------------
// Console and emitter control
// -----------------------------------------------------------------------------

void AnalyzerApp::pollUsbConsole() {
    while (Serial.available() > 0) {
        const char c = static_cast<char>(Serial.read());
        if (c == '\r') {
            continue;
        }

        if (c == '\n') {
            _usbLineBuffer[_usbLineLength] = '\0';
            if (_usbLineLength > 0) {
                handleUsbLine(_usbLineBuffer);
            }
            _usbLineLength = 0;
            continue;
        }

        if (_usbLineLength < sizeof(_usbLineBuffer) - 1) {
            _usbLineBuffer[_usbLineLength++] = c;
        }
    }
}

void AnalyzerApp::pollEmitterSerial() {
    while (Serial2.available() > 0) {
        const char c = static_cast<char>(Serial2.read());
        if (c == '\r') {
            continue;
        }

        if (c == '\n') {
            _emitterLineBuffer[_emitterLineLength] = '\0';
            // Expected acknowledgements are suppressed so the console stays readable.
            if (_emitterLineLength > 0
                && !_valMode
                && !startsWithTokenIgnoreCase(_emitterLineBuffer, "OK CHIRP")
                && !startsWithTokenIgnoreCase(_emitterLineBuffer, "OK MODE REMOTE")) {
                Serial.print("EMIT< ");
                Serial.println(_emitterLineBuffer);
            }
            _emitterLineLength = 0;
            continue;
        }

        if (_emitterLineLength < sizeof(_emitterLineBuffer) - 1) {
            _emitterLineBuffer[_emitterLineLength++] = c;
        }
    }
}

// -----------------------------------------------------------------------------
// Command parsing
// -----------------------------------------------------------------------------

void AnalyzerApp::handleUsbLine(const char* line) {
    if (equalsIgnoreCase(line, "HELP")) {
        if (_valMode) {
            return;
        }
        Serial.println("CMD: BASE dur=10000 quiet");
        Serial.println("CMD: BASE stop");
        Serial.println("CMD: PARAM onset=23.0 release=20.0 cooldown=50 releaseDebounce=10 minMs=90 maxMs=240 minStrength=40.0 freqScore=10000 freqContrast=20.0");
        Serial.println("CMD: EMIT CHIRP freq=3200 dur=100");
        Serial.println("CMD: EMIT MODE REMOTE");
        Serial.println("CMD: EMIT MODE AUTO interval=2000 freq=3200 dur=100");
        Serial.println("CMD: EMIT SWEEP start=3000 stop=3500 step=100 dur=80 pause=1000");
        Serial.println("CMD: TEST");
        Serial.println("CMD: RAW trigger f=3200 dur=100 post=1000 dump=bin");
        Serial.println("CMD: SEQ");
        Serial.println("CMD: SEQ help");
        Serial.println("CMD: SEQ stop");
        Serial.println("CMD: CAP");
        Serial.println("CMD: CAP stop");
        Serial.println("CMD: VAL");
        Serial.println("CMD: VAL OFF");
        return;
    }

    if (startsWithTokenIgnoreCase(line, "PARAM")) {
        char buffer[128];
        strncpy(buffer, line, sizeof(buffer));
        buffer[sizeof(buffer) - 1] = '\0';

        char* savePtr = nullptr;
        char* token = strtok_r(buffer, " ", &savePtr);
        DetectorParameters::Values params = DetectorParameters::capture(_audioOnsetDetector);
        FrequencyMatchEvaluation::Values freqTuning = _frequencyEvidenceTuning;

        while ((token = strtok_r(nullptr, " ", &savePtr)) != nullptr) {
            DetectorParameters::parseToken(token, params);
            FrequencyMatchEvaluation::parseToken(token, freqTuning);
        }

        DetectorParameters::apply(params, _audioOnsetDetector);
        _frequencyEvidenceTuning = freqTuning;

        Serial.print("PARAM onset=");
        Serial.print(detectorOnsetDetectionThreshold(), 1);
        Serial.print(" release=");
        Serial.print(detectorOnsetReleaseThreshold(), 1);
        Serial.print(" cooldown=");
        Serial.print(detectorCooldownAfterOnsetMs());
        Serial.print(" releaseDebounce=");
        Serial.print(detectorReleaseDebounceMs());
        Serial.print(" minMs=");
        Serial.print(detectorMinTransientDurationMs());
        Serial.print(" maxMs=");
        Serial.print(detectorMaxTransientDurationMs());
        Serial.print(" minStrength=");
        Serial.print(detectorMinTransientPeakStrength(), 1);
        Serial.print(" freqScore=");
        Serial.print(_frequencyEvidenceTuning.scoreMin, 0);
        Serial.print(" freqContrast=");
        Serial.println(_frequencyEvidenceTuning.contrastMin, 1);
        return;
    }

    if (startsWithTokenIgnoreCase(line, "BASE")) {
        if (_valMode) {
            return;
        }
        char buffer[96];
        strncpy(buffer, line, sizeof(buffer));
        buffer[sizeof(buffer) - 1] = '\0';

        char* savePtr = nullptr;
        char* token = strtok_r(buffer, " ", &savePtr);
        token = token != nullptr ? strtok_r(nullptr, " ", &savePtr) : nullptr;

        if (token != nullptr && equalsIgnoreCase(token, "stop")) {
            if (_baseSession.active) {
                printBaseSummary();
            }
            stopBaseSession();
            Serial.println("BASE stopped");
            return;
        }

        unsigned long durationMs = 10000;
        bool quiet = false;

        while (token != nullptr) {
            if (startsWithTokenIgnoreCase(token, "dur=")) {
                durationMs = strtoul(token + 4, nullptr, 10);
            } else if (equalsIgnoreCase(token, "quiet")) {
                quiet = true;
            }
            token = strtok_r(nullptr, " ", &savePtr);
        }

        startBaseSession(durationMs, quiet);
        return;
    }

    if (equalsIgnoreCase(line, "TEST")) {
        if (_valMode) {
            return;
        }
        sendEmitterCommand("MODE REMOTE");
        char chirpCommand[64];
        snprintf(chirpCommand, sizeof(chirpCommand), "CHIRP freq=%lu dur=%lu",
                 static_cast<unsigned long>(runtime::kDefaultChirpFrequencyHz),
                 runtime::kDefaultChirpDurationMs);
        sendEmitterCommand(chirpCommand);
        return;
    }

    if (startsWithTokenIgnoreCase(line, "RAW")) {
        if (_valMode) {
            return;
        }
        char buffer[96];
        strncpy(buffer, line, sizeof(buffer));
        buffer[sizeof(buffer) - 1] = '\0';

        char* savePtr = nullptr;
        char* token = strtok_r(buffer, " ", &savePtr);
        token = token != nullptr ? strtok_r(nullptr, " ", &savePtr) : nullptr;

        if (token == nullptr || !equalsIgnoreCase(token, "trigger")) {
            Serial.println("RAW_ERR usage=RAW trigger f=3200 dur=100 post=1000");
            return;
        }

        unsigned long toneHz = runtime::kDefaultChirpFrequencyHz;
        unsigned long durationMs = 100;
        unsigned long postMs = 1000;
        unsigned long preMs = 0;
        unsigned long decim = 1;
        bool dumpChunks = false;
        bool dumpBinary = false;

        while ((token = strtok_r(nullptr, " ", &savePtr)) != nullptr) {
            if (startsWithTokenIgnoreCase(token, "f=")) {
                toneHz = strtoul(token + 2, nullptr, 10);
            } else if (startsWithTokenIgnoreCase(token, "dur=")) {
                durationMs = strtoul(token + 4, nullptr, 10);
            } else if (startsWithTokenIgnoreCase(token, "post=")) {
                postMs = strtoul(token + 5, nullptr, 10);
            } else if (startsWithTokenIgnoreCase(token, "pre=")) {
                preMs = strtoul(token + 4, nullptr, 10);
            } else if (startsWithTokenIgnoreCase(token, "decim=")) {
                decim = strtoul(token + 6, nullptr, 10);
            } else if (startsWithTokenIgnoreCase(token, "dump=")) {
                const char* value = token + 5;
                dumpChunks = equalsIgnoreCase(value, "chunks");
                dumpBinary = equalsIgnoreCase(value, "bin") || equalsIgnoreCase(value, "binary");
            }
        }

        runRawTrigger(toneHz, durationMs, postMs, preMs, decim, dumpChunks, dumpBinary);
        return;
    }

    if (startsWithTokenIgnoreCase(line, "SEQ")) {
        if (_valMode) {
            return;
        }
        char buffer[96];
        strncpy(buffer, line, sizeof(buffer));
        buffer[sizeof(buffer) - 1] = '\0';

        char* savePtr = nullptr;
        char* token = strtok_r(buffer, " ", &savePtr);
        token = token != nullptr ? strtok_r(nullptr, " ", &savePtr) : nullptr;

        bool externalEmitter = false;
        if (token != nullptr && (equalsIgnoreCase(token, "obs") || equalsIgnoreCase(token, "observe") || equalsIgnoreCase(token, "passive"))) {
            externalEmitter = true;
            token = strtok_r(nullptr, " ", &savePtr);
        }

        if (token != nullptr && (equalsIgnoreCase(token, "help") || equalsIgnoreCase(token, "?"))) {
            printSequenceHelp();
            return;
        }

        if (token != nullptr && equalsIgnoreCase(token, "stop")) {
            if (_sequenceTest.active) {
                printSequenceFinalOutput();
            }
            stopSequenceTest();
            Serial.println("SEQ stopped");
            return;
        }

        unsigned long totalTrials = 100;
        unsigned long periodMs = externalEmitter ? 2000 : 2500;
        unsigned long windowEndOffsetMs = externalEmitter ? 600 : 2200;
        unsigned long toneHz = runtime::kDefaultChirpFrequencyHz;
        unsigned long durationMs = 100;
        uint32_t logFlags = AnalyzerApp::DEFAULT_ANALYZER_LOG_FLAGS;
        bool customLogRequested = false;
        const char* setupLabel = nullptr;
        bool sampleDumpEnabled = false;
        unsigned long sampleDumpFirstTrials = 2;
        unsigned long sampleDumpEveryNth = 0;
        unsigned long sampleDumpLeadMs = 50;
        unsigned long sampleDumpTailMs = 800;
        unsigned long sampleDumpStepMs = 1;
        unsigned long sampleDumpMaxRows = 5000;
        detection::DetectionProfileKind profileKind = detection::DetectionProfileKind::FreqAmp;
        while (token != nullptr) {
            if (equalsIgnoreCase(token, "start")) {
                // Optional human-friendly token; no-op.
            } else if (startsWithTokenIgnoreCase(token, "tries=")) {
                totalTrials = strtoul(token + 6, nullptr, 10);
            } else if (startsWithTokenIgnoreCase(token, "period=")) {
                periodMs = strtoul(token + 7, nullptr, 10);
            } else if (startsWithTokenIgnoreCase(token, "window=")) {
                windowEndOffsetMs = strtoul(token + 7, nullptr, 10);
            } else if (startsWithTokenIgnoreCase(token, "freq=")) {
                toneHz = strtoul(token + 5, nullptr, 10);
            } else if (startsWithTokenIgnoreCase(token, "dur=")) {
                durationMs = strtoul(token + 4, nullptr, 10);
            } else if (startsWithTokenIgnoreCase(token, "test=")) {
                setupLabel = token + 5;
            } else if (startsWithTokenIgnoreCase(token, "dumpSamples=")) {
                sampleDumpEnabled = strtoul(token + 12, nullptr, 10) != 0;
            } else if (startsWithTokenIgnoreCase(token, "curveFormat=")) {
                const char* value = token + 12;
                if (equalsIgnoreCase(value, "samples")) {
                    sampleDumpEnabled = true;
                } else if (equalsIgnoreCase(value, "off") || equalsIgnoreCase(value, "none")) {
                    sampleDumpEnabled = false;
                }
            } else if (startsWithTokenIgnoreCase(token, "sampleFirst=")) {
                sampleDumpFirstTrials = strtoul(token + 12, nullptr, 10);
            } else if (startsWithTokenIgnoreCase(token, "sampleEvery=")) {
                sampleDumpEveryNth = strtoul(token + 12, nullptr, 10);
            } else if (startsWithTokenIgnoreCase(token, "sampleLead=")) {
                sampleDumpLeadMs = strtoul(token + 11, nullptr, 10);
            } else if (startsWithTokenIgnoreCase(token, "sampleTail=")) {
                sampleDumpTailMs = strtoul(token + 11, nullptr, 10);
            } else if (startsWithTokenIgnoreCase(token, "sampleStep=")) {
                sampleDumpStepMs = strtoul(token + 11, nullptr, 10);
            } else if (startsWithTokenIgnoreCase(token, "sampleMax=")) {
                sampleDumpMaxRows = strtoul(token + 10, nullptr, 10);
            } else if (startsWithTokenIgnoreCase(token, "profile=")) {
                const char* value = token + 8;
                if (!detection::detectionProfileKindFromName(value, profileKind)) {
                    Serial.print("SEQ_VERBOSE_WARN reason=unknown_profile value=");
                    Serial.println(value);
                }
            } else if (startsWithTokenIgnoreCase(token, "mode=")) {
                const char* value = token + 5;
                if (!detection::detectionProfileKindFromName(value, profileKind)) {
                    // profile selection is now explicit for Analyzer runs.
                }
            } else if (equalsIgnoreCase(token, "log")) {
                token = strtok_r(nullptr, " ", &savePtr);
                if (token != nullptr) {
                    logFlags = analyzerLogFlagsFromToken(token);
                    token = strtok_r(nullptr, " ", &savePtr);
                    continue;
                }
            } else if (startsWithTokenIgnoreCase(token, "debug=")) {
                logFlags = analyzerLogFlagsFromLevel(strtoul(token + 6, nullptr, 10));
            } else if (startsWithTokenIgnoreCase(token, "log=")) {
                logFlags = analyzerLogFlagsFromToken(token + 4);
                customLogRequested = analyzerLogEnabled(logFlags, AnalyzerApp::ANALYZER_LOG_CUSTOM);
            }
            token = strtok_r(nullptr, " ", &savePtr);
        }

        if (customLogRequested) {
            logFlags = AnalyzerApp::ANALYZER_LOG_CUSTOM;
        }

        PendingSequenceStart& pending = _pendingSequenceStart;
        pending.active = true;
        pending.totalTrials = totalTrials;
        pending.periodMs = periodMs;
        pending.windowEndOffsetMs = windowEndOffsetMs;
        pending.toneHz = toneHz;
        pending.durationMs = durationMs;
        pending.quiet = false;
        pending.showDetails = true;
        pending.logFlags = logFlags;
        pending.sampleDumpEnabled = sampleDumpEnabled;
        pending.sampleDumpFirstTrials = sampleDumpFirstTrials;
        pending.sampleDumpEveryNth = sampleDumpEveryNth;
        pending.sampleDumpLeadMs = sampleDumpLeadMs;
        pending.sampleDumpTailMs = sampleDumpTailMs;
        pending.sampleDumpStepMs = sampleDumpStepMs;
        pending.sampleDumpMaxRows = sampleDumpMaxRows;
        pending.profileKind = profileKind;
        pending.externalEmitter = externalEmitter;
        if (setupLabel != nullptr && setupLabel[0] != '\0') {
            strncpy(pending.setupLabelStorage, setupLabel, sizeof(pending.setupLabelStorage));
            pending.setupLabelStorage[sizeof(pending.setupLabelStorage) - 1] = '\0';
            pending.setupLabel = pending.setupLabelStorage;
        } else {
            pending.setupLabelStorage[0] = '\0';
            pending.setupLabel = nullptr;
        }
        return;
    }

    if (startsWithTokenIgnoreCase(line, "CAP")) {
        if (_valMode) {
            return;
        }
        char buffer[96];
        strncpy(buffer, line, sizeof(buffer));
        buffer[sizeof(buffer) - 1] = '\0';

        char* savePtr = nullptr;
        char* token = strtok_r(buffer, " ", &savePtr);
        token = token != nullptr ? strtok_r(nullptr, " ", &savePtr) : nullptr;

        if (token != nullptr && equalsIgnoreCase(token, "stop")) {
            if (_captureSession.active) {
                printCaptureSummary();
            }
            stopCaptureSession();
            Serial.println("CAP stopped");
            return;
        }

        unsigned long totalTrials = 20;
        unsigned long periodMs = 2500;
        unsigned long windowEndOffsetMs = 500;
        unsigned long toneHz = runtime::kDefaultChirpFrequencyHz;
        unsigned long durationMs = 100;

        while (token != nullptr) {
            if (startsWithTokenIgnoreCase(token, "tries=")) {
                totalTrials = strtoul(token + 6, nullptr, 10);
            } else if (startsWithTokenIgnoreCase(token, "period=")) {
                periodMs = strtoul(token + 7, nullptr, 10);
            } else if (startsWithTokenIgnoreCase(token, "window=")) {
                windowEndOffsetMs = strtoul(token + 7, nullptr, 10);
            } else if (startsWithTokenIgnoreCase(token, "freq=")) {
                toneHz = strtoul(token + 5, nullptr, 10);
            } else if (startsWithTokenIgnoreCase(token, "dur=")) {
                durationMs = strtoul(token + 4, nullptr, 10);
            }
            token = strtok_r(nullptr, " ", &savePtr);
        }

        startCaptureSession(totalTrials, periodMs, windowEndOffsetMs, toneHz, durationMs);
        return;
    }

    if (equalsIgnoreCase(line, "VAL") || equalsIgnoreCase(line, "VAL ON") || equalsIgnoreCase(line, "VERBOSE ON")) {
        _valMode = true;
        return;
    }

    if (equalsIgnoreCase(line, "VAL OFF") || equalsIgnoreCase(line, "VERBOSE OFF")) {
        _valMode = false;
        return;
    }

    if (startsWithTokenIgnoreCase(line, "EMIT ")) {
        if (_valMode) {
            return;
        }
        sendEmitterCommand(line + 5);
        return;
    }

    if (_valMode) {
        return;
    }

    Serial.print("EVT analyzer_unknown_cmd line=");
    Serial.println(line);
}

void AnalyzerApp::sendEmitterCommand(const char* command) {
    Serial2.println(command);
}

// -----------------------------------------------------------------------------
// Raw trigger and value-mode helpers
// -----------------------------------------------------------------------------

void AnalyzerApp::runRawTrigger(unsigned long toneHz, unsigned long durationMs, unsigned long postMs, unsigned long preMs, unsigned long decim, bool dumpChunks, bool dumpBinary) {
    if (_valMode) {
        return;
    }

    stopSequenceTest();
    stopCaptureSession();
    resetDetectorState();
    _audioSource.resetStats();

    if (toneHz == 0) {
        toneHz = 1;
    }
    if (durationMs == 0) {
        durationMs = 1;
    }
    if (postMs == 0) {
        postMs = 1;
    }
    if (decim == 0) {
        decim = 1;
    }
    if (dumpChunks) {
        Serial.println("RAW_INFO dump=chunks");
    }

    const uint32_t sampleRateHz = _audioSource.sampleRateHz() > 0 ? _audioSource.sampleRateHz() : 16000UL;
    const unsigned long maxSamples = 16000UL;
    unsigned long preWantedSamples = static_cast<unsigned long>((static_cast<uint64_t>(preMs) * static_cast<uint64_t>(sampleRateHz)) / 1000ULL);
    unsigned long postWantedSamples = static_cast<unsigned long>((static_cast<uint64_t>(postMs) * static_cast<uint64_t>(sampleRateHz)) / 1000ULL);
    if (preWantedSamples > maxSamples) {
        preWantedSamples = maxSamples;
    }
    if (postWantedSamples > maxSamples) {
        postWantedSamples = maxSamples;
    }
    if (preWantedSamples + postWantedSamples > maxSamples) {
        if (preWantedSamples >= maxSamples) {
            preWantedSamples = maxSamples;
            postWantedSamples = 0;
        } else {
            postWantedSamples = maxSamples - preWantedSamples;
        }
    }
    if (preWantedSamples == 0 && postWantedSamples == 0) {
        postWantedSamples = 1;
    }
    const unsigned long captureSamples = preWantedSamples + postWantedSamples;
    int16_t* preRingBuffer = nullptr;
    if (preWantedSamples > 0) {
        preRingBuffer = static_cast<int16_t*>(malloc(static_cast<size_t>(preWantedSamples) * sizeof(int16_t)));
        if (preRingBuffer == nullptr) {
            Serial.println("RAW_ERR memory=pre_ring_alloc_failed");
            return;
        }
    }

    static int16_t rawBuffer[16000];
    unsigned long flushedSamples = 0;
    int discardedSample = 0;
    uint32_t discardedSampleTimeUs = 0;
    while (flushedSamples < kRawCaptureFlushSamples && _audioSource.readRawSample(discardedSample, discardedSampleTimeUs)) {
        ++flushedSamples;
    }

    const unsigned long captureId = ++_rawCaptureSequenceId;
    unsigned long preCaptured = 0;
    unsigned long preWriteIndex = 0;
    const unsigned long preDeadlineMs = millis() + preMs + kRawCaptureTimeoutSlackMs;
    while (preCaptured < preWantedSamples && millis() <= preDeadlineMs) {
        int rawSample = 0;
        uint32_t sampleTimeUs = 0;
        if (_audioSource.readRawSample(rawSample, sampleTimeUs)) {
            preRingBuffer[preWriteIndex] = rawCaptureSampleToInt16(rawSample);
            preWriteIndex = (preWriteIndex + 1UL) % (preWantedSamples > 0 ? preWantedSamples : 1UL);
            ++preCaptured;
        } else {
            delay(1);
        }
    }
    if (preCaptured > 0) {
        const unsigned long preStartIndex = preCaptured == preWantedSamples ? preWriteIndex : 0UL;
        for (unsigned long i = 0; i < preCaptured; ++i) {
            const unsigned long ringIndex = (preStartIndex + i) % (preCaptured > 0 ? preCaptured : 1UL);
            rawBuffer[i] = preRingBuffer[ringIndex];
        }
    }

    const unsigned long triggerMs = millis();
    char emitterCommand[96];
    snprintf(emitterCommand, sizeof(emitterCommand), "CHIRP freq=%lu dur=%lu", toneHz, durationMs);
    sendEmitterCommand(emitterCommand);
    Serial2.flush();

    unsigned long postCaptured = 0;
    const unsigned long postDeadlineMs = triggerMs + postMs + kRawCaptureTimeoutSlackMs;
    while (postCaptured < postWantedSamples && millis() <= postDeadlineMs) {
        int rawSample = 0;
        uint32_t sampleTimeUs = 0;
        if (_audioSource.readRawSample(rawSample, sampleTimeUs)) {
            rawBuffer[preCaptured + postCaptured] = rawCaptureSampleToInt16(rawSample);
            ++postCaptured;
        } else {
            delay(1);
        }
    }

    const unsigned long capturedSamples = preCaptured + postCaptured;
    const unsigned long droppedSamples = captureSamples > capturedSamples ? (captureSamples - capturedSamples) : 0;

    float env = 0.0f;
    float maxEnv = 0.0f;
    int maxRaw = 0;
    int maxAbs = 0;
    for (unsigned long i = 0; i < capturedSamples; ++i) {
        const int rawSample = static_cast<int>(rawBuffer[i]);
        const int absSample = rawSample < 0 ? -rawSample : rawSample;
        env = env * 0.95f + static_cast<float>(absSample) * 0.05f;
        if (absSample > maxRaw) {
            maxRaw = absSample;
        }
        if (absSample > maxAbs) {
            maxAbs = absSample;
        }
        if (env > maxEnv) {
            maxEnv = env;
        }
    }

    Serial.print("RAW_BEGIN id=");
    Serial.print(captureId);
    Serial.print(" sr=");
    Serial.print(sampleRateHz);
    Serial.print(" trigger_ms=");
    Serial.print(triggerMs);
    Serial.print(" f=");
    Serial.print(toneHz);
    Serial.print(" dur=");
    Serial.print(durationMs);
    Serial.print(" pre_ms=");
    Serial.print(preMs);
    Serial.print(" post_ms=");
    Serial.print(postMs);
    Serial.print(" decim=");
    Serial.print(decim);
    Serial.print(" pre_samples=");
    Serial.print(preCaptured);
    Serial.print(" post_samples=");
    Serial.print(postCaptured);
    if (dumpBinary) {
        Serial.print(" fields=raw16");
        Serial.print(" dump=bin");
        Serial.print(" samples=");
        Serial.print(capturedSamples);
        Serial.print(" bytes=");
        Serial.print(capturedSamples * sizeof(int16_t));
    } else if (dumpChunks) {
        Serial.print(" fields=min,max,rms,mean_abs");
        Serial.print(" dump=chunks");
        Serial.print(" chunk_samples=");
        Serial.print(rawCaptureChunkSize(sampleRateHz, decim));
    } else {
        Serial.print(" fields=i,raw,abs,env");
        Serial.print(" dump=full");
    }
    Serial.println();

    if (dumpBinary) {
        Serial.write(reinterpret_cast<const uint8_t*>(rawBuffer), capturedSamples * sizeof(int16_t));
        Serial.println();
    } else if (dumpChunks) {
        const unsigned long emittedSamples = (capturedSamples + decim - 1UL) / decim;
        const unsigned long chunkSamples = rawCaptureChunkSize(sampleRateHz, decim);
        for (unsigned long emittedStart = 0; emittedStart < emittedSamples; emittedStart += chunkSamples) {
            const unsigned long emittedEnd = emittedStart + chunkSamples < emittedSamples ? emittedStart + chunkSamples : emittedSamples;
            long chunkMin = 0;
            long chunkMax = 0;
            uint64_t sumAbs = 0;
            uint64_t sumSquares = 0;
            bool first = true;
            for (unsigned long emittedIndex = emittedStart; emittedIndex < emittedEnd; ++emittedIndex) {
                const unsigned long rawIndex = emittedIndex * decim;
                if (rawIndex >= capturedSamples) {
                    break;
                }
                const int sample = static_cast<int>(rawBuffer[rawIndex]);
                const long absSample = sample < 0 ? -static_cast<long>(sample) : static_cast<long>(sample);
                if (first) {
                    chunkMin = sample;
                    chunkMax = sample;
                    first = false;
                } else {
                    if (sample < chunkMin) {
                        chunkMin = sample;
                    }
                    if (sample > chunkMax) {
                        chunkMax = sample;
                    }
                }
                const uint64_t abs64 = static_cast<uint64_t>(absSample);
                sumAbs += abs64;
                sumSquares += abs64 * abs64;
            }
            const unsigned long chunkCount = emittedEnd > emittedStart ? emittedEnd - emittedStart : 0UL;
            if (chunkCount == 0) {
                continue;
            }
            const double meanAbs = static_cast<double>(sumAbs) / static_cast<double>(chunkCount);
            const double rms = sqrt(static_cast<double>(sumSquares) / static_cast<double>(chunkCount));
            const long i0 = static_cast<long>(emittedStart * decim) - static_cast<long>(preCaptured);
            const unsigned long lastRawIndex = (emittedEnd - 1UL) * decim;
            const long i1 = static_cast<long>(lastRawIndex < capturedSamples ? lastRawIndex : (capturedSamples - 1UL)) - static_cast<long>(preCaptured);
            Serial.print("RAW_CHUNK i0=");
            Serial.print(i0);
            Serial.print(" i1=");
            Serial.print(i1);
            Serial.print(" min=");
            Serial.print(chunkMin);
            Serial.print(" max=");
            Serial.print(chunkMax);
            Serial.print(" rms=");
            Serial.print(rms, 1);
            Serial.print(" mean_abs=");
            Serial.println(meanAbs, 1);
        }
    } else {
        env = 0.0f;
        for (long displayIndex = -static_cast<long>(preCaptured); displayIndex < static_cast<long>(postCaptured); displayIndex += static_cast<long>(decim)) {
            const unsigned long rawIndex = static_cast<unsigned long>(displayIndex + static_cast<long>(preCaptured));
            const int rawSample = static_cast<int>(rawBuffer[rawIndex]);
            const int absSample = rawSample < 0 ? -rawSample : rawSample;
            env = env * 0.95f + static_cast<float>(absSample) * 0.05f;
            Serial.print(displayIndex);
            Serial.print(',');
            Serial.print(rawSample);
            Serial.print(',');
            Serial.print(absSample);
            Serial.print(',');
            Serial.println(env, 2);
        }
    }

    Serial.print("RAW_END id=");
    Serial.print(captureId);
    Serial.print(" samples=");
    Serial.print(capturedSamples);
    Serial.print(" dropped=");
    Serial.print(droppedSamples);
    Serial.print(" elapsed_ms=");
    Serial.print(millis() - triggerMs);
    Serial.print(" max_raw=");
    Serial.print(maxRaw);
    Serial.print(" max_abs=");
    Serial.print(maxAbs);
    Serial.print(" max_env=");
    Serial.println(maxEnv, 2);

    if (preRingBuffer != nullptr) {
        free(preRingBuffer);
    }
}

void AnalyzerApp::printValueModeBanner() const {
    if (_valMode) {
        return;
    }
    Serial.print("EVT analyzer_val on source=");
    Serial.print("I2S");
    Serial.println(" detector=AMP");
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

void AnalyzerApp::startSequenceTest(unsigned long totalTrials, unsigned long periodMs, unsigned long windowEndOffsetMs, unsigned long toneHz, unsigned long durationMs, bool quiet, bool showDetails, const char* setupLabel, uint32_t logFlags, bool sampleDumpEnabled, unsigned long sampleDumpFirstTrials, unsigned long sampleDumpEveryNth, unsigned long sampleDumpLeadMs, unsigned long sampleDumpTailMs, unsigned long sampleDumpStepMs, unsigned long sampleDumpMaxRows, detection::DetectionProfileKind profileKind, bool externalEmitter) {
    if (_valMode) {
        return;
    }
    if (totalTrials == 0) {
        totalTrials = 1;
    }
    if (periodMs == 0) {
        periodMs = 1;
    }
    if (windowEndOffsetMs < 250) {
        windowEndOffsetMs = 250;
    }
    if (windowEndOffsetMs >= periodMs) {
        windowEndOffsetMs = periodMs > 250 ? periodMs - 250 : periodMs;
    }
    if (externalEmitter && windowEndOffsetMs < durationMs + 500) {
        windowEndOffsetMs = durationMs + 500;
    }
    if (sampleDumpStepMs == 0) {
        sampleDumpStepMs = 1;
    }
    if (sampleDumpTailMs < sampleDumpLeadMs) {
        sampleDumpTailMs = sampleDumpLeadMs;
    }
    free(_sequenceTest.trialReports);
    _sequenceTest.trialReports = nullptr;
    _sequenceTest.trialReportCapacity = 0;
    _sequenceTest.trialReportCount = 0;

    _sequenceTest.active = true;
    _sequenceTest.quiet = quiet;
    _sequenceTest.showDetails = showDetails;
    _sequenceTest.externalEmitter = externalEmitter;
    _sequenceTest.profileKind = profileKind;
    _sequenceTest.progressLineStarted = false;
    _sequenceTest.logFlags = logFlags;
    _sequenceTest.totalTrials = totalTrials;
    _sequenceTest.periodMs = periodMs;
    _sequenceTest.windowStartOffsetMs = 0;
    _sequenceTest.windowEndOffsetMs = windowEndOffsetMs;
    _sequenceTest.toneHz = toneHz;
    _sequenceTest.durationMs = durationMs;
    _sequenceTest.sampleDumpEnabled = sampleDumpEnabled;
    _sequenceTest.sampleDumpFirstTrials = sampleDumpFirstTrials;
    _sequenceTest.sampleDumpEveryNth = sampleDumpEveryNth;
    _sequenceTest.sampleDumpLeadMs = sampleDumpLeadMs;
    _sequenceTest.sampleDumpTailMs = sampleDumpTailMs;
    _sequenceTest.sampleDumpStepMs = sampleDumpStepMs;
    _sequenceTest.sampleDumpMaxRows = sampleDumpMaxRows == 0 ? 1 : sampleDumpMaxRows;
    _sequenceTest.sampleDumpWarned = false;
    clearSequenceSampleDump();

    if (_detection == nullptr) {
        _detection = new detection::DetectionRuntime();
    }
    _detection->reset();
    _detection->setFrequencyTuning(_frequencyEvidenceTuning);
    const detection::DetectionProfile& selectedProfile = detection::detectionProfileForKind(_sequenceTest.profileKind);
    _detection->setSignalEmitter(selectedProfile.signalEmitter);
    _detection->setInspectionRules(selectedProfile.inspectionRules);
    _detection->setInspectionConfig(selectedProfile.inspectionConfig);
    _detection->setPatternRulesConfig(selectedProfile.patternRulesConfig);
    _detection->setFieldStateConfig(selectedProfile.fieldStateConfig);
    _detection->setProfileName(detection::detectionProfileName(selectedProfile.kind));
    _freqBandStream.setSampleRateHz(_audioSource.sampleRateHz());
    _freqBandStream.setTargetFrequencyHz(toneHz);
    _freqBandStream.resetState();

    const bool wantSummaryReports =
        analyzerLogEnabled(logFlags, AnalyzerApp::ANALYZER_LOG_SUMMARY);
    if (wantSummaryReports) {
        const size_t desiredCapacity = static_cast<size_t>(totalTrials < SequenceTest::kMaxTrialReports ? totalTrials : SequenceTest::kMaxTrialReports);
        if (desiredCapacity > 0) {
            _sequenceTest.trialReports = static_cast<AnalyzerReport*>(calloc(desiredCapacity, sizeof(AnalyzerReport)));
            if (_sequenceTest.trialReports != nullptr) {
                _sequenceTest.trialReportCapacity = desiredCapacity;
            } else {
                Serial.print("SEQ_VERBOSE_WARN reason=analyzer_report_alloc_failed requested=");
                Serial.print(desiredCapacity);
                Serial.println(" reports");
            }
        }
    }
    if (setupLabel != nullptr && setupLabel[0] != '\0') {
        strncpy(_sequenceTest.setupLabel, setupLabel, sizeof(_sequenceTest.setupLabel));
        _sequenceTest.setupLabel[sizeof(_sequenceTest.setupLabel) - 1] = '\0';
    } else {
        strncpy(_sequenceTest.setupLabel, TEST_SETUP_LABEL, sizeof(_sequenceTest.setupLabel));
        _sequenceTest.setupLabel[sizeof(_sequenceTest.setupLabel) - 1] = '\0';
    }

    if (_sequenceTest.sampleDumpEnabled) {
        const unsigned long selectedTrialsEstimate = countSelectedSampleDumpTrials(totalTrials, sampleDumpFirstTrials, sampleDumpEveryNth);
        const unsigned long rowsPerTrial = ((sampleDumpLeadMs + sampleDumpTailMs) / sampleDumpStepMs) + 1UL;
        const unsigned long requestedRows = selectedTrialsEstimate * rowsPerTrial;
        const unsigned long maxAllowedRows = _sequenceTest.sampleDumpMaxRows < SequenceTest::kMaxSampleRows
            ? _sequenceTest.sampleDumpMaxRows
            : SequenceTest::kMaxSampleRows;
        if (requestedRows > _sequenceTest.sampleDumpMaxRows || rowsPerTrial > SequenceTest::kMaxSampleRows) {
            Serial.print("SAMPLES_WARN reason=too_many_samples requested=");
            Serial.print(requestedRows);
            Serial.print(" max_allowed=");
            Serial.println(maxAllowedRows);
            _sequenceTest.sampleDumpEnabled = false;
        }
    }
    _sequenceTest.startedAtMs = millis();
    _sequenceTest.nextTriggerAtMs = _sequenceTest.startedAtMs + kSequenceWarmupMs;
    _sequenceTest.currentTrial = 0;
    _sequenceTest.currentTrialScheduledAtMs = 0;
    _sequenceTest.currentTrialStartMs = 0;
    _sequenceTest.currentTrialEndMs = 0;
    _sequenceTest.currentTrialOnsetDetectedMs = 0;
    _sequenceTest.currentTrialTransientDetectedMs = 0;
    _sequenceTest.currentTrialHit = false;
    _sequenceTest.currentTrialFinalized = false;
    _sequenceTest.currentTrialUnexpected = 0;
    _sequenceTest.currentTrialDiagnostics = {};
    _sequenceTest.hits = 0;
    _sequenceTest.expectedHits = 0;
    _sequenceTest.lateHits = 0;
    _sequenceTest.misses = 0;
    _sequenceTest.unexpected = 0;
    _sequenceTest.duplicates = 0;
    _sequenceTest.invalidAudio = 0;
    _sequenceTest.samplesProcessed = 0;
    _sequenceTest.maxSamplesPerLoop = 0;
    _sequenceTest.emptySourceLoops = 0;
    _sequenceTest.totalHitStrengthScaled = 0;
    _sequenceTest.totalHitDurationMs = 0;
    _sequenceTest.patternMatchedExpected = 0;
    _sequenceTest.patternUnmatchedExpected = 0;
    _sequenceTest.patternMatchedDuplicates = 0;
    _sequenceTest.patternUnmatchedDuplicates = 0;
    _sequenceTest.patternMatchedUnexpected = 0;
    _sequenceTest.patternUnmatchedUnexpected = 0;
    _sequenceTest.freqRejectScore = 0;
    _sequenceTest.freqRejectContrast = 0;
    _sequenceTest.freqRejectBoth = 0;
    _sequenceTest.freqRejectNoEvidence = 0;
    _sequenceTest.freqRejectInvalidWindow = 0;

    if (!_sequenceTest.externalEmitter) {
        // Rebase before the first trial so every run starts from the quiet floor.
        const unsigned long sequenceClaimSendMs = millis();
        sendEmitterCommand("MODE REMOTE");
        const bool sequenceClaimAcked = waitForEmitterAck("OK MODE REMOTE", 1500);
        const unsigned long sequenceClaimAckMs = millis();
        if (_sequenceTest.showDetails && !_sequenceTest.quiet) {
            Serial.print("SEQ remote claim: send=");
            Serial.print(sequenceClaimSendMs);
            Serial.print("ms ack=");
            Serial.print(sequenceClaimAckMs);
            Serial.print("ms wait=");
            Serial.print(sequenceClaimAckMs - sequenceClaimSendMs);
            Serial.print("ms status=");
            Serial.println(sequenceClaimAcked ? "ok" : "timeout");
        }
    }

    const unsigned long sequenceRebaseStartMs = millis();
    delay(100);
    _audioSignal.rebase();
    if (_sequenceTest.showDetails && !_sequenceTest.quiet) {
        Serial.print("SEQ rebase: start=");
        Serial.print(sequenceRebaseStartMs);
        Serial.print("ms end=");
        Serial.print(millis());
        Serial.print("ms elapsed=");
        Serial.print(millis() - sequenceRebaseStartMs);
        Serial.println("ms");
    }
    resetDetectorState();
    _audioSignal.resetStats();
    _audioSource.resetStats();
    Serial.println("AUDIO stats reset");

    Serial.print("SEQ start test=");
    Serial.print(_sequenceTest.setupLabel);
    Serial.print(" warmup_ms=");
    Serial.print(kSequenceWarmupMs);
    Serial.print(" loopDelayMs=");
    Serial.print(TEST_LOOP_DELAY_MS);
    Serial.print(" logStress=");
    Serial.println(TEST_LOG_STRESS ? 1 : 0);

    if (_sequenceTest.showDetails) {
        Serial.print("SEQ start source=");
        Serial.print("I2S");
        Serial.print(" detector=AMP");
        Serial.print(" profile=");
        Serial.print(detection::detectionProfileName(_sequenceTest.profileKind));
        Serial.print(" mode=");
        Serial.print(_sequenceTest.externalEmitter ? "OBS" : "SEQ");
        Serial.print(" test=");
        Serial.print(_sequenceTest.setupLabel);
        Serial.print(" warmup_ms=");
        Serial.print(kSequenceWarmupMs);
        Serial.print(" loopDelayMs=");
        Serial.print(TEST_LOOP_DELAY_MS);
        Serial.print(" logStress=");
        Serial.print(TEST_LOG_STRESS ? 1 : 0);
        Serial.print(" quiet=");
        Serial.print(_sequenceTest.quiet ? 1 : 0);
        Serial.print(" tries=");
        Serial.print(totalTrials);
        Serial.print(" period_ms=");
        Serial.print(periodMs);
        Serial.print(" window_start_ms=");
        Serial.print(_sequenceTest.windowStartOffsetMs);
        Serial.print(" window_end_ms=");
        Serial.print(windowEndOffsetMs);
        Serial.print(" freq_hz=");
        Serial.print(toneHz);
        Serial.print(" dur_ms=");
        Serial.println(durationMs);
        printDetectionParameters();
        if (!_sequenceTest.quiet) {
            Serial.println(_sequenceTest.externalEmitter ? "OBS running" : "SEQ running");
        }
    }
}

void AnalyzerApp::startCaptureSession(unsigned long totalTrials, unsigned long periodMs, unsigned long windowEndOffsetMs, unsigned long toneHz, unsigned long durationMs, bool quiet) {
    if (totalTrials == 0) {
        totalTrials = 1;
    }
    if (periodMs == 0) {
        periodMs = 1;
    }
    if (windowEndOffsetMs == 0) {
        windowEndOffsetMs = 1;
    }
    if (windowEndOffsetMs >= periodMs) {
        windowEndOffsetMs = periodMs > 1 ? periodMs - 1 : periodMs;
    }

    stopSequenceTest();

    _captureSession.active = true;
    _captureSession.quiet = quiet;
    _captureSession.totalTrials = totalTrials;
    _captureSession.periodMs = periodMs;
    _captureSession.windowStartOffsetMs = 0;
    _captureSession.windowEndOffsetMs = windowEndOffsetMs;
    _captureSession.toneHz = toneHz;
    _captureSession.durationMs = durationMs;
    _captureSession.startedAtMs = millis();
    _captureSession.nextTriggerAtMs = _captureSession.startedAtMs;
    _captureSession.currentTrial = 0;
    _captureSession.currentTrialStartMs = 0;
    _captureSession.currentTrialEndMs = 0;
    _captureSession.currentTrialFinalized = false;
    _captureSession.currentRawMin = 0;
    _captureSession.currentRawMax = 0;
    _captureSession.currentDeltaMin = 0.0f;
    _captureSession.currentDeltaMax = 0.0f;
    _captureSession.quietRawMin = 0;
    _captureSession.quietRawMax = 0;
    _captureSession.quietRawSum = 0;
    _captureSession.quietRawSamples = 0;
    _captureSession.quietDeltaMin = 0.0f;
    _captureSession.quietDeltaMax = 0.0f;
    _captureSession.quietDeltaSum = 0.0f;
    _captureSession.quietDeltaSamples = 0;
    _captureSession.completed = 0;
    _captureSession.totalRawSwing = 0;
    _captureSession.totalDeltaSwing = 0.0f;
    _captureSession.bestRawSwing = 0;
    _captureSession.bestDeltaSwing = 0.0f;
    _captureSession.lastStatusPrintMs = _captureSession.startedAtMs;
    _freqBandStream.setSampleRateHz(_audioSource.sampleRateHz());
    _freqBandStream.setTargetFrequencyHz(toneHz);
    _freqBandStream.resetState();

    // Capture uses the same emitter hand-off and rebase step as sequence mode.
    const unsigned long captureClaimSendMs = millis();
    sendEmitterCommand("MODE REMOTE");
    const bool captureClaimAcked = waitForEmitterAck("OK MODE REMOTE", 1500);
    const unsigned long captureClaimAckMs = millis();
    if (!_captureSession.quiet) {
        Serial.print("CAP remote claim: send=");
        Serial.print(captureClaimSendMs);
        Serial.print("ms ack=");
        Serial.print(captureClaimAckMs);
        Serial.print("ms wait=");
        Serial.print(captureClaimAckMs - captureClaimSendMs);
        Serial.print("ms status=");
        Serial.println(captureClaimAcked ? "ok" : "timeout");
    }

    const unsigned long captureRebaseStartMs = millis();
    delay(100);
    _audioSignal.rebase();
    if (!_captureSession.quiet) {
        Serial.print("CAP rebase: start=");
        Serial.print(captureRebaseStartMs);
        Serial.print("ms end=");
        Serial.print(millis());
        Serial.print("ms elapsed=");
        Serial.print(millis() - captureRebaseStartMs);
        Serial.println("ms");
    }
    resetDetectorState();
    _audioSignal.resetStats();
    _audioSource.resetStats();
    Serial.println("AUDIO stats reset");

    Serial.print("CAP start tries=");
    Serial.print(totalTrials);
    Serial.print(" period_ms=");
    Serial.print(periodMs);
    Serial.print(" window_ms=");
    Serial.print(windowEndOffsetMs);
    Serial.print(" freq_hz=");
    Serial.print(toneHz);
    Serial.print(" dur_ms=");
    Serial.println(durationMs);
    if (!_captureSession.quiet) {
        Serial.println("CAP running");
    }
}

void AnalyzerApp::stopCaptureSession() {
    _captureSession.active = false;
}

void AnalyzerApp::updateCaptureSession(unsigned long now) {
    if (_valMode) {
        return;
    }
    if (!_captureSession.active) {
        return;
    }

    const bool inTrialWindow = _captureSession.currentTrial > 0
                               && !_captureSession.currentTrialFinalized
                               && now >= _captureSession.currentTrialStartMs
                               && now <= _captureSession.currentTrialEndMs;
    if (inTrialWindow) {
        updateCaptureTrial(now);
    }

    if (_captureSession.currentTrial > 0 && now >= _captureSession.currentTrialEndMs && !_captureSession.currentTrialFinalized) {
        finalizeCaptureTrial(now);
    }

    if (!inTrialWindow) {
        updateCaptureQuietStats(now);
    }

    if (!_captureSession.active) {
        return;
    }

    if (now < _captureSession.nextTriggerAtMs) {
        return;
    }

    if (_captureSession.currentTrial >= _captureSession.totalTrials) {
        return;
    }

    const unsigned long trialNumber = _captureSession.currentTrial + 1;
    const unsigned long scheduledAtMs = _captureSession.nextTriggerAtMs;
    _captureSession.currentTrial = trialNumber;
    _captureSession.currentTrialStartMs = scheduledAtMs;
    _captureSession.currentTrialEndMs = scheduledAtMs + _captureSession.windowEndOffsetMs;
    _captureSession.currentTrialFinalized = false;
    _captureSession.nextTriggerAtMs = scheduledAtMs + _captureSession.periodMs;

    const int raw = _audioSignal.rawSignal();
    const float delta = static_cast<float>(_audioSignal.centeredSignal());
    _captureSession.currentRawMin = raw;
    _captureSession.currentRawMax = raw;
    _captureSession.currentDeltaMin = delta;
    _captureSession.currentDeltaMax = delta;

    char command[64];
    snprintf(command, sizeof(command), "CHIRP freq=%lu dur=%lu", _captureSession.toneHz, _captureSession.durationMs);
    sendEmitterCommand(command);
}

void AnalyzerApp::updateCaptureQuietStats(unsigned long now) {
    if (!_captureSession.active) {
        return;
    }
    if (_captureSession.currentTrial > 0 && !_captureSession.currentTrialFinalized && now >= _captureSession.currentTrialStartMs && now <= _captureSession.currentTrialEndMs) {
        return;
    }

    const int raw = _audioSignal.rawSignal();
    const float delta = static_cast<float>(_audioSignal.centeredSignal());

    if (_captureSession.quietRawSamples == 0) {
        _captureSession.quietRawMin = raw;
        _captureSession.quietRawMax = raw;
        _captureSession.quietDeltaMin = delta;
        _captureSession.quietDeltaMax = delta;
    } else {
        if (raw < _captureSession.quietRawMin) {
            _captureSession.quietRawMin = raw;
        }
        if (raw > _captureSession.quietRawMax) {
            _captureSession.quietRawMax = raw;
        }
        if (delta < _captureSession.quietDeltaMin) {
            _captureSession.quietDeltaMin = delta;
        }
        if (delta > _captureSession.quietDeltaMax) {
            _captureSession.quietDeltaMax = delta;
        }
    }

    _captureSession.quietRawSum += static_cast<unsigned long>(raw);
    _captureSession.quietRawSamples++;
    _captureSession.quietDeltaSum += delta;
    _captureSession.quietDeltaSamples++;
}

void AnalyzerApp::updateCaptureTrial(unsigned long now) {
    if (!_captureSession.active || _captureSession.currentTrial == 0 || _captureSession.currentTrialFinalized) {
        return;
    }

    if (now < _captureSession.currentTrialStartMs || now > _captureSession.currentTrialEndMs) {
        return;
    }

    const int raw = _audioSignal.rawSignal();
    const float delta = static_cast<float>(_audioSignal.centeredSignal());

    if (raw < _captureSession.currentRawMin) {
        _captureSession.currentRawMin = raw;
    }
    if (raw > _captureSession.currentRawMax) {
        _captureSession.currentRawMax = raw;
    }
    if (delta < _captureSession.currentDeltaMin) {
        _captureSession.currentDeltaMin = delta;
    }
    if (delta > _captureSession.currentDeltaMax) {
        _captureSession.currentDeltaMax = delta;
    }
}

void AnalyzerApp::finalizeCaptureTrial(unsigned long now) {
    if (!_captureSession.active || _captureSession.currentTrial == 0 || _captureSession.currentTrialFinalized) {
        return;
    }

    const int rawSwing = _captureSession.currentRawMax - _captureSession.currentRawMin;
    const float deltaSwing = _captureSession.currentDeltaMax - _captureSession.currentDeltaMin;

    _captureSession.completed++;
    _captureSession.totalRawSwing += static_cast<unsigned long>(rawSwing);
    _captureSession.totalDeltaSwing += deltaSwing;
    if (rawSwing > _captureSession.bestRawSwing) {
        _captureSession.bestRawSwing = rawSwing;
    }
    if (deltaSwing > _captureSession.bestDeltaSwing) {
        _captureSession.bestDeltaSwing = deltaSwing;
    }

    Serial.print("CAP trial=");
    Serial.print(_captureSession.currentTrial);
    Serial.print(" t=");
    Serial.print(now);
    Serial.print(" rawSample_min=");
    Serial.print(_captureSession.currentRawMin);
    Serial.print(" rawSample_max=");
    Serial.print(_captureSession.currentRawMax);
    Serial.print(" rawSample_swing=");
    Serial.print(rawSwing);
    Serial.print(" centeredSample_min=");
    Serial.print(_captureSession.currentDeltaMin, 1);
    Serial.print(" centeredSample_max=");
    Serial.print(_captureSession.currentDeltaMax, 1);
    Serial.print(" centeredSample_swing=");
    Serial.println(deltaSwing, 1);

    _captureSession.currentTrialFinalized = true;

    if (_captureSession.currentTrial >= _captureSession.totalTrials) {
        stopCaptureSession();
        printCaptureSummary();
        Serial.println("CAP stopped");
    }
}

void AnalyzerApp::stopSequenceTest() {
    _sequenceTest.active = false;
    _sequenceTest.sampleDumpCapturing = false;
    free(_sequenceTest.trialReports);
    _sequenceTest.trialReports = nullptr;
    _sequenceTest.trialReportCapacity = 0;
    _sequenceTest.trialReportCount = 0;
}

void AnalyzerApp::updateSequenceTest(unsigned long now) {
    if (_valMode) {
        return;
    }
    if (!_sequenceTest.active) {
        return;
    }

    if (_sequenceTest.currentTrial > 0 && now >= _sequenceTest.currentTrialEndMs) {
        finalizeSequenceTrial(now);
    }

    if (!_sequenceTest.active) {
        return;
    }

    if (now < _sequenceTest.nextTriggerAtMs) {
        return;
    }

    if (_sequenceTest.currentTrial >= _sequenceTest.totalTrials) {
        return;
    }

    const unsigned long trialNumber = _sequenceTest.currentTrial + 1;
    const unsigned long scheduledAtMs = _sequenceTest.nextTriggerAtMs;
    _sequenceTest.currentTrial = trialNumber;
    _sequenceTest.currentTrialScheduledAtMs = scheduledAtMs;
    _sequenceTest.currentTrialStartMs = scheduledAtMs;
    _sequenceTest.currentTrialEndMs = scheduledAtMs + _sequenceTest.windowEndOffsetMs;
    _sequenceTest.currentTrialOnsetDetectedMs = 0;
    _sequenceTest.currentTrialTransientDetectedMs = 0;
    _sequenceTest.currentTrialHit = false;
    _sequenceTest.currentTrialFinalized = false;
    _sequenceTest.currentTrialUnexpected = 0;
    _sequenceTest.trialHadAudioOverflow = false;
    _sequenceTest.trialOverflowCountAtStart = _audioSource.stats().overflowCount;
    _sequenceTest.trialTransientRejectTooShortCountAtStart = _audioOnsetDetector.transientRejectedDurationTooShortCount();
    _sequenceTest.trialTransientRejectTooLongCountAtStart = _audioOnsetDetector.transientRejectedDurationTooLongCount();
    _sequenceTest.trialTransientRejectWeakCountAtStart = _audioOnsetDetector.transientRejectedStrengthTooLowCount();
    _sequenceTest.currentTrialDiagnostics = {};
    _sequenceTest.currentTrialDiagnostics.acceptedAmbientBaseline = _audioSignal.baseline();
    _sequenceTest.currentTrialDiagnostics.runtimePatternCaptured = false;
    _sequenceTest.currentTrialDiagnostics.runtimePatternResult = {};
    _sequenceTest.currentTrialDiagnostics.runtimeFieldState = {};
    _sequenceTest.currentTrialDiagnostics.strongestRejectReason = AmpTransientDetector::TransientRejectReason::None;
    _sequenceTest.currentTrialDiagnostics.strongestRejectDtFromTriggerMs = -1;
    _sequenceTest.currentTrialDiagnostics.strongestRejectDurationMs = 0;
    _sequenceTest.currentTrialDiagnostics.strongestRejectStrength = 0.0f;
    _sequenceTest.nextTriggerAtMs = scheduledAtMs + _sequenceTest.periodMs;

    beginSequenceSampleDump(trialNumber);

    if (!_sequenceTest.externalEmitter) {
        char command[64];
        snprintf(command, sizeof(command), "CHIRP freq=%lu dur=%lu", _sequenceTest.toneHz, _sequenceTest.durationMs);
        sendEmitterCommand(command);
    }
}

void AnalyzerApp::handleSequenceTransient(unsigned long now) {
    if (_valMode) {
        return;
    }
    if (!_sequenceTest.active || _sequenceTest.currentTrial == 0) {
        _sequenceTest.unexpected++;
        _sequenceTest.currentTrialUnexpected++;
        return;
    }
    if (_sequenceTest.currentTrialFinalized) {
        return;
    }

    const bool inWindow = now >= _sequenceTest.currentTrialStartMs + _sequenceTest.windowStartOffsetMs && now <= _sequenceTest.currentTrialEndMs;
    if (!inWindow) {
        _sequenceTest.unexpected++;
        _sequenceTest.currentTrialUnexpected++;
        return;
    }

    _sequenceTest.currentTrialDiagnostics.onsetSeen = true;
    if (_sequenceTest.currentTrialDiagnostics.firstOnsetMs == 0) {
        _sequenceTest.currentTrialDiagnostics.firstOnsetMs = now;
    }
    _sequenceTest.currentTrialDiagnostics.lastOnsetMs = now;
    if (_sequenceTest.currentTrialOnsetDetectedMs == 0) {
        _sequenceTest.currentTrialOnsetDetectedMs = now;
    }

        if (_sequenceTest.currentTrialHit) {
        if (_sequenceTest.currentTrialDiagnostics.duplicateCount == 0) {
            _sequenceTest.currentTrialDiagnostics.duplicateTransientMs = now;
            _sequenceTest.currentTrialDiagnostics.duplicateTransientStrength = detectorTransientStrength();
            _sequenceTest.currentTrialDiagnostics.duplicateTransientDurationMs = detectorTransientDurationMs();
            _sequenceTest.currentTrialDiagnostics.duplicateFrequencyEvidence = captureFrequencyEvidence(static_cast<unsigned long>(now));
            _sequenceTest.currentTrialDiagnostics.duplicateFrequencyProcessedAtMs = now;
            _sequenceTest.currentTrialDiagnostics.duplicateDeltaFromPrimaryMs = _sequenceTest.currentTrialDiagnostics.transientAccepted
                ? static_cast<long>(now) - static_cast<long>(_sequenceTest.currentTrialDiagnostics.acceptedTransientMs)
                : 0;
            _sequenceTest.currentTrialDiagnostics.duplicateOriginWindow = true;
            strncpy(_sequenceTest.currentTrialDiagnostics.duplicateReason, "duplicate_after_primary", sizeof(_sequenceTest.currentTrialDiagnostics.duplicateReason) - 1);
            _sequenceTest.currentTrialDiagnostics.duplicateReason[sizeof(_sequenceTest.currentTrialDiagnostics.duplicateReason) - 1] = '\0';
        }
        _sequenceTest.currentTrialDiagnostics.duplicateCount++;
        if (_sequenceTest.currentTrialDiagnostics.duplicateDtCount < SequenceTest::kMaxDuplicateDts) {
            _sequenceTest.currentTrialDiagnostics.duplicateDts[_sequenceTest.currentTrialDiagnostics.duplicateDtCount++] = now >= _sequenceTest.currentTrialTransientDetectedMs
                ? now - _sequenceTest.currentTrialTransientDetectedMs
                : 0;
        }
        return;
    }

    _sequenceTest.currentTrialDiagnostics.transientAccepted = true;
    _sequenceTest.currentTrialDiagnostics.acceptedTransientMs = now;
    _sequenceTest.currentTrialDiagnostics.acceptedTransientOnsetStrength = detectorOnsetStrength();
    _sequenceTest.currentTrialDiagnostics.acceptedTransientStrength = detectorTransientStrength();
    _sequenceTest.currentTrialDiagnostics.acceptedTransientDurationMs = detectorTransientDurationMs();
    _sequenceTest.currentTrialDiagnostics.acceptedTransientReleaseStrength = detectorTransientStrength();
    _sequenceTest.currentTrialDiagnostics.acceptedTransientPeakMs = now;
    _sequenceTest.currentTrialDiagnostics.acceptedTransientReleaseMs = now + detectorTransientDurationMs();
    _sequenceTest.currentTrialDiagnostics.lastTransientRejectReason = AmpTransientDetector::TransientRejectReason::None;
    _sequenceTest.currentTrialDiagnostics.lastRejectStrength = 0.0f;
    _sequenceTest.currentTrialDiagnostics.lastRejectDurationMs = 0;
    _sequenceTest.currentTrialTransientDetectedMs = now;

    _sequenceTest.currentTrialHit = true;
}

void AnalyzerApp::updateSequenceAmbientStats() {
    if (_valMode) {
        return;
    }
    if (!_sequenceTest.active || _sequenceTest.currentTrial == 0 || _sequenceTest.currentTrialFinalized) {
        return;
    }

    auto& diagnostics = _sequenceTest.currentTrialDiagnostics;
    const float baseline = _audioSignal.baseline();
    const int signalLevel = _audioSignal.smoothedSignalMagnitude();

    if (diagnostics.ambientBaselineSamples == 0) {
        diagnostics.ambientBaselineMin = baseline;
        diagnostics.ambientBaselineMax = baseline;
        diagnostics.maxSignalLevel = signalLevel;
    } else {
        if (baseline < diagnostics.ambientBaselineMin) {
            diagnostics.ambientBaselineMin = baseline;
        }
        if (baseline > diagnostics.ambientBaselineMax) {
            diagnostics.ambientBaselineMax = baseline;
        }
        if (signalLevel > diagnostics.maxSignalLevel) {
            diagnostics.maxSignalLevel = signalLevel;
        }
    }

    diagnostics.ambientBaselineSamples++;
    diagnostics.ambientBaselineSum += baseline;
}

bool AnalyzerApp::sequenceSampleDumpSelected(unsigned long trialNumber) const {
    if (!_sequenceTest.sampleDumpEnabled) {
        return false;
    }

    const bool firstSelected = _sequenceTest.sampleDumpFirstTrials > 0 && trialNumber <= _sequenceTest.sampleDumpFirstTrials;
    const bool everySelected = _sequenceTest.sampleDumpEveryNth > 0 && trialNumber % _sequenceTest.sampleDumpEveryNth == 0;
    return firstSelected || everySelected;
}

void AnalyzerApp::clearSequenceSampleDump() {
    _sequenceTest.sampleDumpSelectedForTrial = false;
    _sequenceTest.sampleDumpCapturing = false;
    _sequenceTest.sampleDumpCurrentTrial = 0;
    _sequenceTest.sampleDumpTriggerMs = 0;
    _sequenceTest.sampleDumpTriggerSampleMs = 0;
    _sequenceTest.sampleDumpCaptureStartMs = 0;
    _sequenceTest.sampleDumpCaptureEndMs = 0;
    _sequenceTest.sampleDumpNextEmitMs = 0;
    _sequenceTest.sampleRowCount = 0;
    _sequenceTest.sampleHistoryStart = 0;
    _sequenceTest.sampleHistoryCount = 0;
    _sequenceTest.sampleHistoryLastMs = 0;
    _sequenceTest.sampleHistoryHasPending = false;
    _sequenceTest.sampleHistoryPending = {};
}

void AnalyzerApp::flushSequenceSampleHistory(unsigned long currentSampleMs) {
    if (!_sequenceTest.sampleHistoryHasPending) {
        return;
    }
    if (_sequenceTest.sampleHistoryPending.sampleMs >= currentSampleMs) {
        return;
    }

    const CurveSnapshot committed = _sequenceTest.sampleHistoryPending;
    _sequenceTest.sampleHistoryHasPending = false;
    _sequenceTest.sampleHistoryPending = {};

    if (_sequenceTest.sampleHistoryCount < SequenceTest::kMaxSampleHistory) {
        const size_t index = (_sequenceTest.sampleHistoryStart + _sequenceTest.sampleHistoryCount) % SequenceTest::kMaxSampleHistory;
        _sequenceTest.sampleHistory[index] = committed;
        ++_sequenceTest.sampleHistoryCount;
    } else {
        _sequenceTest.sampleHistory[_sequenceTest.sampleHistoryStart] = committed;
        _sequenceTest.sampleHistoryStart = (_sequenceTest.sampleHistoryStart + 1) % SequenceTest::kMaxSampleHistory;
    }

    _sequenceTest.sampleHistoryLastMs = committed.sampleMs;

    if (!_sequenceTest.sampleDumpCapturing
        || !_sequenceTest.sampleDumpSelectedForTrial
        || _sequenceTest.sampleDumpCurrentTrial != _sequenceTest.currentTrial) {
        return;
    }

    if (committed.sampleMs < _sequenceTest.sampleDumpCaptureStartMs || committed.sampleMs > _sequenceTest.sampleDumpCaptureEndMs) {
        return;
    }
    if (committed.sampleMs < _sequenceTest.sampleDumpNextEmitMs) {
        return;
    }

    if (_sequenceTest.sampleRowCount >= SequenceTest::kMaxSampleRows) {
        if (!_sequenceTest.sampleDumpWarned) {
            Serial.print("SAMPLES_WARN reason=too_many_samples requested=");
            Serial.print(_sequenceTest.sampleRowCount + 1UL);
            Serial.print(" max_allowed=");
            Serial.println(SequenceTest::kMaxSampleRows);
            _sequenceTest.sampleDumpWarned = true;
        }
        _sequenceTest.sampleDumpCapturing = false;
        return;
    }

    _sequenceTest.sampleRows[_sequenceTest.sampleRowCount++] = committed;
    _sequenceTest.sampleDumpNextEmitMs = committed.sampleMs + _sequenceTest.sampleDumpStepMs;
}

void AnalyzerApp::recordSequenceSample(const CurveSnapshot& snapshot) {
    const unsigned long sampleMs = snapshot.sampleMs;

    if (!_sequenceTest.sampleHistoryHasPending) {
        _sequenceTest.sampleHistoryPending = snapshot;
        _sequenceTest.sampleHistoryHasPending = true;
        return;
    }

    if (sampleMs == _sequenceTest.sampleHistoryPending.sampleMs) {
        _sequenceTest.sampleHistoryPending = snapshot;
        return;
    }

    flushSequenceSampleHistory(sampleMs);
    _sequenceTest.sampleHistoryPending = snapshot;
    _sequenceTest.sampleHistoryHasPending = true;
}

void AnalyzerApp::beginSequenceSampleDump(unsigned long trialNumber) {
    _sequenceTest.sampleDumpSelectedForTrial = sequenceSampleDumpSelected(trialNumber);
    _sequenceTest.sampleDumpCurrentTrial = trialNumber;
    _sequenceTest.sampleDumpCapturing = _sequenceTest.sampleDumpSelectedForTrial;
    _sequenceTest.sampleDumpTriggerMs = _sequenceTest.currentTrialStartMs;
    _sequenceTest.sampleDumpTriggerSampleMs = _audioSignal.sampleTimeUs() / 1000UL;
    _sequenceTest.sampleDumpCaptureStartMs = _sequenceTest.sampleDumpTriggerSampleMs > _sequenceTest.sampleDumpLeadMs
        ? _sequenceTest.sampleDumpTriggerSampleMs - _sequenceTest.sampleDumpLeadMs
        : 0;
    _sequenceTest.sampleDumpCaptureEndMs = _sequenceTest.sampleDumpTriggerSampleMs + _sequenceTest.sampleDumpTailMs;
    _sequenceTest.sampleDumpNextEmitMs = _sequenceTest.sampleDumpCaptureStartMs;
    _sequenceTest.sampleRowCount = 0;

    flushSequenceSampleHistory(_sequenceTest.sampleDumpTriggerSampleMs + 1UL);

    if (!_sequenceTest.sampleDumpCapturing) {
        return;
    }

    for (size_t i = 0; i < _sequenceTest.sampleHistoryCount; ++i) {
        const size_t index = (_sequenceTest.sampleHistoryStart + i) % SequenceTest::kMaxSampleHistory;
        const auto& snapshot = _sequenceTest.sampleHistory[index];
        if (snapshot.sampleMs < _sequenceTest.sampleDumpCaptureStartMs) {
            continue;
        }
        if (snapshot.sampleMs > _sequenceTest.sampleDumpTriggerSampleMs) {
            break;
        }
        if (snapshot.sampleMs >= _sequenceTest.sampleDumpNextEmitMs) {
            if (_sequenceTest.sampleRowCount < SequenceTest::kMaxSampleRows) {
                _sequenceTest.sampleRows[_sequenceTest.sampleRowCount++] = snapshot;
                _sequenceTest.sampleDumpNextEmitMs = snapshot.sampleMs + _sequenceTest.sampleDumpStepMs;
            } else if (!_sequenceTest.sampleDumpWarned) {
                Serial.print("SAMPLES_WARN reason=too_many_samples requested=");
                Serial.print(_sequenceTest.sampleRowCount + 1UL);
                Serial.print(" max_allowed=");
                Serial.println(SequenceTest::kMaxSampleRows);
                _sequenceTest.sampleDumpWarned = true;
                _sequenceTest.sampleDumpCapturing = false;
                break;
            }
        }
    }
}

void AnalyzerApp::printSequenceSampleDump(unsigned long trialNumber) const {
    if (!_sequenceTest.sampleDumpEnabled || !_sequenceTest.sampleDumpSelectedForTrial || _sequenceTest.sampleDumpCurrentTrial != trialNumber) {
        return;
    }

    Serial.print("SAMPLES_BEGIN trial=");
    Serial.print(trialNumber);
    Serial.print(" trigger_ms=");
    Serial.print(_sequenceTest.sampleDumpTriggerMs);
    Serial.print(" sample_rate_ms=");
    Serial.print(_sequenceTest.sampleDumpStepMs);
    Serial.print(" fields=t,current,env,peak,open onset=");
    Serial.print(detectorOnsetDetectionThreshold(), 1);
    Serial.print(" release=");
    Serial.print(detectorOnsetReleaseThreshold(), 1);
    Serial.print(" minStrength=");
    Serial.print(detectorMinTransientPeakStrength(), 1);
    Serial.print(" minMs=");
    Serial.print(detectorMinTransientDurationMs());
    Serial.print(" maxMs=");
    Serial.print(detectorMaxTransientDurationMs());
    Serial.println();

    for (size_t i = 0; i < _sequenceTest.sampleRowCount; ++i) {
        const auto& sample = _sequenceTest.sampleRows[i];
        const long tMs = static_cast<long>(sample.sampleMs) - static_cast<long>(_sequenceTest.sampleDumpTriggerSampleMs);
        Serial.print(tMs);
        Serial.print(",");
        Serial.print(sample.current);
        Serial.print(",");
        Serial.print(sample.env);
        Serial.print(",");
        Serial.print(sample.peak, 1);
        Serial.print(",");
        Serial.println(sample.open ? 1 : 0);
    }

    Serial.print("SAMPLES_END trial=");
    Serial.println(trialNumber);
}

void AnalyzerApp::sequenceCurveSampleCallback(const CurveSnapshot& snapshot, void* context) {
    auto* self = static_cast<AnalyzerApp*>(context);
    if (self == nullptr) {
        return;
    }
    self->recordSequenceSample(snapshot);
}

detection::FrequencyEvidence AnalyzerApp::captureFrequencyEvidence(unsigned long observedAtMs) const {
    detection::FrequencyEvidence evidence;
    evidence.observedAtMs = observedAtMs;
    const bool present = _freqBandStream.windowReady();
    const float totalEnergy = _freqBandStream.lastTotalEnergy();

    evidence.present = present;
    evidence.matched = false;
    evidence.targetHz = present ? _freqBandStream.targetFrequencyHz() : 0;
    evidence.windowSampleCount = _freqBandStream.sampleCount();
    evidence.windowAvailable = present;
    evidence.score = _freqBandStream.lastFrequencyScore();
    evidence.confidence = 0.0f;
    evidence.targetPower = _freqBandStream.lastTargetPower();
    evidence.neighborPower = _freqBandStream.lastNeighborPower();
    evidence.totalEnergy = totalEnergy;
    evidence.spectralContrast = _freqBandStream.lastSpectralContrast();
    evidence.validWindow = present;
    return evidence;
}

detection::FrequencyEvidence AnalyzerApp::scanSequenceFrequencyParity64(const detection::PatternCandidate& patternCandidate, unsigned long observedAtMs) const {
    detection::FrequencyEvidence evidence;
    DetectorCandidate detectorCandidate;
    detectorCandidate.onsetSample = patternCandidate.onsetSample;
    detectorCandidate.peakSample = patternCandidate.peakSample;
    detectorCandidate.releaseSample = patternCandidate.releaseSample;
    detectorCandidate.onsetMillisApprox = patternCandidate.startMs;
    detectorCandidate.releaseMillisApprox = patternCandidate.startMs + patternCandidate.durationMs;
    detectorCandidate.onsetStrength = patternCandidate.onsetStrength;
    detectorCandidate.peakStrength = patternCandidate.peakStrength;
    detectorCandidate.releaseStrength = patternCandidate.releaseStrength;
    detectorCandidate.ambientBaseline = patternCandidate.ambientBaseline;
    detectorCandidate.durationMs = patternCandidate.durationMs;
    detectorCandidate.audioOverflowDuringCandidate = patternCandidate.audioOverflowDuringCandidate;

    const unsigned long sampleRateHz = _audioSource.sampleRateHz() > 0 ? _audioSource.sampleRateHz() : 16000UL;
    if (detection::measureCandidateWindowFrequencyParityScan64(_audioSignal,
                                                                       detectorCandidate,
                                                                       sampleRateHz,
                                                                       _freqBandStream.targetFrequencyHz(),
                                                                       observedAtMs,
                                                                       evidence,
                                                                       64UL)) {
        return evidence;
    }

    return {};
}

void AnalyzerApp::noteSequenceTransientReject(unsigned long eventMs) {
    if (_valMode) {
        return;
    }
    if (!_sequenceTest.active || _sequenceTest.currentTrial == 0 || _sequenceTest.currentTrialFinalized) {
        return;
    }

    const char* reasonName = detectorTransientRejectReasonName();
    if (strcmp(reasonName, "none") == 0) {
        return;
    }

    noteSequenceTransientRejectReason(eventMs, reasonName, detectorTransientRejectedDurationMs(), detectorTransientRejectedStrength());
}

void AnalyzerApp::noteSequenceTransientRejectReason(unsigned long eventMs, const char* reasonName, unsigned long durationMs, float strength) {
    if (_valMode) {
        return;
    }
    if (!_sequenceTest.active || _sequenceTest.currentTrial == 0 || _sequenceTest.currentTrialFinalized) {
        return;
    }

    auto& diagnostics = _sequenceTest.currentTrialDiagnostics;
    const unsigned long shortCount = _audioOnsetDetector.transientRejectedDurationTooShortCount() - _sequenceTest.trialTransientRejectTooShortCountAtStart;
    const unsigned long longCount = _audioOnsetDetector.transientRejectedDurationTooLongCount() - _sequenceTest.trialTransientRejectTooLongCountAtStart;
    const unsigned long weakCount = _audioOnsetDetector.transientRejectedStrengthTooLowCount() - _sequenceTest.trialTransientRejectWeakCountAtStart;

    diagnostics.transientRejectTooShortCount = shortCount;
    diagnostics.transientRejectTooLongCount = longCount;
    diagnostics.transientRejectWeakCount = weakCount;

    const AmpTransientDetector::TransientRejectReason reason =
        strcmp(reasonName, "duration_too_short") == 0 ? AmpTransientDetector::TransientRejectReason::DurationTooShort :
        strcmp(reasonName, "duration_too_long") == 0 ? AmpTransientDetector::TransientRejectReason::DurationTooLong :
        strcmp(reasonName, "strength_too_low") == 0 ? AmpTransientDetector::TransientRejectReason::StrengthTooLow :
        strcmp(reasonName, "peak_still_active") == 0 ? AmpTransientDetector::TransientRejectReason::PeakStillActive :
        AmpTransientDetector::TransientRejectReason::None;

    if (reason != AmpTransientDetector::TransientRejectReason::None && strength >= diagnostics.strongestRejectStrength) {
        diagnostics.strongestRejectReason = reason;
        diagnostics.strongestRejectDtFromTriggerMs = static_cast<long>(eventMs - _sequenceTest.currentTrialStartMs);
        diagnostics.strongestRejectDurationMs = durationMs;
        diagnostics.strongestRejectStrength = strength;
    }
}

const char* AnalyzerApp::sequenceTrialClassificationName(const char* result, long dtMs, long durMs, const SequenceTest::TrialDiagnostics& diagnostics) const {
    if (strcmp(result, "invalid_audio") == 0) {
        return "invalid_audio";
    }
    if (strcmp(result, "unexpected") == 0) {
        return "unexpected";
    }
    if (strcmp(result, "late") == 0) {
        return "late";
    }
    if (strcmp(result, "miss") == 0) {
        switch (diagnostics.strongestRejectReason) {
            case AmpTransientDetector::TransientRejectReason::DurationTooLong:
                return "miss_too_long";
            case AmpTransientDetector::TransientRejectReason::StrengthTooLow:
                return "miss_weak";
            case AmpTransientDetector::TransientRejectReason::None:
            case AmpTransientDetector::TransientRejectReason::DurationTooShort:
            case AmpTransientDetector::TransientRejectReason::PeakStillActive:
                return "miss_no_onset";
        }
    }
    if (dtMs >= kLateOnsetMinMs) {
        return "late";
    }
    if (durMs >= kTooLongDurationMinMs) {
        return "expected_too_long";
    }
    if (durMs >= kSmearedDurationMinMs && durMs <= kSmearedDurationMaxMs) {
        return "expected_smeared";
    }
    if (durMs >= kCleanDurationMinMs && durMs <= kCleanDurationMaxMs) {
        return "expected_clean";
    }
    return "expected_clean";
}

void AnalyzerApp::recordSequenceClassifierOutcome(const detection::PatternResult& patternResult, bool duplicateCandidate, bool unexpectedCandidate) {
    if (_valMode || !patternResult.patternCandidateAccepted) {
        return;
    }

    const auto freqEval = FrequencyMatchEvaluation::evaluate(patternResult.freq, _frequencyEvidenceTuning);
    const bool patternMatched = patternResult.valid;

    if (unexpectedCandidate) {
        if (patternMatched) {
            ++_sequenceTest.patternMatchedUnexpected;
        } else {
            ++_sequenceTest.patternUnmatchedUnexpected;
        }
    } else if (duplicateCandidate) {
        if (patternMatched) {
            ++_sequenceTest.patternMatchedDuplicates;
        } else {
            ++_sequenceTest.patternUnmatchedDuplicates;
        }
    } else {
        if (patternMatched) {
            ++_sequenceTest.patternMatchedExpected;
        } else {
            ++_sequenceTest.patternUnmatchedExpected;
        }
    }

    switch (freqEval.reason) {
        case FrequencyMatchEvaluation::Reason::None:
            break;
        case FrequencyMatchEvaluation::Reason::NoEvidence:
            ++_sequenceTest.freqRejectNoEvidence;
            break;
        case FrequencyMatchEvaluation::Reason::InvalidWindow:
            ++_sequenceTest.freqRejectInvalidWindow;
            break;
        case FrequencyMatchEvaluation::Reason::ScoreTooLow:
            ++_sequenceTest.freqRejectScore;
            break;
        case FrequencyMatchEvaluation::Reason::ContrastTooLow:
            ++_sequenceTest.freqRejectContrast;
            break;
        case FrequencyMatchEvaluation::Reason::ScoreAndContrastTooLow:
            ++_sequenceTest.freqRejectBoth;
            break;
    }
}

void AnalyzerApp::handleSequenceCandidate(const detection::PatternResult& patternResult, const detection::FrequencyEvidence* liveFrequencyEvidence) {
    if (_valMode) {
        return;
    }

    if (!_sequenceTest.active || _sequenceTest.currentTrial == 0) {
        return;
    }

    auto& diagnostics = _sequenceTest.currentTrialDiagnostics;
    diagnostics.rawCandidateCount++;

    const auto& candidate = patternResult.candidate;
    const unsigned long candidateIdx = diagnostics.rawCandidateCount;
    const unsigned long onsetMs = candidate.startMs;
    const long dtFromTriggerMs = static_cast<long>(onsetMs) - static_cast<long>(_sequenceTest.currentTrialScheduledAtMs);
    const long dtFromTrialStartMs = static_cast<long>(onsetMs) - static_cast<long>(_sequenceTest.currentTrialStartMs);
    const long processLagMs = patternResult.processedAtMs >= onsetMs
        ? static_cast<long>(patternResult.processedAtMs - onsetMs)
        : -1;
    const uint32_t sampleRateHz = _audioSource.sampleRateHz() > 0 ? _audioSource.sampleRateHz() : 16000UL;
    const unsigned long peakOffsetMs = candidate.peakSample >= candidate.onsetSample
        ? static_cast<unsigned long>(((candidate.peakSample - candidate.onsetSample) * 1000ULL) / static_cast<uint64_t>(sampleRateHz))
        : 0UL;

    const bool overflowSeenNow = candidate.audioOverflowDuringCandidate
                                 || _audioSource.stats().overflowCount != _sequenceTest.trialOverflowCountAtStart;
    if (overflowSeenNow) {
        _sequenceTest.trialHadAudioOverflow = true;
    }

    const bool preWindow = onsetMs < _sequenceTest.currentTrialStartMs + _sequenceTest.windowStartOffsetMs;
    const bool postWindow = onsetMs > _sequenceTest.currentTrialEndMs;
    const bool inWindow = !preWindow && !postWindow;
    const bool duplicateCandidate = _sequenceTest.currentTrialHit && inWindow;
    const char* candidateClass = h3SequenceCandidateClass(duplicateCandidate, inWindow, dtFromTriggerMs);

    const SequenceTest::CandidateOrigin origin = preWindow
        ? SequenceTest::CandidateOrigin::PreWindow
        : postWindow
            ? SequenceTest::CandidateOrigin::PostWindow
            : SequenceTest::CandidateOrigin::InWindow;

    if (diagnostics.firstCandidateMs == 0) {
        diagnostics.firstCandidateMs = onsetMs;
    }

    if (diagnostics.candidateCount < SequenceTest::kMaxTrialCandidates) {
        auto& entry = diagnostics.candidates[diagnostics.candidateCount++];
        entry.candidateMs = onsetMs;
        entry.dtFromTriggerMs = dtFromTriggerMs;
        entry.dtFromTrialStartMs = dtFromTrialStartMs;
        entry.durationMs = candidate.durationMs;
        entry.strength = candidate.peakStrength;
        entry.origin = origin;
    } else {
        diagnostics.candidateOverflowCount++;
    }

    if (origin == SequenceTest::CandidateOrigin::PreWindow) {
        diagnostics.candidatePreWindowCount++;
    } else if (origin == SequenceTest::CandidateOrigin::InWindow) {
        diagnostics.candidateInWindowCount++;
    } else {
        diagnostics.candidatePostWindowCount++;
    }

    recordSequenceClassifierOutcome(patternResult, duplicateCandidate, !inWindow);

    if (!diagnostics.bestCandidateAccepted || candidate.peakStrength > diagnostics.bestCandidateStrength) {
        diagnostics.bestCandidateAccepted = true;
        diagnostics.bestCandidateDtFromTriggerMs = dtFromTriggerMs;
        diagnostics.bestCandidateDurationMs = candidate.durationMs;
        diagnostics.bestCandidateStrength = candidate.peakStrength;
        diagnostics.bestCandidateOrigin = origin;
    }

    if (analyzerLogEnabled(_sequenceTest.logFlags, AnalyzerApp::ANALYZER_LOG_CANDIDATE) && !_sequenceTest.quiet) {
        auto printSequenceCandidateLine = [&](const char* role, const char* indexLabel, bool includeReason) {
            Serial.print("SEQ_CAND role=");
            Serial.print(role);
            Serial.print(" trial=");
            Serial.print(_sequenceTest.currentTrial);
            Serial.print(" ");
            Serial.print(indexLabel);
            Serial.print("=");
            Serial.print(candidateIdx);
            Serial.print(" onset_ms=");
            Serial.print(onsetMs);
            Serial.print(" onset_sample=");
            Serial.print(candidate.onsetSample);
            Serial.print(" peak_sample=");
            Serial.print(candidate.peakSample);
            Serial.print(" release_sample=");
            Serial.print(candidate.releaseSample);
            Serial.print(" onset_dt_ms=");
            Serial.print(dtFromTriggerMs);
            Serial.print(" peak_ms=");
            Serial.print(candidate.startMs + peakOffsetMs);
            Serial.print(" dur=");
            Serial.print(candidate.durationMs);
            Serial.print(" end_dt_ms=");
            if (dtFromTriggerMs >= 0) {
                Serial.print(dtFromTriggerMs + static_cast<long>(candidate.durationMs));
                Serial.print("ms");
            } else {
                Serial.print("-");
            }
            Serial.print(" processed_at_ms=");
            Serial.print(patternResult.processedAtMs);
            Serial.print(" process_lag_ms=");
            if (processLagMs >= 0) {
                Serial.print(processLagMs);
                Serial.print("ms");
            } else {
                Serial.print("-");
            }
            Serial.print(" strength=");
            Serial.print(candidate.peakStrength, 1);
            Serial.print(" transient_present=");
            Serial.print(patternResult.candidate.transient.present ? 1 : 0);
            Serial.print(" freq_present=");
            Serial.print(patternResult.freq.present ? 1 : 0);
            Serial.print(" freq_matched=");
            Serial.print(patternResult.freq.matched ? 1 : 0);
            Serial.print(" freq_score=");
            Serial.print(patternResult.freq.score, 1);
            Serial.print(" freq_conf=");
            Serial.print(patternResult.freq.confidence, 1);
            Serial.print(" freq_target_hz=");
            Serial.print(patternResult.freq.targetHz);
            Serial.print(" freq_contrast=");
            Serial.print(patternResult.freq.spectralContrast, 1);
            printH3FrequencyEvidenceFields(patternResult, patternResult.freq, liveFrequencyEvidence, _frequencyEvidenceTuning, candidateClass, dtFromTriggerMs, patternResult.processedAtMs);
            Serial.print(" pattern_type=");
            Serial.print(detection::patternTypeName(patternResult.type));
            if (includeReason) {
                Serial.print(" reason=");
                Serial.print(detection::patternReasonName(patternResult.reasonCode));
            }
            Serial.println();
        };

        printSequenceCandidateLine("detector", "idx", false);
        printSequenceCandidateLine("pattern", "idx", false);
        printSequenceCandidateLine("result", "primary_idx", true);
    }

    if (!inWindow) {
        if (!_sequenceTest.trialHadAudioOverflow) {
            _sequenceTest.unexpected++;
            _sequenceTest.currentTrialUnexpected++;
        }
        return;
    }

    _sequenceTest.currentTrialDiagnostics.onsetSeen = true;
    if (_sequenceTest.currentTrialDiagnostics.firstOnsetMs == 0) {
        _sequenceTest.currentTrialDiagnostics.firstOnsetMs = onsetMs;
    }
    _sequenceTest.currentTrialDiagnostics.lastOnsetMs = onsetMs;
    if (_sequenceTest.currentTrialOnsetDetectedMs == 0) {
        _sequenceTest.currentTrialOnsetDetectedMs = onsetMs;
    }

    if (_sequenceTest.currentTrialHit) {
        if (diagnostics.duplicateCount == 0) {
            diagnostics.duplicateTransientMs = onsetMs;
            diagnostics.duplicateTransientStrength = candidate.peakStrength;
            diagnostics.duplicateTransientDurationMs = candidate.durationMs;
            diagnostics.duplicateTransientOnsetSample = candidate.onsetSample;
            diagnostics.duplicateTransientPeakSample = candidate.peakSample;
            diagnostics.duplicateTransientReleaseSample = candidate.releaseSample;
            diagnostics.duplicateTransientPeakMs = candidate.startMs + peakOffsetMs;
            diagnostics.duplicateTransientReleaseMs = candidate.startMs + candidate.durationMs;
            diagnostics.duplicateFrequencyEvidence = patternResult.freq;
            diagnostics.duplicateFrequencyEvidenceFull = patternResult.freqFull;
            diagnostics.duplicateFrequencyProcessedAtMs = patternResult.processedAtMs;
            diagnostics.duplicateParityProbe64 = scanSequenceFrequencyParity64(patternResult.candidate, patternResult.processedAtMs);
            diagnostics.duplicateParityProbe64ProcessedAtMs = patternResult.processedAtMs;
            diagnostics.duplicateDeltaFromPrimaryMs = diagnostics.transientAccepted
                ? static_cast<long>(onsetMs) - static_cast<long>(diagnostics.acceptedTransientMs)
                : 0;
            strncpy(diagnostics.duplicateReason, "duplicate_after_primary", sizeof(diagnostics.duplicateReason) - 1);
            diagnostics.duplicateReason[sizeof(diagnostics.duplicateReason) - 1] = '\0';
        }
        _sequenceTest.currentTrialDiagnostics.duplicateCount++;
        if (_sequenceTest.currentTrialDiagnostics.duplicateDtCount < SequenceTest::kMaxDuplicateDts) {
            _sequenceTest.currentTrialDiagnostics.duplicateDts[_sequenceTest.currentTrialDiagnostics.duplicateDtCount++] = onsetMs >= _sequenceTest.currentTrialTransientDetectedMs
                ? onsetMs - _sequenceTest.currentTrialTransientDetectedMs
                : 0;
        }
        return;
    }

    _sequenceTest.currentTrialDiagnostics.transientAccepted = true;
    _sequenceTest.currentTrialDiagnostics.acceptedTransientMs = onsetMs;
    _sequenceTest.currentTrialDiagnostics.acceptedTransientOnsetStrength = candidate.onsetStrength;
    _sequenceTest.currentTrialDiagnostics.acceptedTransientStrength = candidate.peakStrength;
    _sequenceTest.currentTrialDiagnostics.acceptedTransientDurationMs = candidate.durationMs;
    _sequenceTest.currentTrialDiagnostics.acceptedTransientReleaseStrength = candidate.releaseStrength;
    _sequenceTest.currentTrialDiagnostics.acceptedTransientOnsetSample = candidate.onsetSample;
    _sequenceTest.currentTrialDiagnostics.acceptedTransientPeakSample = candidate.peakSample;
    _sequenceTest.currentTrialDiagnostics.acceptedTransientReleaseSample = candidate.releaseSample;
    _sequenceTest.currentTrialDiagnostics.acceptedTransientPeakMs = candidate.startMs + peakOffsetMs;
    _sequenceTest.currentTrialDiagnostics.acceptedTransientReleaseMs = candidate.startMs + candidate.durationMs;
    _sequenceTest.currentTrialDiagnostics.acceptedAmbientBaseline = candidate.ambientBaseline;
    _sequenceTest.currentTrialDiagnostics.acceptedFrequencyEvidence = patternResult.freq;
    _sequenceTest.currentTrialDiagnostics.acceptedFrequencyEvidenceFull = patternResult.freqFull;
    _sequenceTest.currentTrialDiagnostics.acceptedFrequencyProcessedAtMs = patternResult.processedAtMs;
    _sequenceTest.currentTrialDiagnostics.acceptedParityProbe64 = scanSequenceFrequencyParity64(patternResult.candidate, patternResult.processedAtMs);
    _sequenceTest.currentTrialDiagnostics.acceptedParityProbe64ProcessedAtMs = patternResult.processedAtMs;
    _sequenceTest.currentTrialDiagnostics.lastTransientRejectReason = AmpTransientDetector::TransientRejectReason::None;
    _sequenceTest.currentTrialDiagnostics.lastRejectStrength = 0.0f;
    _sequenceTest.currentTrialDiagnostics.lastRejectDurationMs = 0;
    _sequenceTest.currentTrialTransientDetectedMs = onsetMs;

    _sequenceTest.currentTrialHit = true;

    if (analyzerLogEnabled(_sequenceTest.logFlags, AnalyzerApp::ANALYZER_LOG_CANDIDATE) && !_sequenceTest.quiet) {
        Serial.print("SEQ_CAND role=result trial=");
        Serial.print(_sequenceTest.currentTrial);
        Serial.print(" primary_idx=");
        Serial.print(candidateIdx);
        Serial.print(" onset_ms=");
        Serial.print(candidate.startMs);
        Serial.print(" onset_sample=");
        Serial.print(candidate.onsetSample);
        Serial.print(" peak_sample=");
        Serial.print(candidate.peakSample);
        Serial.print(" release_sample=");
        Serial.print(candidate.releaseSample);
        Serial.print(" onset_dt_ms=");
        Serial.print(dtFromTriggerMs);
        Serial.print(" peak_ms=");
        Serial.print(candidate.startMs + peakOffsetMs);
        Serial.print(" dur=");
        Serial.print(candidate.durationMs);
        Serial.print(" end_dt_ms=");
        if (dtFromTriggerMs >= 0) {
            Serial.print(dtFromTriggerMs + static_cast<long>(candidate.durationMs));
            Serial.print("ms");
        } else {
            Serial.print("-");
        }
        Serial.print(" processed_at_ms=");
        Serial.print(patternResult.processedAtMs);
        Serial.print(" process_lag_ms=");
        if (processLagMs >= 0) {
            Serial.print(processLagMs);
            Serial.print("ms");
        } else {
            Serial.print("-");
        }
        Serial.print(" strength=");
        Serial.print(candidate.peakStrength, 1);
        Serial.print(" transient_present=");
        Serial.print(patternResult.candidate.transient.present ? 1 : 0);
        Serial.print(" freq_present=");
        Serial.print(patternResult.freq.present ? 1 : 0);
        Serial.print(" freq_matched=");
        Serial.print(patternResult.freq.matched ? 1 : 0);
        Serial.print(" freq_score=");
        Serial.print(patternResult.freq.score, 1);
        Serial.print(" freq_conf=");
        Serial.print(patternResult.freq.confidence, 1);
        Serial.print(" freq_target_hz=");
        Serial.print(patternResult.freq.targetHz);
        Serial.print(" freq_contrast=");
        Serial.print(patternResult.freq.spectralContrast, 1);
        Serial.print(" reason=");
        Serial.print(detection::patternReasonName(patternResult.reasonCode));
        printH3FrequencyEvidenceFields(patternResult, patternResult.freq, liveFrequencyEvidence, _frequencyEvidenceTuning, candidateClass, dtFromTriggerMs, patternResult.processedAtMs);
        Serial.println();
    }
}

void AnalyzerApp::finalizeSequenceTrial(unsigned long now) {
    if (_valMode) {
        return;
    }
    if (_sequenceTest.currentTrial == 0) {
        return;
    }

    if (_sequenceTest.currentTrialFinalized) {
        return;
    }

    auto& diagnostics = _sequenceTest.currentTrialDiagnostics;

    diagnostics.peakActiveAtEnd = detectorTransientPeakActive();
    const char* transientRejectReason = detectorTransientRejectReasonName();
    if (strcmp(transientRejectReason, "duration_too_short") == 0) {
        diagnostics.lastTransientRejectReason = AmpTransientDetector::TransientRejectReason::DurationTooShort;
    } else if (strcmp(transientRejectReason, "duration_too_long") == 0) {
        diagnostics.lastTransientRejectReason = AmpTransientDetector::TransientRejectReason::DurationTooLong;
    } else if (strcmp(transientRejectReason, "strength_too_low") == 0) {
        diagnostics.lastTransientRejectReason = AmpTransientDetector::TransientRejectReason::StrengthTooLow;
    } else if (strcmp(transientRejectReason, "peak_still_active") == 0) {
        diagnostics.lastTransientRejectReason = AmpTransientDetector::TransientRejectReason::PeakStillActive;
    } else {
        diagnostics.lastTransientRejectReason = AmpTransientDetector::TransientRejectReason::None;
    }
    diagnostics.lastRejectDurationMs = detectorTransientRejectedDurationMs();
    diagnostics.lastRejectStrength = detectorTransientRejectedStrength();

    const bool invalidAudioTrial = _sequenceTest.trialHadAudioOverflow
                                   || _audioSource.stats().overflowCount != _sequenceTest.trialOverflowCountAtStart;
    const bool unexpectedTrial = !invalidAudioTrial && _sequenceTest.currentTrialUnexpected > 0;
    const bool hitTrial = !invalidAudioTrial && _sequenceTest.currentTrialHit;

    AnalyzerResult result = AnalyzerResult::Miss;
    long dtMs = -1;
    long durMs = -1;
    float strength = 0.0f;

    if (invalidAudioTrial) {
        _sequenceTest.invalidAudio++;
        result = AnalyzerResult::InvalidAudio;
    } else if (unexpectedTrial) {
        _sequenceTest.unexpected++;
        result = AnalyzerResult::Unexpected;
    } else if (hitTrial) {
        _sequenceTest.hits++;
        dtMs = static_cast<long>(diagnostics.acceptedTransientMs - _sequenceTest.currentTrialStartMs);
        durMs = static_cast<long>(diagnostics.acceptedTransientDurationMs);
        strength = diagnostics.acceptedTransientStrength;
        if (dtMs >= kLateOnsetMinMs) {
            result = AnalyzerResult::Late;
            _sequenceTest.lateHits++;
        } else {
            result = AnalyzerResult::Expected;
            _sequenceTest.expectedHits++;
        }
        _sequenceTest.totalHitStrengthScaled += static_cast<unsigned long>(diagnostics.acceptedTransientStrength * 100.0f);
        _sequenceTest.totalHitDurationMs += diagnostics.acceptedTransientDurationMs;
    } else {
        _sequenceTest.misses++;
    }

    _sequenceTest.duplicates += diagnostics.duplicateCount;
    AnalyzerReport finalizedReport = buildSequenceAnalyzerReport(_sequenceTest.currentTrial, result, dtMs, durMs, strength, invalidAudioTrial, diagnostics.duplicateCount, diagnostics);
    const bool summaryTrial = analyzerLogEnabled(_sequenceTest.logFlags, AnalyzerApp::ANALYZER_LOG_TRIAL_SUMMARY);
    flushSequenceSampleHistory(now + 1UL);
    printSequenceSampleDump(_sequenceTest.currentTrial);
    if (summaryTrial) {
        printSequenceTrialResult(_sequenceTest.currentTrial, result, dtMs, durMs, strength, invalidAudioTrial, diagnostics.duplicateCount, diagnostics);
    } else {
        printSequenceTrialResult(finalizedReport);
    }
    if (analyzerLogEnabled(_sequenceTest.logFlags, AnalyzerApp::ANALYZER_LOG_EXPLAIN)) {
        printSequenceExplain(finalizedReport);
    }
    if (analyzerLogEnabled(_sequenceTest.logFlags, AnalyzerApp::ANALYZER_LOG_CUSTOM) &&
        !analyzerLogEnabled(_sequenceTest.logFlags, AnalyzerApp::ANALYZER_LOG_EXPLAIN)) {
        printSequenceAmpWindow(finalizedReport);
    }
    if (_sequenceTest.trialReports != nullptr) {
        const size_t reportIndex = static_cast<size_t>(_sequenceTest.currentTrial - 1UL);
        if (reportIndex < _sequenceTest.trialReportCapacity) {
            _sequenceTest.trialReports[reportIndex] = finalizedReport;
            const size_t storedCount = reportIndex + 1UL;
            if (storedCount > _sequenceTest.trialReportCount) {
                _sequenceTest.trialReportCount = storedCount;
            }
        }
    }

    Serial.flush();
    _sequenceTest.currentTrialFinalized = true;

    if (_sequenceTest.currentTrial >= _sequenceTest.totalTrials) {
        printSequenceFinalOutput();
        stopSequenceTest();
    }
}

static AnalyzerReason analyzerReasonFromSequenceOutcome(AnalyzerResult result,
                                                       long dtMs,
                                                       unsigned long rawCandidateCount,
                                                       AmpTransientDetector::TransientRejectReason strongestRejectReason,
                                                       bool audioOverflow) {
    if (audioOverflow) {
        return AnalyzerReason::InvalidAudio;
    }
    switch (result) {
        case AnalyzerResult::Expected:
        return AnalyzerReason::ValidPatternInExpectedWindow;
        case AnalyzerResult::Late:
        return AnalyzerReason::ValidPatternAfterWindow;
        case AnalyzerResult::Unexpected:
        return AnalyzerReason::UnexpectedValidPatternWithoutTrigger;
        case AnalyzerResult::Duplicate:
        return AnalyzerReason::DuplicatePatternAfterPrimary;
        case AnalyzerResult::InvalidAudio:
        return AnalyzerReason::InvalidAudio;
        case AnalyzerResult::Miss:
        if (rawCandidateCount == 0) {
            return AnalyzerReason::NoSignalCandidate;
        }
        switch (strongestRejectReason) {
            case AmpTransientDetector::TransientRejectReason::DurationTooShort:
            case AmpTransientDetector::TransientRejectReason::DurationTooLong:
            case AmpTransientDetector::TransientRejectReason::StrengthTooLow:
                return AnalyzerReason::SignalSeenButRejected;
            case AmpTransientDetector::TransientRejectReason::PeakStillActive:
                return AnalyzerReason::InspectionFailed;
            case AmpTransientDetector::TransientRejectReason::None:
            default:
                break;
        }
        return dtMs >= 0 ? AnalyzerReason::PatternCandidateRejected : AnalyzerReason::NoSignalCandidate;
        case AnalyzerResult::Rejected:
        return AnalyzerReason::PatternCandidateRejected;
        case AnalyzerResult::Ambiguous:
        return AnalyzerReason::MultipleCompetingPatterns;
        case AnalyzerResult::TooDense:
        return AnalyzerReason::FieldTooDense;
        case AnalyzerResult::Unknown:
        default:
            return AnalyzerReason::Unknown;
    }
}

static size_t analyzerReasonIndex(AnalyzerReason value) {
    return static_cast<size_t>(value);
}

const char* AnalyzerApp::activeAnalyzerProfileName() const {
    return detection::detectionProfileName(_sequenceTest.profileKind);
}

const char* analyzerProfileDetailNamespace(detection::DetectionProfileKind profileKind) {
    switch (profileKind) {
        case detection::DetectionProfileKind::Chirp:
            return "chirp";
        case detection::DetectionProfileKind::FreqAmp:
        default:
            return "freq_amp";
    }
}

const char* analyzerProfileDetailSummary(detection::DetectionProfileKind profileKind) {
    switch (profileKind) {
        case detection::DetectionProfileKind::Chirp:
            return "chirp profile view";
        case detection::DetectionProfileKind::FreqAmp:
        default:
            return "generic freq-amp profile view";
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
    const detection::InspectedSignal* runtimeInspectedSignal = actualPipelineAvailable && pipelineResult->hasInspectedSignal
        ? &pipelineResult->inspectedSignal
        : nullptr;
    const detection::FieldState* runtimeFieldState = actualPipelineAvailable && pipelineResult->hasField
        ? &pipelineResult->field
        : nullptr;
    const auto artifactReason = [&]() -> const char* {
        if (actualPipelineAvailable) {
            return "captured_from_runtime_pipeline";
        }
        return "missing_pipeline_result";
    }();

    report.classification.result = result;
    report.classification.reason = actualPipelineAvailable
        ? analyzerReasonFromSequenceOutcome(result, dtMs, diagnostics.rawCandidateCount, diagnostics.strongestRejectReason, audioOverflow)
        : AnalyzerReason::MissingPipelineResult;
    report.classification.dtMs = dtMs;
    report.classification.confidence = actualPipelineAvailable && runtimePatternResult != nullptr ? runtimePatternResult->confidence : 0.0f;
    {
        // Analyzer consumes the PatternResult produced by DetectionRuntime.
        // Analyzer does not re-run signal inspection or pattern interpretation.
        AnalyzerPatternObservation pattern = {};
        pattern.type = actualPipelineAvailable && runtimePatternResult != nullptr ? detection::patternTypeName(runtimePatternResult->type) : "unknown";
        pattern.accepted = actualPipelineAvailable && runtimePatternResult != nullptr
            ? runtimePatternResult->valid
            : false;
        pattern.candidateAccepted = actualPipelineAvailable && runtimePatternResult != nullptr ? runtimePatternResult->patternCandidateAccepted : false;
        pattern.patternMatched = actualPipelineAvailable && runtimePatternResult != nullptr ? runtimePatternResult->patternMatched : false;
        pattern.supportMatched = actualPipelineAvailable && runtimePatternResult != nullptr ? runtimePatternResult->supportMatched : false;
        pattern.behaviorEligible = pattern.accepted;
        pattern.confidence = actualPipelineAvailable && runtimePatternResult != nullptr ? runtimePatternResult->confidence : 0.0f;
        pattern.dtMs = report.classification.dtMs;
        pattern.ampSupport = actualPipelineAvailable && runtimePatternResult != nullptr ? ampSupportName(runtimePatternResult->ampSupport) : "unknown";
        pattern.reason = actualPipelineAvailable && runtimePatternResult != nullptr ? detection::patternReasonName(runtimePatternResult->reasonCode) : analyzerReasonName(report.classification.reason);
        pattern.rejectReason = actualPipelineAvailable && runtimePatternResult != nullptr ? detection::patternRejectReasonName(runtimePatternResult->rejectReason) : analyzerReasonName(report.classification.reason);
        pattern.involvedSignals = actualPipelineAvailable && runtimePatternResult != nullptr ? runtimePatternResult->signalCount : 0U;
        report.primaryPattern = pattern;
    }

    report.signals.total = diagnostics.rawCandidateCount;
    report.signals.accepted = actualPipelineAvailable && runtimePatternResult != nullptr && runtimePatternResult->valid ? 1U : 0U;
    report.signals.rejected = diagnostics.rawCandidateCount > report.signals.accepted ? diagnostics.rawCandidateCount - report.signals.accepted : 0U;
    report.signals.primarySource = actualPipelineAvailable && runtimeInspectedSignal != nullptr && runtimeInspectedSignal->signal.present
        ? signalSourceName(runtimeInspectedSignal->signal.source)
        : "unknown";
    report.signals.primaryDtMs = dtMs;
    report.signals.primaryDurationMs = durMs >= 0 ? static_cast<unsigned long>(durMs) : 0UL;
    report.signals.primaryStrength = strength;
    report.signals.primaryConfidence = actualPipelineAvailable && runtimePatternResult != nullptr ? runtimePatternResult->confidence : 0.0f;
    report.signals.mainRejectReason = actualPipelineAvailable && runtimeInspectedSignal != nullptr
        ? (runtimeInspectedSignal->rejected ? signalRejectReasonName(runtimeInspectedSignal->rejectReason) : "none")
        : analyzerReasonName(report.classification.reason);
    report.signals.duplicateRisk = duplicateCount > 0;

    report.inspection.inspected = diagnostics.rawCandidateCount;
    report.inspection.accepted = report.signals.accepted;
    report.inspection.rejected = diagnostics.rawCandidateCount > report.inspection.accepted ? diagnostics.rawCandidateCount - report.inspection.accepted : 0U;
    if (actualPipelineAvailable && runtimeInspectedSignal != nullptr && runtimeInspectedSignal->signal.present) {
        report.inspection.primaryEvidence = signalSourceName(runtimeInspectedSignal->signal.source);
        report.inspection.ampSupport = ampSupportName(runtimeInspectedSignal->ampSupport);
        report.inspection.supportClass = ampSupportName(runtimeInspectedSignal->ampSupport);
        report.inspection.mainRejectReason = runtimeInspectedSignal->rejected ? signalRejectReasonName(runtimeInspectedSignal->rejectReason) : "none";
    } else {
        report.inspection.primaryEvidence = "none";
        report.inspection.ampSupport = "unknown";
        report.inspection.supportClass = "unsupported";
        report.inspection.mainRejectReason = analyzerReasonName(report.classification.reason);
    }

    if (actualPipelineAvailable && runtimeFieldState != nullptr) {
        report.field.state = runtimeFieldState->dense ? "dense" : (runtimeFieldState->active ? (runtimeFieldState->quiet ? "quiet" : "active") : "unknown");
        report.field.rawActivity = runtimeFieldState->activity;
        report.field.validPatternActivity = runtimeFieldState->density;
        report.field.recentValidPatterns = runtimeFieldState->recentPatternCount;
        report.field.recentRejects = runtimeFieldState->recentSignalCount > runtimeFieldState->recentPatternCount
            ? runtimeFieldState->recentSignalCount - runtimeFieldState->recentPatternCount
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
    const detection::DetectionProfile& selectedProfile = detection::detectionProfileForKind(_sequenceTest.profileKind);
    report.profileDetail.emitter = detection::profileSignalEmitterName(selectedProfile.signalEmitter);
    report.profileDetail.inspectionRules = detection::profileInspectionRulesName(selectedProfile.inspectionRules);
    report.profileDetail.patternRules = detection::profilePatternRulesName(selectedProfile.patternRules);
    report.profileDetail.ampSupport = selectedProfile.patternRulesConfig.requireSupportForAcceptance ? "enabled" : "disabled";
    report.profileDetail.ampSupportMin = "medium";
    report.profileDetail.assembler = "single_pulse";
    report.profileDetail.requireSupportForAcceptance = selectedProfile.patternRulesConfig.requireSupportForAcceptance;
    report.profileDetail.freqScore = actualPipelineAvailable && runtimePatternResult != nullptr ? runtimePatternResult->freq.score : 0.0f;
    report.profileDetail.freqContrast = actualPipelineAvailable && runtimePatternResult != nullptr ? runtimePatternResult->freq.spectralContrast : 0.0f;
    report.profileDetail.freqScoreMin = _frequencyEvidenceTuning.scoreMin;
    report.profileDetail.freqContrastMin = _frequencyEvidenceTuning.contrastMin;
    report.profileDetail.ampLevel = report.signals.primaryStrength;
    report.profileDetail.ampBase = diagnostics.acceptedAmbientBaseline;
    report.profileDetail.ampLift = report.profileDetail.ampLevel - report.profileDetail.ampBase;
    report.profileDetail.ampSupport = report.primaryPattern.ampSupport;
    const detection::AmpWindowEvidence ampWindowEvidence = actualPipelineAvailable && runtimeInspectedSignal != nullptr
        ? runtimeInspectedSignal->ampWindow
        : detection::AmpWindowEvidence{};
    report.profileDetail.ampWindow.available = ampWindowEvidence.available;
    report.profileDetail.ampWindow.observedOnly = ampWindowEvidence.observedOnly;
    report.profileDetail.ampWindow.note = ampWindowEvidence.available
        ? "amp_window_seen"
        : (actualPipelineAvailable ? "inspector_no_amp_window" : "missing_pipeline_result");
    report.profileDetail.ampWindow.windowStartMs = ampWindowEvidence.windowStartMs;
    report.profileDetail.ampWindow.windowEndMs = ampWindowEvidence.windowEndMs;
    report.profileDetail.ampWindow.peak = ampWindowEvidence.peak;
    report.profileDetail.ampWindow.baseline = ampWindowEvidence.baseline;
    report.profileDetail.ampWindow.lift = ampWindowEvidence.lift;
    report.profileDetail.ampWindow.supportClass = ampSupportName(ampWindowEvidence.supportClass);

    report.debug.signals = diagnostics.rawCandidateCount;
    report.debug.inspected = diagnostics.rawCandidateCount;
    report.debug.patterns = diagnostics.transientAccepted ? 1U : 0U;
    report.debug.rejects = report.signals.rejected;
    report.debug.duplicates = duplicateCount;
    report.debug.unexpected = result == AnalyzerResult::Unexpected ? 1U : 0U;
    report.debug.artifactCaptured = actualPipelineAvailable;
    report.debug.artifactFallback = !actualPipelineAvailable;
    report.debug.artifactState = actualPipelineAvailable ? "CAPTURED" : "MISSING_PIPELINE";
    report.debug.artifactReason = artifactReason;
    report.debug.pipelineSource = actualPipelineAvailable ? "actual_pipeline" : "missing_runtime_pipeline";
    report.debug.pipelineFallback = !actualPipelineAvailable;
    report.debug.mainRejectReason = actualPipelineAvailable && runtimeInspectedSignal != nullptr
        ? (runtimeInspectedSignal->rejected ? signalRejectReasonName(runtimeInspectedSignal->rejectReason) : "none")
        : analyzerReasonName(report.classification.reason);

    return report;
}

