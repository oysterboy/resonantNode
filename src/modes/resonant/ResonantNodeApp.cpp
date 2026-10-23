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
    serialOut().println("RB behavior");
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
    // PAR-010 boot order: the fields hold the firmware defaults now (profile,
    // roster rotation), so capture them, then let valid stored values win
    // (decisions/2026-10-23-param-persistence-save-only.md) and apply once.
    _paramRegistry.captureDefaults();
    loadStoredParams();
    {
        const param::ParamBinding* rotation = _paramRegistry.find("output.hop_rotation");
        if (rotation != nullptr && !_paramRegistry.isDefault(*rotation)) {
            serialOut().print("RB node rotation_source=stored rotation=");
            serialOut().println(_hopRotation);
        }
    }
    printSignalConfig();
    printHopGapCheck();
    printInspectionPlanCheck();
    _behavior.resetState();
    resetDetectionState();
    _audioSignal.resetStats();
    _audioSource.resetStats();
    resetRbCounters();
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
    _silentApplied = _silent;
    if (_silent) {
        serialOut().println("RB silent=1 (output.silent: listening, never sounding)");
    }
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
// (the behavior.* params) is kept; performRbRebase does the same plus the audio reset
// (issue #31 R2-3).
void Node::quiesceBehaviorForOutputsOff() {
    // Also an idle call waiting in its gap (before step 10f its train still started later).
    if (_soundOutput.busy() || _chirpOutput.isActive()) {
        _soundOutput.stop();
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
    _rbValidStrengthSum = 0;
    _rbDurationSumMs = 0;
}

void Node::resetDetectionState() {
    _audioSignal.resetSignalState();
    _detection.resetState();
    // Detection config only: a rebase must not put live behavior.* values back
    // to the profile defaults (issue #31). Profile switches and boot apply
    // both through applyActiveProfiles().
    applyActiveDetectionProfile();
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
}

// --- baseline / startup ---

bool Node::rbOutputsEnabled() const {
    if (_silent) {
        return false;
    }
    return _rbBaselineState == RBBaselineState::Active
        || _rbBaselineState == RBBaselineState::FailedNoQuiet;
}

// output.silent changed (PARAM SET, LOAD, RESET, or a stored value at boot):
// stop a sounding emission and drop Behavior's state either way, so a pattern
// heard or a call queued before the change does not fire after it, and the
// idle call starts its schedule fresh.
void Node::applySilent() {
    if (_silent == _silentApplied) {
        return;
    }
    _silentApplied = _silent;
    quiesceBehaviorForOutputsOff();
    _behavior.seedIdleSchedule(timebase::nowMs());
    serialOut().print("RB silent=");
    serialOut().println(_silent ? 1 : 0);
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
    registerBehaviorParams();
    loadInspectionLimitsFromProfile();
    _paramRegistry.addFloat(param::ParamId::Detection_ContrastMedium, param::ModuleId::Detection,
                            "detection.contrast_medium", &_contrastMediumThreshold, 0.0f, 32767.0f);
    _paramRegistry.addFloat(param::ParamId::Detection_AmpMedium, param::ModuleId::Detection,
                            "detection.amp_medium", &_ampMediumThreshold, 0.0f, 32767.0f);
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
    _paramRegistry.addBool(ParamId::Output_Silent, ModuleId::Output, "output.silent", &_silent);
}

// The registered detection params (detection.scalar_*, detection.band_*) applied
// in place: scalar thresholds through the detector's setters, the band through
// FreqBandStream. No detector reset and no FeatureHistory wipe, so a live PARAM
// SET does not drop an occurrence in flight (step 10 item 2, issue #14,
// decisions/2026-10-22-live-params-profile-reset-no-rb-behav.md D2). A profile
// switch still goes through applyActiveDetectionProfile().
// Behavior's BehaviorGateConfig on the registry (PAR-006, step 10 item 4). The
// params bind into the Node-owned _activeBehaviorProfile; a PARAM SET applies it
// through ResonantBehavior::configure(), which only calls setters (no state
// reset). Paths are the snake_case field names; the RB BEHAV keys they replace
// are mapped in decisions/2026-10-22-live-params-profile-reset-no-rb-behav.md.
void Node::registerBehaviorParams() {
    static_assert(sizeof(unsigned long) == sizeof(uint32_t), "behavior ms fields are bound as uint32_t");
    BehaviorGateConfig& b = _activeBehaviorProfile;
    using param::ModuleId;
    using param::ParamId;
    auto u32 = [](unsigned long* field) { return reinterpret_cast<uint32_t*>(field); };
    _paramRegistry.addBool(ParamId::Behavior_IdleEnabled, ModuleId::Behavior, "behavior.idle_enabled", &b.idleEnabled);
    _paramRegistry.addUInt32(ParamId::Behavior_WaitAfterHeardMs, ModuleId::Behavior,
                             "behavior.wait_after_heard_ms", u32(&b.waitAfterHeardMs), 0U, 10000U);
    _paramRegistry.addUInt32(ParamId::Behavior_RefractoryAfterEmitMs, ModuleId::Behavior,
                             "behavior.refractory_after_emit_ms", u32(&b.refractoryAfterEmitMs), 0U, 60000U);
    _paramRegistry.addUInt32(ParamId::Behavior_SuppressSelfChirpMs, ModuleId::Behavior,
                             "behavior.suppress_self_chirp_ms", u32(&b.behaviorSuppressSelfChirpMs), 0U, 10000U);
    _paramRegistry.addUInt32(ParamId::Behavior_DetectionSuppressTailMs, ModuleId::Behavior,
                             "behavior.detection_suppress_tail_ms", u32(&b.detectionSuppressTailMsOwnEmit), 0U, 5000U);
    _paramRegistry.addUInt32(ParamId::Behavior_IdleTimeoutMs, ModuleId::Behavior,
                             "behavior.idle_timeout_ms", u32(&b.idleTimeoutMs), 0U, 3600000U);
    _paramRegistry.addUInt32(ParamId::Behavior_IdleTimeoutVariationMs, ModuleId::Behavior,
                             "behavior.idle_timeout_variation_ms", u32(&b.idleTimeVariationMs), 0U, 3600000U);
    _paramRegistry.addUInt32(ParamId::Behavior_IdleBlockedAfterHeardMs, ModuleId::Behavior,
                             "behavior.idle_blocked_after_heard_ms", u32(&b.idleBlockedAfterHeardMs), 0U, 600000U);
    _paramRegistry.addUInt32(ParamId::Behavior_IdleBlockedAfterOwnEmitMs, ModuleId::Behavior,
                             "behavior.idle_blocked_after_own_emit_ms", u32(&b.idleBlockedAfterOwnEmitMs), 0U, 600000U);
    _paramRegistry.addUInt32(ParamId::Behavior_RestAfterFieldPatterns, ModuleId::Behavior,
                             "behavior.rest_after_field_patterns", u32(&b.restAfterFieldPatterns), 0U, 100U);
    _paramRegistry.addUInt32(ParamId::Behavior_OwnEmitBudget, ModuleId::Behavior,
                             "behavior.own_emit_budget", u32(&b.ownEmitBudget), 0U, 100U);
    _paramRegistry.addUInt32(ParamId::Behavior_OwnEmitBudgetWindowMs, ModuleId::Behavior,
                             "behavior.own_emit_budget_window_ms", u32(&b.ownEmitBudgetWindowMs), 0U, 600000U);
    _paramRegistry.addUInt32(ParamId::Behavior_RestMs, ModuleId::Behavior,
                             "behavior.rest_ms", u32(&b.restMs), 0U, 600000U);
}

namespace {
// The plan module that inspects `target`, or nullptr (step 10 item 7).
detection::InspectionModuleConfig* moduleForTarget(detection::InspectionPlan& plan, detection::InspectionTarget target) {
    const size_t count = plan.count > detection::kMaxInspectionModules ? detection::kMaxInspectionModules : plan.count;
    for (size_t i = 0; i < count; ++i) {
        if (plan.modules[i].target == target) {
            return &plan.modules[i];
        }
    }
    return nullptr;
}
}  // namespace

// The inspectors' medium limits as live params (step 10 item 7). They are Node
// values copied from the active profile's plan, so the binding does not depend
// on the plan's module order; applyLiveDetectionParams() writes them back into
// the module with the matching target. A profile without such a module ignores
// the value.
void Node::loadInspectionLimitsFromProfile() {
    detection::InspectionModuleConfig* contrast = moduleForTarget(_activeDetectionProfile.inspectionPlan, detection::InspectionTarget::Contrast);
    detection::InspectionModuleConfig* amp = moduleForTarget(_activeDetectionProfile.inspectionPlan, detection::InspectionTarget::Amp);
    _contrastMediumThreshold = contrast != nullptr ? contrast->magnitude.supportStrength.mediumPeakThreshold : 0.0f;
    _ampMediumThreshold = amp != nullptr ? amp->magnitude.supportStrength.mediumPeakThreshold : 0.0f;
}

void Node::applyLiveDetectionParams() {
    _detection.setScalarTransientConfig(_activeDetectionProfile.scalarTransient);
    detection::InspectionModuleConfig* contrast = moduleForTarget(_activeDetectionProfile.inspectionPlan, detection::InspectionTarget::Contrast);
    detection::InspectionModuleConfig* amp = moduleForTarget(_activeDetectionProfile.inspectionPlan, detection::InspectionTarget::Amp);
    if (contrast != nullptr) {
        contrast->magnitude.supportStrength.mediumPeakThreshold = _contrastMediumThreshold;
    }
    if (amp != nullptr) {
        amp->magnitude.supportStrength.mediumPeakThreshold = _ampMediumThreshold;
    }
    _detection.setInspectionPlan(_activeDetectionProfile.inspectionPlan);  // same streams: history kept
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

// The node's network signal as a request (BEH-005): the hop train on the band's
// bins, rotated per node. False when the band has no bins.
bool Node::composeHopTrain(OutputRequest& request) const {
    const size_t n = hopFrequencies(request.hopHz, kMaxHopSteps);
    if (n == 0 || _hopPulseMs == 0) {
        request.hopSteps = 0;
        return false;
    }
    request.hopSteps = n;
    request.pulseMs = _hopPulseMs;
    request.gapMs = _hopGapMs;
    return true;
}

void Node::printHopShape(const OutputRequest& request) {
    if (!rbShouldLogDetail()) {
        return;
    }
    serialOut().print("RB emit shape=hop steps=");
    serialOut().print(static_cast<unsigned long>(request.hopSteps));
    serialOut().print(" pulse_ms=");
    serialOut().print(request.pulseMs);
    serialOut().print(" gap_ms=");
    serialOut().print(request.gapMs);
    serialOut().print(" order=");
    for (size_t k = 0; k < request.hopSteps; ++k) {
        if (k > 0) {
            serialOut().print(',');
        }
        serialOut().print(request.hopHz[k]);
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
    applySilent();  // output.silent from any path (two compares when unchanged)
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
        startEmission(now, sawPatternThisLoop, selfChirpSuppressed);
    }
    updateEmissions(now);

    _debug.updateLed(now);

    if (_selfTest.phase != SelfTestState::Phase::Idle) {
        updateSelfTest(timebase::nowMs());
    }

    _debug.endLoop(timebase::nowUs());
}

// Behavior asked for a chirp: start the emission it asked for (step 10 item 9:
// out of update(), unchanged). A heard pattern is answered with the network
// signal (the hop train); the idle call is an announcement tone, then the call.
void Node::startEmission(unsigned long now, bool sawPatternThisLoop, bool selfChirpSuppressed) {
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
    // train inside the band (BEH-005). The idle call is an announcement tone
    // outside the band (not detected), a gap, then the node's own call (owner
    // 2026-10-17): the hop train, or the plain chirp when the hop is off. Idle
    // voice and self-test stay plain chirps. SoundOutput plays the request
    // (step 10f); own-emit suppression covers its span.
    OutputRequest request;
    request.pattern = chirpPattern;
    if (_hopEnabled && _behavior.signalShape() == ResonantBehavior::SignalShape::NetworkSignal &&
        composeHopTrain(request)) {
        request.kind = OutputRequest::Kind::HopTrain;
    } else if (_idleAnnounceHz > 0 && chirpPattern == ChirpOutput::ChirpPattern::Idle) {
        request.kind = OutputRequest::Kind::IdleCall;
        request.announceHz = _idleAnnounceHz;
        request.announceMs = _idleAnnounceMs;
        request.announceGapMs = kIdleAnnounceGapMs;
        if (!_hopEnabled || !composeHopTrain(request)) {
            request.hopSteps = 0;
        }
    } else {
        request.kind = OutputRequest::Kind::Pattern;
    }
    bool started = _soundOutput.start(request);
    if (!started && request.kind != OutputRequest::Kind::Pattern) {
        // As before step 10f: a train or call that does not start falls back to the pattern.
        serialOut().print("RB emit rejected reason=");
        serialOut().print(outputRejectName(_soundOutput.status().lastReject));
        serialOut().println(" fallback=pattern");
        request.kind = OutputRequest::Kind::Pattern;
        started = _soundOutput.start(request);
    }
    const unsigned long emitSpanMs = started ? _soundOutput.status().spanMs : 0;
    if (started && request.kind == OutputRequest::Kind::HopTrain) {
        printHopShape(request);
    }
    if (started && request.kind == OutputRequest::Kind::IdleCall && rbShouldLogDetail()) {
        serialOut().print("RB emit shape=idle_call announce_hz=");
        serialOut().print(request.announceHz);
        serialOut().print(" announce_ms=");
        serialOut().print(request.announceMs);
        serialOut().print(" gap_ms=");
        serialOut().print(request.announceGapMs);
        serialOut().println(request.hopSteps > 0 ? " then=hop" : " then=chirp");
    }
    _behavior.notifyChirpStarted(now, emitSpanMs);
    ++_rbChirpStartedCount;
    if (!started) {
        // Nothing sounds: end the emission at once so Behavior does not wait in Chirping.
        serialOut().print("RB emit rejected reason=");
        serialOut().println(outputRejectName(_soundOutput.status().lastReject));
        const unsigned long finishedMs = timebase::nowMs();
        _lastOwnChirpEndMs = finishedMs;
        _behavior.notifyChirpFinished(finishedMs);
    }
    selfTestNoteChirpStarted(now, sourceName);
}

// The output tick, the idle call's second half (the train after the
// announcement) and the end of an emission (step 10 item 9: out of update(),
// unchanged).
void Node::updateEmissions(unsigned long now) {
    const uint8_t events = _soundOutput.update();
    if ((events & SoundOutput::kEventCallStarted) != 0 && _soundOutput.callIsHop()) {
        printHopShape(_soundOutput.current());
    }
    if ((events & SoundOutput::kEventFinished) != 0) {
        // Fresh time (SoundOutput stamps it at the actual end): the own-emit
        // tail runs from the toneOff, not from the loop start (issue #31).
        const unsigned long finishedMs = _soundOutput.finishedAtMs();
        _lastOwnChirpEndMs = finishedMs;
        _behavior.notifyChirpFinished(finishedMs);
        _debug.observeChirpFinished(now);
        selfTestNoteChirpFinished(now);
    }
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
        serialOut().println("RB CMD: PARAM SET behavior.<key> <value> (Behavior timing, rest and idle call; PARAM LIST)");
        serialOut().println("RB CMD: RB STATUS");
        serialOut().println("RB CMD: RB PROFILE name=TonalPulseScalar");
        serialOut().println("RB CMD: RB PROFILE name=AmpExperimental");
        serialOut().println("RB CMD: RB rebase");
        serialOut().println("RB CMD: RB rebase force");
        serialOut().println("RB CMD: RB CALL (play the node's network signal now; a kick for multi-node runs)");
        serialOut().println("RB CMD: RB log off|minimal|full");
        serialOut().println("RB CMD: RB debug off|events");
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
        printRbStatus(line);
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
        // RB BEHAV was removed in step 10 (decisions/2026-10-22-live-params-
        // profile-reset-no-rb-behav.md): Behavior's values are behavior.*
        // params; PARAM DUMP shows them, PARAM SET changes them.
        serialOut().println("RB BEHAV ERR removed; use PARAM SET behavior.<key> <value> (PARAM LIST shows the keys)");
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
        if (_silent) {
            serialOut().println("RB CALL ignored silent=1");
            return;
        }
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
    } else {
        serialOut().print("RB debug mode=");
        serialOut().print(_debug.debugModeName());
        serialOut().println(" (use off|events)");
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
// The one command path for every registered knob since step 10 (RB PARAM and
// RB BEHAV were removed; docs/roadmaps/roadmap-param-config.md PAR-015).
void Node::handleParamCommand(const char* line) {
    char buffer[kSerialLineCapacity];
    strncpy(buffer, line, sizeof(buffer));
    buffer[sizeof(buffer) - 1] = '\0';

    char* savePtr = nullptr;
    char* token = strtok_r(buffer, " ", &savePtr);
    token = token != nullptr ? strtok_r(nullptr, " ", &savePtr) : nullptr;

    if (token == nullptr) {
        serialOut().println("PARAM usage=PARAM LIST|GET <path>|SET <path> <value>|DUMP|SAVE|LOAD|RESET");
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

    // Persistence (PAR-011): only SAVE writes flash. LOAD re-reads the store,
    // RESET clears it and restores the firmware defaults; both apply at once.
    if (equalsIgnoreCase(token, "SAVE")) {
        const param::ParamSaveReport report = _paramRegistry.save(_paramStore);
        serialOut().print("PARAM SAVE written=");
        serialOut().print(static_cast<unsigned long>(report.written));
        serialOut().print(" unchanged=");
        serialOut().print(static_cast<unsigned long>(report.unchanged));
        serialOut().print(" failed=");
        serialOut().print(static_cast<unsigned long>(report.failed));
        serialOut().print(" status=");
        serialOut().println(report.failed == 0 ? "ok" : "error");
        return;
    }

    if (equalsIgnoreCase(token, "LOAD")) {
        loadStoredParams();
        applyParamModule(param::ModuleId::Detection);
        return;
    }

    if (equalsIgnoreCase(token, "RESET")) {
        const bool cleared = _paramStore.clear();
        _paramRegistry.restoreDefaults();
        serialOut().print("PARAM RESET store=");
        serialOut().print(cleared ? "cleared" : "error");
        serialOut().println(" values=defaults");
        applyParamModule(param::ModuleId::Behavior);
        applyParamModule(param::ModuleId::Detection);  // also prints hop_check and plan_check
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

    serialOut().println("PARAM usage=PARAM LIST|GET <path>|SET <path> <value>|DUMP|SAVE|LOAD|RESET");
}

// Stored values into the fields (PAR-010), then every module applies once. At
// boot this runs before the signal config prints; PARAM LOAD prints the checks
// after it. Each value that differs from the default, and each stale or invalid
// entry, has its own line (ParamRegistry::load).
void Node::loadStoredParams() {
    const param::ParamLoadReport report = _paramRegistry.load(_paramStore, serialOut());
    serialOut().print("PARAM load stored=");
    serialOut().print(static_cast<unsigned long>(report.stored));
    serialOut().print(" applied=");
    serialOut().print(static_cast<unsigned long>(report.applied));
    serialOut().print(" stale=");
    serialOut().print(static_cast<unsigned long>(report.stale));
    serialOut().print(" invalid=");
    serialOut().println(static_cast<unsigned long>(report.invalid));
    if (report.applied > 0) {
        applyLiveDetectionParams();
        applyActiveBehaviorGateConfig();
    }
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
            applySilent();
            printHopGapCheck();
            break;
        case param::ModuleId::Behavior:
            applyActiveBehaviorGateConfig();
            break;
        case param::ModuleId::Node:
            break;
    }
}

bool Node::setProfileFromName(const char* name) {
    detection::DetectionProfileKind parsed = _profileKind;
    if (!detection::detectionProfileKindFromName(name, parsed)) {
        return false;
    }

    // D1 (decisions/2026-10-22-live-params-profile-reset-no-rb-behav.md): a
    // profile switch starts from the new profile's defaults (detection.scalar_*
    // and behavior.*; band and output.* are Node values and stay), and says
    // which live values it reset.
    static double before[param::ParamRegistry::kMaxParams];  // static: not on the loop stack (3.4 KB headroom, step 8b)
    const size_t count = _paramRegistry.count();
    for (size_t i = 0; i < count; ++i) {
        before[i] = _paramRegistry.numericValue(_paramRegistry.at(i));
    }
    _profileKind = parsed;
    _activeDetectionProfile = detection::detectionProfileForKind(_profileKind);
    _activeBehaviorProfile = makeTonalPulseBehaviorProfile();
    loadInspectionLimitsFromProfile();
    applyActiveProfiles();
    unsigned long resetCount = 0;
    for (size_t i = 0; i < count; ++i) {
        const param::ParamBinding& binding = _paramRegistry.at(i);
        if (_paramRegistry.numericValue(binding) == before[i]) {
            continue;
        }
        ++resetCount;
        serialOut().print("RB PROFILE reset path=");
        serialOut().print(binding.path);
        serialOut().print(" old=");
        serialOut().print(before[i], binding.type == param::ParamType::Float ? 3 : 0);
        serialOut().print(" new=");
        _paramRegistry.printValue(serialOut(), binding);
        serialOut().println();
    }
    serialOut().print("RB PROFILE reset_count=");
    serialOut().println(resetCount);
    return true;
}

const char* Node::profileName() const {
    return detectionProfileKindName(_profileKind);
}

const detection::DetectionProfile& Node::activeDetectionProfile() const {
    return _activeDetectionProfile;
}

const BehaviorGateConfig& Node::activeBehaviorProfile() const {
    // One behavior program for every detection profile (decisions/2026-10-17-
    // behavior-contract-and-reproducible-sessions.md); its live values are the
    // behavior.* params.
    return _activeBehaviorProfile;
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
            _rbValidStrengthSum += static_cast<uint64_t>(verdict.primaryStrength + 0.5f);
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
            // Step 10c: per-board strength drift, attributed to a sender by nodegroup.py.
            serialOut().print(" strength=");
            serialOut().print(static_cast<unsigned long>(verdict.primaryStrength + 0.5f));
            serialOut().print(" duration_ms=");
            serialOut().print(verdict.primaryDurationMs);
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


