#include "AnalyzerApp.h"

#include <Arduino.h>
#include <stdlib.h>
#include <string.h>

#include "../../AudioDebugConfig.h"
#include "../../detection/DetectionPipeline.h"
#include "../../detection/FrequencyWindowProbe.h"

namespace {
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
           AnalyzerApp::ANALYZER_LOG_FREQ_CLASS |
           AnalyzerApp::ANALYZER_LOG_RAW_DEBUG;
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
               AnalyzerApp::ANALYZER_LOG_FREQ_CLASS |
               AnalyzerApp::ANALYZER_LOG_RAW_DEBUG;
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
        } else if (equalsIgnoreCase(part, "report") || equalsIgnoreCase(part, "freq_class") || equalsIgnoreCase(part, "freq")) {
            flags |= AnalyzerApp::ANALYZER_LOG_FREQ_CLASS;
        } else if (equalsIgnoreCase(part, "liveraw") || equalsIgnoreCase(part, "raw_debug") || equalsIgnoreCase(part, "raw")) {
            flags |= AnalyzerApp::ANALYZER_LOG_RAW_DEBUG;
        } else if (equalsIgnoreCase(part, "default")) {
            flags |= AnalyzerApp::DEFAULT_ANALYZER_LOG_FLAGS;
        } else if (equalsIgnoreCase(part, "full")) {
            flags |= AnalyzerApp::ANALYZER_LOG_SUMMARY |
                     AnalyzerApp::ANALYZER_LOG_TRIAL |
                     AnalyzerApp::ANALYZER_LOG_CANDIDATE |
                     AnalyzerApp::ANALYZER_LOG_FREQ_CLASS |
                     AnalyzerApp::ANALYZER_LOG_RAW_DEBUG;
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
    Serial.println("SEQ IN: [log=default|none|quiet|summary+trial+candidate+report+liveraw]");
    Serial.println("SEQ IN: [debug=0|1|2] [dumpSamples=0|1] [curveFormat=off|samples]");
    Serial.println("SEQ IN: [sampleFirst=N] [sampleEvery=N] [sampleLead=MS] [sampleTail=MS] [sampleStep=MS] [sampleMax=N]");
    Serial.println("SEQ OUT: SEQ start / SEQ running / SEQ_CAND / SEQ_REPORT / SEQ_TRIAL / SEQ_SUMMARY");
    Serial.println("SEQ OUT: candidate fields include onset_sample peak_sample release_sample peak_ms dur end_dt_ms freq_*");
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

void printH3FrequencyEvidenceFields(const DetectionPipeline::PatternResult& patternResult,
                                    const DetectionPipeline::FrequencyEvidence& frequencyEvidence,
                                    const DetectionPipeline::FrequencyEvidence* liveFrequencyEvidence,
                                    const char* candidateClass,
                                    long transientAgeOrDtMs,
                                    unsigned long referenceMs) {
    Serial.print(" candidate_class=");
    Serial.print(candidateClass);
    Serial.print(" pattern_valid=");
    Serial.print(patternResult.valid ? 1 : 0);
    Serial.print(" pattern_type=");
    Serial.print(DetectionPipeline::patternTypeName(patternResult.type));
    Serial.print(" pattern_reason=");
    Serial.print(DetectionPipeline::patternReasonName(patternResult.reasonCode));
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
    Serial.print(frequencyEvidence.present ? 1 : 0);
    Serial.print(" freq_matched=");
    Serial.print(frequencyEvidence.matched ? 1 : 0);
    Serial.print(" freq_score=");
    Serial.print(frequencyEvidence.score, 1);
    Serial.print(" freq_conf=");
    Serial.print(frequencyEvidence.confidence, 1);
    Serial.print(" freq_target_hz=");
    Serial.print(frequencyEvidence.targetHz);
    Serial.print(" freq_target_power=");
    Serial.print(frequencyEvidence.targetPower, 1);
    Serial.print(" freq_neighbor_power=");
    Serial.print(frequencyEvidence.neighborPower, 1);
    Serial.print(" freq_total_energy=");
    Serial.print(frequencyEvidence.totalEnergy, 1);
    Serial.print(" freq_contrast=");
    Serial.print(frequencyEvidence.spectralContrast, 2);
    Serial.print(" freq_observed_at_ms=");
    Serial.print(frequencyEvidence.observedAtMs);
    Serial.print(" freq_age_ms=");
    if (frequencyEvidence.observedAtMs > 0 && referenceMs >= frequencyEvidence.observedAtMs) {
        Serial.print(referenceMs - frequencyEvidence.observedAtMs);
        Serial.print("ms");
    } else {
        Serial.print("-");
    }
    Serial.print(" freq_valid_window=");
    Serial.print(frequencyEvidence.validWindow ? 1 : 0);
    Serial.print(" freqEarly[avail=");
    Serial.print(frequencyEvidence.windowAvailable ? 1 : 0);
    Serial.print(" score=");
    Serial.print(frequencyEvidence.score, 1);
    Serial.print(" target=");
    Serial.print(frequencyEvidence.targetHz);
    Serial.print(" contrast=");
    Serial.print(frequencyEvidence.spectralContrast, 2);
    Serial.print(" win=");
    Serial.print(frequencyEvidence.windowSampleCount);
    Serial.print("]");
    if (liveFrequencyEvidence != nullptr) {
        Serial.print(" liveFreq[avail=");
        Serial.print(liveFrequencyEvidence->present ? 1 : 0);
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

void printH3SequenceRoleFields(const char* role,
                               long dtMs,
                               long durMs,
                               float strength,
                               const DetectionPipeline::FrequencyEvidence& freq,
                               unsigned long observedAtMs,
                               unsigned long referenceMs,
                               const char* reason,
                               long deltaFromPrimaryMs) {
    Serial.print(" ");
    Serial.print(role);
    Serial.print("_dt_ms=");
    if (dtMs >= 0) {
        Serial.print(dtMs);
        Serial.print("ms");
    } else {
        Serial.print("-");
    }
    Serial.print(" ");
    Serial.print(role);
    Serial.print("_dur_ms=");
    if (durMs >= 0) {
        Serial.print(durMs);
        Serial.print("ms");
    } else {
        Serial.print("-");
    }
    Serial.print(" ");
    Serial.print(role);
    Serial.print("_strength=");
    Serial.print(strength, 1);
    Serial.print(" ");
    Serial.print(role);
    Serial.print("_freq_score=");
    Serial.print(freq.score, 1);
    Serial.print(" ");
    Serial.print(role);
    Serial.print("_freq_contrast=");
    Serial.print(freq.spectralContrast, 2);
    Serial.print(" ");
    Serial.print(role);
    Serial.print("_freq_present=");
    Serial.print(freq.present ? 1 : 0);
    Serial.print(" ");
    Serial.print(role);
    Serial.print("_freq_observed_at_ms=");
    Serial.print(observedAtMs);
    Serial.print(" ");
    Serial.print(role);
    Serial.print("_freq_age_ms=");
    if (observedAtMs > 0 && referenceMs >= observedAtMs) {
        Serial.print(referenceMs - observedAtMs);
        Serial.print("ms");
    } else {
        Serial.print("-");
    }
    Serial.print(" ");
    Serial.print(role);
    Serial.print("_freq_valid_window=");
    Serial.print(freq.validWindow ? 1 : 0);
    if (role != nullptr && strcmp(role, "duplicate") == 0) {
        Serial.print(" duplicate_delta_from_primary_ms=");
        if (deltaFromPrimaryMs >= 0) {
            Serial.print(deltaFromPrimaryMs);
            Serial.print("ms");
        } else {
            Serial.print("-");
        }
        Serial.print(" duplicate_reason=");
        Serial.print(reason != nullptr ? reason : "none");
    }
}
}

// -----------------------------------------------------------------------------
// Construction and setup
// -----------------------------------------------------------------------------

AnalyzerApp::AnalyzerApp(int inputPin, AudioSourceKind sourceKind)
    : _inputPin(inputPin),
      _analogSource(inputPin),
      _i2sSource(14, 27, 33, 16000, 32),
      _sourceKind(sourceKind),
      _audioSource(sourceKind == AudioSourceKind::I2S
                       ? static_cast<AudioSource&>(_i2sSource)
                       : static_cast<AudioSource&>(_analogSource)),
      _audioSignal(_audioSource),
      _audioFrequencyDetector(_audioSignal),
      _audioOnsetDetector() {}

void AnalyzerApp::begin() {
    beginEmitterControl();
    const unsigned long controlClaimSendMs = millis();
    sendEmitterCommand("MODE REMOTE");
    const bool emitterAcked = waitForEmitterAck("OK MODE REMOTE", 1500);
    if (!emitterAcked) {
        Serial.println("EVT analyzer_control_claim timeout");
    } else {
        Serial.print("EVT analyzer_control_claim acked_ms=");
        Serial.println(millis() - controlClaimSendMs);
    }

    delay(500);

    configureParameters();
    _audioSource.begin();
    _audioSignal.begin();
    _audioSignal.setCurveSampleCallback(&AnalyzerApp::sequenceCurveSampleCallback, this);
    _audioFrequencyDetector.begin();
    _audioFrequencyDetector.setDiagnosticsEnabled(false);
    _audioOnsetDetector.begin();
    _audioSignal.setDiagnosticsEnabled(AUDIO_VERBOSE_DEBUG);
    _audioOnsetDetector.setDiagnosticsEnabled(AUDIO_VERBOSE_DEBUG);
    _lastPrintMs = 0;
    _usbLineLength = 0;
    _usbLineBuffer[0] = '\0';
    _emitterLineLength = 0;
    _emitterLineBuffer[0] = '\0';
    _controlClaimPending = false;
    _controlClaimSent = true;
    _controlClaimAtMs = millis();

    Serial.println("EVT analyzer_ready");
    Serial.println("EVT analyzer_help type='HELP', 'BASE', 'TEST', 'RAW trigger f=2400 dur=100 post=1000 dump=bin', 'SEQ log=default|summary+trial|candidate|freq_class|raw dumpSamples=1 curveFormat=samples', 'CAP', 'DET AMP', 'VAL', 'VAL OFF'");
}

void AnalyzerApp::configureParameters() {
    configureSharedParameters();

    if (_sourceKind == AudioSourceKind::I2S) {
        configureI2SParameters();
    } else {
        configureAnalogParameters();
    }
}

void AnalyzerApp::configureSharedParameters() {
    _audioSignal.setSmoothingFactor(0.5f);
    _audioSignal.setBaselineUpdateFactor(0.005f);
}

void AnalyzerApp::configureAnalogParameters() {
    _audioSignal.setBaselineTrackingQuietThreshold(40);
    setDetectorOnsetDetectionThreshold(36.0f);
    setDetectorOnsetReleaseThreshold(26.0f);
    setDetectorCooldownAfterOnsetMs(50);
    setDetectorReleaseDebounceMs(10);
    setDetectorMinTransientDurationMs(90);
    setDetectorMaxTransientDurationMs(240);
    setDetectorMinTransientPeakStrength(40.0f);
}

void AnalyzerApp::configureI2SParameters() {
    _audioSignal.setBaselineTrackingQuietThreshold(20);
    setDetectorOnsetDetectionThreshold(36.0f);
    setDetectorOnsetReleaseThreshold(26.0f);
    setDetectorCooldownAfterOnsetMs(50);
    setDetectorReleaseDebounceMs(10);
    setDetectorMinTransientDurationMs(90);
    setDetectorMaxTransientDurationMs(240);
    setDetectorMinTransientPeakStrength(40.0f);
}

// -----------------------------------------------------------------------------
// Main runtime loop
// -----------------------------------------------------------------------------

void AnalyzerApp::update() {
    const unsigned long now = millis();

    if (_sequenceTest.active && _sequenceTest.currentTrial > 0 && _sourceKind == AudioSourceKind::Analog && !_audioSource.available()) {
        _sequenceTest.emptySourceLoops++;
    }

    int processedSamples = 0;
    if (_sourceKind == AudioSourceKind::I2S) {
        AudioBlock block;
        while (processedSamples < kMaxSamplesPerLoop && _i2sSource.readBlock(block)) {
            if (block.sampleCount == 0 || block.samples == nullptr) {
                break;
            }

            _audioSignal.processBlock(block);
            for (uint16_t i = 0; i < block.sampleCount; ++i) {
                const int centeredSample = static_cast<int>(block.samples[i] - _audioSignal.baseline());
                _audioFrequencyDetector.observeCenteredSample(centeredSample);
            }
            updateSequenceAmbientStats();
            noteSequenceTransientReject(_audioSignal.sampleTimeUs() / 1000UL);
            const unsigned long queueDepthBeforeDrain = static_cast<unsigned long>(_audioSignal.candidateQueueDepth());

            DetectorCandidate candidate;
            while (_audioSignal.popCandidate(candidate)) {
                DetectionPipeline::PatternResult patternResult;
                const auto liveFrequencyEvidence = captureFrequencyEvidence();
                DetectionPipeline::FrequencyEvidence frequencyEvidence = liveFrequencyEvidence;
                DetectionPipeline::FrequencyEvidence fullFrequencyEvidence = liveFrequencyEvidence;
                DetectionPipeline::measureCandidateWindowFrequency(
                    _audioSignal,
                    candidate,
                    _audioSource.sampleRateHz() > 0 ? _audioSource.sampleRateHz() : 16000UL,
                    _audioFrequencyDetector.targetFrequencyHz(),
                    now,
                    frequencyEvidence);
                DetectionPipeline::measureCandidateWindowFrequency(
                    _audioSignal,
                    candidate,
                    _audioSource.sampleRateHz() > 0 ? _audioSource.sampleRateHz() : 16000UL,
                    _audioFrequencyDetector.targetFrequencyHz(),
                    now,
                    fullFrequencyEvidence,
                    candidate.durationMs);
                if (!DetectionPipeline::processDetectorCandidate(candidate, patternResult, now, &frequencyEvidence)) {
                    continue;
                }
                patternResult.candidate.frequencyFull = fullFrequencyEvidence;

                if (_valMode) {
                    if (patternResult.valid) {
                        _valOnsetLatchedUntilMs = (patternResult.candidate.startMs) + 250UL;
                        _valTransientLatchedUntilMs = (patternResult.candidate.startMs) + 250UL;
                    }
                }

                if (_sequenceTest.active) {
                    handleSequenceCandidate(patternResult, queueDepthBeforeDrain, &liveFrequencyEvidence);
                }
            }

            processedSamples += static_cast<int>(block.sampleCount);
            if (processedSamples > kMaxSamplesPerLoop) {
                processedSamples = kMaxSamplesPerLoop;
            }
        }
    } else {
        while (processedSamples < kMaxSamplesPerLoop && _audioSource.available()) {
            int sample = 0;
            uint32_t sampleTimeUs = 0;
            if (!_audioSource.readSample(sample, sampleTimeUs)) {
                break;
            }
            const unsigned long sampleTimeMs = sampleTimeUs / 1000UL;
            _audioSignal.update(sample, sampleTimeUs);
            _audioFrequencyDetector.observeCenteredSample(_audioSignal.centeredSignal());
            _audioOnsetDetector.update(static_cast<float>(_audioSignal.signalMagnitude()), sampleTimeUs);
            updateSequenceAmbientStats();

            if (detectorOnsetDetected()) {
                _valOnsetLatchedUntilMs = sampleTimeMs + 250;
            }
            if (detectorTransientDetected()) {
                _valTransientLatchedUntilMs = sampleTimeMs + 250;
            }
            if (_sequenceTest.active && _sequenceTest.currentTrial > 0) {
                if (detectorOnsetDetected()) {
                    _sequenceTest.currentTrialDiagnostics.onsetSeen = true;
                    if (_sequenceTest.currentTrialDiagnostics.firstOnsetMs == 0) {
                        _sequenceTest.currentTrialDiagnostics.firstOnsetMs = sampleTimeMs;
                    }
                    _sequenceTest.currentTrialDiagnostics.lastOnsetMs = sampleTimeMs;
                    if (_sequenceTest.currentTrialOnsetDetectedMs == 0) {
                        _sequenceTest.currentTrialOnsetDetectedMs = sampleTimeMs;
                    }
                } else {
                    const char* onsetRejectReason = detectorOnsetRejectReasonName();
                    if (strcmp(onsetRejectReason, "below_threshold") == 0) {
                        _sequenceTest.currentTrialDiagnostics.onsetRejectBelowThreshold++;
                    } else if (strcmp(onsetRejectReason, "peak_active") == 0) {
                        _sequenceTest.currentTrialDiagnostics.onsetRejectPeakActive++;
                    } else if (strcmp(onsetRejectReason, "cooldown_active") == 0) {
                        _sequenceTest.currentTrialDiagnostics.onsetRejectCooldown++;
                    } else if (strcmp(onsetRejectReason, "none") != 0) {
                        _sequenceTest.currentTrialDiagnostics.onsetRejectOther++;
                    }

                    if (strcmp(onsetRejectReason, "none") != 0) {
                        if (_sequenceTest.currentTrialDiagnostics.firstOnsetRejectMs == 0) {
                            _sequenceTest.currentTrialDiagnostics.firstOnsetRejectMs = sampleTimeMs;
                        }
                        _sequenceTest.currentTrialDiagnostics.lastOnsetRejectMs = sampleTimeMs;
                    }
                }
            }

            if (_sequenceTest.active && detectorTransientDetected()) {
                handleSequenceTransient(sampleTimeMs);
            } else {
                noteSequenceTransientReject(sampleTimeMs);
            }

            processedSamples++;
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
    _audioSignal.resetDetectorState();
    _audioOnsetDetector.resetState();
}

bool AnalyzerApp::detectorOnsetDetected() const {
    return _sourceKind == AudioSourceKind::I2S ? _audioSignal.onsetDetected() : _audioOnsetDetector.onsetDetected();
}

float AnalyzerApp::detectorOnsetStrength() const {
    return _sourceKind == AudioSourceKind::I2S ? _audioSignal.onsetStrength() : _audioOnsetDetector.onsetStrength();
}

bool AnalyzerApp::detectorTransientDetected() const {
    return _sourceKind == AudioSourceKind::I2S ? _audioSignal.transientDetected() : _audioOnsetDetector.transientDetected();
}

float AnalyzerApp::detectorTransientStrength() const {
    return _sourceKind == AudioSourceKind::I2S ? _audioSignal.transientStrength() : _audioOnsetDetector.transientStrength();
}

unsigned long AnalyzerApp::detectorTransientDurationMs() const {
    return _sourceKind == AudioSourceKind::I2S ? _audioSignal.transientDurationMs() : _audioOnsetDetector.transientDurationMs();
}

bool AnalyzerApp::detectorTransientPeakActive() const {
    return _sourceKind == AudioSourceKind::I2S ? _audioSignal.peakActive() : _audioOnsetDetector.peakActive();
}

const char* AnalyzerApp::detectorOnsetRejectReasonName() const {
    return _sourceKind == AudioSourceKind::I2S ? _audioSignal.lastOnsetRejectReasonName() : _audioOnsetDetector.lastOnsetRejectReasonName();
}

const char* AnalyzerApp::detectorTransientRejectReasonName() const {
    return _sourceKind == AudioSourceKind::I2S ? _audioSignal.lastTransientRejectReasonName() : _audioOnsetDetector.lastTransientRejectReasonName();
}

unsigned long AnalyzerApp::detectorTransientRejectedDurationMs() const {
    return _sourceKind == AudioSourceKind::I2S ? _audioSignal.lastTransientRejectedDurationMs() : _audioOnsetDetector.lastTransientRejectedDurationMs();
}

float AnalyzerApp::detectorTransientRejectedStrength() const {
    return _sourceKind == AudioSourceKind::I2S ? _audioSignal.lastTransientRejectedStrength() : _audioOnsetDetector.lastTransientRejectedStrength();
}

float AnalyzerApp::detectorOnsetDetectionThreshold() const {
    return _sourceKind == AudioSourceKind::I2S ? _audioSignal.onsetDetectionThreshold() : _audioOnsetDetector.onsetDetectionThreshold();
}

float AnalyzerApp::detectorOnsetReleaseThreshold() const {
    return _sourceKind == AudioSourceKind::I2S ? _audioSignal.onsetReleaseThreshold() : _audioOnsetDetector.onsetReleaseThreshold();
}

unsigned long AnalyzerApp::detectorCooldownAfterOnsetMs() const {
    return _sourceKind == AudioSourceKind::I2S ? _audioSignal.cooldownAfterOnsetMs() : _audioOnsetDetector.cooldownAfterOnsetMs();
}

unsigned long AnalyzerApp::detectorMinTransientDurationMs() const {
    return _sourceKind == AudioSourceKind::I2S ? _audioSignal.minTransientDurationMs() : _audioOnsetDetector.minTransientDurationMs();
}

unsigned long AnalyzerApp::detectorMaxTransientDurationMs() const {
    return _sourceKind == AudioSourceKind::I2S ? _audioSignal.maxTransientDurationMs() : _audioOnsetDetector.maxTransientDurationMs();
}

float AnalyzerApp::detectorMinTransientPeakStrength() const {
    return _sourceKind == AudioSourceKind::I2S ? _audioSignal.minTransientPeakStrength() : _audioOnsetDetector.minTransientPeakStrength();
}

unsigned long AnalyzerApp::detectorReleaseDebounceMs() const {
    return _sourceKind == AudioSourceKind::I2S ? _audioSignal.releaseDebounceMs() : _audioOnsetDetector.releaseDebounceMs();
}

void AnalyzerApp::setDetectorOnsetDetectionThreshold(float value) {
    _audioSignal.setOnsetDetectionThreshold(value);
    _audioOnsetDetector.setOnsetDetectionThreshold(value);
}

void AnalyzerApp::setDetectorOnsetReleaseThreshold(float value) {
    _audioSignal.setOnsetReleaseThreshold(value);
    _audioOnsetDetector.setOnsetReleaseThreshold(value);
}

void AnalyzerApp::setDetectorCooldownAfterOnsetMs(unsigned long value) {
    _audioSignal.setCooldownAfterOnsetMs(value);
    _audioOnsetDetector.setCooldownAfterOnsetMs(value);
}

void AnalyzerApp::setDetectorMinTransientDurationMs(unsigned long value) {
    _audioSignal.setMinTransientDurationMs(value);
    _audioOnsetDetector.setMinTransientDurationMs(value);
}

void AnalyzerApp::setDetectorMaxTransientDurationMs(unsigned long value) {
    _audioSignal.setMaxTransientDurationMs(value);
    _audioOnsetDetector.setMaxTransientDurationMs(value);
}

void AnalyzerApp::setDetectorMinTransientPeakStrength(float value) {
    _audioSignal.setMinTransientPeakStrength(value);
    _audioOnsetDetector.setMinTransientPeakStrength(value);
}

void AnalyzerApp::setDetectorReleaseDebounceMs(unsigned long value) {
    _audioSignal.setReleaseDebounceMs(value);
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
// Console and emitter I/O
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
        Serial.println("CMD: EMIT CHIRP freq=3200 dur=100");
        Serial.println("CMD: EMIT MODE REMOTE");
        Serial.println("CMD: EMIT MODE AUTO interval=2000 freq=3200 dur=100");
        Serial.println("CMD: EMIT SWEEP start=3000 stop=3500 step=100 dur=80 pause=1000");
        Serial.println("CMD: TEST");
        Serial.println("CMD: raw trigger f=2400 dur=100 post=1000 dump=bin");
        Serial.println("CMD: SEQ");
        Serial.println("CMD: SEQ help");
        Serial.println("CMD: SEQ stop");
        Serial.println("CMD: CAP");
        Serial.println("CMD: CAP stop");
        Serial.println("CMD: VAL");
        Serial.println("CMD: VAL OFF");
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
        sendEmitterCommand("CHIRP freq=3200 dur=100");
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
            Serial.println("RAW_ERR usage=raw trigger f=2400 dur=100 post=1000");
            return;
        }

        unsigned long toneHz = 2400;
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
        unsigned long periodMs = 2500;
        unsigned long windowEndOffsetMs = 2200;
        unsigned long toneHz = 3200;
        unsigned long durationMs = 100;
        uint32_t logFlags = AnalyzerApp::DEFAULT_ANALYZER_LOG_FLAGS;
        const char* setupLabel = nullptr;
        bool sampleDumpEnabled = false;
        unsigned long sampleDumpFirstTrials = 2;
        unsigned long sampleDumpEveryNth = 0;
        unsigned long sampleDumpLeadMs = 50;
        unsigned long sampleDumpTailMs = 800;
        unsigned long sampleDumpStepMs = 1;
        unsigned long sampleDumpMaxRows = 5000;

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
            }
            token = strtok_r(nullptr, " ", &savePtr);
        }

        startSequenceTest(totalTrials, periodMs, windowEndOffsetMs, toneHz, durationMs, false, true, setupLabel, logFlags, sampleDumpEnabled, sampleDumpFirstTrials, sampleDumpEveryNth, sampleDumpLeadMs, sampleDumpTailMs, sampleDumpStepMs, sampleDumpMaxRows);
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
        unsigned long toneHz = 3200;
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

void AnalyzerApp::runRawTrigger(unsigned long toneHz, unsigned long durationMs, unsigned long postMs, unsigned long preMs, unsigned long decim, bool dumpChunks, bool dumpBinary) {
    if (_valMode) {
        return;
    }

    if (_sourceKind != AudioSourceKind::I2S) {
        Serial.println("RAW_ERR source=analog unsupported");
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
    Serial.print(_sourceKind == AudioSourceKind::I2S ? "I2S" : "Analog");
    Serial.println(" detector=AMP");
    printDetectionParameters();
}

// -----------------------------------------------------------------------------
// Sequence test, capture, and tuning sessions
// -----------------------------------------------------------------------------

void AnalyzerApp::startSequenceTest(unsigned long totalTrials, unsigned long periodMs, unsigned long windowEndOffsetMs, unsigned long toneHz, unsigned long durationMs, bool quiet, bool showDetails, const char* setupLabel, uint32_t logFlags, bool sampleDumpEnabled, unsigned long sampleDumpFirstTrials, unsigned long sampleDumpEveryNth, unsigned long sampleDumpLeadMs, unsigned long sampleDumpTailMs, unsigned long sampleDumpStepMs, unsigned long sampleDumpMaxRows) {
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

    const bool wantVerboseTrialReports =
        analyzerLogEnabled(logFlags, AnalyzerApp::ANALYZER_LOG_FREQ_CLASS) ||
        analyzerLogEnabled(logFlags, AnalyzerApp::ANALYZER_LOG_RAW_DEBUG);
    if (wantVerboseTrialReports) {
        const size_t desiredCapacity = static_cast<size_t>(totalTrials < SequenceTest::kMaxTrialReports ? totalTrials : SequenceTest::kMaxTrialReports);
        if (desiredCapacity > 0) {
            _sequenceTest.trialReports = static_cast<SequenceTest::TrialReport*>(calloc(desiredCapacity, sizeof(SequenceTest::TrialReport)));
            if (_sequenceTest.trialReports != nullptr) {
                _sequenceTest.trialReportCapacity = desiredCapacity;
            } else {
                Serial.print("SEQ_VERBOSE_WARN reason=trial_report_alloc_failed requested=");
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
        Serial.print(_sourceKind == AudioSourceKind::I2S ? "I2S" : "Analog");
        Serial.print(" detector=AMP");
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
            Serial.println("SEQ running");
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
    _captureSession.currentTrial = trialNumber;
    _captureSession.currentTrialStartMs = now;
    _captureSession.currentTrialEndMs = now + _captureSession.windowEndOffsetMs;
    _captureSession.currentTrialFinalized = false;
    _captureSession.nextTriggerAtMs = now + _captureSession.periodMs;

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
    _sequenceTest.currentTrialStartMs = now;
    _sequenceTest.currentTrialEndMs = now + _sequenceTest.windowEndOffsetMs;
    _sequenceTest.currentTrialOnsetDetectedMs = 0;
    _sequenceTest.currentTrialTransientDetectedMs = 0;
    _sequenceTest.currentTrialHit = false;
    _sequenceTest.currentTrialFinalized = false;
    _sequenceTest.currentTrialUnexpected = 0;
    _sequenceTest.trialHadAudioOverflow = false;
    _sequenceTest.trialOverflowCountAtStart = _audioSource.stats().overflowCount;
    _sequenceTest.trialTransientRejectTooShortCountAtStart = _sourceKind == AudioSourceKind::I2S ? _audioSignal.transientRejectedDurationTooShortCount() : _audioOnsetDetector.transientRejectedDurationTooShortCount();
    _sequenceTest.trialTransientRejectTooLongCountAtStart = _sourceKind == AudioSourceKind::I2S ? _audioSignal.transientRejectedDurationTooLongCount() : _audioOnsetDetector.transientRejectedDurationTooLongCount();
    _sequenceTest.trialTransientRejectWeakCountAtStart = _sourceKind == AudioSourceKind::I2S ? _audioSignal.transientRejectedStrengthTooLowCount() : _audioOnsetDetector.transientRejectedStrengthTooLowCount();
    _sequenceTest.currentTrialDiagnostics = {};
    _sequenceTest.currentTrialDiagnostics.acceptedAmbientBaseline = _audioSignal.baseline();
    _sequenceTest.currentTrialDiagnostics.strongestRejectReason = AudioOnsetDetector::TransientRejectReason::None;
    _sequenceTest.currentTrialDiagnostics.strongestRejectDtFromTriggerMs = -1;
    _sequenceTest.currentTrialDiagnostics.strongestRejectDurationMs = 0;
    _sequenceTest.currentTrialDiagnostics.strongestRejectStrength = 0.0f;
    _sequenceTest.nextTriggerAtMs = now + _sequenceTest.periodMs;

    beginSequenceSampleDump(trialNumber);

    char command[64];
    snprintf(command, sizeof(command), "CHIRP freq=%lu dur=%lu", _sequenceTest.toneHz, _sequenceTest.durationMs);
    sendEmitterCommand(command);
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
            _sequenceTest.currentTrialDiagnostics.duplicateFrequencyEvidence = captureFrequencyEvidence();
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
    _sequenceTest.currentTrialDiagnostics.lastTransientRejectReason = AudioOnsetDetector::TransientRejectReason::None;
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

DetectionPipeline::FrequencyEvidence AnalyzerApp::captureFrequencyEvidence() const {
    DetectionPipeline::FrequencyEvidence evidence;
    evidence.observedAtMs = millis();
    const float totalEnergy = _audioFrequencyDetector.lastTotalEnergy();
    const bool present = totalEnergy > 0.0f;

    evidence.present = present;
    evidence.matched = false;
    evidence.targetHz = present ? _audioFrequencyDetector.targetFrequencyHz() : 0;
    evidence.score = _audioFrequencyDetector.lastFrequencyScore();
    evidence.confidence = 0.0f;
    evidence.targetPower = _audioFrequencyDetector.lastTargetPower();
    evidence.neighborPower = _audioFrequencyDetector.lastNeighborPower();
    evidence.totalEnergy = totalEnergy;
    evidence.spectralContrast = _audioFrequencyDetector.lastSpectralContrast();
    evidence.validWindow = present;
    return evidence;
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
    const unsigned long shortCount = (_sourceKind == AudioSourceKind::I2S ? _audioSignal.transientRejectedDurationTooShortCount() : _audioOnsetDetector.transientRejectedDurationTooShortCount()) - _sequenceTest.trialTransientRejectTooShortCountAtStart;
    const unsigned long longCount = (_sourceKind == AudioSourceKind::I2S ? _audioSignal.transientRejectedDurationTooLongCount() : _audioOnsetDetector.transientRejectedDurationTooLongCount()) - _sequenceTest.trialTransientRejectTooLongCountAtStart;
    const unsigned long weakCount = (_sourceKind == AudioSourceKind::I2S ? _audioSignal.transientRejectedStrengthTooLowCount() : _audioOnsetDetector.transientRejectedStrengthTooLowCount()) - _sequenceTest.trialTransientRejectWeakCountAtStart;

    diagnostics.transientRejectTooShortCount = shortCount;
    diagnostics.transientRejectTooLongCount = longCount;
    diagnostics.transientRejectWeakCount = weakCount;

    const AudioOnsetDetector::TransientRejectReason reason =
        strcmp(reasonName, "duration_too_short") == 0 ? AudioOnsetDetector::TransientRejectReason::DurationTooShort :
        strcmp(reasonName, "duration_too_long") == 0 ? AudioOnsetDetector::TransientRejectReason::DurationTooLong :
        strcmp(reasonName, "strength_too_low") == 0 ? AudioOnsetDetector::TransientRejectReason::StrengthTooLow :
        strcmp(reasonName, "peak_still_active") == 0 ? AudioOnsetDetector::TransientRejectReason::PeakStillActive :
        AudioOnsetDetector::TransientRejectReason::None;

    if (reason != AudioOnsetDetector::TransientRejectReason::None && strength >= diagnostics.strongestRejectStrength) {
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
            case AudioOnsetDetector::TransientRejectReason::DurationTooLong:
                return "miss_too_long";
            case AudioOnsetDetector::TransientRejectReason::StrengthTooLow:
                return "miss_weak";
            case AudioOnsetDetector::TransientRejectReason::None:
            case AudioOnsetDetector::TransientRejectReason::DurationTooShort:
            case AudioOnsetDetector::TransientRejectReason::PeakStillActive:
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

void AnalyzerApp::handleSequenceCandidate(const DetectionPipeline::PatternResult& patternResult, unsigned long queueDepthBeforeDrain, const DetectionPipeline::FrequencyEvidence* liveFrequencyEvidence) {
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
    const long dtFromTriggerMs = static_cast<long>(onsetMs) - static_cast<long>(_sequenceTest.currentTrialStartMs);
    const long dtFromTrialStartMs = static_cast<long>(onsetMs) - static_cast<long>(_sequenceTest.currentTrialScheduledAtMs);
    const long processLagMs = patternResult.processedAtMs >= onsetMs
        ? static_cast<long>(patternResult.processedAtMs - onsetMs)
        : -1;

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

    if (!diagnostics.bestCandidateValid || candidate.peakStrength > diagnostics.bestCandidateStrength) {
        diagnostics.bestCandidateValid = true;
        diagnostics.bestCandidateDtFromTriggerMs = dtFromTriggerMs;
        diagnostics.bestCandidateDurationMs = candidate.durationMs;
        diagnostics.bestCandidateStrength = candidate.peakStrength;
        diagnostics.bestCandidateOrigin = origin;
    }

    if (analyzerLogEnabled(_sequenceTest.logFlags, AnalyzerApp::ANALYZER_LOG_CANDIDATE) && !_sequenceTest.quiet) {
        const uint32_t sampleRateHz = _audioSource.sampleRateHz() > 0 ? _audioSource.sampleRateHz() : 16000UL;
        const unsigned long peakOffsetMs = candidate.peakSample >= candidate.onsetSample
            ? static_cast<unsigned long>(((candidate.peakSample - candidate.onsetSample) * 1000ULL) / static_cast<uint64_t>(sampleRateHz))
            : 0UL;
        Serial.print("SEQ_CAND role=detector trial=");
        Serial.print(_sequenceTest.currentTrial);
        Serial.print(" idx=");
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
        Serial.print(" queue_before=");
        Serial.print(queueDepthBeforeDrain);
        Serial.print(" strength=");
        Serial.print(candidate.peakStrength, 1);
        Serial.print(" transient_present=");
        Serial.print(patternResult.candidate.transient.present ? 1 : 0);
        Serial.print(" freq_present=");
        Serial.print(patternResult.candidate.frequency.present ? 1 : 0);
        Serial.print(" freq_matched=");
        Serial.print(patternResult.candidate.frequency.matched ? 1 : 0);
        Serial.print(" freq_score=");
        Serial.print(patternResult.candidate.frequency.score, 1);
        Serial.print(" freq_conf=");
        Serial.print(patternResult.candidate.frequency.confidence, 1);
        Serial.print(" freq_target_hz=");
        Serial.print(patternResult.candidate.frequency.targetHz);
        Serial.print(" freq_contrast=");
        Serial.print(patternResult.candidate.frequency.spectralContrast, 1);
        printH3FrequencyEvidenceFields(patternResult, patternResult.candidate.frequency, liveFrequencyEvidence, candidateClass, dtFromTriggerMs, patternResult.processedAtMs);
        Serial.println(" source=detector");

        Serial.print("SEQ_CAND role=pattern trial=");
        Serial.print(_sequenceTest.currentTrial);
        Serial.print(" idx=");
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
        Serial.print(" queue_before=");
        Serial.print(queueDepthBeforeDrain);
        Serial.print(" strength=");
        Serial.print(candidate.peakStrength, 1);
        Serial.print(" transient_present=");
        Serial.print(patternResult.candidate.transient.present ? 1 : 0);
        Serial.print(" freq_present=");
        Serial.print(patternResult.candidate.frequency.present ? 1 : 0);
        Serial.print(" freq_matched=");
        Serial.print(patternResult.candidate.frequency.matched ? 1 : 0);
        Serial.print(" freq_score=");
        Serial.print(patternResult.candidate.frequency.score, 1);
        Serial.print(" freq_conf=");
        Serial.print(patternResult.candidate.frequency.confidence, 1);
        Serial.print(" freq_target_hz=");
        Serial.print(patternResult.candidate.frequency.targetHz);
        Serial.print(" freq_contrast=");
        Serial.print(patternResult.candidate.frequency.spectralContrast, 1);
        printH3FrequencyEvidenceFields(patternResult, patternResult.candidate.frequency, liveFrequencyEvidence, candidateClass, dtFromTriggerMs, patternResult.processedAtMs);
        Serial.println(" source=pattern");
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
            diagnostics.duplicateFrequencyEvidence = patternResult.candidate.frequency;
            diagnostics.duplicateFrequencyEvidenceFull = patternResult.candidate.frequencyFull;
            diagnostics.duplicateFrequencyProcessedAtMs = patternResult.processedAtMs;
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
    _sequenceTest.currentTrialDiagnostics.acceptedAmbientBaseline = candidate.ambientBaseline;
    _sequenceTest.currentTrialDiagnostics.acceptedFrequencyEvidence = patternResult.candidate.frequency;
    _sequenceTest.currentTrialDiagnostics.acceptedFrequencyEvidenceFull = patternResult.candidate.frequencyFull;
    _sequenceTest.currentTrialDiagnostics.acceptedFrequencyProcessedAtMs = patternResult.processedAtMs;
    _sequenceTest.currentTrialDiagnostics.lastTransientRejectReason = AudioOnsetDetector::TransientRejectReason::None;
    _sequenceTest.currentTrialDiagnostics.lastRejectStrength = 0.0f;
    _sequenceTest.currentTrialDiagnostics.lastRejectDurationMs = 0;
    _sequenceTest.currentTrialTransientDetectedMs = onsetMs;

    _sequenceTest.currentTrialHit = true;

    if (analyzerLogEnabled(_sequenceTest.logFlags, AnalyzerApp::ANALYZER_LOG_CANDIDATE) && !_sequenceTest.quiet) {
        const uint32_t sampleRateHz = _audioSource.sampleRateHz() > 0 ? _audioSource.sampleRateHz() : 16000UL;
        const unsigned long peakOffsetMs = candidate.peakSample >= candidate.onsetSample
            ? static_cast<unsigned long>(((candidate.peakSample - candidate.onsetSample) * 1000ULL) / static_cast<uint64_t>(sampleRateHz))
            : 0UL;
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
        Serial.print(" queue_before=");
        Serial.print(queueDepthBeforeDrain);
        Serial.print(" strength=");
        Serial.print(candidate.peakStrength, 1);
        Serial.print(" transient_present=");
        Serial.print(patternResult.candidate.transient.present ? 1 : 0);
        Serial.print(" freq_present=");
        Serial.print(patternResult.candidate.frequency.present ? 1 : 0);
        Serial.print(" freq_matched=");
        Serial.print(patternResult.candidate.frequency.matched ? 1 : 0);
        Serial.print(" freq_score=");
        Serial.print(patternResult.candidate.frequency.score, 1);
        Serial.print(" freq_conf=");
        Serial.print(patternResult.candidate.frequency.confidence, 1);
        Serial.print(" freq_target_hz=");
        Serial.print(patternResult.candidate.frequency.targetHz);
        Serial.print(" freq_contrast=");
        Serial.print(patternResult.candidate.frequency.spectralContrast, 1);
        Serial.print(" reason=");
        Serial.print(DetectionPipeline::patternReasonName(patternResult.reasonCode));
        printH3FrequencyEvidenceFields(patternResult, patternResult.candidate.frequency, liveFrequencyEvidence, candidateClass, dtFromTriggerMs, patternResult.processedAtMs);
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
        diagnostics.lastTransientRejectReason = AudioOnsetDetector::TransientRejectReason::DurationTooShort;
    } else if (strcmp(transientRejectReason, "duration_too_long") == 0) {
        diagnostics.lastTransientRejectReason = AudioOnsetDetector::TransientRejectReason::DurationTooLong;
    } else if (strcmp(transientRejectReason, "strength_too_low") == 0) {
        diagnostics.lastTransientRejectReason = AudioOnsetDetector::TransientRejectReason::StrengthTooLow;
    } else if (strcmp(transientRejectReason, "peak_still_active") == 0) {
        diagnostics.lastTransientRejectReason = AudioOnsetDetector::TransientRejectReason::PeakStillActive;
    } else {
        diagnostics.lastTransientRejectReason = AudioOnsetDetector::TransientRejectReason::None;
    }
    diagnostics.lastRejectDurationMs = detectorTransientRejectedDurationMs();
    diagnostics.lastRejectStrength = detectorTransientRejectedStrength();

    const bool invalidAudioTrial = _sequenceTest.trialHadAudioOverflow
                                   || _audioSource.stats().overflowCount != _sequenceTest.trialOverflowCountAtStart;
    const bool unexpectedTrial = !invalidAudioTrial && _sequenceTest.currentTrialUnexpected > 0;
    const bool hitTrial = !invalidAudioTrial && _sequenceTest.currentTrialHit;

    const char* result = "miss";
    long dtMs = -1;
    long durMs = -1;
    float strength = 0.0f;

    if (invalidAudioTrial) {
        _sequenceTest.invalidAudio++;
        result = "invalid_audio";
    } else if (unexpectedTrial) {
        _sequenceTest.unexpected++;
        result = "unexpected";
    } else if (hitTrial) {
        _sequenceTest.hits++;
        dtMs = static_cast<long>(diagnostics.acceptedTransientMs - _sequenceTest.currentTrialStartMs);
        durMs = static_cast<long>(diagnostics.acceptedTransientDurationMs);
        strength = diagnostics.acceptedTransientStrength;
        if (dtMs >= kLateOnsetMinMs) {
            result = "late";
            _sequenceTest.lateHits++;
        } else {
            result = "expected";
            _sequenceTest.expectedHits++;
        }
        _sequenceTest.totalHitStrengthScaled += static_cast<unsigned long>(diagnostics.acceptedTransientStrength * 100.0f);
        _sequenceTest.totalHitDurationMs += diagnostics.acceptedTransientDurationMs;
    } else {
        _sequenceTest.misses++;
    }

    _sequenceTest.duplicates += diagnostics.duplicateCount;
    flushSequenceSampleHistory(now + 1UL);
    printSequenceSampleDump(_sequenceTest.currentTrial);
    printSequenceTrialResult(_sequenceTest.currentTrial, result, dtMs, durMs, strength, invalidAudioTrial, diagnostics.duplicateCount, diagnostics);

    Serial.flush();
    _sequenceTest.currentTrialFinalized = true;

    if (_sequenceTest.currentTrial >= _sequenceTest.totalTrials) {
        printSequenceFinalOutput();
        stopSequenceTest();
    }
}

void AnalyzerApp::printSequenceTrialDebug(unsigned long trialNumber, const char* result, const SequenceTest::TrialDiagnostics& diagnostics) const {
    if (_valMode) {
        return;
    }

    const long acceptedDtMs = diagnostics.transientAccepted
        ? static_cast<long>(diagnostics.acceptedTransientMs) - static_cast<long>(_sequenceTest.currentTrialStartMs)
        : -1;
    const float ambientBaselineAvg = diagnostics.ambientBaselineSamples > 0
        ? diagnostics.ambientBaselineSum / static_cast<float>(diagnostics.ambientBaselineSamples)
        : 0.0f;
    const char* strongestRejectReasonName = "none";
    switch (diagnostics.strongestRejectReason) {
        case AudioOnsetDetector::TransientRejectReason::None:
            strongestRejectReasonName = "none";
            break;
        case AudioOnsetDetector::TransientRejectReason::DurationTooShort:
            strongestRejectReasonName = "too_short";
            break;
        case AudioOnsetDetector::TransientRejectReason::DurationTooLong:
            strongestRejectReasonName = "too_long";
            break;
        case AudioOnsetDetector::TransientRejectReason::StrengthTooLow:
            strongestRejectReasonName = "weak";
            break;
        case AudioOnsetDetector::TransientRejectReason::PeakStillActive:
            strongestRejectReasonName = "peak_active";
            break;
    }

    const bool isMiss = strcmp(result, "miss") == 0;
    const bool isLate = strcmp(result, "late") == 0;
    const bool isUnexpected = strcmp(result, "unexpected") == 0;
    const bool hasDuplicates = diagnostics.duplicateCount > 0;
    const bool expectedDtSlow = strcmp(result, "expected") == 0 && acceptedDtMs >= kLateOnsetMinMs;
    const bool expectedDurLong = strcmp(result, "expected") == 0 && diagnostics.acceptedTransientDurationMs >= kSmearedDurationMinMs;
    const unsigned long totalRejects = diagnostics.transientRejectTooShortCount + diagnostics.transientRejectTooLongCount + diagnostics.transientRejectWeakCount;

    if (!(isMiss || isLate || isUnexpected || hasDuplicates || expectedDtSlow || expectedDurLong)) {
        return;
    }

    const char* classification = sequenceTrialClassificationName(result, acceptedDtMs, diagnostics.acceptedTransientDurationMs, diagnostics);

    const char* candidateClass = h3SequenceCandidateClassFromResult(result);
    const auto& freq = diagnostics.acceptedFrequencyEvidence;
    const bool validPattern = strcmp(result, "miss") != 0 && strcmp(result, "invalid_audio") != 0;
    const unsigned long freqAgeMs = freq.observedAtMs > 0 && diagnostics.acceptedFrequencyProcessedAtMs >= freq.observedAtMs
        ? diagnostics.acceptedFrequencyProcessedAtMs - freq.observedAtMs
        : 0;

    Serial.print("SEQ_FREQ_CLASS trial=");
    Serial.print(trialNumber);
    Serial.print(" result=");
    Serial.print(result);
    Serial.print(" class=");
    Serial.print(classification);
    Serial.print(" candidate_class=");
    Serial.print(candidateClass);
    Serial.print(" pattern_valid=");
    Serial.print(validPattern ? 1 : 0);
    Serial.print(" pattern_type=");
    Serial.print(validPattern ? "valid_transient" : "invalid");
    Serial.print(" pattern_reason=");
    Serial.print(validPattern ? "from_accepted_transient" : "detector_rejected");
    Serial.print(" transient_duration_ms=");
    Serial.print(diagnostics.acceptedTransientDurationMs);
    Serial.print(" transient_peak_strength=");
    Serial.print(diagnostics.acceptedTransientStrength, 1);
    Serial.print(" transient_age_or_dt_ms=");
    if (diagnostics.transientAccepted && diagnostics.acceptedTransientMs >= _sequenceTest.currentTrialStartMs) {
        Serial.print(static_cast<long>(diagnostics.acceptedTransientMs) - static_cast<long>(_sequenceTest.currentTrialStartMs));
        Serial.print("ms");
    } else {
        Serial.print("-");
    }
    Serial.print(" freq_present=");
    Serial.print(freq.present ? 1 : 0);
    Serial.print(" freq_matched=");
    Serial.print(freq.matched ? 1 : 0);
    Serial.print(" freq_score=");
    Serial.print(freq.score, 1);
    Serial.print(" freq_conf=");
    Serial.print(freq.confidence, 1);
    Serial.print(" freq_target_hz=");
    Serial.print(freq.targetHz);
    Serial.print(" freq_target_power=");
    Serial.print(freq.targetPower, 1);
    Serial.print(" freq_neighbor_power=");
    Serial.print(freq.neighborPower, 1);
    Serial.print(" freq_total_energy=");
    Serial.print(freq.totalEnergy, 1);
    Serial.print(" freq_contrast=");
    Serial.print(freq.spectralContrast, 2);
    Serial.print(" freq_observed_at_ms=");
    Serial.print(freq.observedAtMs);
    Serial.print(" freq_age_ms=");
    Serial.print(freqAgeMs);
    Serial.print("ms");
    Serial.print(" freq_valid_window=");
    Serial.print(freq.validWindow ? 1 : 0);
    printH3SequenceRoleFields("primary", acceptedDtMs, static_cast<long>(diagnostics.acceptedTransientDurationMs), diagnostics.acceptedTransientStrength, freq, freq.observedAtMs, diagnostics.acceptedFrequencyProcessedAtMs, "none", 0);
    printH3SequenceRoleFields("duplicate", diagnostics.duplicateCount > 0 ? static_cast<long>(diagnostics.duplicateTransientMs >= _sequenceTest.currentTrialStartMs ? diagnostics.duplicateTransientMs - _sequenceTest.currentTrialStartMs : 0) : -1,
                              diagnostics.duplicateCount > 0 ? static_cast<long>(diagnostics.duplicateTransientDurationMs) : -1,
                              diagnostics.duplicateCount > 0 ? diagnostics.duplicateTransientStrength : 0.0f,
                              diagnostics.duplicateCount > 0 ? diagnostics.duplicateFrequencyEvidence : DetectionPipeline::FrequencyEvidence{},
                              diagnostics.duplicateCount > 0 ? diagnostics.duplicateFrequencyProcessedAtMs : 0,
                              diagnostics.duplicateCount > 0 ? diagnostics.duplicateFrequencyProcessedAtMs : 0,
                              diagnostics.duplicateCount > 0 ? diagnostics.duplicateReason : "none",
                              diagnostics.duplicateDeltaFromPrimaryMs);
    Serial.print(" detector_candidates=");
    Serial.print(diagnostics.rawCandidateCount);
    Serial.print(" accepted=");
    Serial.print(diagnostics.transientAccepted ? 1 : 0);
    Serial.print(" duplicates=");
    Serial.println(diagnostics.duplicateCount);

    if (!analyzerLogEnabled(_sequenceTest.logFlags, AnalyzerApp::ANALYZER_LOG_RAW_DEBUG)) {
        return;
    }

    Serial.print("SEQ_RAW timing trial_start_ms=");
    Serial.print(_sequenceTest.currentTrialScheduledAtMs);
    Serial.print(" trigger_sent_ms=");
    Serial.print(_sequenceTest.currentTrialStartMs);
    Serial.print(" first_candidate_ms=");
    if (diagnostics.firstCandidateMs > 0) {
        Serial.print(diagnostics.firstCandidateMs);
    } else {
        Serial.print("-");
    }
    Serial.print(" ambient_baseline_avg=");
    Serial.print(ambientBaselineAvg, 1);
    Serial.print(" ambient_baseline_min=");
    Serial.print(diagnostics.ambientBaselineSamples > 0 ? diagnostics.ambientBaselineMin : 0.0f, 1);
    Serial.print(" ambient_baseline_max=");
    Serial.print(diagnostics.ambientBaselineSamples > 0 ? diagnostics.ambientBaselineMax : 0.0f, 1);
    Serial.print(" max_signal_level=");
    Serial.println(diagnostics.maxSignalLevel);

    Serial.print("SEQ_RAW origin_counts={pre_window:");
    Serial.print(diagnostics.candidatePreWindowCount);
    Serial.print(",in_window:");
    Serial.print(diagnostics.candidateInWindowCount);
    Serial.print(",post_window:");
    Serial.print(diagnostics.candidatePostWindowCount);
    Serial.println("}");

    Serial.print("SEQ_RAW rejects={too_short:");
    Serial.print(diagnostics.transientRejectTooShortCount);
    Serial.print(",too_long:");
    Serial.print(diagnostics.transientRejectTooLongCount);
    Serial.print(",weak:");
    Serial.print(diagnostics.transientRejectWeakCount);
    Serial.println("}");

    Serial.print("SEQ_RAW strongest_reject={reason:");
    if (totalRejects > 0) {
        Serial.print(strongestRejectReasonName);
        Serial.print(",dt:");
        if (diagnostics.strongestRejectDtFromTriggerMs >= 0) {
            Serial.print(diagnostics.strongestRejectDtFromTriggerMs);
        } else {
            Serial.print("-");
        }
        Serial.print(",dur:");
        Serial.print(diagnostics.strongestRejectDurationMs);
        Serial.print(",strength:");
        Serial.print(diagnostics.strongestRejectStrength, 1);
    } else {
        Serial.print("none,dt:-,dur:0,strength:0.0");
    }
    Serial.println("}");

    Serial.print("SEQ_RAW best_candidate={dt:");
    if (diagnostics.bestCandidateValid) {
        Serial.print(diagnostics.bestCandidateDtFromTriggerMs);
    } else {
        Serial.print("-");
    }
    Serial.print(",dur:");
    if (diagnostics.bestCandidateValid) {
        Serial.print(diagnostics.bestCandidateDurationMs);
    } else {
        Serial.print("-");
    }
    Serial.print(",end_dt:");
    if (diagnostics.bestCandidateValid && diagnostics.bestCandidateDtFromTriggerMs >= 0) {
        Serial.print(diagnostics.bestCandidateDtFromTriggerMs + static_cast<long>(diagnostics.bestCandidateDurationMs));
        Serial.print("ms");
    } else {
        Serial.print("-");
    }
    Serial.print(",strength:");
    if (diagnostics.bestCandidateValid) {
        Serial.print(diagnostics.bestCandidateStrength, 1);
    } else {
        Serial.print("0");
    }
    Serial.println("}");

    if (isMiss || isLate || isUnexpected || hasDuplicates || expectedDtSlow || expectedDurLong) {
        Serial.print("SEQ_RAW issues=[");
        bool firstIssue = true;
        auto printIssue = [&](const char* label) {
            if (!firstIssue) {
                Serial.print(",");
            }
            Serial.print(label);
            firstIssue = false;
        };
        if (isMiss) {
            printIssue("miss");
        }
        if (isLate) {
            printIssue("late");
        }
        if (isUnexpected) {
            printIssue("unexpected");
        }
        if (hasDuplicates) {
            printIssue("duplicates");
        }
        if (expectedDtSlow) {
            printIssue("expected_dt_gt_200ms");
        }
        if (expectedDurLong) {
            printIssue("expected_dur_gt_180ms");
        }
        Serial.println("]");
    }

    if (analyzerLogEnabled(_sequenceTest.logFlags, AnalyzerApp::ANALYZER_LOG_RAW_DEBUG)) {
        Serial.print("SEQ_RAW duplicate_dts=[");
        for (unsigned long i = 0; i < diagnostics.duplicateDtCount; ++i) {
            if (i > 0) {
                Serial.print(",");
            }
            Serial.print(diagnostics.duplicateDts[i]);
        }
        Serial.println("]");
        for (unsigned long i = 0; i < diagnostics.candidateCount; ++i) {
            const auto& entry = diagnostics.candidates[i];
            const char* originName = "in_window";
            switch (entry.origin) {
                case SequenceTest::CandidateOrigin::PreWindow:
                    originName = "pre_window";
                    break;
                case SequenceTest::CandidateOrigin::InWindow:
                    originName = "in_window";
                    break;
                case SequenceTest::CandidateOrigin::PostWindow:
                    originName = "post_window";
                    break;
            }
            Serial.print("SEQ_RAW candidate[");
            Serial.print(i);
            Serial.print("] origin=");
            Serial.print(originName);
            Serial.print(" onset_dt_ms=");
            Serial.print(entry.dtFromTriggerMs);
            Serial.print(" dur=");
            Serial.print(entry.durationMs);
            Serial.print(" end_dt_ms=");
            if (entry.dtFromTriggerMs >= 0) {
                Serial.print(entry.dtFromTriggerMs + static_cast<long>(entry.durationMs));
                Serial.print("ms");
            } else {
                Serial.print("-");
            }
            Serial.print(" strength=");
            Serial.println(entry.strength, 1);
        }
    }
}

void AnalyzerApp::printSequenceTrialReports() const {
    if (_valMode) {
        return;
    }
    const bool reportEnabled = analyzerLogEnabled(_sequenceTest.logFlags, AnalyzerApp::ANALYZER_LOG_FREQ_CLASS);
    const bool liveRawEnabled = analyzerLogEnabled(_sequenceTest.logFlags, AnalyzerApp::ANALYZER_LOG_RAW_DEBUG);
    if (!reportEnabled && !liveRawEnabled) {
        return;
    }
    if (_sequenceTest.trialReports == nullptr || _sequenceTest.trialReportCount == 0) {
        return;
    }

    Serial.println("SEQ_REPORT_BEGIN");
    for (size_t i = 0; i < _sequenceTest.trialReportCount && i < _sequenceTest.trialReportCapacity; ++i) {
        const auto& report = _sequenceTest.trialReports[i];
        const char* candidateClass = h3SequenceCandidateClassFromResult(report.result);

        Serial.print("SEQ_REPORT trial=");
        Serial.print(report.trialNumber);
        Serial.print(" result=");
        Serial.print(report.result);
        Serial.print(" candidate_class=");
        Serial.print(candidateClass);
        Serial.print(" start_ms=");
        Serial.print(report.startMs);
        Serial.print(" end_ms=");
        Serial.print(report.endMs);
        Serial.print(" dt_ms=");
        if (report.dtMs >= 0) {
            Serial.print(report.dtMs);
            Serial.print("ms");
        } else {
            Serial.print("-");
        }
        Serial.print(" dur_ms=");
        if (report.durMs >= 0) {
            Serial.print(report.durMs);
            Serial.print("ms");
        } else {
            Serial.print("-");
        }
        Serial.print(" strength=");
        Serial.print(report.strength, 1);
        Serial.print(" duplicates=");
        Serial.print(report.duplicates);
        Serial.print(" best_candidate=");
        if (report.bestCandidateValid) {
            Serial.print("dt:");
            Serial.print(report.bestCandidateDtFromTriggerMs);
            Serial.print("ms,dur:");
            Serial.print(report.bestCandidateDurationMs);
            Serial.print("ms,str:");
            Serial.print(report.bestCandidateStrength, 1);
            Serial.print(",origin:");
            switch (report.bestCandidateOrigin) {
                case SequenceTest::CandidateOrigin::PreWindow:
                    Serial.print("pre_window");
                    break;
                case SequenceTest::CandidateOrigin::InWindow:
                    Serial.print("in_window");
                    break;
                case SequenceTest::CandidateOrigin::PostWindow:
                    Serial.print("post_window");
                    break;
            }
        } else {
            Serial.print("-");
        }
        Serial.print(" candidates=");
        Serial.print(report.candidateCount);
        Serial.print(" overflow=");
        Serial.print(report.candidateOverflowCount);
        Serial.print(" origin_counts={pre:");
        Serial.print(report.candidatePreWindowCount);
        Serial.print(",in:");
        Serial.print(report.candidateInWindowCount);
        Serial.print(",post:");
        Serial.print(report.candidatePostWindowCount);
        Serial.print("}");
        Serial.print(" freqEarly[avail=");
        Serial.print(report.freqEarly.windowAvailable ? 1 : 0);
        Serial.print(" score=");
        Serial.print(report.freqEarly.score, 1);
        Serial.print(" target=");
        Serial.print(report.freqEarly.targetHz);
        Serial.print(" contrast=");
        Serial.print(report.freqEarly.spectralContrast, 2);
        Serial.print(" win=");
        Serial.print(report.freqEarly.windowSampleCount);
        Serial.print("]");
        Serial.print(" freqFull[avail=");
        Serial.print(report.freqFull.windowAvailable ? 1 : 0);
        Serial.print(" score=");
        Serial.print(report.freqFull.score, 1);
        Serial.print(" target=");
        Serial.print(report.freqFull.targetHz);
        Serial.print(" contrast=");
        Serial.print(report.freqFull.spectralContrast, 2);
        Serial.print(" win=");
        Serial.print(report.freqFull.windowSampleCount);
        Serial.print("]");
        Serial.println();
    }
    Serial.println("SEQ_REPORT_END");
}

void AnalyzerApp::printSequenceTrialResult(unsigned long trialNumber, const char* result, long dtMs, long durMs, float strength, bool audioOverflow, unsigned long duplicateCount, const SequenceTest::TrialDiagnostics& diagnostics) const {
    if (_valMode) {
        return;
    }
    if (!analyzerLogEnabled(_sequenceTest.logFlags, AnalyzerApp::ANALYZER_LOG_TRIAL)) {
        return;
    }

    const DetectionPipeline::FrequencyEvidence* trialEarlyFrequency = nullptr;
    const DetectionPipeline::FrequencyEvidence* trialFullFrequency = nullptr;
    if (diagnostics.transientAccepted) {
        trialEarlyFrequency = &diagnostics.acceptedFrequencyEvidence;
        trialFullFrequency = &diagnostics.acceptedFrequencyEvidenceFull;
    } else if (diagnostics.duplicateCount > 0) {
        trialEarlyFrequency = &diagnostics.duplicateFrequencyEvidence;
        trialFullFrequency = &diagnostics.duplicateFrequencyEvidenceFull;
    }

    Serial.println();
    Serial.print("SEQ_TRIAL trial=");
    Serial.print(trialNumber);
    Serial.print(" result=");
    Serial.print(result);
    Serial.print(" dt=");
    if (dtMs >= 0) {
        Serial.print(dtMs);
        Serial.print("ms");
    } else {
        Serial.print("-");
    }
    Serial.print(" dur=");
    if (durMs >= 0) {
        Serial.print(durMs);
        Serial.print("ms");
    } else {
        Serial.print("-");
    }
    Serial.print(" dur_class=");
    Serial.print(sequenceTrialDurationClass(durMs));
    Serial.print(" strength=");
    Serial.print(strength, 1);
    Serial.print(" dup=");
    Serial.print(duplicateCount);
    Serial.print(" candidates=");
    Serial.print(diagnostics.candidateCount);
    Serial.print(" freqEarly=");
    if (trialEarlyFrequency != nullptr && trialEarlyFrequency->present) {
        Serial.print(trialEarlyFrequency->score, 1);
        Serial.print("/");
        Serial.print(trialEarlyFrequency->spectralContrast, 2);
    } else {
        Serial.print("-/-");
    }
    Serial.print(" freqFull=");
    if (trialFullFrequency != nullptr && trialFullFrequency->present) {
        Serial.print(trialFullFrequency->score, 1);
        Serial.print("/");
        Serial.print(trialFullFrequency->spectralContrast, 2);
    } else {
        Serial.print("-/-");
    }
    Serial.print(" full_ratio=");
    if (trialEarlyFrequency != nullptr && trialFullFrequency != nullptr &&
        trialEarlyFrequency->present && trialFullFrequency->present &&
        trialEarlyFrequency->score != 0.0f) {
        Serial.print(trialFullFrequency->score / trialEarlyFrequency->score, 3);
    } else {
        Serial.print("-");
    }
    Serial.println();

    if (trialNumber > 0) {
        const size_t reportIndex = static_cast<size_t>(trialNumber - 1UL);
        if (_sequenceTest.trialReports != nullptr && reportIndex < _sequenceTest.trialReportCapacity) {
            auto& report = _sequenceTest.trialReports[reportIndex];
            report.trialNumber = trialNumber;
            report.startMs = _sequenceTest.currentTrialStartMs;
            report.endMs = _sequenceTest.currentTrialEndMs;
            report.dtMs = dtMs;
            report.durMs = durMs;
            report.strength = strength;
            report.duplicates = duplicateCount;
            report.bestCandidateDtFromTriggerMs = diagnostics.bestCandidateDtFromTriggerMs >= 0 ? static_cast<unsigned long>(diagnostics.bestCandidateDtFromTriggerMs) : 0UL;
            report.bestCandidateDurationMs = diagnostics.bestCandidateDurationMs;
            report.bestCandidateStrength = diagnostics.bestCandidateStrength;
            report.bestCandidateValid = diagnostics.bestCandidateValid;
            report.bestCandidateOrigin = diagnostics.bestCandidateOrigin;
            report.candidateCount = diagnostics.candidateCount;
            report.candidateOverflowCount = diagnostics.candidateOverflowCount;
            report.candidatePreWindowCount = diagnostics.candidatePreWindowCount;
            report.candidateInWindowCount = diagnostics.candidateInWindowCount;
            report.candidatePostWindowCount = diagnostics.candidatePostWindowCount;
            report.freqEarly = {};
            report.freqFull = {};
            if (diagnostics.transientAccepted) {
                report.freqEarly = diagnostics.acceptedFrequencyEvidence;
                report.freqFull = diagnostics.acceptedFrequencyEvidenceFull;
            } else if (diagnostics.duplicateCount > 0) {
                report.freqEarly = diagnostics.duplicateFrequencyEvidence;
                report.freqFull = diagnostics.duplicateFrequencyEvidenceFull;
            }
            strncpy(report.result, result, sizeof(report.result));
            report.result[sizeof(report.result) - 1] = '\0';
            const size_t storedCount = reportIndex + 1UL;
            if (storedCount > _sequenceTest.trialReportCount) {
                _sequenceTest.trialReportCount = storedCount;
            }
        }
    }

    (void)audioOverflow;
    (void)diagnostics;
}

void AnalyzerApp::printDetectionParameters() const {
    if (_valMode) {
        return;
    }
    Serial.print("SEQ det mode=");
    Serial.print("AMP");
    Serial.print(" onset=");
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
    Serial.println();
}

void AnalyzerApp::printTransientAcceptedDebug(unsigned long now, float strength, unsigned long durationMs) const {
    if (_valMode) {
        return;
    }
    Serial.print("DET transient accepted t=");
    Serial.print(now);
    Serial.print(" dur=");
    Serial.print(durationMs);
    Serial.print(" strength=");
    Serial.println(strength, 1);
}

void AnalyzerApp::printTransientStatsDebug(unsigned long now) const {
    if (_valMode) {
        return;
    }
    const unsigned long elapsedMs = now - _sequenceTest.startedAtMs;
    const unsigned long expectedCount = (elapsedMs + (detectorCooldownAfterOnsetMs() / 2)) / detectorCooldownAfterOnsetMs();
    const unsigned long successRate = expectedCount > 0 ? ((_sequenceTest.hits * 100UL) / expectedCount) : 0;

    Serial.print("DET transient stats t=");
    Serial.print(now);
    Serial.print(" hits=");
    Serial.print(_sequenceTest.hits);
    Serial.print(" expected=");
    Serial.print(expectedCount);
    Serial.print(" success=");
    Serial.print(successRate);
    Serial.println("%");
}

void AnalyzerApp::printSequenceSummary() const {
    if (_valMode) {
        return;
    }
    const bool verboseTrialReports =
        analyzerLogEnabled(_sequenceTest.logFlags, AnalyzerApp::ANALYZER_LOG_FREQ_CLASS) ||
        analyzerLogEnabled(_sequenceTest.logFlags, AnalyzerApp::ANALYZER_LOG_RAW_DEBUG);
    const bool summaryEnabled = analyzerLogEnabled(_sequenceTest.logFlags, AnalyzerApp::ANALYZER_LOG_SUMMARY);
    if (!verboseTrialReports && !summaryEnabled) {
        return;
    }
    if (verboseTrialReports) {
        printSequenceTrialReports();
    }
    if (!summaryEnabled) {
        return;
    }
    const unsigned long total = _sequenceTest.totalTrials;
    const unsigned long validPrimary = _sequenceTest.expectedHits + _sequenceTest.lateHits;
    const unsigned long completed = validPrimary + _sequenceTest.misses + _sequenceTest.invalidAudio;
    const unsigned long primaryHits = validPrimary;
    const float primaryAvgStrength = primaryHits > 0 ? (static_cast<float>(_sequenceTest.totalHitStrengthScaled) / 100.0f) / static_cast<float>(primaryHits) : 0.0f;
    const float primaryAvgDuration = primaryHits > 0 ? static_cast<float>(_sequenceTest.totalHitDurationMs) / static_cast<float>(primaryHits) : 0.0f;

    Serial.print("SEQ_SUMMARY test=");
    Serial.print(_sequenceTest.setupLabel);
    Serial.print(" tries=");
    Serial.print(total);
    Serial.print(" completed=");
    Serial.print(completed);
    Serial.print(" valid_primary=");
    Serial.print(validPrimary);
    Serial.print(" expected_hits=");
    Serial.print(_sequenceTest.expectedHits);
    Serial.print(" late_hits=");
    Serial.print(_sequenceTest.lateHits);
    Serial.print(" misses=");
    Serial.print(_sequenceTest.misses);
    Serial.print(" unexpected=");
    Serial.print(_sequenceTest.unexpected);
    Serial.print(" duplicates=");
    Serial.print(_sequenceTest.duplicates);
    Serial.print(" invalid_audio=");
    Serial.print(_sequenceTest.invalidAudio);
    Serial.print(" samples=");
    Serial.print(_sequenceTest.samplesProcessed);
    Serial.print(" max_loop_samples=");
    Serial.print(_sequenceTest.maxSamplesPerLoop);
    Serial.print(" empty_source_loops=");
    Serial.print(_sequenceTest.emptySourceLoops);
    Serial.print(" source_dropped=");
    Serial.print(_audioSource.droppedSamples());
    Serial.print(" source_buffer_max=");
    Serial.print(_audioSource.bufferedSamplesMax());
    Serial.print(" primary_avg_strength=");
    Serial.print(primaryAvgStrength, 3);
    Serial.print(" primary_avg_dur=");
    Serial.print(primaryAvgDuration, 3);
    Serial.println(" ms");
    if (_sequenceTest.showDetails) {
        printDetectionParameters();
    }
    printAudioSourceSummary();
    printSignalSummary();
}

void AnalyzerApp::printSequenceFinalOutput() const {
    if (_valMode) {
        return;
    }
    printSequenceTrialReports();
    printSequenceSummary();
}

void AnalyzerApp::printBaseSummary() const {
    const unsigned long samples = _baseSession.samples;
    const unsigned long rawAvg = samples > 0 ? _baseSession.rawSum / samples : 0;
    const float deltaAvg = samples > 0 ? _baseSession.deltaSum / static_cast<float>(samples) : 0.0f;
    const float baselineAvg = samples > 0 ? _baseSession.baselineSum / static_cast<float>(samples) : 0.0f;
    const float baselineDrift = _baseSession.baselineMax - _baseSession.baselineMin;
    const int rawSwing = _baseSession.rawMax - _baseSession.rawMin;
    const float deltaSwing = _baseSession.deltaMax - _baseSession.deltaMin;
    const float deltaPeak = _baseSession.deltaMax >= 0.0f ? _baseSession.deltaMax : -_baseSession.deltaMax;
    const float deltaFloor = _baseSession.deltaMin <= 0.0f ? -_baseSession.deltaMin : _baseSession.deltaMin;
    const float deltaQuietPeak = deltaPeak > deltaFloor ? deltaPeak : deltaFloor;

    Serial.print("BASE done: samples=");
    Serial.print(samples);
    if (_baseSession.ignoredRawSamples > 0) {
        Serial.print(" ignored_raw=");
        Serial.print(_baseSession.ignoredRawSamples);
    }
    Serial.print(" rawSample_avg=");
    Serial.print(rawAvg);
    Serial.print(" rawSample_min=");
    Serial.print(_baseSession.rawMin);
    Serial.print(" rawSample_max=");
    Serial.print(_baseSession.rawMax);
    Serial.print(" rawSample_swing=");
    Serial.print(rawSwing);
    Serial.print(" centeredSample_avg=");
    Serial.print(deltaAvg, 1);
    Serial.print(" centeredSample_min=");
    Serial.print(_baseSession.deltaMin, 1);
    Serial.print(" centeredSample_max=");
    Serial.print(_baseSession.deltaMax, 1);
    Serial.print(" centeredSample_swing=");
    Serial.print(deltaSwing, 1);
    Serial.print(" baseline_avg=");
    Serial.print(baselineAvg, 1);
    Serial.print(" baseline_min=");
    Serial.print(_baseSession.baselineMin, 1);
    Serial.print(" baseline_max=");
    Serial.print(_baseSession.baselineMax, 1);
    Serial.print(" baseline_drift=");
    Serial.println(baselineDrift, 1);
    Serial.print("BASE quiet: quiet_rawSample_min=");
    Serial.print(_baseSession.rawMin);
    Serial.print(" quiet_rawSample_max=");
    Serial.print(_baseSession.rawMax);
    Serial.print(" quiet_rawSample_swing=");
    Serial.print(rawSwing);
    Serial.print(" quiet_centeredSample_min=");
    Serial.print(_baseSession.deltaMin, 1);
    Serial.print(" quiet_centeredSample_max=");
    Serial.print(_baseSession.deltaMax, 1);
    Serial.print(" quiet_centeredSample_swing=");
    Serial.print(deltaSwing, 1);
    Serial.print(" quiet_centeredSample_peak=");
    Serial.println(deltaQuietPeak, 1);
    printBaseHints();
    printAudioSourceSummary();
    printSignalSummary();
}

void AnalyzerApp::printBaseHints() const {
    const float quietDeltaPeak = _baseSession.deltaMax >= 0.0f ? _baseSession.deltaMax : -_baseSession.deltaMax;
    const float quietDeltaFloor = _baseSession.deltaMin <= 0.0f ? -_baseSession.deltaMin : _baseSession.deltaMin;
    const float quietNoisePeak = quietDeltaPeak > quietDeltaFloor ? quietDeltaPeak : quietDeltaFloor;
    const unsigned long suggestedMinStrength = static_cast<unsigned long>(quietNoisePeak) + 6;
    const unsigned long suggestedAttack = static_cast<unsigned long>(quietNoisePeak) + 10;
    const unsigned long suggestedRelease = suggestedAttack > 6 ? suggestedAttack - 6 : suggestedAttack;

    Serial.print("BASE hints: quiet_rawSample_peak=");
    Serial.print(_baseSession.rawMax);
    Serial.print(" quiet_centeredSample_max=");
    Serial.print(_baseSession.deltaMax, 1);
    Serial.print(" quiet_centeredSample_peak=");
    Serial.print(quietNoisePeak, 1);
    Serial.print(" suggested_minStrength=");
    Serial.print(suggestedMinStrength);
    Serial.print(" suggested_attack=");
    Serial.print(suggestedAttack);
    Serial.print(" suggested_release=");
    Serial.println(suggestedRelease);
}

void AnalyzerApp::printCaptureSummary() const {
    const unsigned long completed = _captureSession.completed;
    const float avgRawSwing = completed > 0 ? static_cast<float>(_captureSession.totalRawSwing) / static_cast<float>(completed) : 0.0f;
    const float avgDeltaSwing = completed > 0 ? _captureSession.totalDeltaSwing / static_cast<float>(completed) : 0.0f;
    const unsigned long quietRawAvg = _captureSession.quietRawSamples > 0 ? _captureSession.quietRawSum / _captureSession.quietRawSamples : 0;
    const float quietDeltaAvg = _captureSession.quietDeltaSamples > 0 ? _captureSession.quietDeltaSum / static_cast<float>(_captureSession.quietDeltaSamples) : 0.0f;

    Serial.print("CAP done: tries=");
    Serial.print(_captureSession.totalTrials);
    Serial.print(" completed=");
    Serial.print(completed);
    Serial.print(" avg_rawSample_swing=");
    Serial.print(avgRawSwing, 1);
    Serial.print(" avg_centeredSample_swing=");
    Serial.print(avgDeltaSwing, 1);
    Serial.print(" best_rawSample_swing=");
    Serial.print(_captureSession.bestRawSwing);
    Serial.print(" best_centeredSample_swing=");
    Serial.println(_captureSession.bestDeltaSwing, 1);
    Serial.print("CAP quiet: rawSample_avg=");
    Serial.print(quietRawAvg);
    Serial.print(" rawSample_peak=");
    Serial.print(_captureSession.quietRawMax);
    Serial.print(" centeredSample_avg=");
    Serial.print(quietDeltaAvg, 1);
    Serial.print(" centeredSample_peak=");
    Serial.println(_captureSession.quietDeltaMax, 1);
    printCaptureHints();
    printAudioSourceSummary();
    printSignalSummary();
}

void AnalyzerApp::printCaptureHints() const {
    const unsigned long quietRawAvg = _captureSession.quietRawSamples > 0 ? _captureSession.quietRawSum / _captureSession.quietRawSamples : 0;
    const float quietDeltaPeak = _captureSession.quietDeltaMax >= 0.0f ? _captureSession.quietDeltaMax : -_captureSession.quietDeltaMax;
    const float quietDeltaFloor = _captureSession.quietDeltaMin <= 0.0f ? -_captureSession.quietDeltaMin : _captureSession.quietDeltaMin;
    const float quietNoisePeak = quietDeltaPeak > quietDeltaFloor ? quietDeltaPeak : quietDeltaFloor;
    const unsigned long suggestedMinStrength = static_cast<unsigned long>(quietNoisePeak) + 6;
    const unsigned long suggestedAttack = static_cast<unsigned long>(quietNoisePeak) + 10;
    const unsigned long suggestedRelease = suggestedAttack > 6 ? suggestedAttack - 6 : suggestedAttack;

    Serial.print("CAP hints: suggested_minStrength=");
    Serial.print(suggestedMinStrength);
    Serial.print(" suggested_attack=");
    Serial.print(suggestedAttack);
    Serial.print(" suggested_release=");
    Serial.print(suggestedRelease);
    Serial.print(" quiet_rawSample_avg=");
    Serial.print(quietRawAvg);
    Serial.print(" quiet_centeredSample_peak=");
    Serial.println(quietNoisePeak, 1);
}

void AnalyzerApp::printAudioSourceSummary() const {
    const AudioSourceStats& stats = _audioSource.stats();
    Serial.println("AUDIO summary:");
    Serial.print("reads=");
    Serial.print(stats.reads);
    Serial.print(" readBytes=");
    Serial.print(stats.readBytes);
    Serial.print(" zeroReads=");
    Serial.print(stats.zeroReads);
    Serial.print(" shortReads=");
    Serial.print(stats.shortReads);
    Serial.print(" maxReadBytes=");
    Serial.print(stats.maxReadBytes);
    Serial.print(" noSampleLoops=");
    Serial.print(stats.noSampleLoops);
    Serial.print(" readErrors=");
    Serial.print(stats.readErrors);
    Serial.print(" overflow=");
    Serial.print(stats.overflowCount);
    Serial.print(" droppedBlocks=");
    Serial.print(stats.droppedBlockCount);
    Serial.print(" totalSamples=");
    Serial.println(static_cast<unsigned long long>(stats.totalSamplesRead));
}

void AnalyzerApp::printSignalSummary() const {
    const AudioSignalStats& stats = _audioSignal.stats();
    Serial.println("SIGNAL summary:");
    Serial.print("blocks=");
    Serial.print(stats.blocksProcessed);
    Serial.print(" samples=");
    Serial.print(static_cast<unsigned long long>(stats.samplesProcessed));
    Serial.print(" candidates=");
    Serial.print(stats.candidatesEmitted);
    Serial.print(" candidateDrops=");
    Serial.print(stats.candidatesDropped);
    Serial.print(" lastBlockStart=");
    Serial.print(static_cast<unsigned long long>(_audioSignal.lastBlockStartSample()));
    Serial.print(" lastBlockCount=");
    Serial.print(_audioSignal.lastBlockSampleCount());
    Serial.print(" lastBlockMicros=");
    Serial.println(_audioSignal.lastBlockApproxStartMicros());
}

void AnalyzerApp::printValueFrame(unsigned long now) const {
    if (_lastPrintMs != 0 && now - _lastPrintMs < kPrintIntervalMs) {
        return;
    }

    _lastPrintMs = now;
    const bool onsetVisible = detectorOnsetDetected() || now < _valOnsetLatchedUntilMs;
    const bool transientVisible = detectorTransientDetected() || now < _valTransientLatchedUntilMs;

    // Compact frame: source sample, centered sample, detector level, and smoothing.
    Serial.print("rawSample:");
    Serial.print(_audioSignal.rawSignal());
    Serial.print('\t');
    Serial.print("baseline:");
    Serial.print(static_cast<int>(_audioSignal.baseline()));
    Serial.print('\t');
    Serial.print("centeredSample:");
    Serial.print(_audioSignal.centeredSignal());
    Serial.print('\t');
    Serial.print("signalLevel:");
    Serial.print(_audioSignal.signalMagnitude());
    Serial.print('\t');
    Serial.print("smoothedLevel:");
    Serial.print(static_cast<int>(_audioSignal.smoothedSignalMagnitude()));
    Serial.print('\t');
    Serial.print("onset:");
    Serial.print(onsetVisible ? 1 : 0);
    Serial.print('\t');
    Serial.print("transient:");
    Serial.println(transientVisible ? 1 : 0);
}
