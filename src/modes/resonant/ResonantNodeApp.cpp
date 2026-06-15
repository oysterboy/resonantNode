#include "ResonantNodeApp.h"
#include <Arduino.h>
#include <string.h>

#include <esp_system.h>

#include "../../app/RuntimeDefaults.h"
#include "../../app/TimingUtils.h"
#include "../../detection/features/FrequencyMeasurementPacketBuilder.h"
#include "../../detection/detectors/frequency/FrequencyMatchCriteria.h"
#include "../../detection/patterns/PatternNames.h"

#ifndef BUILD_DATE
#define BUILD_DATE "unknown-date"
#endif

#ifndef BUILD_REV
#define BUILD_REV 0
#endif

/*
Node

Owns orchestration for the Resonant node.

Responsibilities:
- wire hardware sources and outputs
- feed audio into occurrence and detection layers
- pass PatternResult objects into ResonantBehavior
- start and finish chirps when behavior requests them
- manage startup baseline state for the I2S path
- handle serial commands, logging, and summary reporting

Does NOT:
- implement the behavior state machine
- classify patterns
- generate waveforms
- own probe thresholds as diagnostics, not core behavior logic

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
constexpr int kMaxSamplesPerLoop = 512;
constexpr int kRbStartupQuietThreshold = 20;
constexpr unsigned long kRbStartupQuietHoldMs = 1000;
constexpr unsigned long kRbStartupBaselineTimeoutMs = 8000;
constexpr unsigned long kRbPostRebaseSettleMs = 500;

uint32_t sampleOffsetUs(uint32_t sampleOffset, uint32_t sampleRateHz) {
    if (sampleRateHz == 0) {
        return 0;
    }

    return static_cast<uint32_t>((static_cast<uint64_t>(sampleOffset) * 1000000ULL) / static_cast<uint64_t>(sampleRateHz));
}

const char* evidenceTargetName(detection::EvidenceTarget value) {
    switch (value) {
        case detection::EvidenceTarget::SupportStrength:
            return "SupportStrength";
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
            case detection::EvidenceTarget::SupportStrength:
            default:
                return "support_strength";
        }
    }
    if (plan.count == 2 &&
        plan.modules[0].kind == detection::InspectionModuleKind::ScalarFeatureStrength &&
        plan.modules[0].target == detection::EvidenceTarget::FrequencyScoreStrength &&
        plan.modules[1].kind == detection::InspectionModuleKind::ScalarFeatureStrength &&
        plan.modules[1].target == detection::EvidenceTarget::FrequencyContrastQuality) {
        return "frequency_score_contrast";
    }

    return "custom";
}
}

static bool startsWithTokenIgnoreCase(const char* line, const char* token) {
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

static bool equalsIgnoreCase(const char* a, const char* b) {
    while (*a != '\0' && *b != '\0') {
        if (toupper(static_cast<unsigned char>(*a)) != toupper(static_cast<unsigned char>(*b))) {
            return false;
        }
        ++a;
        ++b;
    }

    return *a == '\0' && *b == '\0';
}

BehaviorGateConfig makeTonalPulseBehaviorProfile() {
    return BehaviorGateConfig{};
}

const char* detectionProfileKindName(detection::DetectionProfileKind kind) {
    return detection::detectionProfileName(kind);
}

void printProfileComposition(const detection::DetectionProfile& profile) {
    Serial.print(" emitters=");
    Serial.print(detection::detectorSelectionName(profile.detectorSelection));
    Serial.print(" inspectionPlan=");
    Serial.print(inspectionPlanName(profile.inspectionPlan));
    Serial.print(" fieldStateConfig=");
    Serial.print(profile.fieldStateConfig.occurrenceWindowMs);
    Serial.print("/");
    Serial.print(profile.fieldStateConfig.patternWindowMs);
}

void printDetectionProfileDetails(const detection::DetectionProfile& profile) {
    Serial.println("RB DETECT");
    Serial.print("  kind=");
    Serial.println(detection::detectionProfileName(profile.kind));
    Serial.print("  detectorSelection=");
    Serial.println(detection::detectorSelectionName(profile.detectorSelection));
    Serial.print("  inspectionPlan=");
    Serial.println(inspectionPlanName(profile.inspectionPlan));
    Serial.print("  freqMatch.releaseDebounceMs=");
    Serial.println(profile.frequencyMatch.releaseDebounceMs);
    Serial.print("  freqMatch.cooldownAfterReleaseMs=");
    Serial.println(profile.frequencyMatch.cooldownAfterReleaseMs);
    Serial.print("  freqMatch.minDurationMs=");
    Serial.println(profile.frequencyMatch.minDurationMs);
    Serial.print("  freqMatch.attackScoreMin=");
    Serial.println(profile.frequencyMatch.attackScoreMin, 0);
    Serial.print("  freqMatch.releaseScoreMin=");
    Serial.println(profile.frequencyMatch.releaseScoreMin, 0);
    Serial.print("  freqMatch.attackContrastMin=");
    Serial.println(profile.frequencyMatch.attackContrastMin, 1);
    Serial.print("  freqMatch.releaseContrastMin=");
    Serial.println(profile.frequencyMatch.releaseContrastMin, 1);
    for (size_t i = 0; i < profile.inspectionPlan.count; ++i) {
        const auto& module = profile.inspectionPlan.modules[i];
        Serial.print("  inspectionPlan.module[");
        Serial.print(static_cast<unsigned int>(i));
        Serial.print("].kind=");
        Serial.print(module.kind == detection::InspectionModuleKind::ScalarFeatureStrength ? "ScalarFeatureStrength" : "None");
        Serial.print(" target=");
        Serial.print(evidenceTargetName(module.target));
        if (module.kind == detection::InspectionModuleKind::ScalarFeatureStrength) {
            Serial.print(" stream=");
            Serial.print(module.scalar.stream == detection::FeatureStreamId::AmpEnvelope ? "AmpEnvelope" :
                         (module.scalar.stream == detection::FeatureStreamId::FrequencyScore ? "FrequencyScore" :
                         (module.scalar.stream == detection::FeatureStreamId::FrequencyContrast ? "FrequencyContrast" : "Unknown")));
            Serial.print(" windowPreMs=");
            Serial.print(module.scalar.windowPreMs);
            Serial.print(" windowPostMs=");
            Serial.println(module.scalar.windowPostMs);
        } else {
            Serial.println();
        }
    }
    Serial.print("  pattern.requireSupportForAcceptance=");
    Serial.println(profile.patternMatcherConfig.requireSupportForAcceptance ? 1 : 0);
    Serial.print("  pattern.requiredSupportTarget=");
    Serial.println(evidenceTargetName(profile.patternMatcherConfig.requiredSupportTarget));
    Serial.print("  fieldState.occurrenceWindowMs=");
    Serial.println(profile.fieldStateConfig.occurrenceWindowMs);
    Serial.print("  fieldState.patternWindowMs=");
    Serial.println(profile.fieldStateConfig.patternWindowMs);
    Serial.print("  fieldState.busyOccurrenceCountThreshold=");
    Serial.println(profile.fieldStateConfig.busyOccurrenceCountThreshold);
    Serial.print("  fieldState.denseOccurrenceCountThreshold=");
    Serial.println(profile.fieldStateConfig.denseOccurrenceCountThreshold);
    Serial.print("  fieldState.quietOccurrenceCountThreshold=");
    Serial.println(profile.fieldStateConfig.quietOccurrenceCountThreshold);
    Serial.print("  fieldState.quietActivityThreshold=");
    Serial.println(profile.fieldStateConfig.quietActivityThreshold, 2);
    Serial.print("  fieldState.busyActivityThreshold=");
    Serial.println(profile.fieldStateConfig.busyActivityThreshold, 2);
}

void printBehaviorRuntimeDetails(const ResonantBehavior& behavior) {
    Serial.println("RB BEHAV");
    Serial.print("  idleEnabled=");
    Serial.println(behavior.idleEnabled() ? 1 : 0);
    Serial.print("  waitAfterHeardMs=");
    Serial.println(behavior.waitAfterHeardMs());
    Serial.print("  refractoryAfterEmitMs=");
    Serial.println(behavior.refractoryAfterEmitMs());
    Serial.print("  behaviorSuppressSelfChirpMs=");
    Serial.println(behavior.behaviorSuppressSelfChirpMs());
    Serial.print("  detectionSuppressTailMsOwnEmit=");
    Serial.println(behavior.detectionSuppressTailMsOwnEmit());
    Serial.print("  idleTimeoutMs=");
    Serial.println(behavior.idleTimeoutMs());
    Serial.print("  idleTimeVariationMs=");
    Serial.println(behavior.idleTimeVariationMs());
    Serial.print("  idleBlockedAfterHeardMs=");
    Serial.println(behavior.idleBlockedAfterHeardMs());
    Serial.print("  idleBlockedAfterOwnEmitMs=");
    Serial.println(behavior.idleBlockedAfterOwnEmitMs());
}

void printBuildIdentity() {
    char nodeIdBuffer[17];
    const uint64_t nodeId = ESP.getEfuseMac();
    snprintf(nodeIdBuffer, sizeof(nodeIdBuffer), "0x%012llX", static_cast<unsigned long long>(nodeId));

    Serial.print(" build=");
    Serial.print(BUILD_DATE);
    Serial.print(" rev=");
    Serial.print(BUILD_REV);
    Serial.print(" node_id=");
    Serial.print(nodeIdBuffer);
}

const char* behaviorGateName(const ResonantBehavior& behavior, unsigned long now, bool patternDetected, bool selfChirpSuppressed) {
    if (selfChirpSuppressed) {
        return "self_ignore";
    }
    if (behavior.refractoryRemainingMs(now) > 0) {
        return "refractory";
    }
    if (behavior.waitRemainingMs(now) > 0) {
        return "wait";
    }
    if (patternDetected) {
        return "pattern";
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

// --- constructors ---

Node::Node(int inputPin, int ledPin, int chirpPin, int chirpBtlPin)
    : _ledPin(ledPin),
      _i2sSource(14, 27, 33, 16000, 32),
      _audioSource(_i2sSource),
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
    _freqBandStream.resetState();
    _freqBandStream.setSampleRateHz(_audioSource.sampleRateHz());
    _freqBandStream.setTargetFrequencyHz(_chirpOutput.toneHz());
    _detection.resetState();
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
    printBuildIdentity();
    Serial.println();
    printDetectionProfileDetails(activeDetectionProfile());
    Serial.println();
    printBehaviorRuntimeDetails(_behavior);
    Serial.println();
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
    _rbPendingCount = 0;
    _rbPatternAcceptedCount = 0;
    _rbValidPatternCount = 0;
    _rbChirpStartedCount = 0;
    _rbOverflowPending = 0;
    _rbLastPendingMs = 0;
    _rbHaveLastPendingMs = false;
    _rbStrengthSumScaled = 0;
    _rbDurationSumMs = 0;
}

void Node::resetDetectionState() {
    _audioSignal.resetSignalState();
    _detection.resetState();
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
                } else if (_rbBaselineLastLogMs == 0 || timing::elapsedSince(now, _rbBaselineLastLogMs, 1000UL)) {
                    _rbBaselineLastLogMs = now;
                    Serial.print("RB rebase waiting smooth=");
                    Serial.print(smooth, 1);
                    Serial.print(" quietMs=");
                    Serial.println(quietMs);
                }
            } else {
                _rbBaselineQuietSinceMs = 0;
                if (timing::elapsedSince(now, _rbBaselineStateStartedMs, kRbStartupBaselineTimeoutMs)) {
                    _rbBaselineState = RBBaselineState::FailedNoQuiet;
                    Serial.print("RB rebase skipped reason=no_quiet smooth=");
                    Serial.println(smooth, 1);
                }
            }
        } break;

        case RBBaselineState::Rebase:
            break;

        case RBBaselineState::Settle:
            if (timing::atOrAfter(now, _rbBaselineSettleUntilMs)) {
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
    // I2S/audio conditioning.
    _audioSignal.setSmoothingFactor(0.5f);
    _audioSignal.setBaselineUpdateFactor(0.005f);

    // Transitional node wiring: keep the active tone and detector target aligned here
    // while the node still runs frequency-based profiles.
    _chirpOutput.setToneHz(runtime::kDefaultChirpFrequencyHz);
    _freqBandStream.setTargetFrequencyHz(_chirpOutput.toneHz());
    _audioSignal.setBaselineTrackingQuietThreshold(20);
}

// --- main update loop ---

void Node::update() {
    const unsigned long now = millis();
    const unsigned long nowUs = micros();
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
            AudioSamplePacket audioSamplePacket;
            _audioSignal.update(static_cast<int>(block.samples[i]), sampleTimeUs, audioSamplePacket);
            const bool ownEmitSuppressed = audioSamplePacket.timeMs < _behavior.ownEmitDetectionSuppressUntilMs();
            if (!ownEmitSuppressed) {
                _freqBandStream.observeCenteredSample(audioSamplePacket.centeredAudioValue, audioSamplePacket.timeMs);
                processDetectionFrame(audioSamplePacket, now, selfChirpSuppressed, sawPatternThisLoop);
            }
        }
        sawI2SSample = true;

        processedSamples += static_cast<int>(block.sampleCount);
        if (processedSamples > kMaxSamplesPerLoop) {
            processedSamples = kMaxSamplesPerLoop;
        }
    }

    updateRbBaselineState(now);
    const bool rbOutputsEnabledNow = rbOutputsEnabled();

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
    ++_rbChirpStartedCount;
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
        Serial.println("RB CMD: RB PARAM freqScore=18000 freqContrast=50.0 freqReleaseScore=12000 freqReleaseContrast=50.0");
        Serial.println("RB CMD: RB BEHAV wait=100 refractory=0 suppressSelfChirp=250 detectionSuppressTail=0 idleTimeout=20000 idleTimeoutVariation=10000 idleBlockedAfterHeard=3000 idleBlockedAfterOwnEmit=5000");
        Serial.println("RB CMD: RB STATUS");
        Serial.println("RB CMD: RB PROFILE name=TonalPulseFreq");
        Serial.println("RB CMD(ALT): RB PROFILE name=TonalPulseFreq");
        Serial.println("RB CMD: RB PROFILE name=TonalPulseScalar");
        Serial.println("RB CMD: RB PROFILE name=AmpExperimental");
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
    if (startsWithTokenIgnoreCase(line, "RB STATUS")) {
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
            Serial.println("RB PARAM usage=RB PARAM freqScore=18000 freqContrast=50.0 freqReleaseScore=12000 freqReleaseContrast=50.0");
            return;
        }

        FrequencyMatchCriteria::Values freqTuning = {};
        freqTuning.attackScoreMin = _activeDetectionProfile.frequencyMatch.attackScoreMin;
        freqTuning.releaseScoreMin = _activeDetectionProfile.frequencyMatch.releaseScoreMin;
        freqTuning.attackContrastMin = _activeDetectionProfile.frequencyMatch.attackContrastMin;
        freqTuning.releaseContrastMin = _activeDetectionProfile.frequencyMatch.releaseContrastMin;

        while ((token = strtok_r(nullptr, " ", &savePtr)) != nullptr) {
            FrequencyMatchCriteria::parseToken(token, freqTuning);
        }

        _activeDetectionProfile.frequencyMatch.attackScoreMin = freqTuning.attackScoreMin;
        _activeDetectionProfile.frequencyMatch.releaseScoreMin = freqTuning.releaseScoreMin;
        _activeDetectionProfile.frequencyMatch.attackContrastMin = freqTuning.attackContrastMin;
        _activeDetectionProfile.frequencyMatch.releaseContrastMin = freqTuning.releaseContrastMin;
        applyActiveDetectionProfile();

        Serial.print(" freqScore=");
        Serial.print(_activeDetectionProfile.frequencyMatch.attackScoreMin, 0);
        Serial.print(" freqContrast=");
        Serial.print(_activeDetectionProfile.frequencyMatch.attackContrastMin, 1);
        Serial.print(" freqReleaseScore=");
        Serial.print(_activeDetectionProfile.frequencyMatch.releaseScoreMin, 0);
        Serial.print(" freqReleaseContrast=");
        Serial.println(_activeDetectionProfile.frequencyMatch.releaseContrastMin, 1);
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
            Serial.println("RB BEHAV usage=RB BEHAV wait=100 refractory=0 suppressSelfChirp=250 detectionSuppressTail=0 idleTimeout=20000 idleTimeoutVariation=10000 idleBlockedAfterHeard=3000 idleBlockedAfterOwnEmit=5000");
            return;
        }

        unsigned long waitAfterHeardMs = _behavior.waitAfterHeardMs();
        unsigned long refractoryAfterEmitMs = _behavior.refractoryAfterEmitMs();
        unsigned long behaviorSuppressSelfChirpMs = _behavior.behaviorSuppressSelfChirpMs();
        unsigned long detectionSuppressTailMsOwnEmit = _behavior.detectionSuppressTailMsOwnEmit();
        unsigned long idleTimeoutMs = _behavior.idleTimeoutMs();
        unsigned long idleTimeoutVariationMs = _behavior.idleTimeVariationMs();
        unsigned long idleBlockedAfterHeardMs = _behavior.idleBlockedAfterHeardMs();
        unsigned long idleBlockedAfterOwnEmitMs = _behavior.idleBlockedAfterOwnEmitMs();
        bool idleEnabled = _behavior.idleEnabled();

        while ((token = strtok_r(nullptr, " ", &savePtr)) != nullptr) {
            if (startsWithTokenIgnoreCase(token, "wait=") || startsWithTokenIgnoreCase(token, "waitMs=")) {
                const char* value = token + (startsWithTokenIgnoreCase(token, "waitMs=") ? 7 : 5);
                waitAfterHeardMs = strtoul(value, nullptr, 10);
            } else if (startsWithTokenIgnoreCase(token, "refractory=") || startsWithTokenIgnoreCase(token, "refractoryMs=")) {
                const char* value = token + (startsWithTokenIgnoreCase(token, "refractoryMs=") ? 13 : 11);
                refractoryAfterEmitMs = strtoul(value, nullptr, 10);
            } else if (startsWithTokenIgnoreCase(token, "suppressSelfChirp=") || startsWithTokenIgnoreCase(token, "behaviorSuppressSelfChirpMs=")) {
                const char* value = token + (startsWithTokenIgnoreCase(token, "behaviorSuppressSelfChirpMs=") ? 28 : 18);
                behaviorSuppressSelfChirpMs = strtoul(value, nullptr, 10);
            } else if (startsWithTokenIgnoreCase(token, "detectionSuppressTail=") || startsWithTokenIgnoreCase(token, "detectionSuppressTailMsOwnEmit=")) {
                const char* value = token + (startsWithTokenIgnoreCase(token, "detectionSuppressTailMsOwnEmit=") ? 31 : 22);
                detectionSuppressTailMsOwnEmit = strtoul(value, nullptr, 10);
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

        _behavior.setWaitAfterHeardMs(waitAfterHeardMs);
        _behavior.setRefractoryAfterEmitMs(refractoryAfterEmitMs);
        _behavior.setBehaviorSuppressSelfChirpMs(behaviorSuppressSelfChirpMs);
        _behavior.setDetectionSuppressTailMsOwnEmit(detectionSuppressTailMsOwnEmit);
        _behavior.setIdleTimeoutMs(idleTimeoutMs);
        _behavior.setIdleTimeVariationMs(idleTimeoutVariationMs);
        _behavior.setIdleBlockedAfterHeardMs(idleBlockedAfterHeardMs);
        _behavior.setIdleBlockedAfterOwnEmitMs(idleBlockedAfterOwnEmitMs);
        _behavior.setIdleEnabled(idleEnabled);

        Serial.print("RB BEHAV wait=");
        Serial.print(_behavior.waitAfterHeardMs());
        Serial.print(" refractory=");
        Serial.print(_behavior.refractoryAfterEmitMs());
        Serial.print(" suppressSelfChirp=");
        Serial.print(_behavior.behaviorSuppressSelfChirpMs());
        Serial.print(" detectionSuppressTail=");
        Serial.print(_behavior.detectionSuppressTailMsOwnEmit());
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

    if (equalsIgnoreCase(mode, "off")) {
        _rbLogMode = RbLogMode::Off;
    } else if (equalsIgnoreCase(mode, "minimal")) {
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
    const detection::DetectionProfile& detectionProfile = activeDetectionProfile();
    Serial.print("RB STATUS profile=");
    Serial.print(detection::detectionProfileName(detectionProfile.kind));
    Serial.print(" emitters=");
    Serial.print(detection::detectorSelectionName(detectionProfile.detectorSelection));
    Serial.print(" inspectionPlan=");
    Serial.print(inspectionPlanName(detectionProfile.inspectionPlan));
    Serial.print(" requireSupportForAcceptance=");
    Serial.print(detectionProfile.patternMatcherConfig.requireSupportForAcceptance ? 1 : 0);
    Serial.print(" requiredSupportTarget=");
    Serial.print(evidenceTargetName(detectionProfile.patternMatcherConfig.requiredSupportTarget));
    Serial.print(" freqScore=");
    Serial.print(detectionProfile.frequencyMatch.attackScoreMin, 0);
    Serial.print(" freqContrast=");
    Serial.print(detectionProfile.frequencyMatch.attackContrastMin, 1);
    Serial.print(" freqReleaseScore=");
    Serial.print(detectionProfile.frequencyMatch.releaseScoreMin, 0);
    Serial.print(" freqReleaseContrast=");
    Serial.print(detectionProfile.frequencyMatch.releaseContrastMin, 1);
    Serial.print(" behavior_state=");
    Serial.print(_behavior.stateName());
    Serial.print(" behavior_idle=");
    Serial.print(_behavior.idleEnabled() ? 1 : 0);
    Serial.print(" behavior_suppressSelfChirpMs=");
    Serial.print(_behavior.behaviorSuppressSelfChirpMs());
    Serial.print(" behavior_detectionSuppressTailMsOwnEmit=");
    Serial.print(_behavior.detectionSuppressTailMsOwnEmit());
    Serial.print(" output_busy=");
    Serial.print(_behavior.outputBusy() ? 1 : 0);
    const unsigned long sampleRateHz = _audioSource.sampleRateHz() > 0 ? _audioSource.sampleRateHz() : 16000UL;
    const unsigned long windowSizeSamples = _freqBandStream.windowSizeSamples();
    const unsigned long frequencyUpdateEverySamples = _freqBandStream.frequencyUpdateEverySamples();
    const unsigned long ageSamples = _freqBandStream.lastPacketAgeSamples();
    const float windowMs = sampleRateHz > 0
        ? (static_cast<float>(windowSizeSamples) * 1000.0f) / static_cast<float>(sampleRateHz)
        : 0.0f;
    const float updateStepMs = sampleRateHz > 0
        ? (static_cast<float>(frequencyUpdateEverySamples) * 1000.0f) / static_cast<float>(sampleRateHz)
        : 0.0f;
    const float ageMs = sampleRateHz > 0
        ? (static_cast<float>(ageSamples) * 1000.0f) / static_cast<float>(sampleRateHz)
        : 0.0f;
    Serial.print(" freq.window_samples=");
    Serial.print(windowSizeSamples);
    Serial.print(" freq.window_ms=");
    Serial.print(windowMs, 2);
    Serial.print(" freq.update_every_samples=");
    Serial.print(frequencyUpdateEverySamples);
    Serial.print(" freq.update_period_ms=");
    Serial.print(updateStepMs, 3);
    Serial.print(" freq.target_hz=");
    Serial.print(_freqBandStream.targetFrequencyHz());
    Serial.print(" freq.produced_fresh_packet=");
    Serial.print(_freqBandStream.producedFreshPacketOnLastObserve() ? 1 : 0);
    Serial.print(" freq.packet_age_samples=");
    Serial.print(ageSamples);
    Serial.print(" freq.packet_age_ms=");
    Serial.print(ageMs, 3);
    printBuildIdentity();
    Serial.println();
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
        Serial.println("RB PROFILE usage=name=TonalPulseFreq|TonalPulseScalar|AmpExperimental");
    }
}

bool Node::setProfileFromName(const char* name) {
    detection::DetectionProfileKind parsed = _profileKind;
    if (!detection::detectionProfileKindFromName(name, parsed)) {
        return false;
    }

    _profileKind = parsed;
    _activeDetectionProfile = detection::detectionProfileForKind(_profileKind);
    applyActiveProfiles();
    return true;
}

const char* Node::profileName() const {
    return detectionProfileKindName(_profileKind);
}

const detection::DetectionProfile& Node::activeDetectionProfile() const {
    return _activeDetectionProfile;
}

const BehaviorGateConfig& Node::activeBehaviorProfile() const {
    static const BehaviorGateConfig kTonalPulseProfile = makeTonalPulseBehaviorProfile();

    switch (_profileKind) {
        case detection::DetectionProfileKind::TonalPulseScalar:
        case detection::DetectionProfileKind::AmpExperimental:
        case detection::DetectionProfileKind::TonalPulseFreq:
        default:
            return kTonalPulseProfile;
    }
}

void Node::applyActiveProfiles() {
    applyActiveDetectionProfile();
    applyActiveBehaviorGateConfig();
}

void Node::applyActiveDetectionProfile() {
    const detection::DetectionProfile& detectionProfile = activeDetectionProfile();

    _detection.setDetectorSelection(detectionProfile.detectorSelection);
    _detection.setFrequencyMatchConfig(detectionProfile.frequencyMatch);
    _detection.setScalarTransientConfig(detectionProfile.scalarTransient);
    _detection.setInspectionPlan(detectionProfile.inspectionPlan);
    _detection.setPatternMatcherConfig(detectionProfile.patternMatcherConfig);
    _detection.setFieldStateConfig(detectionProfile.fieldStateConfig);
    _detection.setProfileName(detection::detectionProfileName(detectionProfile.kind));
    _freqBandStream.setSampleRateHz(_audioSource.sampleRateHz());
    _freqBandStream.setTargetFrequencyHz(_chirpOutput.toneHz());
}

void Node::applyActiveBehaviorGateConfig() {
    _behavior.configure(activeBehaviorProfile());
}

void Node::processDetectionFrame(const AudioSamplePacket& audioSamplePacket,
                              unsigned long now,
                              bool selfChirpSuppressed,
                              bool& sawPatternThisLoop) {
    const auto liveFrequencyMeasurementPacket = captureFrequencyMeasurementPacket(audioSamplePacket);
    _detection.observeFrame(audioSamplePacket, liveFrequencyMeasurementPacket, audioSamplePacket.timeMs);

    detection::PatternResult patternResult;
    while (_detection.popPatternResult(patternResult)) {
        const auto behaviorDecision = _behavior.handlePatternResult(patternResult, _detection.fieldState(), now);
        if (behaviorDecision == ResonantBehavior::BehaviorDecision::ConsumedPattern) {
            sawPatternThisLoop = true;
        }

        ++_rbPendingCount;
        if (patternResult.patternAccepted) {
            ++_rbPatternAcceptedCount;
        }
        if (patternResult.valid) {
            ++_rbValidPatternCount;
        }

        if (patternResult.primaryAudioOverflow) {
            ++_rbOverflowPending;
        }
        _rbStrengthSumScaled += static_cast<unsigned long>(patternResult.primaryStrength * 100.0f);
        _rbDurationSumMs += patternResult.primaryDurationMs;
        _rbHaveLastPendingMs = true;
        _rbLastPendingMs = patternResult.primaryHeardAtMs;

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
    switch (_rbLogMode) {
        case RbLogMode::Off:
            return "off";
        case RbLogMode::Minimal:
            return "minimal";
        case RbLogMode::Full:
            return "full";
    }
    return "off";
}

detection::FrequencyBandMeasurementPacket Node::captureFrequencyMeasurementPacket(const AudioSamplePacket& audioSamplePacket) const {
    return detection::buildFrequencyMeasurementPacket(_freqBandStream, audioSamplePacket);
}

void Node::printRbSummary() const {
    const float avgStrength = _rbPendingCount > 0 ? (static_cast<float>(_rbStrengthSumScaled) / 100.0f) / static_cast<float>(_rbPendingCount) : 0.0f;
    const float avgDuration = _rbPendingCount > 0 ? static_cast<float>(_rbDurationSumMs) / static_cast<float>(_rbPendingCount) : 0.0f;
    const AudioSignalStats& stats = _audioSignal.stats();

    Serial.print("RB summary: pending=");
    Serial.print(_rbPendingCount);
    Serial.print(" patternAccepted=");
    Serial.print(_rbPatternAcceptedCount);
    Serial.print(" validPatterns=");
    Serial.print(_rbValidPatternCount);
    Serial.print(" chirpStarted=");
    Serial.print(_rbChirpStartedCount);
    Serial.print(" overflowPending=");
    Serial.print(_rbOverflowPending);
    Serial.print(" avg_strength=");
    Serial.print(avgStrength, 1);
    Serial.print(" avg_duration=");
    Serial.print(avgDuration, 1);
    Serial.print("ms");
    Serial.print(" profile=");
    Serial.print(profileName());
    printBuildIdentity();
    Serial.println();
    Serial.print("RB baseline state=");
    Serial.println(rbBaselineStateName());
    Serial.print("RB log mode=");
    Serial.println(rbLogModeName());

    printRbBehaviorSummary();
    printRbSignalSummary();
}

void Node::printRbBehaviorSummary() const {
    Serial.print("RB behavior idle=");
    Serial.print(_behavior.idleEnabled() ? 1 : 0);
    Serial.print(" wait=");
    Serial.print(_behavior.waitAfterHeardMs());
    Serial.print(" refractory=");
    Serial.print(_behavior.refractoryAfterEmitMs());
    Serial.print(" suppressSelfChirp=");
    Serial.print(_behavior.behaviorSuppressSelfChirpMs());
    Serial.print(" detectionSuppressTail=");
    Serial.print(_behavior.detectionSuppressTailMsOwnEmit());
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
    Serial.print("RB occurrence baseline=");
    Serial.print(_audioSignal.baseline(), 1);
    Serial.print(" mag=");
    Serial.print(_audioSignal.signalMagnitude());
    Serial.print(" smooth=");
    Serial.println(_audioSignal.smoothedSignalMagnitude());
}


