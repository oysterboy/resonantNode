#include "ResonantNodeApp.h"
#include <Arduino.h>
#include <string.h>

#include <esp_system.h>

#include "../../app/RuntimeDefaults.h"
#include "../../app/TimingUtils.h"
#include "../../app/BuildInfo.h"
#include "../../detection/features/FrequencyMeasurementPacketBuilder.h"
#include "../../detection/detectors/frequency/FrequencyMatchCriteria.h"
#include "../../detection/inspection/InspectionNames.h"
#include "../../detection/evaluation/VerdictNames.h"
#include "../../param/ParamRegistry.h"
#include "../../app/TimeBase.h"

/*
Node

Owns orchestration for the Resonant node.

Responsibilities:
- wire hardware sources and outputs
- feed audio into occurrence and detection layers
- pass OccurrenceVerdict objects into ResonantBehavior
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
// Smoothed signal magnitude under which the startup baseline counts as
// quiet. The D-AMP mic's floor sits at ~230-250 in a quiet room (#20,
// 2026-10-09), so the piezo-era 20 was never reached there.
#if defined(BOARD_PIEZO)
constexpr int kRbStartupQuietThreshold = 20;
#else
constexpr int kRbStartupQuietThreshold = 400;
#endif
constexpr unsigned long kRbStartupMicWarmupMs = runtime::kDefaultAudioSignalStartupWarmupMs;
constexpr unsigned long kRbStartupQuietHoldMs = 1000;
constexpr unsigned long kRbStartupBaselineTimeoutMs = 8000;
constexpr unsigned long kRbPostRebaseSettleMs = 500;

uint32_t sampleOffsetUs(uint32_t sampleOffset, uint32_t sampleRateHz) {
    if (sampleRateHz == 0) {
        return 0;
    }

    return static_cast<uint32_t>((static_cast<uint64_t>(sampleOffset) * 1000000ULL) / static_cast<uint64_t>(sampleRateHz));
}

}

// Case-insensitive prefix match that ends on a token boundary: the next
// character is the end or a space, unless the prefix itself ends in '='
// (key=value). A plain prefix match ran "RB rebase forced" and
// "RB rebasefoo" as rebases (issue #31 R2-19).
static bool startsWithTokenIgnoreCase(const char* line, const char* token) {
    char last = '\0';
    while (*token != '\0') {
        if (*line == '\0') {
            return false;
        }
        if (toupper(static_cast<unsigned char>(*line)) != toupper(static_cast<unsigned char>(*token))) {
            return false;
        }
        last = *token;
        ++line;
        ++token;
    }

    return last == '=' || *line == '\0' || *line == ' ';
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

// Returns the remainder of `token` after `prefix` if `token` starts with
// `prefix` (case-insensitive), or nullptr otherwise. Using strlen(prefix)
// instead of a hand-computed offset means the stripped length can never
// silently desync from the literal it's stripping.
static const char* valueAfterPrefix(const char* token, const char* prefix) {
    return startsWithTokenIgnoreCase(token, prefix) ? token + strlen(prefix) : nullptr;
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
    Serial.print(detection::inspectionPlanName(profile.inspectionPlan));
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
    Serial.println(detection::inspectionPlanName(profile.inspectionPlan));
    if (profile.detectorSelection == detection::DetectorSelection::ScalarTransient) {
        Serial.print("  scalarTransient.observedStream=");
        switch (profile.scalarTransient.observedStream) {
            case detection::FeatureStreamId::AmpMagnitude:
                Serial.println("AmpMagnitude");
                break;
            case detection::FeatureStreamId::AmpEnvelope:
                Serial.println("AmpEnvelope");
                break;
            case detection::FeatureStreamId::FrequencyTarget:
                Serial.println("FrequencyTarget");
                break;
            case detection::FeatureStreamId::FrequencyContrast:
                Serial.println("FrequencyContrast");
                break;
            case detection::FeatureStreamId::Unknown:
            default:
                Serial.println("Unknown");
                break;
        }
        Serial.print("  scalarTransient.onsetDetectionThreshold=");
        Serial.println(profile.scalarTransient.onsetDetectionThreshold, 1);
        Serial.print("  scalarTransient.onsetReleaseThreshold=");
        Serial.println(profile.scalarTransient.onsetReleaseThreshold, 1);
        Serial.print("  scalarTransient.cooldownAfterOnsetMs=");
        Serial.println(profile.scalarTransient.cooldownAfterOnsetMs);
        Serial.print("  scalarTransient.minTransientDurationMs=");
        Serial.println(profile.scalarTransient.minTransientDurationMs);
        Serial.print("  scalarTransient.maxTransientDurationMs=");
        Serial.println(profile.scalarTransient.maxTransientDurationMs);
        Serial.print("  scalarTransient.releaseDebounceMs=");
        Serial.println(profile.scalarTransient.releaseDebounceMs);
        Serial.print("  scalarTransient.minTransientPeakStrength=");
        Serial.println(profile.scalarTransient.minTransientPeakStrength, 1);
    } else {
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
    }
    for (size_t i = 0; i < profile.inspectionPlan.count; ++i) {
        const auto& module = profile.inspectionPlan.modules[i];
        Serial.print("  inspectionPlan.module[");
        Serial.print(static_cast<unsigned int>(i));
        Serial.print("].kind=");
        Serial.print(module.kind == detection::InspectionModuleKind::MagnitudeFeatureStrength ? "MagnitudeFeatureStrength" : "None");
        Serial.print(" label=");
        Serial.print(detection::inspectionTargetName(module.target));
        if (module.kind == detection::InspectionModuleKind::MagnitudeFeatureStrength) {
            Serial.print(" stream=");
            switch (module.magnitude.stream) {
                case detection::FeatureStreamId::AmpMagnitude:
                    Serial.print("AmpMagnitude");
                    break;
                case detection::FeatureStreamId::AmpEnvelope:
                    Serial.print("AmpEnvelope");
                    break;
                case detection::FeatureStreamId::FrequencyTarget:
                    Serial.print("FrequencyTarget");
                    break;
                case detection::FeatureStreamId::FrequencyContrast:
                    Serial.print("FrequencyContrast");
                    break;
                case detection::FeatureStreamId::Unknown:
                default:
                    Serial.print("Unknown");
                    break;
            }
            Serial.print(" windowPreMs=");
            Serial.print(module.magnitude.windowPreMs);
            Serial.print(" windowPostMs=");
            Serial.println(module.magnitude.windowPostMs);
        } else {
            Serial.println();
        }
    }
    const detection::InspectionModuleConfig* supportRequirement =
        detection::evaluatorFirstEnabledRequirement(profile.inspectionPlan);
    const bool supportRequired = supportRequirement != nullptr;
    Serial.print("  verdict.has_requirement=");
    Serial.println(supportRequired ? 1 : 0);
    Serial.print("  verdict.required_label=");
    Serial.println(supportRequired ? detection::inspectionTargetName(supportRequirement->target) : "none");
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

void printNodeBuildIdentity() {
    char nodeIdBuffer[17];
    const uint64_t nodeId = ESP.getEfuseMac();
    snprintf(nodeIdBuffer, sizeof(nodeIdBuffer), "0x%012llX", static_cast<unsigned long long>(nodeId));

    printBuildIdentity(Serial, "node");
    Serial.print("RB node_id=");
    Serial.println(nodeIdBuffer);
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

Node::Node(int ledPin, int chirpPin, int chirpBtlPin)
    : _ledPin(ledPin),
      _i2sSource(runtime::kDefaultAudioI2SSckPin,
                  runtime::kDefaultAudioI2SWsPin,
                  runtime::kDefaultAudioI2SDataPin,
                  static_cast<int>(runtime::kDefaultAudioI2SSampleRateHz),
                  static_cast<int>(runtime::kDefaultAudioI2SBitsPerSample),
                  runtime::kDefaultAudioI2SDataOutPin),
      _audioSource(_i2sSource),
      _audioSignal(_audioSource),
      _freqBandStream(),
#if defined(BOARD_PIEZO)
      _toneOutput(chirpPin),
      _toneOutputBTL(chirpPin, chirpBtlPin),
      _chirpOutput(chirpBtlPin >= 0
                       ? static_cast<ToneOutput&>(_toneOutputBTL)
                       : static_cast<ToneOutput&>(_toneOutput)) {}
#else
      _toneOutput(_i2sSource),
      _chirpOutput(_toneOutput) {
    (void)chirpPin;
    (void)chirpBtlPin;
}
#endif

// --- lifecycle ---

void Node::begin() {
    printNodeBuildIdentity();
    Serial.println();

    randomSeed(esp_random() ^ timebase::nowMs());
    configureParameters();
    _audioSource.begin();
    _audioSignal.begin(false);
    _freqBandStream.resetState();
    _freqBandStream.setSampleRateHz(_audioSource.sampleRateHz());
    _freqBandStream.setTargetFrequencyHz(_chirpOutput.toneHz());
    _detection.resetState();
    applyActiveProfiles();
    // Per-node start bin for the hop train, from the MAC (BEH-005); the
    // band is applied inside applyActiveProfiles().
    {
        const size_t bins = bandBinCount();
        _hopRotation = bins > 0 ? static_cast<uint32_t>(ESP.getEfuseMac() % bins) : 0U;
    }
    printSignalConfig();
    _behavior.resetState();
    resetDetectionState();
    _audioSignal.resetStats();
    _audioSource.resetStats();
    resetRbCounters();
    _rbLastLoggedOnsetRejectCount = 0;
    _rbLastLoggedTransientRejectCount = 0;
    _rbBaselineState = RBBaselineState::ListenForQuiet;
    _rbBaselineStateStartedMs = timebase::nowMs();
    _rbBaselineQuietSinceMs = 0;
    _rbBaselineLastLogMs = 0;
    _rbBaselineSettleUntilMs = 0;
    _rbLogMode = RbLogMode::Minimal;
    _rbLastWouldEmitHeardMs = 0;
    _rbLastWouldEmitDecision = ResonantBehavior::BehaviorDecision::None;
    _chirpOutput.begin();
    _debug.begin(_ledPin);
    _behavior.seedIdleSchedule(timebase::nowMs());
    _debug.markLoopStart(timebase::nowUs());
    _serialLineLength = 0;
    _serialLineOverflow = false;
    _serialLineBuffer[0] = '\0';

    Serial.print("RB PROFILE name=");
    Serial.print(profileName());
    printProfileComposition(activeDetectionProfile());
    Serial.println();
    printDetectionProfileDetails(activeDetectionProfile());
    Serial.println();
    printBehaviorRuntimeDetails(_behavior);
    Serial.println();
    Serial.print("RB startup baseline=");
    Serial.println(rbBaselineStateName());
}

// Outputs go off: Behavior must not carry a HeardPattern (or a sounding
// chirp) across, or it answers seconds late once outputs return. Config
// (RB BEHAV) is kept; performRbRebase does the same plus the audio reset
// (issue #31 R2-3).
void Node::quiesceBehaviorForOutputsOff() {
    if (_chirpOutput.isActive()) {
        _chirpOutput.stop();
        _lastOwnChirpEndMs = timebase::nowMs();
        _debug.observeChirpFinished(timebase::nowMs());
        selfTestNoteChirpFinished(timebase::nowMs());
    }
    _behavior.resetState();
}

void Node::startRbQuietBaseline() {
    quiesceBehaviorForOutputsOff();
    _rbBaselineState = RBBaselineState::ListenForQuiet;
    _rbBaselineStateStartedMs = timebase::nowMs();
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
    // Detection config only: a rebase must not put live RB BEHAV values back
    // to the profile defaults (issue #31). Profile switches and boot apply
    // both through applyActiveProfiles().
    applyActiveDetectionProfile();
    _wasSelfChirpSuppressed = false;
    _rbLastLoggedOnsetRejectCount = 0;
    _rbLastLoggedTransientRejectCount = 0;
}

void Node::performRbRebase() {
    // Behavior's reset drops the own-emit suppression; a chirp left sounding
    // would then be heard and answered (issue #31). Its ring-down falls in
    // the post-rebase Settle, where verdicts don't reach Behavior.
    quiesceBehaviorForOutputsOff();
    if (!_audioSignal.rebase()) {
        Serial.println("RB WARN rebase_timeout no_audio baseline_unchanged");
    }
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
            if (timing::elapsedSince(now, _rbBaselineStateStartedMs, kRbStartupMicWarmupMs)) {
                startRbQuietBaseline();
            } else if (_rbBaselineLastLogMs == 0 || timing::elapsedSince(now, _rbBaselineLastLogMs, 1000UL)) {
                _rbBaselineLastLogMs = now;
                Serial.print("RB warmup waiting ms=");
                Serial.print(now - _rbBaselineStateStartedMs);
                Serial.print(" of ");
                Serial.println(kRbStartupMicWarmupMs);
            }
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
                    _rbLastRebaseSmooth = smooth;
                    noteRebaseResult(RbRebaseResult::Done);
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
                    _rbLastRebaseSmooth = smooth;
                    noteRebaseResult(RbRebaseResult::NoQuiet);
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
    registerDetectionParams();
    registerOutputParams();
}

// Bind the currently-editable detection fields into the flat ParamRegistry.
// Fields live inside _activeDetectionProfile, which is a stable Node member,
// so the bound pointers stay valid across profile value changes; switching
// profiles by kind still replaces the whole struct's contents, same as
// before this registry existed.
void Node::registerDetectionParams() {
    detection::FrequencyMatchConfig& freq = _activeDetectionProfile.frequencyMatch;
    _paramRegistry.addFloat(param::ParamId::Detection_FreqAttackScoreMin, param::ModuleId::Detection,
                             "detection.freq_attack_score_min", &freq.attackScoreMin, 0.0f, 200000.0f);
    _paramRegistry.addFloat(param::ParamId::Detection_FreqReleaseScoreMin, param::ModuleId::Detection,
                             "detection.freq_release_score_min", &freq.releaseScoreMin, 0.0f, 200000.0f);
    _paramRegistry.addFloat(param::ParamId::Detection_FreqAttackContrastMin, param::ModuleId::Detection,
                             "detection.freq_attack_contrast_min", &freq.attackContrastMin, 0.0f, 1000.0f);
    _paramRegistry.addFloat(param::ParamId::Detection_FreqReleaseContrastMin, param::ModuleId::Detection,
                             "detection.freq_release_contrast_min", &freq.releaseContrastMin, 0.0f, 1000.0f);
    // TonalPulseScalar (the Node's production profile, decisions/
    // 2026-10-14-node-ships-tonalpulsescalar.md). Same flat names as the
    // Analyzer's PARAM keys, under "detection.". The ms fields are
    // `unsigned long` in the config; on this 32-bit target it is the same
    // size as uint32_t, which the registry stores.
    static_assert(sizeof(unsigned long) == sizeof(uint32_t), "scalar ms fields are bound as uint32_t");
    detection::ScalarTransientConfig& sc = _activeDetectionProfile.scalarTransient;
    using param::ModuleId;
    using param::ParamId;
    _paramRegistry.addFloat(ParamId::Detection_ScalarOnsetThreshold, ModuleId::Detection,
                             "detection.scalar_onset_threshold", &sc.onsetDetectionThreshold, 0.0f, 200000.0f);
    _paramRegistry.addFloat(ParamId::Detection_ScalarReleaseThreshold, ModuleId::Detection,
                             "detection.scalar_release_threshold", &sc.onsetReleaseThreshold, 0.0f, 200000.0f);
    _paramRegistry.addUInt32(ParamId::Detection_ScalarCooldownMs, ModuleId::Detection,
                             "detection.scalar_cooldown_ms", reinterpret_cast<uint32_t*>(&sc.cooldownAfterOnsetMs), 0U, 10000U);
    _paramRegistry.addUInt32(ParamId::Detection_ScalarMinDurationMs, ModuleId::Detection,
                             "detection.scalar_min_duration_ms", reinterpret_cast<uint32_t*>(&sc.minTransientDurationMs), 0U, 5000U);
    _paramRegistry.addUInt32(ParamId::Detection_ScalarMaxDurationMs, ModuleId::Detection,
                             "detection.scalar_max_duration_ms", reinterpret_cast<uint32_t*>(&sc.maxTransientDurationMs), 0U, 5000U);
    _paramRegistry.addFloat(ParamId::Detection_ScalarMinPeakStrength, ModuleId::Detection,
                             "detection.scalar_min_peak_strength", &sc.minTransientPeakStrength, 0.0f, 200000.0f);
    _paramRegistry.addUInt32(ParamId::Detection_ScalarReleaseDebounceMs, ModuleId::Detection,
                             "detection.scalar_release_debounce_ms", reinterpret_cast<uint32_t*>(&sc.releaseDebounceMs), 0U, 1000U);
    _paramRegistry.addBool(ParamId::Detection_ScalarRequireCarrierQuality, ModuleId::Detection,
                            "detection.scalar_require_carrier_quality", &sc.requireCarrierQuality);
    _paramRegistry.addBool(ParamId::Detection_ScalarRequireMinStrength, ModuleId::Detection,
                            "detection.scalar_require_min_strength", &sc.requireMinStrength);
    _paramRegistry.addFloat(ParamId::Detection_ScalarMinStrength, ModuleId::Detection,
                             "detection.scalar_min_strength", &sc.minMatchedMeanStrength, 0.0f, 200000.0f);
    _paramRegistry.addUInt32(ParamId::Detection_ScalarMinReleaseCoverageMs, ModuleId::Detection,
                             "detection.scalar_min_release_coverage_ms", reinterpret_cast<uint32_t*>(&sc.minCoverageAboveReleaseMs), 0U, 5000U);
    _paramRegistry.addUInt32(ParamId::Detection_ScalarMinLongestIslandMs, ModuleId::Detection,
                             "detection.scalar_min_longest_island_ms", reinterpret_cast<uint32_t*>(&sc.minLongestIslandMs), 0U, 5000U);
    _paramRegistry.addUInt32(ParamId::Detection_ScalarMaxGapMs, ModuleId::Detection,
                             "detection.scalar_max_gap_ms", reinterpret_cast<uint32_t*>(&sc.maxGapMs), 0U, 5000U);
    // Network band (DET-013). Applied by applyBandParams() when the values
    // change; 0 = single target at the chirp tone.
    _paramRegistry.addUInt32(ParamId::Detection_BandLowHz, ModuleId::Detection,
                             "detection.band_low_hz", &_bandLowHz, 0U, 7900U);
    _paramRegistry.addUInt32(ParamId::Detection_BandHighHz, ModuleId::Detection,
                             "detection.band_high_hz", &_bandHighHz, 0U, 7900U);
}

void Node::registerOutputParams() {
    using param::ModuleId;
    using param::ParamId;
    _paramRegistry.addBool(ParamId::Output_HopEnabled, ModuleId::Output, "output.hop_enabled", &_hopEnabled);
    _paramRegistry.addUInt32(ParamId::Output_HopRotation, ModuleId::Output, "output.hop_rotation", &_hopRotation, 0U, 15U);
    _paramRegistry.addUInt32(ParamId::Output_HopPulseMs, ModuleId::Output, "output.hop_pulse_ms", &_hopPulseMs, 1U, 1000U);
    _paramRegistry.addUInt32(ParamId::Output_HopGapMs, ModuleId::Output, "output.hop_gap_ms", &_hopGapMs, 0U, 2000U);
    _paramRegistry.addUInt32(ParamId::Output_HopSteps, ModuleId::Output, "output.hop_steps", &_hopSteps, 1U, 8U);
}

void Node::applyBandParams() {
    // An empty or inverted band means "single target at the chirp tone".
    const bool bandValid = _bandLowHz != 0 && _bandHighHz >= _bandLowHz;
    const unsigned long lo = bandValid ? _bandLowHz : 0UL;
    const unsigned long hi = bandValid ? _bandHighHz : 0UL;
    if (_freqBandStream.bandLowHz() == lo && _freqBandStream.bandHighHz() == hi) {
        return;
    }
    _freqBandStream.setBand(lo, hi);
}

// The grid the hop train is played on is the grid the detector listens on:
// both come from FreqBandStream (band edges as applied, clamped to its bank,
// and its bin spacing), never recomputed here. Call after applyBandParams().
size_t Node::bandBinCount() const {
    return static_cast<size_t>(_freqBandStream.bandBinCount());
}

size_t Node::hopFrequencies(uint32_t* out, size_t capacity) const {
    const size_t bins = bandBinCount();
    if (bins == 0 || capacity == 0) {
        return 0;
    }
    const uint32_t spacing = static_cast<uint32_t>(_freqBandStream.bandSpacingHz() + 0.5f);
    const uint32_t lowHz = static_cast<uint32_t>(_freqBandStream.bandLowHz());
    size_t steps = _hopSteps;
    if (steps > capacity) {
        steps = capacity;
    }
    // Bin-aligned by construction (the detector's bank sits on the same
    // grid); the rotation picks the start bin, the order is the grid's.
    for (size_t k = 0; k < steps; ++k) {
        const size_t bin = (k + static_cast<size_t>(_hopRotation)) % bins;
        out[k] = lowHz + static_cast<uint32_t>(bin) * spacing;
    }
    return steps;
}

unsigned long Node::startNetworkSignal() {
    uint32_t hz[8];
    const size_t n = hopFrequencies(hz, sizeof(hz) / sizeof(hz[0]));
    if (n == 0 || _hopPulseMs == 0) {
        return 0;
    }
    _chirpOutput.setPulseHop(hz, n);
    _chirpOutput.startPulseTrain(static_cast<uint32_t>(n), _hopPulseMs, _hopGapMs, runtime::kDefaultI2sToneRampMs);
    if (!_chirpOutput.isActive()) {
        _chirpOutput.setPulseHop(nullptr, 0);
        return 0;
    }
    if (rbShouldLogDetail()) {
        Serial.print("RB emit shape=hop steps=");
        Serial.print(static_cast<unsigned long>(n));
        Serial.print(" pulse_ms=");
        Serial.print(_hopPulseMs);
        Serial.print(" gap_ms=");
        Serial.print(_hopGapMs);
        Serial.print(" order=");
        for (size_t k = 0; k < n; ++k) {
            if (k > 0) {
                Serial.print(',');
            }
            Serial.print(hz[k]);
        }
        Serial.println();
    }
    return static_cast<unsigned long>(n) * _hopPulseMs + static_cast<unsigned long>(n - 1) * _hopGapMs;
}

void Node::printSignalConfig() {
    uint32_t hz[8];
    const size_t n = hopFrequencies(hz, sizeof(hz) / sizeof(hz[0]));
    Serial.print("RB signal band=");
    Serial.print(_bandLowHz);
    Serial.print('-');
    Serial.print(_bandHighHz);
    Serial.print(" bins=");
    Serial.print(static_cast<unsigned long>(bandBinCount()));
    Serial.print(" hop=");
    Serial.print(_hopEnabled ? "on" : "off");
    Serial.print(" rotation=");
    Serial.print(_hopRotation);
    Serial.print(" steps=");
    Serial.print(_hopSteps);
    Serial.print(" pulse_ms=");
    Serial.print(_hopPulseMs);
    Serial.print(" gap_ms=");
    Serial.print(_hopGapMs);
    Serial.print(" order=");
    for (size_t k = 0; k < n; ++k) {
        if (k > 0) {
            Serial.print(',');
        }
        Serial.print(hz[k]);
    }
    Serial.println();
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
    const unsigned long now = timebase::nowMs();
    const unsigned long nowUs = timebase::nowUs();
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
            const uint64_t sampleTimeUs64 = block.approxStartUs64 + sampleOffsetUs(static_cast<uint32_t>(i), sampleRateHz);
            AudioSamplePacket audioSamplePacket;
            _audioSignal.update(static_cast<int>(block.samples[i]), sampleTimeUs64, audioSamplePacket, block.overflowBeforeBlock);
            const bool ownEmitSuppressed = _behavior.ownEmitDetectionSuppressed(audioSamplePacket.timeMs);
            if (!ownEmitSuppressed) {
                _freqBandStream.observeCenteredSample(audioSamplePacket.baselineCorrectedValue, audioSamplePacket.timeMs);
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
    applyBandParams();  // PARAM SET detection.band_* takes effect here (two compares when unchanged)

    if (rbOutputsEnabledNow) {
        _behavior.update(now);
    }
    const bool behaviorWouldEmit = _behavior.takeWouldEmit();
    const bool shouldLogWouldEmit = behaviorWouldEmit && rbShouldLogDetail() &&
                                    (_behavior.lastHeardMs() != _rbLastWouldEmitHeardMs ||
                                     _behavior.lastDecision() != _rbLastWouldEmitDecision);
    if (shouldLogWouldEmit) {
        Serial.print("BEH verdict=");
        Serial.print(_behavior.lastVerdictTypeName());
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
    _debug.flashLed(now, 2);
    // A response to a heard pattern is the network signal: the node's hop
    // train inside the band (BEH-005). Idle voice and self-test stay plain
    // chirps. Own-emit suppression covers the train's span.
    unsigned long emitSpanMs = 0;
    if (_hopEnabled && _behavior.signalShape() == ResonantBehavior::SignalShape::NetworkSignal) {
        emitSpanMs = startNetworkSignal();
    }
    if (emitSpanMs == 0) {
        _chirpOutput.start(chirpPattern);
    }
    _behavior.notifyChirpStarted(now, emitSpanMs);
    ++_rbChirpStartedCount;
    selfTestNoteChirpStarted(now, sourceName);
    }

    _chirpOutput.update();

    if (_chirpOutput.finished()) {
        // Fresh time: the own-emit tail runs from the actual toneOff in
        // update() above, not from the loop start (issue #31).
        const unsigned long finishedMs = timebase::nowMs();
        _lastOwnChirpEndMs = finishedMs;
        _behavior.notifyChirpFinished(finishedMs);
        _debug.observeChirpFinished(now);
        selfTestNoteChirpFinished(now);
    }

    _debug.updateLed(now);

    if (_selfTest.phase != SelfTestState::Phase::Idle) {
        updateSelfTest(timebase::nowMs());
    }

    _debug.endLoop(timebase::nowUs());
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
            if (_serialLineOverflow) {
                Serial.print("RB ERR line_too_long max=");
                Serial.println(static_cast<unsigned long>(sizeof(_serialLineBuffer) - 1));
            } else if (_serialLineLength > 0) {
                handleSerialLine(_serialLineBuffer);
            }
            _serialLineLength = 0;
            _serialLineOverflow = false;
            continue;
        }
        if (_serialLineLength < sizeof(_serialLineBuffer) - 1) {
            _serialLineBuffer[_serialLineLength++] = c;
        } else {
            _serialLineOverflow = true;
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
        Serial.println("RB CMD: PARAM LIST");
        Serial.println("RB CMD: PARAM GET <path>");
        Serial.println("RB CMD: PARAM SET <path> <value>");
        Serial.println("RB CMD: PARAM DUMP");
        Serial.println("RB CMD: SELFTEST | SELFTEST external wait_ms=10000 | SELFTEST chirp n=5 | SELFTEST help");
        return;
    }
    if (startsWithTokenIgnoreCase(line, "SELFTEST")) {
        handleSelfTestCommand(line);
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
        char buffer[kSerialLineCapacity];
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
            if (!FrequencyMatchCriteria::parseToken(token, freqTuning)) {
                Serial.print("RB PARAM ERR token=");
                Serial.print(token);
                Serial.println(" (unknown key or invalid value; unchanged)");
            }
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
        char buffer[kSerialLineCapacity];
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
            if (const char* value = valueAfterPrefix(token, "waitMs=")) {
                waitAfterHeardMs = strtoul(value, nullptr, 10);
            } else if (const char* value = valueAfterPrefix(token, "wait=")) {
                waitAfterHeardMs = strtoul(value, nullptr, 10);
            } else if (const char* value = valueAfterPrefix(token, "refractoryMs=")) {
                refractoryAfterEmitMs = strtoul(value, nullptr, 10);
            } else if (const char* value = valueAfterPrefix(token, "refractory=")) {
                refractoryAfterEmitMs = strtoul(value, nullptr, 10);
            } else if (const char* value = valueAfterPrefix(token, "behaviorSuppressSelfChirpMs=")) {
                behaviorSuppressSelfChirpMs = strtoul(value, nullptr, 10);
            } else if (const char* value = valueAfterPrefix(token, "suppressSelfChirp=")) {
                behaviorSuppressSelfChirpMs = strtoul(value, nullptr, 10);
            } else if (const char* value = valueAfterPrefix(token, "detectionSuppressTailMsOwnEmit=")) {
                detectionSuppressTailMsOwnEmit = strtoul(value, nullptr, 10);
            } else if (const char* value = valueAfterPrefix(token, "detectionSuppressTail=")) {
                detectionSuppressTailMsOwnEmit = strtoul(value, nullptr, 10);
            } else if (const char* value = valueAfterPrefix(token, "idleTimeoutMs=")) {
                idleTimeoutMs = strtoul(value, nullptr, 10);
            } else if (const char* value = valueAfterPrefix(token, "idleMs=")) {
                idleTimeoutMs = strtoul(value, nullptr, 10);
            } else if (const char* value = valueAfterPrefix(token, "idleTimeout=")) {
                idleTimeoutMs = strtoul(value, nullptr, 10);
            } else if (const char* value = valueAfterPrefix(token, "idle=")) {
                idleTimeoutMs = strtoul(value, nullptr, 10);
            } else if (const char* value = valueAfterPrefix(token, "idleTimeoutVariation=")) {
                idleTimeoutVariationMs = strtoul(value, nullptr, 10);
            } else if (const char* value = valueAfterPrefix(token, "idleVariation=")) {
                idleTimeoutVariationMs = strtoul(value, nullptr, 10);
            } else if (const char* value = valueAfterPrefix(token, "idleVar=")) {
                idleTimeoutVariationMs = strtoul(value, nullptr, 10);
            } else if (const char* value = valueAfterPrefix(token, "idleBlockedAfterHeard=")) {
                idleBlockedAfterHeardMs = strtoul(value, nullptr, 10);
            } else if (const char* value = valueAfterPrefix(token, "idleBlockedHeardMs=")) {
                idleBlockedAfterHeardMs = strtoul(value, nullptr, 10);
            } else if (const char* value = valueAfterPrefix(token, "idleBlockedAfterOwnEmit=")) {
                idleBlockedAfterOwnEmitMs = strtoul(value, nullptr, 10);
            } else if (const char* value = valueAfterPrefix(token, "idleBlockedOwnEmitMs=")) {
                idleBlockedAfterOwnEmitMs = strtoul(value, nullptr, 10);
            } else if (const char* value = valueAfterPrefix(token, "idleBlockedHeard=")) {
                // The keys RB BEHAV and RB summary echo, so an echoed line
                // pastes back (issue #31 R2-19).
                idleBlockedAfterHeardMs = strtoul(value, nullptr, 10);
            } else if (const char* value = valueAfterPrefix(token, "idleBlockedOwnEmit=")) {
                idleBlockedAfterOwnEmitMs = strtoul(value, nullptr, 10);
            } else if (const char* value = valueAfterPrefix(token, "idleEnabled=")) {
                idleEnabled = equalsIgnoreCase(value, "1") || equalsIgnoreCase(value, "on") || equalsIgnoreCase(value, "true");
            } else {
                Serial.print("RB BEHAV ERR token=");
                Serial.print(token);
                Serial.println(" (unknown key; ignored)");
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
    if (startsWithTokenIgnoreCase(line, "PARAM")) {
        handleParamCommand(line);
        return;
    }
    if (startsWithTokenIgnoreCase(line, "RB rebase force")) {
        // A rebase stops and resets what SELFTEST is waiting on (issue #31
        // R2-13): finish or wait out the self-test first.
        if (_selfTest.phase != SelfTestState::Phase::Idle) {
            Serial.println("RB rebase refused busy=selftest");
            return;
        }
        _rbBaselineState = RBBaselineState::Rebase;
        performRbRebase();
        _rbBaselineState = RBBaselineState::Settle;
        _rbBaselineSettleUntilMs = timebase::nowMs() + kRbPostRebaseSettleMs;
        ++_rbForcedRebaseCount;
        Serial.println("RB rebase forced");
        return;
    }
    if (startsWithTokenIgnoreCase(line, "RB rebase")) {
        // Only a bare "RB rebase" (or "force", above): "RB rebase forced"
        // used to start a quiet-gated rebase (issue #32 check 17).
        const char* extra = line + strlen("RB rebase");
        while (*extra == ' ' || *extra == '\t') {
            ++extra;
        }
        if (*extra != '\0') {
            Serial.print("RB ERR rebase unknown token=");
            Serial.println(extra);
            return;
        }
        if (_selfTest.phase != SelfTestState::Phase::Idle) {
            Serial.println("RB rebase refused busy=selftest");
            return;
        }
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
    Serial.print(detection::inspectionPlanName(detectionProfile.inspectionPlan));
    const detection::InspectionModuleConfig* supportRequirement =
        detection::evaluatorFirstEnabledRequirement(detectionProfile.inspectionPlan);
    const bool supportRequired = supportRequirement != nullptr;
    Serial.print(" has_requirement=");
    Serial.print(supportRequired ? 1 : 0);
    Serial.print(" required_label=");
    Serial.print(supportRequired ? detection::inspectionTargetName(supportRequirement->target) : "none");
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
    Serial.print(" band=");
    Serial.print(_freqBandStream.bandLowHz());
    Serial.print('-');
    Serial.print(_freqBandStream.bandHighHz());
    Serial.print(" hop_enabled=");
    Serial.print(_hopEnabled ? 1 : 0);
    Serial.print(" hop_rotation=");
    Serial.print(_hopRotation);
    Serial.print(" hop_steps=");
    Serial.print(_hopSteps);
    Serial.print(" hop_pulse_ms=");
    Serial.print(_hopPulseMs);
    Serial.print(" hop_gap_ms=");
    Serial.print(_hopGapMs);
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
    const AudioSourceStats& audioStats = _audioSource.stats();
    Serial.print(" i2s.samples_read=");
    Serial.print(static_cast<unsigned long>(audioStats.totalSamplesRead));
    Serial.print(" i2s.dropped_dma_buffers=");
    Serial.print(audioStats.droppedDmaBuffers);
    Serial.print(" i2s.clock_rate_mhz=");
    Serial.print(audioStats.sampleClockRateMilliHz);
    Serial.print(" ");
    printBuildIdentity(Serial, "node");
    Serial.println();
}

void Node::handleProfileCommand(const char* line) {
    const char* name = line + 10; // strlen("RB PROFILE")
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

// PARAM LIST / GET / SET / DUMP: the generic ParamRegistry command surface.
// Kept separate from the ad hoc RB PARAM / RB BEHAV commands until those are
// deliberately migrated (see docs/roadmaps/roadmap-param-config.md).
void Node::handleParamCommand(const char* line) {
    char buffer[kSerialLineCapacity];
    strncpy(buffer, line, sizeof(buffer));
    buffer[sizeof(buffer) - 1] = '\0';

    char* savePtr = nullptr;
    char* token = strtok_r(buffer, " ", &savePtr);
    token = token != nullptr ? strtok_r(nullptr, " ", &savePtr) : nullptr;

    if (token == nullptr) {
        Serial.println("PARAM usage=PARAM LIST|GET <path>|SET <path> <value>|DUMP");
        return;
    }

    if (equalsIgnoreCase(token, "LIST")) {
        _paramRegistry.list(Serial);
        return;
    }

    if (equalsIgnoreCase(token, "DUMP")) {
        _paramRegistry.dump(Serial);
        return;
    }

    if (equalsIgnoreCase(token, "GET")) {
        const char* path = strtok_r(nullptr, " ", &savePtr);
        const param::ParamBinding* binding = path != nullptr ? _paramRegistry.find(path) : nullptr;
        if (binding == nullptr) {
            Serial.print("PARAM GET path=");
            Serial.print(path != nullptr ? path : "");
            Serial.print(" status=");
            Serial.println(param::paramSetStatusName(param::ParamSetStatus::UnknownParam));
            return;
        }
        Serial.print("PARAM GET path=");
        Serial.print(path);
        Serial.print(" module=");
        Serial.print(param::moduleName(binding->module));
        Serial.print(" value=");
        _paramRegistry.printValue(Serial, *binding);
        Serial.println();
        return;
    }

    if (equalsIgnoreCase(token, "SET")) {
        const char* path = strtok_r(nullptr, " ", &savePtr);
        const char* value = strtok_r(nullptr, " ", &savePtr);
        // One value only: "1.5 junk" is an error, not 1.5 (issue #31 R2-19).
        const char* extra = strtok_r(nullptr, " ", &savePtr);
        if (extra != nullptr) {
            value = nullptr;
        }
        const param::ParamBinding* binding = path != nullptr ? _paramRegistry.find(path) : nullptr;

        if (binding == nullptr) {
            Serial.print("PARAM SET path=");
            Serial.print(path != nullptr ? path : "");
            Serial.print(" status=");
            Serial.println(param::paramSetStatusName(param::ParamSetStatus::UnknownParam));
            return;
        }

        Serial.print("PARAM SET path=");
        Serial.print(path);
        Serial.print(" module=");
        Serial.print(param::moduleName(binding->module));
        Serial.print(" old=");
        _paramRegistry.printValue(Serial, *binding);

        const param::ParamSetStatus status = _paramRegistry.applyValue(*binding, value);

        Serial.print(" new=");
        _paramRegistry.printValue(Serial, *binding);
        Serial.print(" status=");
        Serial.println(param::paramSetStatusName(status));

        if (status == param::ParamSetStatus::Ok) {
            applyParamModule(binding->module);
        }
        return;
    }

    Serial.println("PARAM usage=PARAM LIST|GET <path>|SET <path> <value>|DUMP");
}

// Dirty ModuleId apply route: a successful PARAM SET only writes the bound
// field, so the owning module still needs to be told to pick the change up.
void Node::applyParamModule(param::ModuleId module) {
    switch (module) {
        case param::ModuleId::Detection:
            applyActiveDetectionProfile();
            break;
        case param::ModuleId::Node:
        case param::ModuleId::Behavior:
        case param::ModuleId::Output:
            break;
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
    _detection.setFieldStateConfig(detectionProfile.fieldStateConfig);
    _detection.setProfileName(detection::detectionProfileName(detectionProfile.kind));
    _detection.setVerdictQueueEnabled(true);
    _freqBandStream.setSampleRateHz(_audioSource.sampleRateHz());
    _freqBandStream.setTargetFrequencyHz(_chirpOutput.toneHz());
    _freqBandStream.setBand(0, 0);  // force applyBandParams() to re-apply after a profile switch
    applyBandParams();
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

    detection::OccurrenceVerdict verdict;
    while (_detection.popOccurrenceVerdict(verdict)) {
        // Outputs off (Settle, ListenForQuiet): a verdict must not leave
        // Behavior in HeardPattern to fire late once Active (issue #31).
        const bool behaviorSeesVerdict = rbOutputsEnabled();
        const auto behaviorDecision = behaviorSeesVerdict
            ? _behavior.handleOccurrenceVerdict(verdict, _detection.fieldState(), now)
            : ResonantBehavior::BehaviorDecision::None;
        if (!behaviorSeesVerdict) {
            _behavior.notePatternHeardWhileOutputsOff(verdict, now);
        }
        if (behaviorDecision == ResonantBehavior::BehaviorDecision::ConsumedPattern) {
            sawPatternThisLoop = true;
        }

        ++_rbPendingCount;
        if (verdict.accepted) {
            ++_rbPatternAcceptedCount;
            _debug.flashLed(now, 1);
        }
        if (verdict.valid) {
            ++_rbValidPatternCount;
        }

        if (verdict.primaryAudioOverflow) {
            ++_rbOverflowPending;
        }
        _rbStrengthSumScaled += static_cast<uint64_t>(verdict.primaryStrength * 100.0f);
        _rbDurationSumMs += verdict.primaryDurationMs;
        _rbHaveLastPendingMs = true;
        _rbLastPendingMs = verdict.primaryHeardAtMs;
        if (_selfTest.phase != SelfTestState::Phase::Idle) {
            selfTestObserveVerdict(verdict, now);
        }

        if (rbShouldLogDetail()) {
            Serial.print("RB verdict=");
            Serial.print(detection::verdictTypeName(verdict.type));
            Serial.print(" fieldQuiet=");
            Serial.print(_detection.fieldState().quiet ? 1 : 0);
            Serial.print(" fieldActive=");
            Serial.print(_detection.fieldState().active ? 1 : 0);
            Serial.print(" fieldDense=");
            Serial.print(_detection.fieldState().dense ? 1 : 0);
            // Outputs off: Behavior never saw this verdict, so its last
            // decision belongs to an earlier one (issue #31 R2-11).
            Serial.print(" behaviorEligible=");
            Serial.print(behaviorSeesVerdict && _behavior.behaviorEligible() ? 1 : 0);
            Serial.print(" decision=");
            Serial.print(behaviorSeesVerdict ? _behavior.lastDecisionName() : "outputs_off");
            Serial.print(" valid=");
            Serial.print(verdict.valid ? 1 : 0);
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
    Serial.print(" accepted=");
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
    printBuildIdentity(Serial, "node");
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
    Serial.print(_behavior.lastVerdictTypeName());
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


