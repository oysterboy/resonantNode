#include "ResonantNodeApp.h"
#include <Arduino.h>
#include <string.h>

#include <esp_system.h>

#include "../../app/RuntimeDefaults.h"
#include "../../app/TimingUtils.h"
#include "../../app/BuildInfo.h"
#include "../../app/NodeRoster.h"
#include "../../detection/HopGapContract.h"
#include "../../detection/InspectionPlanContract.h"
#include "../../detection/features/FrequencyMeasurementPacketBuilder.h"
#include "../../detection/inspection/InspectionNames.h"
#include "../../detection/evaluation/VerdictNames.h"
#include <ParamRegistry.h>
#include "../../app/ParamIds.h"
#include "../../app/TimeBase.h"
#include "../../app/SerialOut.h"

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
    serialOut().print(" emitters=");
    serialOut().print(detection::detectorSelectionName(profile.detectorSelection));
    serialOut().print(" inspectionPlan=");
    serialOut().print(detection::inspectionPlanName(profile.inspectionPlan));
    serialOut().print(" fieldStateConfig=");
    serialOut().print(profile.fieldStateConfig.occurrenceWindowMs);
    serialOut().print("/");
    serialOut().print(profile.fieldStateConfig.patternWindowMs);
}

void printDetectionProfileDetails(const detection::DetectionProfile& profile) {
    serialOut().println("RB DETECT");
    serialOut().print("  kind=");
    serialOut().println(detection::detectionProfileName(profile.kind));
    serialOut().print("  detectorSelection=");
    serialOut().println(detection::detectorSelectionName(profile.detectorSelection));
    serialOut().print("  inspectionPlan=");
    serialOut().println(detection::inspectionPlanName(profile.inspectionPlan));
    if (profile.detectorSelection == detection::DetectorSelection::ScalarTransient) {
        serialOut().print("  scalarTransient.observedStream=");
        switch (profile.scalarTransient.observedStream) {
            case detection::FeatureStreamId::AmpMagnitude:
                serialOut().println("AmpMagnitude");
                break;
            case detection::FeatureStreamId::AmpEnvelope:
                serialOut().println("AmpEnvelope");
                break;
            case detection::FeatureStreamId::FrequencyTarget:
                serialOut().println("FrequencyTarget");
                break;
            case detection::FeatureStreamId::FrequencyContrast:
                serialOut().println("FrequencyContrast");
                break;
            case detection::FeatureStreamId::Unknown:
            default:
                serialOut().println("Unknown");
                break;
        }
        serialOut().print("  scalarTransient.onsetDetectionThreshold=");
        serialOut().println(profile.scalarTransient.onsetDetectionThreshold, 1);
        serialOut().print("  scalarTransient.onsetReleaseThreshold=");
        serialOut().println(profile.scalarTransient.onsetReleaseThreshold, 1);
        serialOut().print("  scalarTransient.cooldownAfterOnsetMs=");
        serialOut().println(profile.scalarTransient.cooldownAfterOnsetMs);
        serialOut().print("  scalarTransient.minTransientDurationMs=");
        serialOut().println(profile.scalarTransient.minTransientDurationMs);
        serialOut().print("  scalarTransient.maxTransientDurationMs=");
        serialOut().println(profile.scalarTransient.maxTransientDurationMs);
        serialOut().print("  scalarTransient.releaseDebounceMs=");
        serialOut().println(profile.scalarTransient.releaseDebounceMs);
        serialOut().print("  scalarTransient.minTransientPeakStrength=");
        serialOut().println(profile.scalarTransient.minTransientPeakStrength, 1);
    }
    for (size_t i = 0; i < profile.inspectionPlan.count; ++i) {
        const auto& module = profile.inspectionPlan.modules[i];
        serialOut().print("  inspectionPlan.module[");
        serialOut().print(static_cast<unsigned int>(i));
        serialOut().print("].kind=");
        serialOut().print(module.kind == detection::InspectionModuleKind::MagnitudeFeatureStrength ? "MagnitudeFeatureStrength" : "None");
        serialOut().print(" label=");
        serialOut().print(detection::inspectionTargetName(module.target));
        if (module.kind == detection::InspectionModuleKind::MagnitudeFeatureStrength) {
            serialOut().print(" stream=");
            switch (module.magnitude.stream) {
                case detection::FeatureStreamId::AmpMagnitude:
                    serialOut().print("AmpMagnitude");
                    break;
                case detection::FeatureStreamId::AmpEnvelope:
                    serialOut().print("AmpEnvelope");
                    break;
                case detection::FeatureStreamId::FrequencyTarget:
                    serialOut().print("FrequencyTarget");
                    break;
                case detection::FeatureStreamId::FrequencyContrast:
                    serialOut().print("FrequencyContrast");
                    break;
                case detection::FeatureStreamId::Unknown:
                default:
                    serialOut().print("Unknown");
                    break;
            }
            serialOut().print(" windowPreMs=");
            serialOut().print(module.magnitude.windowPreMs);
            serialOut().print(" windowPostMs=");
            serialOut().println(module.magnitude.windowPostMs);
        } else {
            serialOut().println();
        }
    }
    const detection::InspectionModuleConfig* supportRequirement =
        detection::evaluatorFirstEnabledRequirement(profile.inspectionPlan);
    const bool supportRequired = supportRequirement != nullptr;
    serialOut().print("  verdict.has_requirement=");
    serialOut().println(supportRequired ? 1 : 0);
    serialOut().print("  verdict.required_label=");
    serialOut().println(supportRequired ? detection::inspectionTargetName(supportRequirement->target) : "none");
    serialOut().print("  fieldState.occurrenceWindowMs=");
    serialOut().println(profile.fieldStateConfig.occurrenceWindowMs);
    serialOut().print("  fieldState.patternWindowMs=");
    serialOut().println(profile.fieldStateConfig.patternWindowMs);
    serialOut().print("  fieldState.busyOccurrenceCountThreshold=");
    serialOut().println(profile.fieldStateConfig.busyOccurrenceCountThreshold);
    serialOut().print("  fieldState.denseOccurrenceCountThreshold=");
    serialOut().println(profile.fieldStateConfig.denseOccurrenceCountThreshold);
    serialOut().print("  fieldState.quietOccurrenceCountThreshold=");
    serialOut().println(profile.fieldStateConfig.quietOccurrenceCountThreshold);
    serialOut().print("  fieldState.quietActivityThreshold=");
    serialOut().println(profile.fieldStateConfig.quietActivityThreshold, 2);
    serialOut().print("  fieldState.busyActivityThreshold=");
    serialOut().println(profile.fieldStateConfig.busyActivityThreshold, 2);
}

void printBehaviorRuntimeDetails(const ResonantBehavior& behavior) {
    serialOut().println("RB BEHAV");
    serialOut().print("  idleEnabled=");
    serialOut().println(behavior.idleEnabled() ? 1 : 0);
    serialOut().print("  waitAfterHeardMs=");
    serialOut().println(behavior.waitAfterHeardMs());
    serialOut().print("  refractoryAfterEmitMs=");
    serialOut().println(behavior.refractoryAfterEmitMs());
    serialOut().print("  behaviorSuppressSelfChirpMs=");
    serialOut().println(behavior.behaviorSuppressSelfChirpMs());
    serialOut().print("  detectionSuppressTailMsOwnEmit=");
    serialOut().println(behavior.detectionSuppressTailMsOwnEmit());
    serialOut().print("  idleTimeoutMs=");
    serialOut().println(behavior.idleTimeoutMs());
    serialOut().print("  idleTimeVariationMs=");
    serialOut().println(behavior.idleTimeVariationMs());
    serialOut().print("  idleBlockedAfterHeardMs=");
    serialOut().println(behavior.idleBlockedAfterHeardMs());
    serialOut().print("  idleBlockedAfterOwnEmitMs=");
    serialOut().println(behavior.idleBlockedAfterOwnEmitMs());
}

void printNodeBuildIdentity() {
    char nodeIdBuffer[17];
    const uint64_t nodeId = ESP.getEfuseMac();
    snprintf(nodeIdBuffer, sizeof(nodeIdBuffer), "0x%012llX", static_cast<unsigned long long>(nodeId));

    printBuildIdentity(serialOut(), "node");
    serialOut().print("RB node_id=");
    serialOut().println(nodeIdBuffer);
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
    serialOut().println();

    randomSeed(esp_random() ^ timebase::nowMs());
    configureParameters();
    _audioSource.begin();
    _audioSignal.begin(false);
    _freqBandStream.resetState();
    _freqBandStream.setSampleRateHz(_audioSource.sampleRateHz());
    _freqBandStream.setTargetFrequencyHz(_chirpOutput.toneHz());
    _detection.resetState();
    applyActiveProfiles();
    // Per-node start bin for the hop train (BEH-005): from the roster
    // (app/NodeRoster.h, step 5g: E1=0 ... E5=4), else MAC % bins. The band
    // is applied inside applyActiveProfiles().
    {
        const size_t bins = bandBinCount();
        const roster::Entry* me = roster::find(ESP.getEfuseMac());
        if (me != nullptr && bins > 0) {
            _hopRotation = static_cast<uint32_t>(me->hopRotation % bins);
        } else {
            _hopRotation = bins > 0 ? static_cast<uint32_t>(ESP.getEfuseMac() % bins) : 0U;
        }
        serialOut().print("RB node name=");
        serialOut().print(me != nullptr ? me->name : "unknown");
        serialOut().print(" rotation_source=");
        serialOut().println(me != nullptr ? "roster" : "mac");
    }
    printSignalConfig();
    printHopGapCheck();
    printInspectionPlanCheck();
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

    serialOut().print("RB PROFILE name=");
    serialOut().print(profileName());
    printProfileComposition(activeDetectionProfile());
    serialOut().println();
    printDetectionProfileDetails(activeDetectionProfile());
    serialOut().println();
    printBehaviorRuntimeDetails(_behavior);
    serialOut().println();
    serialOut().print("RB startup baseline=");
    serialOut().println(rbBaselineStateName());
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
        serialOut().println("RB WARN rebase_timeout no_audio baseline_unchanged");
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
                serialOut().print("RB warmup waiting ms=");
                serialOut().print(now - _rbBaselineStateStartedMs);
                serialOut().print(" of ");
                serialOut().println(kRbStartupMicWarmupMs);
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
                    serialOut().print("RB rebase done quietMs=");
                    serialOut().print(quietMs);
                    serialOut().print(" baseline=");
                    serialOut().print(_audioSignal.baseline(), 1);
                    serialOut().print(" smooth=");
                    serialOut().println(_audioSignal.smoothedSignalMagnitude());
                } else if (_rbBaselineLastLogMs == 0 || timing::elapsedSince(now, _rbBaselineLastLogMs, 1000UL)) {
                    _rbBaselineLastLogMs = now;
                    serialOut().print("RB rebase waiting smooth=");
                    serialOut().print(smooth, 1);
                    serialOut().print(" quietMs=");
                    serialOut().println(quietMs);
                }
            } else {
                _rbBaselineQuietSinceMs = 0;
                if (timing::elapsedSince(now, _rbBaselineStateStartedMs, kRbStartupBaselineTimeoutMs)) {
                    _rbBaselineState = RBBaselineState::FailedNoQuiet;
                    _rbLastRebaseSmooth = smooth;
                    noteRebaseResult(RbRebaseResult::NoQuiet);
                    serialOut().print("RB rebase skipped reason=no_quiet smooth=");
                    serialOut().println(smooth, 1);
                }
            }
        } break;

        case RBBaselineState::Rebase:
            break;

        case RBBaselineState::Settle:
            if (timing::atOrAfter(now, _rbBaselineSettleUntilMs)) {
                _rbBaselineState = RBBaselineState::Active;
                serialOut().println("RB baseline active");
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
    _paramRegistry.setGroupNamer(param::moduleNameForGroup);
    registerDetectionParams();
    registerOutputParams();
}

// Bind the currently-editable detection fields into the flat ParamRegistry.
// Fields live inside _activeDetectionProfile, which is a stable Node member,
// so the bound pointers stay valid across profile value changes; switching
// profiles by kind still replaces the whole struct's contents, same as
// before this registry existed.
void Node::registerDetectionParams() {
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
    _paramRegistry.addUInt32(ParamId::Output_IdleAnnounceHz, ModuleId::Output, "output.idle_announce_hz", &_idleAnnounceHz, 0U, 7900U);
    _paramRegistry.addUInt32(ParamId::Output_IdleAnnounceMs, ModuleId::Output, "output.idle_announce_ms", &_idleAnnounceMs, 20U, 2000U);
}

// The registered detection params (detection.scalar_*, detection.band_*) applied
// in place: scalar thresholds through the detector's setters, the band through
// FreqBandStream. No detector reset and no FeatureHistory wipe, so a live PARAM
// SET does not drop an occurrence in flight (step 10 item 2, issue #14,
// decisions/2026-10-22-live-params-profile-reset-no-rb-behav.md D2). A profile
// switch still goes through applyActiveDetectionProfile().
void Node::applyLiveDetectionParams() {
    _detection.setScalarTransientConfig(_activeDetectionProfile.scalarTransient);
    applyBandParams();
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
        serialOut().print("RB emit shape=hop steps=");
        serialOut().print(static_cast<unsigned long>(n));
        serialOut().print(" pulse_ms=");
        serialOut().print(_hopPulseMs);
        serialOut().print(" gap_ms=");
        serialOut().print(_hopGapMs);
        serialOut().print(" order=");
        for (size_t k = 0; k < n; ++k) {
            if (k > 0) {
                serialOut().print(',');
            }
            serialOut().print(hz[k]);
        }
        serialOut().println();
    }
    return static_cast<unsigned long>(n) * _hopPulseMs + static_cast<unsigned long>(n - 1) * _hopGapMs;
}

void Node::printSignalConfig() {
    uint32_t hz[8];
    const size_t n = hopFrequencies(hz, sizeof(hz) / sizeof(hz[0]));
    serialOut().print("RB signal band=");
    serialOut().print(_bandLowHz);
    serialOut().print('-');
    serialOut().print(_bandHighHz);
    serialOut().print(" bins=");
    serialOut().print(static_cast<unsigned long>(bandBinCount()));
    serialOut().print(" hop=");
    serialOut().print(_hopEnabled ? "on" : "off");
    serialOut().print(" rotation=");
    serialOut().print(_hopRotation);
    serialOut().print(" steps=");
    serialOut().print(_hopSteps);
    serialOut().print(" pulse_ms=");
    serialOut().print(_hopPulseMs);
    serialOut().print(" gap_ms=");
    serialOut().print(_hopGapMs);
    serialOut().print(" order=");
    for (size_t k = 0; k < n; ++k) {
        if (k > 0) {
            serialOut().print(',');
        }
        serialOut().print(hz[k]);
    }
    serialOut().println();
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
            _lastSampleTimeMs = audioSamplePacket.timeMs;
            if (ownEmitSuppressed) {
                ++_suppressedSampleCount;
            }
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
    if (_behavior.restsEntered() != _rbLastRestsEntered) {
        _rbLastRestsEntered = _behavior.restsEntered();
        if (rbShouldLogDetail()) {
            serialOut().print("RB rest start reason=");
            serialOut().print(ResonantBehavior::behaviorDecisionName(_behavior.lastRestReason()));
            serialOut().print(" now=");
            serialOut().print(now);
            serialOut().print(" until=");
            serialOut().print(_behavior.restUntilMs());
            serialOut().print(" field_patterns=");
            serialOut().println(_detection.fieldState().recentPatternCount);
        }
    }
    const bool behaviorWouldEmit = _behavior.takeWouldEmit();
    const bool shouldLogWouldEmit = behaviorWouldEmit && rbShouldLogDetail() &&
                                    (_behavior.lastHeardMs() != _rbLastWouldEmitHeardMs ||
                                     _behavior.lastDecision() != _rbLastWouldEmitDecision);
    if (shouldLogWouldEmit) {
        serialOut().print("BEH verdict=");
        serialOut().print(_behavior.lastVerdictTypeName());
        serialOut().print(" heard=");
        serialOut().print(_behavior.lastHeardMs());
        serialOut().print(" now=");
        serialOut().print(now);
        serialOut().print(" decision=");
        serialOut().print(_behavior.lastDecisionName());
        serialOut().print(" block=");
        serialOut().print(_behavior.lastBlockReasonName());
        serialOut().print(" waitMs=");
        serialOut().print(_behavior.waitRemainingMs(now));
        serialOut().print(" refractoryMs=");
        serialOut().print(_behavior.refractoryRemainingMs(now));
        serialOut().print(" behaviorSuppressMs=");
        serialOut().print(_behavior.behaviorSuppressRemainingMs(now));
        serialOut().print(" outputBusy=");
        serialOut().print(_behavior.outputBusy() ? 1 : 0);
        serialOut().print(" wouldEmit=");
        serialOut().println(1);
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
            serialOut().print("BEH emit start now=");
            serialOut().print(now);
            serialOut().print(" source=");
            serialOut().print(sourceName);
            serialOut().print(" state=");
            serialOut().print(_behavior.stateName());
            serialOut().print(" pattern=");
            serialOut().println(chirpPatternName(chirpPattern));
            serialOut().print("RB emit accepted source=");
            serialOut().print(sourceName);
            serialOut().print(" state=");
            serialOut().print(_behavior.stateName());
            serialOut().print(" gate=");
            serialOut().print(behaviorGateName(_behavior, now, sawPatternThisLoop, selfChirpSuppressed));
            serialOut().print(" pattern=");
            serialOut().println(chirpPatternName(chirpPattern));
        }
    _debug.observeChirpStarted(now, sourceName, chirpPattern);
    _debug.flashLed(now, 2);
    // A response to a heard pattern is the network signal: the node's hop
    // train inside the band (BEH-005). Idle voice and self-test stay plain
    // chirps. Own-emit suppression covers the train's span.
    unsigned long emitSpanMs = 0;
    if (_hopEnabled && _behavior.signalShape() == ResonantBehavior::SignalShape::NetworkSignal) {
        emitSpanMs = startNetworkSignal();
    } else if (_idleAnnounceHz > 0 && chirpPattern == ChirpOutput::ChirpPattern::Idle) {
        // Idle call that wakes the network (owner 2026-10-17): an announcement
        // tone outside the band (not detected), a gap, then the node's own call
        // as configured: the hop train, or the plain chirp when the hop is off.
        // The others hear the call and answer. Behavior stays in Chirping and
        // own-emit suppression covers the whole span.
        _chirpOutput.setToneHz(_idleAnnounceHz);
        _chirpOutput.setTiming(_idleAnnounceMs, 0);
        _chirpOutput.start(ChirpOutput::ChirpPattern::Single);
        _idleHopPending = true;
        _idleHopArmed = false;
        const bool hopCall = _hopEnabled && bandBinCount() > 0;
        const unsigned long trainMs = hopCall
            ? static_cast<unsigned long>(_hopSteps) * _hopPulseMs
                  + static_cast<unsigned long>(_hopSteps > 0 ? _hopSteps - 1 : 0) * _hopGapMs
            : runtime::kDefaultChirpDurationMs;
        emitSpanMs = _idleAnnounceMs + kIdleAnnounceGapMs + trainMs;
        if (rbShouldLogDetail()) {
            serialOut().print("RB emit shape=idle_call announce_hz=");
            serialOut().print(_idleAnnounceHz);
            serialOut().print(" announce_ms=");
            serialOut().print(_idleAnnounceMs);
            serialOut().print(" gap_ms=");
            serialOut().print(kIdleAnnounceGapMs);
            serialOut().println(hopCall ? " then=hop" : " then=chirp");
        }
    }
    if (emitSpanMs == 0) {
        _chirpOutput.start(chirpPattern);
    }
    _behavior.notifyChirpStarted(now, emitSpanMs);
    ++_rbChirpStartedCount;
    selfTestNoteChirpStarted(now, sourceName);
    }

    _chirpOutput.update();

    // Idle call, second half: after the announcement and the gap, the hop train.
    if (_idleHopArmed && timing::atOrAfter(timebase::nowMs(), _idleHopAtMs) && !_chirpOutput.isActive()) {
        _idleHopArmed = false;
        _idleHopPending = false;
        // The node's call: the hop train, or the plain chirp when the hop is off.
        unsigned long callSpanMs = 0;
        if (_hopEnabled && bandBinCount() > 0) {
            callSpanMs = startNetworkSignal();
        }
        if (callSpanMs == 0) {
            _chirpOutput.start(ChirpOutput::ChirpPattern::Single);
            callSpanMs = _chirpOutput.isActive() ? runtime::kDefaultChirpDurationMs : 0;
        }
        if (callSpanMs == 0) {
            const unsigned long finishedMs = timebase::nowMs();
            _lastOwnChirpEndMs = finishedMs;
            _behavior.notifyChirpFinished(finishedMs);
        }
    }

    if (_idleHopPending && !_idleHopArmed && _chirpOutput.finished()) {
        // The announcement ended: restore the chirp tone and timing, start the gap.
        // Behavior is not told "finished" yet; that comes when the train ends.
        _chirpOutput.setToneHz(runtime::kDefaultChirpFrequencyHz);
        _chirpOutput.setTiming(runtime::kDefaultChirpDurationMs, 150);
        _idleHopAtMs = timebase::nowMs() + kIdleAnnounceGapMs;
        _idleHopArmed = true;
    } else if (_chirpOutput.finished()) {
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
                serialOut().print("RB ERR line_too_long max=");
                serialOut().println(static_cast<unsigned long>(sizeof(_serialLineBuffer) - 1));
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
        serialOut().println("RB CMD: RB BEHAV wait=100 refractory=0 idleEnabled=0|1 (idle=MS is the idle TIMEOUT, not the switch) restField=N budget=N budgetWindowMs=MS restMs=MS (0 = off) suppressSelfChirp=250 detectionSuppressTail=0 idleTimeout=20000 idleTimeoutVariation=10000 idleBlockedAfterHeard=3000 idleBlockedAfterOwnEmit=5000");
        serialOut().println("RB CMD: RB STATUS");
        serialOut().println("RB CMD: RB PROFILE name=TonalPulseScalar");
        serialOut().println("RB CMD: RB PROFILE name=AmpExperimental");
        serialOut().println("RB CMD: RB rebase");
        serialOut().println("RB CMD: RB rebase force");
        serialOut().println("RB CMD: RB CALL (play the node's network signal now; a kick for multi-node runs)");
        serialOut().println("RB CMD: RB log off|minimal|full");
        serialOut().println("RB CMD: RB debug off|events|plot");
        serialOut().println("RB CMD: RB summary");
        serialOut().println("RB CMD: RB stop");
        serialOut().println("RB CMD: PARAM LIST");
        serialOut().println("RB CMD: PARAM GET <path>");
        serialOut().println("RB CMD: PARAM SET <path> <value>");
        serialOut().println("RB CMD: PARAM DUMP");
        serialOut().println("RB CMD: SELFTEST | SELFTEST external wait_ms=10000 | SELFTEST chirp n=5 | SELFTEST help");
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
        // RB PARAM tuned only the FrequencyMatch thresholds, removed 2026-10-22
        // (docs/decisions/2026-10-22-remove-frequencymatch.md). Detection params
        // are set through the registry.
        serialOut().println("RB PARAM ERR removed (FrequencyMatch removed); use PARAM SET detection.scalar_<key> <value>");
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
            serialOut().println("RB BEHAV usage=RB BEHAV wait=100 refractory=0 idleEnabled=0|1 (idle=MS is the idle TIMEOUT, not the switch) restField=N budget=N budgetWindowMs=MS restMs=MS (0 = off) suppressSelfChirp=250 detectionSuppressTail=0 idleTimeout=20000 idleTimeoutVariation=10000 idleBlockedAfterHeard=3000 idleBlockedAfterOwnEmit=5000");
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
        unsigned long restAfterFieldPatterns = _behavior.restAfterFieldPatterns();
        unsigned long ownEmitBudget = _behavior.ownEmitBudget();
        unsigned long ownEmitBudgetWindowMs = _behavior.ownEmitBudgetWindowMs();
        unsigned long restMs = _behavior.restMs();

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
            } else if (const char* value = valueAfterPrefix(token, "restField=")) {
                restAfterFieldPatterns = strtoul(value, nullptr, 10);
            } else if (const char* value = valueAfterPrefix(token, "budget=")) {
                ownEmitBudget = strtoul(value, nullptr, 10);
            } else if (const char* value = valueAfterPrefix(token, "budgetWindowMs=")) {
                ownEmitBudgetWindowMs = strtoul(value, nullptr, 10);
            } else if (const char* value = valueAfterPrefix(token, "restMs=")) {
                restMs = strtoul(value, nullptr, 10);
            } else {
                serialOut().print("RB BEHAV ERR token=");
                serialOut().print(token);
                serialOut().println(" (unknown key; ignored)");
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
        _behavior.setRestAfterFieldPatterns(restAfterFieldPatterns);
        _behavior.setOwnEmitBudget(ownEmitBudget);
        _behavior.setOwnEmitBudgetWindowMs(ownEmitBudgetWindowMs);
        _behavior.setRestMs(restMs);

        serialOut().print("RB BEHAV wait=");
        serialOut().print(_behavior.waitAfterHeardMs());
        serialOut().print(" refractory=");
        serialOut().print(_behavior.refractoryAfterEmitMs());
        serialOut().print(" suppressSelfChirp=");
        serialOut().print(_behavior.behaviorSuppressSelfChirpMs());
        serialOut().print(" detectionSuppressTail=");
        serialOut().print(_behavior.detectionSuppressTailMsOwnEmit());
        serialOut().print(" idleTimeout=");
        serialOut().print(_behavior.idleTimeoutMs());
        serialOut().print(" idleTimeoutVariation=");
        serialOut().print(_behavior.idleTimeVariationMs());
        serialOut().print(" idleBlockedHeard=");
        serialOut().print(_behavior.idleBlockedAfterHeardMs());
        serialOut().print(" idleBlockedOwnEmit=");
        serialOut().print(_behavior.idleBlockedAfterOwnEmitMs());
        serialOut().print(" idleEnabled=");
        serialOut().print(_behavior.idleEnabled() ? 1 : 0);
        serialOut().print(" restField=");
        serialOut().print(_behavior.restAfterFieldPatterns());
        serialOut().print(" budget=");
        serialOut().print(_behavior.ownEmitBudget());
        serialOut().print(" budgetWindowMs=");
        serialOut().print(_behavior.ownEmitBudgetWindowMs());
        serialOut().print(" restMs=");
        serialOut().println(_behavior.restMs());
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
            serialOut().println("RB rebase refused busy=selftest");
            return;
        }
        _rbBaselineState = RBBaselineState::Rebase;
        performRbRebase();
        _rbBaselineState = RBBaselineState::Settle;
        _rbBaselineSettleUntilMs = timebase::nowMs() + kRbPostRebaseSettleMs;
        ++_rbForcedRebaseCount;
        serialOut().println("RB rebase forced");
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
            serialOut().print("RB ERR rebase unknown token=");
            serialOut().println(extra);
            return;
        }
        if (_selfTest.phase != SelfTestState::Phase::Idle) {
            serialOut().println("RB rebase refused busy=selftest");
            return;
        }
        startRbQuietBaseline();
        serialOut().println("RB rebase requested quiet-gated");
        return;
    }
    if (startsWithTokenIgnoreCase(line, "RB log")) {
        handleLogCommand(line);
        return;
    }
    if (equalsIgnoreCase(line, "RB CALL") || equalsIgnoreCase(line, "RB call")) {
        // Step 5g: play the node's network signal now, through Behavior (kick).
        _behavior.requestNetworkCall();
        serialOut().print("RB CALL queued resting=");
        serialOut().print(_behavior.resting(timebase::nowMs()) ? 1 : 0);
        serialOut().print(" state=");
        serialOut().println(_behavior.stateName());
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
        serialOut().print("RB debug mode=");
        serialOut().print(_debug.debugModeName());
        serialOut().println(" (use off|events|plot)");
        return;
    }

    serialOut().print("RB debug mode=");
    serialOut().println(_debug.debugModeName());
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
        serialOut().print("RB log mode=");
        serialOut().print(rbLogModeName());
        serialOut().println(" (use off|minimal|full)");
        return;
    }

    serialOut().print("RB log mode=");
    serialOut().println(rbLogModeName());
}

void Node::handleDetectCommand(const char* line) {
    (void)line;
    const detection::DetectionProfile& detectionProfile = activeDetectionProfile();
    serialOut().print("RB STATUS profile=");
    serialOut().print(detection::detectionProfileName(detectionProfile.kind));
    serialOut().print(" emitters=");
    serialOut().print(detection::detectorSelectionName(detectionProfile.detectorSelection));
    serialOut().print(" inspectionPlan=");
    serialOut().print(detection::inspectionPlanName(detectionProfile.inspectionPlan));
    const detection::InspectionModuleConfig* supportRequirement =
        detection::evaluatorFirstEnabledRequirement(detectionProfile.inspectionPlan);
    const bool supportRequired = supportRequirement != nullptr;
    serialOut().print(" has_requirement=");
    serialOut().print(supportRequired ? 1 : 0);
    serialOut().print(" required_label=");
    serialOut().print(supportRequired ? detection::inspectionTargetName(supportRequirement->target) : "none");
    serialOut().print(" behavior_state=");
    serialOut().print(_behavior.stateName());
    serialOut().print(" behavior_idle=");
    serialOut().print(_behavior.idleEnabled() ? 1 : 0);
    serialOut().print(" behavior_suppressSelfChirpMs=");
    serialOut().print(_behavior.behaviorSuppressSelfChirpMs());
    serialOut().print(" behavior_detectionSuppressTailMsOwnEmit=");
    serialOut().print(_behavior.detectionSuppressTailMsOwnEmit());
    serialOut().print(" output_busy=");
    serialOut().print(_behavior.outputBusy() ? 1 : 0);
    serialOut().print(" band=");
    serialOut().print(_freqBandStream.bandLowHz());
    serialOut().print('-');
    serialOut().print(_freqBandStream.bandHighHz());
    serialOut().print(" hop_enabled=");
    serialOut().print(_hopEnabled ? 1 : 0);
    serialOut().print(" hop_rotation=");
    serialOut().print(_hopRotation);
    serialOut().print(" hop_steps=");
    serialOut().print(_hopSteps);
    serialOut().print(" hop_pulse_ms=");
    serialOut().print(_hopPulseMs);
    serialOut().print(" hop_gap_ms=");
    serialOut().print(_hopGapMs);
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
    serialOut().print(" freq.window_samples=");
    serialOut().print(windowSizeSamples);
    serialOut().print(" freq.window_ms=");
    serialOut().print(windowMs, 2);
    serialOut().print(" freq.update_every_samples=");
    serialOut().print(frequencyUpdateEverySamples);
    serialOut().print(" freq.update_period_ms=");
    serialOut().print(updateStepMs, 3);
    serialOut().print(" freq.target_hz=");
    serialOut().print(_freqBandStream.targetFrequencyHz());
    serialOut().print(" freq.produced_fresh_packet=");
    serialOut().print(_freqBandStream.producedFreshPacketOnLastObserve() ? 1 : 0);
    serialOut().print(" freq.packet_age_samples=");
    serialOut().print(ageSamples);
    serialOut().print(" freq.packet_age_ms=");
    serialOut().print(ageMs, 3);
    const AudioSourceStats& audioStats = _audioSource.stats();
    serialOut().print(" i2s.samples_read=");
    serialOut().print(static_cast<unsigned long>(audioStats.totalSamplesRead));
    serialOut().print(" i2s.dropped_dma_buffers=");
    serialOut().print(audioStats.droppedDmaBuffers);
    serialOut().print(" i2s.clock_rate_mhz=");
    serialOut().print(audioStats.sampleClockRateMilliHz);
    // Audio-source health without the Analyzer (2026-10-16: a Node that
    // heard nothing had no field saying why). overflows: DMA overflow
    // events; backsteps: blocks whose time went backwards; clock
    // corrections: sample-clock re-anchors and the largest one; deficit: how
    // many seconds of audio are missing against the uptime (reads, drops,
    // stalls), 0.0 when the stream is whole.
    const unsigned long uptimeMs = timebase::nowMs();
    const uint64_t expectedSamples = (static_cast<uint64_t>(uptimeMs) * sampleRateHz) / 1000ULL;
    const double deficitS = expectedSamples > audioStats.totalSamplesRead
        ? static_cast<double>(expectedSamples - audioStats.totalSamplesRead) / static_cast<double>(sampleRateHz)
        : 0.0;
    serialOut().print(" i2s.overflows=");
    serialOut().print(audioStats.overflowCount);
    serialOut().print(" i2s.backsteps=");
    serialOut().print(audioStats.timestampBacksteps);
    serialOut().print(" i2s.max_backstep_us=");
    serialOut().print(audioStats.maxTimestampBackstepUs);
    serialOut().print(" i2s.clock_corrections=");
    serialOut().print(audioStats.sampleClockCorrections);
    serialOut().print(" i2s.max_clock_correction_us=");
    serialOut().print(audioStats.maxSampleClockCorrectionUs);
    serialOut().print(" i2s.read_errors=");
    serialOut().print(audioStats.readErrors);
    serialOut().print(" audio.uptime_ms=");
    serialOut().print(uptimeMs);
    serialOut().print(" audio.deficit_s=");
    serialOut().print(deficitS, 1);
    // Step 8b: verdicts the 4-slot queue dropped (was Analyzer-only), and the
    // loop task's lowest free stack since boot (bytes; getWindow() puts 2 KB
    // on it per inspection module, audit 3.7).
    serialOut().print(" detection.verdict_queue_overflows=");
    serialOut().print(_detection.verdictQueueOverflowCount());
    serialOut().print(" loop.stack_free_min_bytes=");
    serialOut().print(static_cast<unsigned long>(uxTaskGetStackHighWaterMark(nullptr)));
    // Why a Node might not hear (2026-10-17): the sample clock against the
    // system clock, the own-emit suppression deadline on that axis, whether
    // the newest sample would be suppressed right now, how many samples the
    // suppression has swallowed since boot, and whether the chirp output
    // still counts as active (a train that never "finishes" keeps Behavior
    // in Chirping, which suppresses every sample).
    serialOut().print(" clock.now_ms=");
    serialOut().print(uptimeMs);
    serialOut().print(" audio.last_sample_ms=");
    serialOut().print(_lastSampleTimeMs);
    serialOut().print(" behavior.suppress_armed=");
    serialOut().print(_behavior.ownEmitSuppressArmed() ? 1 : 0);
    serialOut().print(" behavior.suppress_until_ms=");
    serialOut().print(_behavior.ownEmitSuppressUntilMs());
    serialOut().print(" behavior.suppressed_now=");
    serialOut().print(_behavior.ownEmitDetectionSuppressed(_lastSampleTimeMs) ? 1 : 0);
    serialOut().print(" behavior.suppressed_samples=");
    serialOut().print(_suppressedSampleCount);
    serialOut().print(" chirp_active=");
    serialOut().print(_chirpOutput.isActive() ? 1 : 0);
    serialOut().print(" field.patterns=");
    serialOut().print(_detection.fieldState().recentPatternCount);
    serialOut().print(" behavior.resting=");
    serialOut().print(_behavior.resting(uptimeMs) ? 1 : 0);
    serialOut().print(" behavior.rest_until_ms=");
    serialOut().print(_behavior.restUntilMs());
    serialOut().print(" behavior.rests=");
    serialOut().print(_behavior.restsEntered());
    serialOut().print(" behavior.last_rest=");
    serialOut().print(ResonantBehavior::behaviorDecisionName(_behavior.lastRestReason()));
    serialOut().print(" behavior.blocked_resting=");
    serialOut().print(_behavior.blockedResting());
    serialOut().print(" ");
    printBuildIdentity(serialOut(), "node");
    serialOut().println();
}

void Node::handleProfileCommand(const char* line) {
    const char* name = line + 10; // strlen("RB PROFILE")
    while (*name == ' ') {
        ++name;
    }

    if (*name == '\0') {
        serialOut().print("RB PROFILE name=");
        serialOut().println(profileName());
        return;
    }

    if (startsWithTokenIgnoreCase(name, "name=")) {
        name += 5;
    }

    if (setProfileFromName(name)) {
        serialOut().print("RB PROFILE name=");
        serialOut().print(profileName());
        printProfileComposition(activeDetectionProfile());
        serialOut().println();
    } else {
        serialOut().println("RB PROFILE usage=name=TonalPulseScalar|AmpExperimental");
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
        serialOut().println("PARAM usage=PARAM LIST|GET <path>|SET <path> <value>|DUMP");
        return;
    }

    if (equalsIgnoreCase(token, "LIST")) {
        _paramRegistry.list(serialOut());
        return;
    }

    if (equalsIgnoreCase(token, "DUMP")) {
        _paramRegistry.dump(serialOut());
        return;
    }

    if (equalsIgnoreCase(token, "GET")) {
        const char* path = strtok_r(nullptr, " ", &savePtr);
        const param::ParamBinding* binding = path != nullptr ? _paramRegistry.find(path) : nullptr;
        if (binding == nullptr) {
            serialOut().print("PARAM GET path=");
            serialOut().print(path != nullptr ? path : "");
            serialOut().print(" status=");
            serialOut().println(param::paramSetStatusName(param::ParamSetStatus::UnknownParam));
            return;
        }
        serialOut().print("PARAM GET path=");
        serialOut().print(path);
        serialOut().print(" module=");
        serialOut().print(_paramRegistry.groupName(binding->group));
        serialOut().print(" value=");
        _paramRegistry.printValue(serialOut(), *binding);
        serialOut().println();
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
            serialOut().print("PARAM SET path=");
            serialOut().print(path != nullptr ? path : "");
            serialOut().print(" status=");
            serialOut().println(param::paramSetStatusName(param::ParamSetStatus::UnknownParam));
            return;
        }

        serialOut().print("PARAM SET path=");
        serialOut().print(path);
        serialOut().print(" module=");
        serialOut().print(_paramRegistry.groupName(binding->group));
        serialOut().print(" old=");
        _paramRegistry.printValue(serialOut(), *binding);

        const param::ParamSetStatus status = _paramRegistry.applyValue(*binding, value);

        serialOut().print(" new=");
        _paramRegistry.printValue(serialOut(), *binding);
        serialOut().print(" status=");
        serialOut().println(param::paramSetStatusName(status));

        if (status == param::ParamSetStatus::Ok) {
            applyParamModule(static_cast<param::ModuleId>(binding->group));
        }
        return;
    }

    serialOut().println("PARAM usage=PARAM LIST|GET <path>|SET <path> <value>|DUMP");
}

// Dirty ModuleId apply route: a successful PARAM SET only writes the bound
// field, so the owning module still needs to be told to pick the change up.
void Node::applyParamModule(param::ModuleId module) {
    switch (module) {
        case param::ModuleId::Detection:
            applyLiveDetectionParams();
            printHopGapCheck();
            printInspectionPlanCheck();
            break;
        case param::ModuleId::Output:
            printHopGapCheck();
            break;
        case param::ModuleId::Node:
        case param::ModuleId::Behavior:
            break;
    }
}

// One line on the fit between the network signal and the scalar detector
// (detection/HopGapContract.h). status=fail: the train can never be one
// occurrence; status=warn: only where the room tail bridges the gaps (the
// shipped 10 ms gap against release debounce 10 is warn, step 5h item 1).
void Node::printHopGapCheck() {
    if (!_hopEnabled) {
        return;
    }
    uint32_t hz[8];
    detection::HopTrainShape shape;
    shape.steps = static_cast<unsigned long>(hopFrequencies(hz, sizeof(hz) / sizeof(hz[0])));
    shape.pulseMs = _hopPulseMs;
    shape.gapMs = _hopGapMs;
    const detection::ScalarTransientConfig& sc = _activeDetectionProfile.scalarTransient;
    const detection::HopGapCheck c = detection::checkHopGap(shape, sc);
    serialOut().print("RB hop_check status=");
    serialOut().print(!c.hardOk() ? "fail" : (!c.softOk() ? "warn" : "ok"));
    serialOut().print(" train_ms=");
    serialOut().print(c.trainMs);
    serialOut().print(" gap_ms=");
    serialOut().print(shape.gapMs);
    serialOut().print(" release_debounce_ms=");
    serialOut().print(sc.releaseDebounceMs);
    serialOut().print(" max_gap_ms=");
    serialOut().print(sc.maxGapMs);
    serialOut().print(" duration_ms=");
    serialOut().print(sc.minTransientDurationMs);
    serialOut().print("..");
    serialOut().print(sc.maxTransientDurationMs);
    serialOut().print(" issues=");
    bool any = false;
    const struct {
        bool ok;
        const char* name;
    } items[] = {
        {c.durationFits, "duration"},
        {c.coverageFits, "coverage"},
        {c.islandFits, "island"},
        {c.gapWithinMaxGap, "max_gap"},
        {c.gapBelowDebounce, "gap_not_below_debounce"},
        {c.tailFits, "no_tail_margin"},
    };
    for (const auto& item : items) {
        if (!item.ok) {
            serialOut().print(any ? "," : "");
            serialOut().print(item.name);
            any = true;
        }
    }
    serialOut().println(any ? "" : "none");
}

// RB plan_check: detection.scalar_* durations against the inspection plan's
// windows and the FeatureHistory depth (detection/InspectionPlanContract.h,
// step 8b item 2). status=fail: inspection reads past the history;
// status=warn: occurrences near the min duration are inspected before their
// window has arrived (shipped: early_window_ms=5, audit 3.1, DET-017).
void Node::printInspectionPlanCheck() {
    const detection::InspectionPlanCheck c =
        detection::checkInspectionPlan(_activeDetectionProfile.scalarTransient, _activeDetectionProfile.inspectionPlan);
    serialOut().print("RB plan_check status=");
    serialOut().print(!c.hardOk() ? "fail" : (!c.softOk() ? "warn" : "ok"));
    serialOut().print(" history_needed_ms=");
    serialOut().print(c.historyNeededMs);
    serialOut().print(" history_ms=");
    serialOut().print(c.historySpanMs);
    serialOut().print(" early_window_ms=");
    serialOut().print(c.earlyWindowMs);
    serialOut().print(" issues=");
    if (c.hardOk() && c.softOk()) {
        serialOut().println("none");
        return;
    }
    serialOut().print(!c.historyFits ? "history_too_short" : "");
    serialOut().print(!c.historyFits && !c.windowCompleteAtRelease ? "," : "");
    serialOut().println(!c.windowCompleteAtRelease ? "window_after_inspection" : "");
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
            serialOut().print("RB verdict=");
            serialOut().print(detection::verdictTypeName(verdict.type));
            serialOut().print(" fieldQuiet=");
            serialOut().print(_detection.fieldState().quiet ? 1 : 0);
            serialOut().print(" fieldActive=");
            serialOut().print(_detection.fieldState().active ? 1 : 0);
            serialOut().print(" fieldDense=");
            serialOut().print(_detection.fieldState().dense ? 1 : 0);
            // Outputs off: Behavior never saw this verdict, so its last
            // decision belongs to an earlier one (issue #31 R2-11).
            serialOut().print(" behaviorEligible=");
            serialOut().print(behaviorSeesVerdict && _behavior.behaviorEligible() ? 1 : 0);
            serialOut().print(" decision=");
            serialOut().print(behaviorSeesVerdict ? _behavior.lastDecisionName() : "outputs_off");
            serialOut().print(" valid=");
            serialOut().print(verdict.valid ? 1 : 0);
            serialOut().print(" profile=");
            serialOut().println(profileName());
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

    serialOut().print("RB summary: pending=");
    serialOut().print(_rbPendingCount);
    serialOut().print(" accepted=");
    serialOut().print(_rbPatternAcceptedCount);
    serialOut().print(" validPatterns=");
    serialOut().print(_rbValidPatternCount);
    serialOut().print(" chirpStarted=");
    serialOut().print(_rbChirpStartedCount);
    serialOut().print(" overflowPending=");
    serialOut().print(_rbOverflowPending);
    serialOut().print(" avg_strength=");
    serialOut().print(avgStrength, 1);
    serialOut().print(" avg_duration=");
    serialOut().print(avgDuration, 1);
    serialOut().print("ms");
    serialOut().print(" profile=");
    serialOut().print(profileName());
    printBuildIdentity(serialOut(), "node");
    serialOut().println();
    serialOut().print("RB baseline state=");
    serialOut().println(rbBaselineStateName());
    serialOut().print("RB log mode=");
    serialOut().println(rbLogModeName());

    printRbBehaviorSummary();
    printRbSignalSummary();
}

void Node::printRbBehaviorSummary() const {
    serialOut().print("RB behavior idle=");
    serialOut().print(_behavior.idleEnabled() ? 1 : 0);
    serialOut().print(" wait=");
    serialOut().print(_behavior.waitAfterHeardMs());
    serialOut().print(" refractory=");
    serialOut().print(_behavior.refractoryAfterEmitMs());
    serialOut().print(" suppressSelfChirp=");
    serialOut().print(_behavior.behaviorSuppressSelfChirpMs());
    serialOut().print(" detectionSuppressTail=");
    serialOut().print(_behavior.detectionSuppressTailMsOwnEmit());
    serialOut().print(" idleTimeout=");
    serialOut().print(_behavior.idleTimeoutMs());
    serialOut().print(" idleTimeoutVariation=");
    serialOut().print(_behavior.idleTimeVariationMs());
    serialOut().print(" idleBlockedHeard=");
    serialOut().print(_behavior.idleBlockedAfterHeardMs());
    serialOut().print(" idleBlockedOwnEmit=");
    serialOut().print(_behavior.idleBlockedAfterOwnEmitMs());
    serialOut().print(" idleEnabled=");
    serialOut().print(_behavior.idleEnabled() ? 1 : 0);
    serialOut().print(" lastDecision=");
    serialOut().print(_behavior.lastDecisionName());
    serialOut().print(" lastBlock=");
    serialOut().print(_behavior.lastBlockReasonName());
    serialOut().print(" lastPattern=");
    serialOut().print(_behavior.lastVerdictTypeName());
    serialOut().print(" lastHeardMs=");
    serialOut().print(_behavior.lastHeardMs());
    serialOut().print(" lastEmitMs=");
    serialOut().print(_behavior.lastEmitMs());
    serialOut().print(" waitUntilMs=");
    serialOut().print(_behavior.waitUntilMs());
    serialOut().print(" refractoryUntilMs=");
    serialOut().print(_behavior.refractoryUntilMs());
    serialOut().print(" ownEmitDetectionSuppressUntilMs=");
    serialOut().print(_behavior.ownEmitDetectionSuppressUntilMs());
    serialOut().print(" outputBusy=");
    serialOut().print(_behavior.outputBusy() ? 1 : 0);
    serialOut().print(" patternsReceived=");
    serialOut().print(_behavior.patternsReceived());
    serialOut().print(" ignoredInvalid=");
    serialOut().print(_behavior.patternsIgnoredInvalid());
    serialOut().print(" ignoredAmbiguous=");
    serialOut().print(_behavior.patternsIgnoredAmbiguous());
    serialOut().print(" blockedOutputBusy=");
    serialOut().print(_behavior.blockedOutputBusy());
    serialOut().print(" blockedWaiting=");
    serialOut().print(_behavior.blockedWaiting());
    serialOut().print(" blockedRefractory=");
    serialOut().print(_behavior.blockedRefractory());
    serialOut().print(" blockedSelfSuppressed=");
    serialOut().print(_behavior.blockedSelfSuppressed());
    serialOut().print(" wouldEmit=");
    serialOut().print(_behavior.wouldEmitCount());
    serialOut().print(" emitted=");
    serialOut().println(_behavior.emittedCount());
}

void Node::printRbSignalSummary() const {
    serialOut().print("RB occurrence baseline=");
    serialOut().print(_audioSignal.baseline(), 1);
    serialOut().print(" mag=");
    serialOut().print(_audioSignal.signalMagnitude());
    serialOut().print(" smooth=");
    serialOut().println(_audioSignal.smoothedSignalMagnitude());
}


