#include "node.h"
#include <Arduino.h>
#include <string.h>

#include "../../detection/DetectionPipeline.h"

#ifndef CHIRP_FREQUENCY_HZ
#define CHIRP_FREQUENCY_HZ 2400
#endif

namespace {
constexpr int kMaxSamplesPerLoop = 128;
constexpr int kRbStartupQuietThreshold = 20;
constexpr unsigned long kRbStartupQuietHoldMs = 1000;
constexpr unsigned long kRbStartupBaselineTimeoutMs = 8000;
constexpr unsigned long kRbPostRebaseSettleMs = 500;

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

const char* behaviorGateName(const ResonantBehavior& behavior, unsigned long now, bool transientDetected, bool selfChirpSuppressed) {
    if (selfChirpSuppressed) {
        return "self_ignore";
    }
    if (behavior.refractoryRemainingMs(now) > 0) {
        return "refractory";
    }
    if (behavior.waitRemainingMs(now) > 0) {
        return "wait";
    }
    if (transientDetected) {
        return "transient";
    }
    return behavior.stateCode() == 0 ? "idle" : behavior.stateName();
}

const char* chirpPatternName(ChirpOutput::ChirpPattern pattern) {
    switch (pattern) {
        case ChirpOutput::ChirpPattern::Single:
            return "single";
        case ChirpOutput::ChirpPattern::Triple:
            return "triple";
        case ChirpOutput::ChirpPattern::Idle:
            return "idle";
    }

    return "unknown";
}

const char* h3RbCandidateClassName(const DetectionPipeline::PatternResult& patternResult, ResonantBehavior::BehaviorDecision behaviorDecision, bool selfChirpSuppressed) {
    if (selfChirpSuppressed || behaviorDecision == ResonantBehavior::BehaviorDecision::SelfSuppressed) {
        return "self_suppressed";
    }
    if (!patternResult.valid) {
        return "unexpected_noise";
    }
    return "expected_primary";
}

void printH3FrequencyEvidenceFields(const DetectionPipeline::PatternResult& patternResult,
                                    const DetectionPipeline::FrequencyEvidence& frequencyEvidence,
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
}
}

Node::Node(int inputPin, int ledPin, int chirpPin, AudioSourceKind sourceKind)
    : Node(inputPin, ledPin, chirpPin, -1, sourceKind) {}

Node::Node(int inputPin, int ledPin, int chirpPin, int chirpBtlPin, AudioSourceKind sourceKind)
    : _ledPin(ledPin),
      _analogSource(inputPin),
      _i2sSource(14, 27, 33, 16000, 32),
      _sourceKind(sourceKind),
      _audioSource(sourceKind == AudioSourceKind::I2S
                       ? static_cast<AudioSource&>(_i2sSource)
                       : static_cast<AudioSource&>(_analogSource)),
      _audioSignal(_audioSource),
      _audioFrequencyDetector(_audioSignal),
      _audioOnsetDetector(),
      _toneOutput(chirpPin),
      _toneOutputBTL(chirpPin, chirpBtlPin),
      _chirpOutput(chirpBtlPin >= 0
                       ? static_cast<ToneOutput&>(_toneOutputBTL)
                       : static_cast<ToneOutput&>(_toneOutput)) {}

void Node::begin() {
    configureParameters();
    _audioSource.begin();
    _audioSignal.begin(false);
    _audioFrequencyDetector.begin();
    _audioFrequencyDetector.setDiagnosticsEnabled(false);
    _audioOnsetDetector.begin();
    _behavior.resetState();
    resetDetectionState();
    _audioSignal.resetStats();
    _audioSource.resetStats();
    resetRbCounters();
    _rbDetectOnly = false;
    _rbBaselineState = _sourceKind == AudioSourceKind::I2S ? RBBaselineState::ListenForQuiet : RBBaselineState::Active;
    _rbBaselineStateStartedMs = millis();
    _rbBaselineQuietSinceMs = 0;
    _rbBaselineLastLogMs = 0;
    _rbBaselineSettleUntilMs = 0;
    _rbLogMode = RbLogMode::Minimal;
    _rbLastWouldEmitHeardMs = 0;
    _rbLastWouldEmitDecision = ResonantBehavior::BehaviorDecision::None;
    _chirpOutput.begin();
    _debug.begin(_ledPin);
    _debug.markLoopStart(micros());
    _serialLineLength = 0;
    _serialLineBuffer[0] = '\0';

    if (_sourceKind == AudioSourceKind::I2S) {
        Serial.print("RB det mode=AMP onset=");
        Serial.print(_audioSignal.onsetDetectionThreshold(), 1);
        Serial.print(" release=");
        Serial.print(_audioSignal.onsetReleaseThreshold(), 1);
        Serial.print(" cooldown=");
        Serial.print(_audioSignal.cooldownAfterOnsetMs());
        Serial.print(" minMs=");
        Serial.print(_audioSignal.minTransientDurationMs());
        Serial.print(" maxMs=");
        Serial.print(_audioSignal.maxTransientDurationMs());
        Serial.print(" minStrength=");
        Serial.println(_audioSignal.minTransientPeakStrength(), 1);
        Serial.print("RB startup baseline=");
        Serial.println(rbBaselineStateName());
    }
}

void Node::startRbQuietBaseline() {
    if (_sourceKind != AudioSourceKind::I2S) {
        performRbRebase();
        return;
    }
    _rbBaselineState = RBBaselineState::ListenForQuiet;
    _rbBaselineStateStartedMs = millis();
    _rbBaselineQuietSinceMs = 0;
    _rbBaselineLastLogMs = 0;
    _rbBaselineSettleUntilMs = 0;
}

void Node::resetRbCounters() {
    _rbCandidateCount = 0;
    _rbActionCount = 0;
    _rbOverflowCandidates = 0;
    _rbLastCandidateMs = 0;
    _rbHaveLastCandidateMs = false;
    _rbStrengthSumScaled = 0;
    _rbDurationSumMs = 0;
}

void Node::resetDetectionState() {
    _audioSignal.resetDetectorState();
    _audioOnsetDetector.resetState();
    _wasSelfChirpSuppressed = false;
}

void Node::performRbRebase() {
    _audioSignal.rebase();
    _behavior.resetState();
    resetDetectionState();
    _audioSignal.resetStats();
    _audioSource.resetStats();
    resetRbCounters();
}

bool Node::rbOutputsEnabled() const {
    return _sourceKind != AudioSourceKind::I2S
        || _rbBaselineState == RBBaselineState::Active
        || _rbBaselineState == RBBaselineState::FailedNoQuiet;
}

const char* Node::rbBaselineStateName() const {
    switch (_rbBaselineState) {
        case RBBaselineState::Boot:
            return "BOOT";
        case RBBaselineState::ListenForQuiet:
            return "LISTEN_FOR_QUIET";
        case RBBaselineState::Rebase:
            return "REBASE";
        case RBBaselineState::Settle:
            return "SETTLE";
        case RBBaselineState::Active:
            return "ACTIVE";
        case RBBaselineState::FailedNoQuiet:
            return "FAILED_NO_QUIET";
    }
    return "UNKNOWN";
}

void Node::updateRbBaselineState(unsigned long now) {
    if (_sourceKind != AudioSourceKind::I2S) {
        return;
    }

    switch (_rbBaselineState) {
        case RBBaselineState::Boot:
            startRbQuietBaseline();
            break;

        case RBBaselineState::ListenForQuiet: {
            const float smooth = static_cast<float>(_audioSignal.smoothedSignalMagnitude());
            const bool quietNow = smooth < static_cast<float>(kRbStartupQuietThreshold);
            if (quietNow) {
                if (_rbBaselineQuietSinceMs == 0) {
                    _rbBaselineQuietSinceMs = now;
                }
                const unsigned long quietMs = now - _rbBaselineQuietSinceMs;
                if (quietMs >= kRbStartupQuietHoldMs) {
                    _rbBaselineState = RBBaselineState::Rebase;
                    performRbRebase();
                    _rbBaselineState = RBBaselineState::Settle;
                    _rbBaselineSettleUntilMs = now + kRbPostRebaseSettleMs;
                    Serial.print("RB rebase done quietMs=");
                    Serial.print(quietMs);
                    Serial.print(" baseline=");
                    Serial.print(_audioSignal.baseline(), 1);
                    Serial.print(" smooth=");
                    Serial.println(_audioSignal.smoothedSignalMagnitude());
                } else if (_rbBaselineLastLogMs == 0 || now - _rbBaselineLastLogMs >= 1000) {
                    _rbBaselineLastLogMs = now;
                    Serial.print("RB rebase waiting smooth=");
                    Serial.print(smooth, 1);
                    Serial.print(" quietMs=");
                    Serial.println(quietMs);
                }
            } else {
                _rbBaselineQuietSinceMs = 0;
                if (now - _rbBaselineStateStartedMs >= kRbStartupBaselineTimeoutMs) {
                    _rbBaselineState = RBBaselineState::FailedNoQuiet;
                    Serial.print("RB rebase skipped reason=no_quiet smooth=");
                    Serial.println(smooth, 1);
                }
            }
        } break;

        case RBBaselineState::Rebase:
            break;

        case RBBaselineState::Settle:
            if (now >= _rbBaselineSettleUntilMs) {
                _rbBaselineState = RBBaselineState::Active;
                Serial.println("RB baseline active");
            }
            break;

        case RBBaselineState::Active:
        case RBBaselineState::FailedNoQuiet:
            break;
    }
}

void Node::configureParameters() {
    configureSharedParameters();

    if (_sourceKind == AudioSourceKind::I2S) {
        configureI2SParameters();
    } else {
        configureAnalogParameters();
    }
}

void Node::configureSharedParameters() {
    // Signal conditioning shared across sources.
    _audioSignal.setSmoothingFactor(0.5f);
    _audioSignal.setBaselineUpdateFactor(0.005f);

    // Chirp tone is configured here so the node owns its output tuning.
    // The fallback/default frequency is defined by CHIRP_FREQUENCY_HZ.
    _chirpOutput.setToneHz(3200);
}

void Node::configureAnalogParameters() {
    _audioSignal.setBaselineTrackingQuietThreshold(40);
    _audioOnsetDetector.setOnsetDetectionThreshold(36.0f);
    _audioOnsetDetector.setOnsetReleaseThreshold(26.0f);
    _audioOnsetDetector.setCooldownAfterOnsetMs(100);
    _audioOnsetDetector.setReleaseDebounceMs(30);
    _audioOnsetDetector.setMinTransientDurationMs(60);
    _audioOnsetDetector.setMaxTransientDurationMs(240);
    _audioOnsetDetector.setMinTransientPeakStrength(40.0f);

    _behavior.setWaitAfterTransientMs(0); // Shared response timing: respond immediately after a transient.
    _behavior.setRefractoryAfterEmitMs(0); // Shared response timing: no post-emit holdoff.
    _behavior.setIdleTimeoutMs(10000); // Idle self-trigger timeout.
}

void Node::configureI2SParameters() {
    _audioSignal.setBaselineTrackingQuietThreshold(20);
    _audioSignal.setOnsetDetectionThreshold(36.0f);
    _audioSignal.setOnsetReleaseThreshold(26.0f);
    _audioSignal.setCooldownAfterOnsetMs(100);
    _audioSignal.setReleaseDebounceMs(30);
    _audioSignal.setMinTransientDurationMs(60);
    _audioSignal.setMaxTransientDurationMs(240);
    _audioSignal.setMinTransientPeakStrength(40.0f);

    _audioOnsetDetector.setOnsetDetectionThreshold(36.0f);
    _audioOnsetDetector.setOnsetReleaseThreshold(26.0f);
    _audioOnsetDetector.setCooldownAfterOnsetMs(100);
    _audioOnsetDetector.setReleaseDebounceMs(30);
    _audioOnsetDetector.setMinTransientDurationMs(60);
    _audioOnsetDetector.setMaxTransientDurationMs(240);
    _audioOnsetDetector.setMinTransientPeakStrength(40.0f);

    _behavior.setWaitAfterTransientMs(500); // Shared response timing: respond immediately after a transient.
    _behavior.setRefractoryAfterEmitMs(0); // Shared response timing: no post-emit holdoff.
    _behavior.setIdleTimeoutMs(10000); // Idle self-trigger timeout.
}

void Node::update() {
    const unsigned long now = millis();
    const unsigned long nowUs = micros();
    updateRbBaselineState(now);
    const bool rbOutputsEnabledNow = rbOutputsEnabled();
    const bool selfChirpSuppressed = _behavior.selfChirpSuppressed(now);
    _wasSelfChirpSuppressed = selfChirpSuppressed;

    _debug.noteCoreLoopUs(nowUs);
    pollSerialCommands();

    // Update input first, then detection, then behavior, so each layer sees the
    // latest state from the layer below it.
    int processedSamples = 0;
    bool sawI2SSample = false;
    bool sawPatternThisLoop = false;
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
            sawI2SSample = true;
            const unsigned long queueDepthBeforeDrain = static_cast<unsigned long>(_audioSignal.candidateQueueDepth());

            DetectorCandidate candidate;
            while (_audioSignal.popCandidate(candidate)) {
                DetectionPipeline::PatternResult patternResult;
                const auto frequencyEvidence = captureFrequencyEvidence();
                const bool patternValid = DetectionPipeline::processDetectorCandidate(candidate, patternResult, now, &frequencyEvidence);
                const auto behaviorDecision = _behavior.handlePatternResult(patternResult, now);

                if (patternValid && behaviorDecision == ResonantBehavior::BehaviorDecision::ConsumedPattern) {
                    sawPatternThisLoop = true;
                }

                if (rbShouldLogDetail()) {
                    Serial.print("BEH pattern=");
                    Serial.print(DetectionPipeline::patternTypeName(patternResult.type));
                    Serial.print(" heard=");
                    Serial.print(patternResult.candidate.heardAtMs);
                    Serial.print(" now=");
                    Serial.print(now);
                    Serial.print(" decision=");
                    Serial.print(_behavior.lastDecisionName());
                    Serial.print(" block=");
                    Serial.print(_behavior.lastBlockReasonName());
                    Serial.print(" waitMs=");
                    Serial.print(_behavior.waitRemainingMs(now));
                    Serial.print(" refractoryMs=");
                    Serial.print(_behavior.refractoryRemainingMs(now));
                    Serial.print(" selfIgnoreMs=");
                    Serial.print(_behavior.selfChirpIgnoreRemainingMs(now));
                    Serial.print(" detectionOnly=");
                    Serial.print(_behavior.detectionOnly() ? 1 : 0);
                    Serial.print(" outputBusy=");
                    Serial.print(_behavior.outputBusy() ? 1 : 0);
                    Serial.println();
                }

                const unsigned long candidateMs = patternResult.candidate.heardAtMs;
                long gapMs = -1;
                if (_rbHaveLastCandidateMs && candidateMs >= _rbLastCandidateMs) {
                    gapMs = static_cast<long>(candidateMs - _rbLastCandidateMs);
                }
                const unsigned long behaviorLagMs = now >= patternResult.processedAtMs ? now - patternResult.processedAtMs : 0;

                if (rbOutputsEnabledNow && !selfChirpSuppressed) {
                    _debug.observeOnset(now, true, patternResult.candidate.peakStrength);
                    _debug.observeTransient(now, true, patternResult.candidate.peakStrength, false);
                }

                const char* action = "queued";
                if (!patternValid) {
                    action = "invalid";
                } else if (selfChirpSuppressed) {
                    action = "self_ignore";
                } else if (_rbDetectOnly) {
                    action = "detectonly";
                }
                const char* candidateClass = h3RbCandidateClassName(patternResult, behaviorDecision, selfChirpSuppressed);

                if (rbShouldLogDetail()) {
                    logCandidate(candidate, patternResult, _rbCandidateCount + 1, gapMs, queueDepthBeforeDrain, behaviorLagMs, candidateClass, action, _behavior.stateName(), behaviorGateName(_behavior, now, sawPatternThisLoop, selfChirpSuppressed));
                }

                ++_rbCandidateCount;
                if (candidate.audioOverflowDuringCandidate) {
                    ++_rbOverflowCandidates;
                }
                if (action[0] != 'n') {
                    ++_rbActionCount;
                }
                _rbStrengthSumScaled += static_cast<unsigned long>(candidate.peakStrength * 100.0f);
                _rbDurationSumMs += candidate.durationMs;
                _rbHaveLastCandidateMs = true;
                _rbLastCandidateMs = candidateMs;
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
            _audioSignal.update(sample, sampleTimeUs);
            _audioFrequencyDetector.observeCenteredSample(_audioSignal.centeredSignal());

            if (rbOutputsEnabledNow && !selfChirpSuppressed) {
                _audioOnsetDetector.update(static_cast<float>(_audioSignal.signalMagnitude()), sampleTimeUs);
            }

            const bool onsetDetected = selfChirpSuppressed ? false : _audioOnsetDetector.onsetDetected();
            _debug.observeOnset(now, onsetDetected, _audioOnsetDetector.onsetStrength());
            _debug.observeTransient(now, _audioOnsetDetector.transientDetected(), _audioOnsetDetector.transientStrength(), selfChirpSuppressed);
            const unsigned long queueDepthBeforeDrain = static_cast<unsigned long>(_audioSignal.candidateQueueDepth());

            DetectorCandidate candidate;
            while (_audioSignal.popCandidate(candidate)) {
                DetectionPipeline::PatternResult patternResult;
                const auto frequencyEvidence = captureFrequencyEvidence();
                const bool patternValid = DetectionPipeline::processDetectorCandidate(candidate, patternResult, now, &frequencyEvidence);
                const auto behaviorDecision = _behavior.handlePatternResult(patternResult, now);

                if (patternValid && behaviorDecision == ResonantBehavior::BehaviorDecision::ConsumedPattern) {
                    sawPatternThisLoop = true;
                }

                if (rbShouldLogDetail()) {
                    Serial.print("BEH pattern=");
                    Serial.print(DetectionPipeline::patternTypeName(patternResult.type));
                    Serial.print(" heard=");
                    Serial.print(patternResult.candidate.heardAtMs);
                    Serial.print(" now=");
                    Serial.print(now);
                    Serial.print(" decision=");
                    Serial.print(_behavior.lastDecisionName());
                    Serial.print(" block=");
                    Serial.print(_behavior.lastBlockReasonName());
                    Serial.print(" waitMs=");
                    Serial.print(_behavior.waitRemainingMs(now));
                    Serial.print(" refractoryMs=");
                    Serial.print(_behavior.refractoryRemainingMs(now));
                    Serial.print(" selfIgnoreMs=");
                    Serial.print(_behavior.selfChirpIgnoreRemainingMs(now));
                    Serial.print(" detectionOnly=");
                    Serial.print(_behavior.detectionOnly() ? 1 : 0);
                    Serial.print(" outputBusy=");
                    Serial.print(_behavior.outputBusy() ? 1 : 0);
                    Serial.println();
                }

                const unsigned long candidateMs = patternResult.candidate.heardAtMs;
                long gapMs = -1;
                if (_rbHaveLastCandidateMs && candidateMs >= _rbLastCandidateMs) {
                    gapMs = static_cast<long>(candidateMs - _rbLastCandidateMs);
                }
                const unsigned long behaviorLagMs = now >= patternResult.processedAtMs ? now - patternResult.processedAtMs : 0;

                const char* action = "queued";
                if (!patternValid) {
                    action = "invalid";
                } else if (selfChirpSuppressed) {
                    action = "self_ignore";
                } else if (_rbDetectOnly) {
                    action = "detectonly";
                }
                const char* candidateClass = h3RbCandidateClassName(patternResult, behaviorDecision, selfChirpSuppressed);

                if (rbShouldLogDetail()) {
                    logCandidate(candidate, patternResult, _rbCandidateCount + 1, gapMs, queueDepthBeforeDrain, behaviorLagMs, candidateClass, action, _behavior.stateName(), behaviorGateName(_behavior, now, sawPatternThisLoop, selfChirpSuppressed));
                }

                ++_rbCandidateCount;
                if (candidate.audioOverflowDuringCandidate) {
                    ++_rbOverflowCandidates;
                }
                if (action[0] != 'n') {
                    ++_rbActionCount;
                }
                _rbStrengthSumScaled += static_cast<unsigned long>(candidate.peakStrength * 100.0f);
                _rbDurationSumMs += candidate.durationMs;
                _rbHaveLastCandidateMs = true;
                _rbLastCandidateMs = candidateMs;
            }

            processedSamples++;
        }
    }

    if (rbOutputsEnabledNow) {
        _behavior.update(now);
    }
    const bool behaviorWouldEmit = _behavior.takeWouldEmit();
    const bool shouldLogWouldEmit = behaviorWouldEmit && rbShouldLogDetail() &&
                                    (_behavior.lastHeardMs() != _rbLastWouldEmitHeardMs ||
                                     _behavior.lastDecision() != _rbLastWouldEmitDecision);
    if (shouldLogWouldEmit) {
        Serial.print("BEH pattern=");
        Serial.print(_behavior.lastPatternTypeName());
        Serial.print(" heard=");
        Serial.print(_behavior.lastHeardMs());
        Serial.print(" now=");
        Serial.print(now);
        Serial.print(" decision=");
        Serial.print(_behavior.lastDecisionName());
        Serial.print(" block=");
        Serial.print(_behavior.lastBlockReasonName());
        Serial.print(" waitMs=");
        Serial.print(_behavior.waitRemainingMs(now));
        Serial.print(" refractoryMs=");
        Serial.print(_behavior.refractoryRemainingMs(now));
        Serial.print(" selfIgnoreMs=");
        Serial.print(_behavior.selfChirpIgnoreRemainingMs(now));
        Serial.print(" detectionOnly=");
        Serial.print(_behavior.detectionOnly() ? 1 : 0);
        Serial.print(" outputBusy=");
        Serial.print(_behavior.outputBusy() ? 1 : 0);
        Serial.print(" wouldEmit=");
        Serial.println(1);
        _rbLastWouldEmitHeardMs = _behavior.lastHeardMs();
        _rbLastWouldEmitDecision = _behavior.lastDecision();
    }
    _debug.observeBehaviorGate(now, _behavior, sawPatternThisLoop, selfChirpSuppressed);

    if (_sourceKind == AudioSourceKind::I2S && sawI2SSample) {
        _debug.observeI2SSignal(now, _audioSignal);
    }

    if (rbOutputsEnabledNow && !_rbDetectOnly && _behavior.shouldStartChirp()) {
        const auto chirpPattern = _behavior.chirpPattern();
        const char* sourceName = _behavior.chirpRequestSourceName();
        Serial.print("BEH emit start now=");
        Serial.print(now);
        Serial.print(" source=");
        Serial.print(sourceName);
        Serial.print(" state=");
        Serial.print(_behavior.stateName());
        Serial.print(" pattern=");
        Serial.println(chirpPatternName(chirpPattern));
        Serial.print("RB emit accepted source=");
        Serial.print(sourceName);
        Serial.print(" state=");
        Serial.print(_behavior.stateName());
        Serial.print(" gate=");
        Serial.print(behaviorGateName(_behavior, now, sawPatternThisLoop, selfChirpSuppressed));
        Serial.print(" pattern=");
        Serial.println(chirpPatternName(chirpPattern));
        _debug.observeChirpStarted(now, sourceName, chirpPattern);
        _chirpOutput.start(chirpPattern);
        _behavior.notifyChirpStarted(now);
        ++_rbActionCount;
    }

    _chirpOutput.update();

    if (_chirpOutput.finished()) {
        _behavior.notifyChirpFinished(now);
        _debug.observeChirpFinished(now);
    }

    _debug.updateLed(now, _behavior, _chirpOutput, selfChirpSuppressed);

    _debug.endLoop(micros());
}

void Node::pollSerialCommands() {
    while (Serial.available() > 0) {
        const char c = static_cast<char>(Serial.read());
        if (c == '\r') {
            continue;
        }
        if (c == '\n') {
            _serialLineBuffer[_serialLineLength] = '\0';
            if (_serialLineLength > 0) {
                handleSerialLine(_serialLineBuffer);
            }
            _serialLineLength = 0;
            continue;
        }
        if (_serialLineLength < sizeof(_serialLineBuffer) - 1) {
            _serialLineBuffer[_serialLineLength++] = c;
        }
    }
}

void Node::handleSerialLine(const char* line) {
    if (startsWithTokenIgnoreCase(line, "RB rebase force")) {
        _rbBaselineState = RBBaselineState::Rebase;
        performRbRebase();
        _rbBaselineState = RBBaselineState::Settle;
        _rbBaselineSettleUntilMs = millis() + kRbPostRebaseSettleMs;
        Serial.println("RB rebase forced");
        return;
    }
    if (startsWithTokenIgnoreCase(line, "RB rebase")) {
        if (_sourceKind == AudioSourceKind::I2S) {
            startRbQuietBaseline();
            Serial.println("RB rebase requested quiet-gated");
        } else {
            performRbRebase();
            Serial.println("RB rebase done");
        }
        return;
    }
    if (startsWithTokenIgnoreCase(line, "RB detectonly")) {
        const char* mode = line + 13;
        while (*mode == ' ') {
            ++mode;
        }
        if (equalsIgnoreCase(mode, "on")) {
            _rbDetectOnly = true;
            _behavior.setDetectionOnly(true);
            Serial.println("RB detectonly=on");
        } else if (equalsIgnoreCase(mode, "off")) {
            _rbDetectOnly = false;
            _behavior.setDetectionOnly(false);
            Serial.println("RB detectonly=off");
        } else {
            Serial.print("RB detectonly=");
            Serial.println(_rbDetectOnly ? "on" : "off");
        }
        return;
    }
    if (startsWithTokenIgnoreCase(line, "RB log")) {
        handleLogCommand(line);
        return;
    }
    if (equalsIgnoreCase(line, "RB summary") || equalsIgnoreCase(line, "RB stop")) {
        printRbSummary();
        return;
    }
    if (startsWithTokenIgnoreCase(line, "RB debug")) {
        handleDebugCommand(line);
        return;
    }
}

void Node::handleDebugCommand(const char* line) {
    const char* mode = line + 8;
    while (*mode == ' ') {
        ++mode;
    }

    if (equalsIgnoreCase(mode, "off")) {
        _debug.setDebugMode(NodeDebug::DebugMode::Off);
    } else if (equalsIgnoreCase(mode, "events")) {
        _debug.setDebugMode(NodeDebug::DebugMode::Events);
    } else if (equalsIgnoreCase(mode, "plot")) {
        _debug.setDebugMode(NodeDebug::DebugMode::Plot);
    } else {
        Serial.print("RB debug mode=");
        Serial.print(_debug.debugModeName());
        Serial.println(" (use off|events|plot)");
        return;
    }

    Serial.print("RB debug mode=");
    Serial.println(_debug.debugModeName());
}

void Node::handleLogCommand(const char* line) {
    const char* mode = line + 6;
    while (*mode == ' ') {
        ++mode;
    }

    if (equalsIgnoreCase(mode, "off") || equalsIgnoreCase(mode, "minimal")) {
        _rbLogMode = RbLogMode::Minimal;
    } else if (equalsIgnoreCase(mode, "full")) {
        _rbLogMode = RbLogMode::Full;
    } else {
        Serial.print("RB log mode=");
        Serial.print(rbLogModeName());
        Serial.println(" (use off|minimal|full)");
        return;
    }

    Serial.print("RB log mode=");
    Serial.println(rbLogModeName());
}

bool Node::rbShouldLogDetail() const {
    return _rbLogMode == RbLogMode::Full;
}

const char* Node::rbLogModeName() const {
    return _rbLogMode == RbLogMode::Full ? "full" : "minimal";
}

DetectionPipeline::FrequencyEvidence Node::captureFrequencyEvidence() const {
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

void Node::logCandidate(const DetectorCandidate& candidate, const DetectionPipeline::PatternResult& patternResult, unsigned long candidateNumber, long gapMs, unsigned long queueDepthBeforeDrain, unsigned long behaviorLagMs, const char* candidateClass, const char* action, const char* stateName, const char* gateName) {
    Serial.print("RB candidate n=");
    Serial.print(candidateNumber);
    Serial.print(" gap=");
    if (gapMs >= 0) {
        Serial.print(gapMs);
        Serial.print("ms");
    } else {
        Serial.print("-");
    }
    Serial.print(" dur=");
    Serial.print(candidate.durationMs);
    Serial.print(" end_dt_ms=");
    if (candidate.onsetMillisApprox > 0) {
        Serial.print(candidate.onsetMillisApprox + candidate.durationMs);
        Serial.print("ms");
    } else {
        Serial.print("-");
    }
    Serial.print(" processed_at_ms=");
    Serial.print(patternResult.processedAtMs);
    Serial.print(" process_lag_ms=");
    if (patternResult.processedAtMs >= candidate.onsetMillisApprox) {
        Serial.print(patternResult.processedAtMs - candidate.onsetMillisApprox);
        Serial.print("ms");
    } else {
        Serial.print("-");
    }
    Serial.print(" behavior_lag_ms=");
    Serial.print(behaviorLagMs);
    Serial.print(" queue_before=");
    Serial.print(queueDepthBeforeDrain);
    Serial.print(" strength=");
    Serial.print(candidate.peakStrength, 1);
    Serial.print(" audio=");
    Serial.print(candidate.audioOverflowDuringCandidate ? "overflow" : "ok");
    Serial.print(" pattern=");
    Serial.print(DetectionPipeline::patternTypeName(patternResult.type));
    Serial.print(" candidate_class=");
    Serial.print(candidateClass);
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
    Serial.print(" freq_target_power=");
    Serial.print(patternResult.candidate.frequency.targetPower, 1);
    Serial.print(" freq_neighbor_power=");
    Serial.print(patternResult.candidate.frequency.neighborPower, 1);
    Serial.print(" freq_total_energy=");
    Serial.print(patternResult.candidate.frequency.totalEnergy, 1);
    Serial.print(" freq_contrast=");
    Serial.print(patternResult.candidate.frequency.spectralContrast, 1);
    Serial.print(" freq_observed_at_ms=");
    Serial.print(patternResult.candidate.frequency.observedAtMs);
    Serial.print(" freq_age_ms=");
    if (patternResult.candidate.frequency.observedAtMs > 0 && patternResult.processedAtMs >= patternResult.candidate.frequency.observedAtMs) {
        Serial.print(patternResult.processedAtMs - patternResult.candidate.frequency.observedAtMs);
        Serial.print("ms");
    } else {
        Serial.print("-");
    }
    Serial.print(" freq_valid_window=");
    Serial.print(patternResult.candidate.frequency.validWindow ? 1 : 0);
    Serial.print(" reason=");
    Serial.print(DetectionPipeline::patternReasonName(patternResult.reasonCode));
    Serial.print(" valid=");
    Serial.print(patternResult.valid ? 1 : 0);
    Serial.print(" action=");
    Serial.print(action);
    Serial.print(" state=");
    Serial.print(stateName);
    Serial.print(" gate=");
    Serial.println(gateName);
}

void Node::printRbSummary() const {
    const float avgStrength = _rbCandidateCount > 0 ? (static_cast<float>(_rbStrengthSumScaled) / 100.0f) / static_cast<float>(_rbCandidateCount) : 0.0f;
    const float avgDuration = _rbCandidateCount > 0 ? static_cast<float>(_rbDurationSumMs) / static_cast<float>(_rbCandidateCount) : 0.0f;
    const AudioSignalStats& stats = _audioSignal.stats();

    Serial.print("RB summary: candidates=");
    Serial.print(_rbCandidateCount);
    Serial.print(" actions=");
    Serial.print(_rbActionCount);
    Serial.print(" overflowCandidates=");
    Serial.print(_rbOverflowCandidates);
    Serial.print(" candidateDrops=");
    Serial.print(stats.candidatesDropped);
    Serial.print(" avg_strength=");
    Serial.print(avgStrength, 1);
    Serial.print(" avg_duration=");
    Serial.print(avgDuration, 1);
    Serial.print("ms detectOnly=");
    Serial.println(_rbDetectOnly ? 1 : 0);
    Serial.print("RB baseline state=");
    Serial.println(rbBaselineStateName());
    Serial.print("RB log mode=");
    Serial.println(rbLogModeName());

    printRbBehaviorSummary();
    printRbSignalSummary();
    printRbDetectorSummary();
}

void Node::printRbBehaviorSummary() const {
    Serial.print("RB behavior detectOnly=");
    Serial.print(_behavior.detectionOnly() ? 1 : 0);
    Serial.print(" lastDecision=");
    Serial.print(_behavior.lastDecisionName());
    Serial.print(" lastBlock=");
    Serial.print(_behavior.lastBlockReasonName());
    Serial.print(" lastPattern=");
    Serial.print(_behavior.lastPatternTypeName());
    Serial.print(" lastHeardMs=");
    Serial.print(_behavior.lastHeardMs());
    Serial.print(" lastEmitMs=");
    Serial.print(_behavior.lastEmitMs());
    Serial.print(" waitUntilMs=");
    Serial.print(_behavior.waitUntilMs());
    Serial.print(" refractoryUntilMs=");
    Serial.print(_behavior.refractoryUntilMs());
    Serial.print(" ignoreOwnEmitUntilMs=");
    Serial.print(_behavior.ignoreOwnEmitUntilMs());
    Serial.print(" outputBusy=");
    Serial.print(_behavior.outputBusy() ? 1 : 0);
    Serial.print(" patternsReceived=");
    Serial.print(_behavior.patternsReceived());
    Serial.print(" ignoredInvalid=");
    Serial.print(_behavior.patternsIgnoredInvalid());
    Serial.print(" ignoredAmbiguous=");
    Serial.print(_behavior.patternsIgnoredAmbiguous());
    Serial.print(" blockedDetectionOnly=");
    Serial.print(_behavior.blockedDetectionOnly());
    Serial.print(" blockedOutputBusy=");
    Serial.print(_behavior.blockedOutputBusy());
    Serial.print(" blockedWaiting=");
    Serial.print(_behavior.blockedWaiting());
    Serial.print(" blockedRefractory=");
    Serial.print(_behavior.blockedRefractory());
    Serial.print(" blockedSelfSuppressed=");
    Serial.print(_behavior.blockedSelfSuppressed());
    Serial.print(" wouldEmit=");
    Serial.print(_behavior.wouldEmitCount());
    Serial.print(" emitted=");
    Serial.println(_behavior.emittedCount());
}

void Node::printRbSignalSummary() const {
    Serial.print("RB signal baseline=");
    Serial.print(_audioSignal.baseline(), 1);
    Serial.print(" mag=");
    Serial.print(_audioSignal.signalMagnitude());
    Serial.print(" smooth=");
    Serial.println(_audioSignal.smoothedSignalMagnitude());
}

void Node::printRbDetectorSummary() const {
    Serial.print("RB det mode=AMP onset=");
    Serial.print(_audioSignal.onsetDetectionThreshold(), 1);
    Serial.print(" release=");
    Serial.print(_audioSignal.onsetReleaseThreshold(), 1);
    Serial.print(" cooldown=");
    Serial.print(_audioSignal.cooldownAfterOnsetMs());
    Serial.print(" minMs=");
    Serial.print(_audioSignal.minTransientDurationMs());
    Serial.print(" maxMs=");
    Serial.print(_audioSignal.maxTransientDurationMs());
    Serial.print(" minStrength=");
    Serial.println(_audioSignal.minTransientPeakStrength(), 1);

    Serial.print("RB det: tooShort=");
    Serial.print(_audioSignal.transientRejectedDurationTooShortCount());
    Serial.print(" tooLong=");
    Serial.print(_audioSignal.transientRejectedDurationTooLongCount());
    Serial.print(" weak=");
    Serial.print(_audioSignal.transientRejectedStrengthTooLowCount());
    Serial.print(" lastReject=");
    Serial.print(_audioSignal.lastTransientRejectReasonName());
    Serial.print(" lastRejectDur=");
    Serial.print(_audioSignal.lastTransientRejectedDurationMs());
    Serial.print(" lastRejectStrength=");
    Serial.println(_audioSignal.lastTransientRejectedStrength(), 1);
}
