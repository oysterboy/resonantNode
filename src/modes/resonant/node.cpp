#include "node.h"
#include <Arduino.h>
#include <string.h>

#include <esp_system.h>

#include "../../detection/DetectorParameters.h"
#include "../../detection/features/FrequencyMatchEvaluation.h"
#include "../../detection/inspector/FrequencyWindowProbe.h"
#include "../../detection/patterns/PatternNames.h"

#ifndef CHIRP_FREQUENCY_HZ
#define CHIRP_FREQUENCY_HZ 3200
#endif

/*
Node

Owns orchestration for the Resonant node.

Responsibilities:
- wire hardware sources and outputs
- feed audio into signal and detection layers
- pass PatternResult objects into ResonantBehavior
- start and finish chirps when behavior requests them
- manage startup baseline state for the I2S path
- handle serial commands, logging, and summary reporting

Does NOT:
- implement the behavior state machine
- classify patterns
- generate waveforms
- own detector thresholds as core behavior logic

File structure:
- local helpers and constants
- constructors
- lifecycle
- baseline / startup
- parameter setup
- main update loop
- serial command handling
- logging and summary helpers
*/

namespace {
constexpr int kMaxSamplesPerLoop = 128;
constexpr int kRbStartupQuietThreshold = 20;
constexpr unsigned long kRbStartupQuietHoldMs = 1000;
constexpr unsigned long kRbStartupBaselineTimeoutMs = 8000;
constexpr unsigned long kRbPostRebaseSettleMs = 500;
constexpr unsigned long kLiveFrequencyReleaseDebounceMs = 20UL;
constexpr unsigned long kLiveFrequencyCooldownAfterOnsetMs = 300UL;
constexpr unsigned long kLiveFrequencyMinTransientDurationMs = 50UL;

uint32_t sampleOffsetUs(uint32_t sampleOffset, uint32_t sampleRateHz) {
    if (sampleRateHz == 0) {
        return 0;
    }

    return static_cast<uint32_t>((static_cast<uint64_t>(sampleOffset) * 1000000ULL) / static_cast<uint64_t>(sampleRateHz));
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

BehaviorGateConfig makeFreqAmpBehaviorProfile() {
    BehaviorGateConfig profile;
    profile.idleEnabled = true;
    profile.waitAfterTransientMs = 100;
    profile.refractoryAfterEmitMs = 0;
    profile.idleTimeoutMs = 20000;
    profile.idleTimeVariationMs = 10000;
    profile.idleBlockedAfterHeardMs = 3000;
    profile.idleBlockedAfterOwnEmitMs = 5000;
    return profile;
}

const char* detectionProfileKindName(detection::DetectionProfileKind kind) {
    return detection::detectionProfileName(kind);
}

void printProfileComposition(const detection::DetectionProfile& profile) {
    Serial.print(" emitters=");
    Serial.print(detection::profileSignalEmitterName(profile.signalEmitter));
    Serial.print(" inspectionRules=");
    Serial.print(detection::profileInspectionRulesName(profile.inspectionRules));
    Serial.print(" patternRules=");
    Serial.print(detection::profilePatternRulesName(profile.patternRules));
    Serial.print(" fieldStateConfig=");
    Serial.print(profile.fieldStateConfig.signalWindowMs);
    Serial.print("/");
    Serial.print(profile.fieldStateConfig.patternWindowMs);
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

const char* h3RbCandidateClassName(const detection::PatternResult& patternResult, ResonantBehavior::BehaviorDecision behaviorDecision, bool selfChirpSuppressed) {
    if (selfChirpSuppressed || behaviorDecision == ResonantBehavior::BehaviorDecision::SelfSuppressed) {
        return "self_suppressed";
    }
    if (!patternResult.valid) {
        return "unexpected_noise";
    }
    return "expected_primary";
}

void printH3FrequencyEvidenceFields(const detection::PatternResult& patternResult,
                                    const detection::FrequencyEvidence& frequencyEvidence,
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
    Serial.print(frequencyEvidence.present ? 1 : 0);
    Serial.print(" freq_matched=");
    Serial.print(frequencyEvidence.matched ? 1 : 0);
    Serial.print(" freq_score_ok=");
    Serial.print(frequencyEval.scoreOk ? 1 : 0);
    Serial.print(" freq_contrast_ok=");
    Serial.print(frequencyEval.contrastOk ? 1 : 0);
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
    Serial.print(" freq_eval_reason=");
    Serial.print(FrequencyMatchEvaluation::reasonName(frequencyEval.reason));
}
}

// --- constructors ---

Node::Node(int inputPin, int ledPin, int chirpPin, int chirpBtlPin)
    : _ledPin(ledPin),
      _i2sSource(14, 27, 33, 16000, 32),
      _audioSource(_i2sSource),
      _audioOnsetDetector(),
      _audioSignal(_audioSource),
      _freqBandStream(),
      _toneOutput(chirpPin),
      _toneOutputBTL(chirpPin, chirpBtlPin),
      _chirpOutput(chirpBtlPin >= 0
                       ? static_cast<ToneOutput&>(_toneOutputBTL)
                       : static_cast<ToneOutput&>(_toneOutput)) {}

// --- lifecycle ---

void Node::begin() {
    randomSeed(esp_random() ^ millis());
    configureParameters();
    _audioSource.begin();
    _audioSignal.begin(false);
    _audioOnsetDetector.begin();
    _freqBandStream.resetState();
    _freqBandStream.setSampleRateHz(_audioSource.sampleRateHz());
    _freqBandStream.setTargetFrequencyHz(_chirpOutput.toneHz());
    _detection.reset();
    applyActiveProfiles();
    _behavior.resetState();
    resetDetectionState();
    _audioSignal.resetStats();
    _audioSource.resetStats();
    resetRbCounters();
    _rbLastLoggedOnsetRejectCount = 0;
    _rbLastLoggedTransientRejectCount = 0;
    _rbBaselineState = RBBaselineState::ListenForQuiet;
    _rbBaselineStateStartedMs = millis();
    _rbBaselineQuietSinceMs = 0;
    _rbBaselineLastLogMs = 0;
    _rbBaselineSettleUntilMs = 0;
    _rbLogMode = RbLogMode::Minimal;
    _rbLastWouldEmitHeardMs = 0;
    _rbLastWouldEmitDecision = ResonantBehavior::BehaviorDecision::None;
    _chirpOutput.begin();
    _debug.begin(_ledPin);
    _behavior.seedIdleSchedule(millis());
    _debug.markLoopStart(micros());
    _serialLineLength = 0;
    _serialLineBuffer[0] = '\0';

    Serial.print("RB PROFILE name=");
    Serial.print(profileName());
    printProfileComposition(activeDetectionProfile());
    Serial.println();

    Serial.print("RB det mode=AMP onset=");
    Serial.print(_audioOnsetDetector.onsetDetectionThreshold(), 1);
    Serial.print(" release=");
    Serial.print(_audioOnsetDetector.onsetReleaseThreshold(), 1);
    Serial.print(" cooldown=");
    Serial.print(_audioOnsetDetector.cooldownAfterOnsetMs());
    Serial.print(" releaseDebounce=");
    Serial.print(_audioOnsetDetector.releaseDebounceMs());
    Serial.print(" minMs=");
    Serial.print(_audioOnsetDetector.minTransientDurationMs());
    Serial.print(" maxMs=");
    Serial.print(_audioOnsetDetector.maxTransientDurationMs());
    Serial.print(" minStrength=");
    Serial.println(_audioOnsetDetector.minTransientPeakStrength(), 1);
    Serial.print("RB startup baseline=");
    Serial.println(rbBaselineStateName());
}

void Node::startRbQuietBaseline() {
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
    _audioSignal.resetSignalState();
    _audioOnsetDetector.resetState();
    _detection.reset();
    applyActiveProfiles();
    _wasSelfChirpSuppressed = false;
    _rbLastLoggedOnsetRejectCount = 0;
    _rbLastLoggedTransientRejectCount = 0;
}

void Node::performRbRebase() {
    _audioSignal.rebase();
    _behavior.resetState();
    resetDetectionState();
    _audioSignal.resetStats();
    _audioSource.resetStats();
    resetRbCounters();
    _rbLastLoggedOnsetRejectCount = 0;
    _rbLastLoggedTransientRejectCount = 0;
}

// --- baseline / startup ---

bool Node::rbOutputsEnabled() const {
    return _rbBaselineState == RBBaselineState::Active
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

// --- parameter setup ---

void Node::configureParameters() {
    configureI2SParameters();
}

void Node::configureI2SParameters() {
    // I2S/audio conditioning and detector tuning live here.
    _audioSignal.setSmoothingFactor(0.5f);
    _audioSignal.setBaselineUpdateFactor(0.005f);
    _frequencyEvidenceTuning.scoreMin = 10000.0f;
    _frequencyEvidenceTuning.contrastMin = 50.0f;

    // Chirp tone is configured here so the node owns its output tuning.
    _chirpOutput.setToneHz(CHIRP_FREQUENCY_HZ);
    _freqBandStream.setTargetFrequencyHz(_chirpOutput.toneHz());
    _audioSignal.setBaselineTrackingQuietThreshold(20);

    _audioOnsetDetector.setOnsetDetectionThreshold(23.0f);
    _audioOnsetDetector.setOnsetReleaseThreshold(20.0f);
    _audioOnsetDetector.setCooldownAfterOnsetMs(10);
    _audioOnsetDetector.setReleaseDebounceMs(30);
    _audioOnsetDetector.setMinTransientDurationMs(60);
    _audioOnsetDetector.setMaxTransientDurationMs(240);
    _audioOnsetDetector.setMinTransientPeakStrength(40.0f);
    // Behavior timing comes from the active profiles via applyActiveProfiles().
}

// --- main update loop ---

void Node::update() {
    const unsigned long now = millis();
    const unsigned long nowUs = micros();
    updateRbBaselineState(now);
    const bool rbOutputsEnabledNow = rbOutputsEnabled();
    const bool selfChirpSuppressed = _behavior.behaviorSuppressed(now);
    _wasSelfChirpSuppressed = selfChirpSuppressed;

    _debug.noteCoreLoopUs(nowUs);
    pollSerialCommands();

    // Update input first, then detection, then behavior, so each layer sees the
    // latest state from the layer below it.
    int processedSamples = 0;
    bool sawI2SSample = false;
    bool sawPatternThisLoop = false;
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
            processDetectionFrame(frame, now, selfChirpSuppressed, sawPatternThisLoop);
        }
        sawI2SSample = true;

        processedSamples += static_cast<int>(block.sampleCount);
        if (processedSamples > kMaxSamplesPerLoop) {
            processedSamples = kMaxSamplesPerLoop;
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
        Serial.print(" behaviorSuppressMs=");
        Serial.print(_behavior.behaviorSuppressRemainingMs(now));
        Serial.print(" outputBusy=");
        Serial.print(_behavior.outputBusy() ? 1 : 0);
        Serial.print(" wouldEmit=");
        Serial.println(1);
        _rbLastWouldEmitHeardMs = _behavior.lastHeardMs();
        _rbLastWouldEmitDecision = _behavior.lastDecision();
    }
    _debug.observeBehaviorGate(now, _behavior, sawPatternThisLoop, selfChirpSuppressed);

    if (sawI2SSample) {
        _debug.observeI2SSignal(now, _audioSignal);
    }

    if (rbOutputsEnabledNow && _behavior.shouldStartChirp()) {
        const auto chirpPattern = _behavior.chirpPattern();
        const char* sourceName = _behavior.chirpRequestSourceName();
        if (rbShouldLogDetail()) {
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
        }
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

// --- serial command handling ---

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

// --- command handlers ---

void Node::handleSerialLine(const char* line) {
    if (equalsIgnoreCase(line, "RB help")) {
        Serial.println("RB CMD: RB PARAM onset=23 release=20 cooldown=50 releaseDebounce=10 minMs=90 maxMs=240 minStrength=40.0 freqScore=10000 freqContrast=50.0");
        Serial.println("RB CMD: RB BEHAV wait=100 refractory=0 idleTimeout=20000 idleTimeoutVariation=10000 idleBlockedAfterHeard=3000 idleBlockedAfterOwnEmit=5000");
        Serial.println("RB CMD: RB DETECT removed; use RB PROFILE");
        Serial.println("RB CMD: RB PROFILE name=freqamp|chirp");
        Serial.println("RB CMD: RB rebase");
        Serial.println("RB CMD: RB rebase force");
        Serial.println("RB CMD: RB log off|minimal|full");
        Serial.println("RB CMD: RB debug off|events|plot");
        Serial.println("RB CMD: RB summary");
        Serial.println("RB CMD: RB stop");
        return;
    }
    if (startsWithTokenIgnoreCase(line, "RB PROFILE")) {
        handleProfileCommand(line);
        return;
    }
    if (startsWithTokenIgnoreCase(line, "RB DETECT")) {
        handleDetectCommand(line);
        return;
    }
    if (startsWithTokenIgnoreCase(line, "RB PARAM")) {
        char buffer[96];
        strncpy(buffer, line, sizeof(buffer));
        buffer[sizeof(buffer) - 1] = '\0';

        char* savePtr = nullptr;
        char* token = strtok_r(buffer, " ", &savePtr);
        token = token != nullptr ? strtok_r(nullptr, " ", &savePtr) : nullptr;

        if (token == nullptr || !equalsIgnoreCase(token, "PARAM")) {
            Serial.println("RB PARAM usage=RB PARAM onset=23 release=20 cooldown=50 releaseDebounce=10 minMs=90 maxMs=240 minStrength=40.0 freqScore=10000 freqContrast=50.0");
            return;
        }

        DetectorParameters::Values params = DetectorParameters::capture(_audioOnsetDetector);
        FrequencyMatchEvaluation::Values freqTuning = _frequencyEvidenceTuning;

        while ((token = strtok_r(nullptr, " ", &savePtr)) != nullptr) {
            DetectorParameters::parseToken(token, params);
            FrequencyMatchEvaluation::parseToken(token, freqTuning);
        }

        DetectorParameters::apply(params, _audioOnsetDetector);
        _frequencyEvidenceTuning = freqTuning;
        _detection.setFrequencyTuning(_frequencyEvidenceTuning);
        applyActiveProfiles();

        Serial.print("RB PARAM onset=");
        Serial.print(_audioOnsetDetector.onsetDetectionThreshold(), 1);
        Serial.print(" release=");
        Serial.print(_audioOnsetDetector.onsetReleaseThreshold(), 1);
        Serial.print(" cooldown=");
        Serial.print(_audioOnsetDetector.cooldownAfterOnsetMs());
        Serial.print(" releaseDebounce=");
        Serial.print(_audioOnsetDetector.releaseDebounceMs());
        Serial.print(" minMs=");
        Serial.print(_audioOnsetDetector.minTransientDurationMs());
        Serial.print(" maxMs=");
        Serial.print(_audioOnsetDetector.maxTransientDurationMs());
        Serial.print(" minStrength=");
        Serial.print(_audioOnsetDetector.minTransientPeakStrength(), 1);
        Serial.print(" freqScore=");
        Serial.print(_frequencyEvidenceTuning.scoreMin, 0);
        Serial.print(" freqContrast=");
        Serial.println(_frequencyEvidenceTuning.contrastMin, 1);
        return;
    }
    if (startsWithTokenIgnoreCase(line, "RB BEHAV")) {
        char buffer[96];
        strncpy(buffer, line, sizeof(buffer));
        buffer[sizeof(buffer) - 1] = '\0';

        char* savePtr = nullptr;
        char* token = strtok_r(buffer, " ", &savePtr);
        token = token != nullptr ? strtok_r(nullptr, " ", &savePtr) : nullptr;

        if (token == nullptr || !equalsIgnoreCase(token, "BEHAV")) {
            Serial.println("RB BEHAV usage=RB BEHAV wait=100 refractory=0 idleTimeout=20000 idleTimeoutVariation=10000 idleBlockedAfterHeard=3000 idleBlockedAfterOwnEmit=5000");
            return;
        }

        unsigned long waitAfterTransientMs = _behavior.waitAfterTransientMs();
        unsigned long refractoryAfterEmitMs = _behavior.refractoryAfterEmitMs();
        unsigned long idleTimeoutMs = _behavior.idleTimeoutMs();
        unsigned long idleTimeoutVariationMs = _behavior.idleTimeVariationMs();
        unsigned long idleBlockedAfterHeardMs = _behavior.idleBlockedAfterHeardMs();
        unsigned long idleBlockedAfterOwnEmitMs = _behavior.idleBlockedAfterOwnEmitMs();
        bool idleEnabled = _behavior.idleEnabled();

        while ((token = strtok_r(nullptr, " ", &savePtr)) != nullptr) {
            if (startsWithTokenIgnoreCase(token, "wait=") || startsWithTokenIgnoreCase(token, "waitMs=")) {
                const char* value = token + (startsWithTokenIgnoreCase(token, "waitMs=") ? 7 : 5);
                waitAfterTransientMs = strtoul(value, nullptr, 10);
            } else if (startsWithTokenIgnoreCase(token, "refractory=") || startsWithTokenIgnoreCase(token, "refractoryMs=")) {
                const char* value = token + (startsWithTokenIgnoreCase(token, "refractoryMs=") ? 13 : 11);
                refractoryAfterEmitMs = strtoul(value, nullptr, 10);
            } else if (startsWithTokenIgnoreCase(token, "idleTimeout=") || startsWithTokenIgnoreCase(token, "idle=") || startsWithTokenIgnoreCase(token, "idleMs=") || startsWithTokenIgnoreCase(token, "idleTimeoutMs=")) {
                const char* value = token + (startsWithTokenIgnoreCase(token, "idleTimeoutMs=") ? 14 : startsWithTokenIgnoreCase(token, "idleMs=") ? 7 : startsWithTokenIgnoreCase(token, "idleTimeout=") ? 12 : 5);
                idleTimeoutMs = strtoul(value, nullptr, 10);
            } else if (startsWithTokenIgnoreCase(token, "idleTimeoutVariation=") || startsWithTokenIgnoreCase(token, "idleVar=") || startsWithTokenIgnoreCase(token, "idleVariation=")) {
                const char* value = token + (startsWithTokenIgnoreCase(token, "idleTimeoutVariation=") ? 21 : startsWithTokenIgnoreCase(token, "idleVariation=") ? 14 : 8);
                idleTimeoutVariationMs = strtoul(value, nullptr, 10);
            } else if (startsWithTokenIgnoreCase(token, "idleBlockedAfterHeard=") || startsWithTokenIgnoreCase(token, "idleBlockedHeardMs=")) {
                const char* value = token + (startsWithTokenIgnoreCase(token, "idleBlockedAfterHeard=") ? 22 : 19);
                idleBlockedAfterHeardMs = strtoul(value, nullptr, 10);
            } else if (startsWithTokenIgnoreCase(token, "idleBlockedAfterOwnEmit=") || startsWithTokenIgnoreCase(token, "idleBlockedOwnEmitMs=")) {
                const char* value = token + (startsWithTokenIgnoreCase(token, "idleBlockedAfterOwnEmit=") ? 24 : 21);
                idleBlockedAfterOwnEmitMs = strtoul(value, nullptr, 10);
            } else if (startsWithTokenIgnoreCase(token, "idleEnabled=")) {
                const char* value = token + 12;
                idleEnabled = equalsIgnoreCase(value, "1") || equalsIgnoreCase(value, "on") || equalsIgnoreCase(value, "true");
            }
        }

        _behavior.setWaitAfterTransientMs(waitAfterTransientMs);
        _behavior.setRefractoryAfterEmitMs(refractoryAfterEmitMs);
        _behavior.setIdleTimeoutMs(idleTimeoutMs);
        _behavior.setIdleTimeVariationMs(idleTimeoutVariationMs);
        _behavior.setIdleBlockedAfterHeardMs(idleBlockedAfterHeardMs);
        _behavior.setIdleBlockedAfterOwnEmitMs(idleBlockedAfterOwnEmitMs);
        _behavior.setIdleEnabled(idleEnabled);

        Serial.print("RB BEHAV wait=");
        Serial.print(_behavior.waitAfterTransientMs());
        Serial.print(" refractory=");
        Serial.print(_behavior.refractoryAfterEmitMs());
        Serial.print(" idleTimeout=");
        Serial.print(_behavior.idleTimeoutMs());
        Serial.print(" idleTimeoutVariation=");
        Serial.print(_behavior.idleTimeVariationMs());
        Serial.print(" idleBlockedHeard=");
        Serial.print(_behavior.idleBlockedAfterHeardMs());
        Serial.print(" idleBlockedOwnEmit=");
        Serial.print(_behavior.idleBlockedAfterOwnEmitMs());
        Serial.print(" idleEnabled=");
        Serial.println(_behavior.idleEnabled() ? 1 : 0);
        return;
    }
    if (startsWithTokenIgnoreCase(line, "RB rebase force")) {
        _rbBaselineState = RBBaselineState::Rebase;
        performRbRebase();
        _rbBaselineState = RBBaselineState::Settle;
        _rbBaselineSettleUntilMs = millis() + kRbPostRebaseSettleMs;
        Serial.println("RB rebase forced");
        return;
    }
    if (startsWithTokenIgnoreCase(line, "RB rebase")) {
        startRbQuietBaseline();
        Serial.println("RB rebase requested quiet-gated");
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

void Node::handleDetectCommand(const char* line) {
    (void)line;
    Serial.println("RB DETECT removed; use RB PROFILE");
}

void Node::handleProfileCommand(const char* line) {
    const char* name = line + 11;
    while (*name == ' ') {
        ++name;
    }

    if (*name == '\0') {
        Serial.print("RB PROFILE name=");
        Serial.println(profileName());
        return;
    }

    if (startsWithTokenIgnoreCase(name, "name=")) {
        name += 5;
    }

    if (setProfileFromName(name)) {
        Serial.print("RB PROFILE name=");
        Serial.print(profileName());
        printProfileComposition(activeDetectionProfile());
        Serial.println();
    } else {
        Serial.println("RB PROFILE usage=name=freqamp|chirp");
    }
}

bool Node::setProfileFromName(const char* name) {
    detection::DetectionProfileKind parsed = _profileKind;
    if (!detection::detectionProfileKindFromName(name, parsed)) {
        return false;
    }

    _profileKind = parsed;
    applyActiveProfiles();
    return true;
}

const char* Node::profileName() const {
    return detectionProfileKindName(_profileKind);
}

const detection::DetectionProfile& Node::activeDetectionProfile() const {
    return detection::detectionProfileForKind(_profileKind);
}

const BehaviorGateConfig& Node::activeBehaviorProfile() const {
    static const BehaviorGateConfig kFreqAmpProfile = makeFreqAmpBehaviorProfile();

    switch (_profileKind) {
        case detection::DetectionProfileKind::Chirp:
        case detection::DetectionProfileKind::FreqAmp:
        default:
            return kFreqAmpProfile;
    }
}

void Node::applyActiveProfiles() {
    const detection::DetectionProfile& detectionProfile = activeDetectionProfile();
    const BehaviorGateConfig& behaviorProfile = activeBehaviorProfile();

    _detection.setSignalEmitter(detectionProfile.signalEmitter);
    _detection.setInspectionRules(detectionProfile.inspectionRules);
    _detection.setFrequencyTuning(_frequencyEvidenceTuning);
    _detection.setInspectionConfig(detectionProfile.inspectionConfig);
    _detection.setPatternRulesConfig(detectionProfile.patternRulesConfig);
    _detection.setFieldStateConfig(detectionProfile.fieldStateConfig);
    _detection.setProfileName(detection::detectionProfileName(detectionProfile.kind));
    _freqBandStream.setSampleRateHz(_audioSource.sampleRateHz());
    _freqBandStream.setTargetFrequencyHz(_chirpOutput.toneHz());

    _behavior.configure(behaviorProfile);
}

void Node::processDetectionFrame(const AudioSignalFrame& frame,
                              unsigned long now,
                              bool selfChirpSuppressed,
                              bool& sawPatternThisLoop) {
    const auto liveFrequencyEvidence = captureFrequencyEvidence(frame.sampleTimeMs);
    _detection.observeFrame(frame, liveFrequencyEvidence, frame.sampleTimeMs);

    detection::PatternResult patternResult;
    bool emittedPattern = false;
    while (_detection.popPatternResult(patternResult)) {
        emittedPattern = true;
        const auto behaviorDecision = _behavior.handlePatternResult(patternResult, _detection.fieldState(), now);
        if (behaviorDecision == ResonantBehavior::BehaviorDecision::ConsumedPattern) {
            sawPatternThisLoop = true;
        }

        ++_rbCandidateCount;
        if (patternResult.patternCandidateAccepted) {
            ++_rbActionCount;
        }

        if (patternResult.candidate.audioOverflowDuringCandidate) {
            ++_rbOverflowCandidates;
        }
        _rbStrengthSumScaled += static_cast<unsigned long>(patternResult.candidate.peakStrength * 100.0f);
        _rbDurationSumMs += patternResult.candidate.durationMs;
        _rbHaveLastCandidateMs = true;
        _rbLastCandidateMs = patternResult.candidate.heardAtMs;

        if (rbShouldLogDetail()) {
            Serial.print("RB pattern=");
            Serial.print(detection::patternTypeName(patternResult.type));
            Serial.print(" fieldQuiet=");
            Serial.print(_detection.fieldState().quiet ? 1 : 0);
            Serial.print(" fieldActive=");
            Serial.print(_detection.fieldState().active ? 1 : 0);
            Serial.print(" fieldDense=");
            Serial.print(_detection.fieldState().dense ? 1 : 0);
            Serial.print(" behaviorEligible=");
            Serial.print(_behavior.behaviorEligible() ? 1 : 0);
            Serial.print(" decision=");
            Serial.print(_behavior.lastDecisionName());
            Serial.print(" valid=");
            Serial.print(patternResult.valid ? 1 : 0);
            Serial.print(" profile=");
            Serial.println(profileName());
        }

    }

}

// --- logging / summaries ---

bool Node::rbShouldLogDetail() const {
    return _rbLogMode == RbLogMode::Full;
}

const char* Node::rbLogModeName() const {
    return _rbLogMode == RbLogMode::Full ? "full" : "off";
}

detection::FrequencyEvidence Node::captureFrequencyEvidence(unsigned long observedAtMs) const {
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

#if 0
void Node::logCandidate(const DetectorCandidate& candidate, const detection::PatternResult& patternResult, const detection::FrequencyEvidence* liveFrequencyEvidence, unsigned long candidateNumber, long gapMs, unsigned long queueDepthBeforeDrain, unsigned long behaviorLagMs, const char* candidateClass, const char* action, const char* stateName, const char* gateName) {
    const uint32_t sampleRateHz = _audioSource.sampleRateHz() > 0 ? _audioSource.sampleRateHz() : 16000UL;
    const unsigned long peakOffsetMs = candidate.peakSample >= candidate.onsetSample
        ? static_cast<unsigned long>(((candidate.peakSample - candidate.onsetSample) * 1000ULL) / static_cast<uint64_t>(sampleRateHz))
        : 0UL;
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
    Serial.print(" onset_sample=");
    Serial.print(candidate.onsetSample);
    Serial.print(" peak_sample=");
    Serial.print(candidate.peakSample);
    Serial.print(" release_sample=");
    Serial.print(candidate.releaseSample);
    Serial.print(" end_dt_ms=");
    if (candidate.onsetMillisApprox > 0) {
        Serial.print(candidate.onsetMillisApprox + candidate.durationMs);
        Serial.print("ms");
    } else {
        Serial.print("-");
    }
    Serial.print(" peak_ms=");
    Serial.print(candidate.onsetMillisApprox + peakOffsetMs);
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
    Serial.print(detection::patternTypeName(patternResult.type));
    Serial.print(" candidate_class=");
    Serial.print(candidateClass);
    Serial.print(" candidateAccepted=");
    Serial.print(patternResult.patternCandidateAccepted ? 1 : 0);
    Serial.print(" patternMatched=");
    Serial.print(patternResult.patternMatched ? 1 : 0);
    Serial.print(" supportMatched=");
    Serial.print(patternResult.supportMatched ? 1 : 0);
    Serial.print(" reject_reason=");
    Serial.print(detection::patternRejectReasonName(patternResult.rejectReason));
    Serial.print(" transient_present=");
    Serial.print(patternResult.candidate.transient.present ? 1 : 0);
    Serial.print(" freq_present=");
    Serial.print(patternResult.freq.present ? 1 : 0);
    Serial.print(" freq_matched=");
    Serial.print(patternResult.freq.matched ? 1 : 0);
    const auto freqEval = FrequencyMatchEvaluation::evaluate(patternResult.freq, _frequencyEvidenceTuning);
    Serial.print(" freq_score_ok=");
    Serial.print(freqEval.scoreOk ? 1 : 0);
    Serial.print(" freq_contrast_ok=");
    Serial.print(freqEval.contrastOk ? 1 : 0);
    Serial.print(" freq_score=");
    Serial.print(patternResult.freq.score, 1);
    Serial.print(" freq_conf=");
    Serial.print(patternResult.freq.confidence, 1);
    Serial.print(" freq_target_hz=");
    Serial.print(patternResult.freq.targetHz);
    Serial.print(" freq_target_power=");
    Serial.print(patternResult.freq.targetPower, 1);
    Serial.print(" freq_neighbor_power=");
    Serial.print(patternResult.freq.neighborPower, 1);
    Serial.print(" freq_total_energy=");
    Serial.print(patternResult.freq.totalEnergy, 1);
    Serial.print(" freq_contrast=");
    Serial.print(patternResult.freq.spectralContrast, 1);
    Serial.print(" freq_observed_at_ms=");
    Serial.print(patternResult.freq.observedAtMs);
    Serial.print(" freq_age_ms=");
    if (patternResult.freq.observedAtMs > 0 && patternResult.processedAtMs >= patternResult.freq.observedAtMs) {
        Serial.print(patternResult.processedAtMs - patternResult.freq.observedAtMs);
        Serial.print("ms");
    } else {
        Serial.print("-");
    }
    Serial.print(" freq_valid_window=");
    Serial.print(patternResult.freq.validWindow ? 1 : 0);
    Serial.print(" freq_eval_reason=");
    Serial.print(FrequencyMatchEvaluation::reasonName(freqEval.reason));
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
    Serial.print(" reason=");
    Serial.print(detection::patternReasonName(patternResult.reasonCode));
    Serial.print(" valid=");
    Serial.print(patternResult.valid ? 1 : 0);
    Serial.print(" action=");
    Serial.print(action);
    Serial.print(" state=");
    Serial.print(stateName);
    Serial.print(" gate=");
    Serial.println(gateName);
}
#endif

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
    Serial.print(" avg_strength=");
    Serial.print(avgStrength, 1);
    Serial.print(" avg_duration=");
    Serial.print(avgDuration, 1);
    Serial.print("ms detectOnly=");
    Serial.print(0);
    Serial.print(" profile=");
    Serial.print(profileName());
    Serial.println();
    Serial.print("RB baseline state=");
    Serial.println(rbBaselineStateName());
    Serial.print("RB log mode=");
    Serial.println(rbLogModeName());

    printRbBehaviorSummary();
    printRbSignalSummary();
    printRbDetectorSummary();
}

void Node::printRbBehaviorSummary() const {
    Serial.print("RB behavior idle=");
    Serial.print(_behavior.idleEnabled() ? 1 : 0);
    Serial.print(" wait=");
    Serial.print(_behavior.waitAfterTransientMs());
    Serial.print(" refractory=");
    Serial.print(_behavior.refractoryAfterEmitMs());
    Serial.print(" idleTimeout=");
    Serial.print(_behavior.idleTimeoutMs());
    Serial.print(" idleTimeoutVariation=");
    Serial.print(_behavior.idleTimeVariationMs());
    Serial.print(" idleBlockedHeard=");
    Serial.print(_behavior.idleBlockedAfterHeardMs());
    Serial.print(" idleBlockedOwnEmit=");
    Serial.print(_behavior.idleBlockedAfterOwnEmitMs());
    Serial.print(" idleEnabled=");
    Serial.print(_behavior.idleEnabled() ? 1 : 0);
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
    Serial.print(" ownEmitDetectionSuppressUntilMs=");
    Serial.print(_behavior.ownEmitDetectionSuppressUntilMs());
    Serial.print(" outputBusy=");
    Serial.print(_behavior.outputBusy() ? 1 : 0);
    Serial.print(" patternsReceived=");
    Serial.print(_behavior.patternsReceived());
    Serial.print(" ignoredInvalid=");
    Serial.print(_behavior.patternsIgnoredInvalid());
    Serial.print(" ignoredAmbiguous=");
    Serial.print(_behavior.patternsIgnoredAmbiguous());
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
    Serial.print(_audioOnsetDetector.onsetDetectionThreshold(), 1);
    Serial.print(" release=");
    Serial.print(_audioOnsetDetector.onsetReleaseThreshold(), 1);
    Serial.print(" cooldown=");
    Serial.print(_audioOnsetDetector.cooldownAfterOnsetMs());
    Serial.print(" releaseDebounce=");
    Serial.print(_audioOnsetDetector.releaseDebounceMs());
    Serial.print(" minMs=");
    Serial.print(_audioOnsetDetector.minTransientDurationMs());
    Serial.print(" maxMs=");
    Serial.print(_audioOnsetDetector.maxTransientDurationMs());
    Serial.print(" minStrength=");
    Serial.println(_audioOnsetDetector.minTransientPeakStrength(), 1);

    Serial.print("RB det: tooShort=");
    Serial.print(_audioOnsetDetector.transientRejectedDurationTooShortCount());
    Serial.print(" tooLong=");
    Serial.print(_audioOnsetDetector.transientRejectedDurationTooLongCount());
    Serial.print(" weak=");
    Serial.print(_audioOnsetDetector.transientRejectedStrengthTooLowCount());
    Serial.print(" lastReject=");
    Serial.print(_audioOnsetDetector.lastTransientRejectReasonName());
    Serial.print(" lastRejectDur=");
    Serial.print(_audioOnsetDetector.lastTransientRejectedDurationMs());
    Serial.print(" lastRejectStrength=");
    Serial.println(_audioOnsetDetector.lastTransientRejectedStrength(), 1);
}
