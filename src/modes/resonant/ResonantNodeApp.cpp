#include "ResonantNodeApp.h"
#include "../../app/FirmwareOta.h"
#include <Arduino.h>

#include <esp_system.h>

#include "../../app/RuntimeDefaults.h"
#include "../../app/TimingUtils.h"
#include "../../app/BuildInfo.h"
#include "../../app/NodeRoster.h"
#include "../../detection/features/FrequencyMeasurementPacketBuilder.h"
#include "../../detection/inspection/InspectionNames.h"
#include "../../detection/evaluation/VerdictNames.h"
#include <ParamRegistry.h>
#include "../../app/ParamIds.h"
#include "../../app/TimeBase.h"
#include "../../app/SerialOut.h"
#include "../../selftest/AudioSelfTest.h"
#include "ResonantNodeInternal.h"

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
- own detector thresholds or inspection rules (DetectionProfile, live detection.* params)

File structure:
- local helpers and constants
- constructors
- lifecycle
- baseline / startup
- main update loop
- logging and summary helpers
Split by role (owner 2026-10-25): configuration in ResonantNodeParams.cpp, command dispatch
in ResonantNodeCommands.cpp, the radio transport in ResonantNodeNet.cpp, diagnostics in
ResonantNodeStatus.cpp + ResonantNodeSelfTest.cpp; shared helpers in ResonantNodeInternal.h.
*/

namespace {
constexpr int kMaxSamplesPerLoop = 512;


}


// RB PROFILE detail prints the stream in CamelCase (detection::featureStreamName is snake_case).
static const char* streamDisplayName(detection::FeatureStreamId stream) {
    switch (stream) {
        case detection::FeatureStreamId::AmpMagnitude:
            return "AmpMagnitude";
        case detection::FeatureStreamId::AmpEnvelope:
            return "AmpEnvelope";
        case detection::FeatureStreamId::FrequencyTarget:
            return "FrequencyTarget";
        case detection::FeatureStreamId::FrequencyContrast:
            return "FrequencyContrast";
        case detection::FeatureStreamId::Unknown:
        default:
            return "Unknown";
    }
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

static void printDetectionProfileDetails(const detection::DetectionProfile& profile) {
    serialOut().println("RB PROFILE detail");
    serialOut().print("  kind=");
    serialOut().println(detection::detectionProfileName(profile.kind));
    serialOut().print("  detectorSelection=");
    serialOut().println(detection::detectorSelectionName(profile.detectorSelection));
    serialOut().print("  inspectionPlan=");
    serialOut().println(detection::inspectionPlanName(profile.inspectionPlan));
    if (profile.detectorSelection == detection::DetectorSelection::ScalarTransient) {
        serialOut().print("  scalarTransient.observedStream=");
        serialOut().println(streamDisplayName(profile.scalarTransient.observedStream));
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
        serialOut().print(module.kind == detection::InspectionModuleKind::MagnitudeFeatureStrength
                              ? "MagnitudeFeatureStrength"
                              : module.kind == detection::InspectionModuleKind::BinShape ? "BinShape"
                                                                                         : "None");
        serialOut().print(" label=");
        serialOut().print(detection::inspectionTargetName(module.target));
        if (module.kind == detection::InspectionModuleKind::MagnitudeFeatureStrength) {
            serialOut().print(" stream=");
            serialOut().print(streamDisplayName(module.magnitude.stream));
            serialOut().print(" windowPreMs=");
            serialOut().print(module.magnitude.windowPreMs);
            serialOut().print(" windowPostMs=");
            serialOut().println(module.magnitude.windowPostMs);
        } else {
            serialOut().println();
        }
    }
    const detection::InspectionModuleConfig* supportRequirement =
        detection::firstEnabledRequirement(profile.inspectionPlan);
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
}

static void printBehaviorRuntimeDetails(const BehaviorStatus& behavior) {
    serialOut().println("RB behavior");
    serialOut().print("  idleEnabled=");
    serialOut().println(behavior.config.idleEnabled ? 1 : 0);
    serialOut().print("  waitAfterHeardMs=");
    serialOut().println(behavior.config.waitAfterHeardMs);
    serialOut().print("  refractoryAfterEmitMs=");
    serialOut().println(behavior.config.refractoryAfterEmitMs);
    serialOut().print("  behaviorSuppressSelfChirpMs=");
    serialOut().println(behavior.config.behaviorSuppressSelfChirpMs);
    serialOut().print("  detectionSuppressTailMsOwnEmit=");
    serialOut().println(behavior.config.detectionSuppressTailMsOwnEmit);
    serialOut().print("  idleTimeoutMs=");
    serialOut().println(behavior.config.idleTimeoutMs);
    serialOut().print("  idleTimeVariationMs=");
    serialOut().println(behavior.config.idleTimeVariationMs);
    serialOut().print("  idleBlockedAfterHeardMs=");
    serialOut().println(behavior.config.idleBlockedAfterHeardMs);
    serialOut().print("  idleBlockedAfterOwnEmitMs=");
    serialOut().println(behavior.config.idleBlockedAfterOwnEmitMs);
}

static void printNodeBuildIdentity() {
    char nodeIdBuffer[17];
    const uint64_t nodeId = ESP.getEfuseMac();
    snprintf(nodeIdBuffer, sizeof(nodeIdBuffer), "0x%012llX", static_cast<unsigned long long>(nodeId));

    printBuildIdentity(serialOut(), "node");
    serialOut().print("RB node_id=");
    serialOut().println(nodeIdBuffer);
}

// --- constructors ---

Node::Node(int ledPin, int chirpPin, int chirpBtlPin)
    : _ledPin(ledPin),
      _i2sSource(runtime::kDefaultAudioI2SSckPin, runtime::kDefaultAudioI2SWsPin, runtime::kDefaultAudioI2SDataPin,
                 static_cast<int>(runtime::kDefaultAudioI2SSampleRateHz),
                 static_cast<int>(runtime::kDefaultAudioI2SBitsPerSample), runtime::kDefaultAudioI2SDataOutPin),
      _audioSource(_i2sSource),
      _audioSignal(_audioSource),
      _freqBandStream(),
#if defined(BOARD_PIEZO)
      _toneOutput(chirpPin),
      _toneOutputBTL(chirpPin, chirpBtlPin),
      _chirpOutput(chirpBtlPin >= 0 ? static_cast<ToneOutput&>(_toneOutputBTL)
                                    : static_cast<ToneOutput&>(_toneOutput)) {
}
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
        const size_t bins = bandGrid().bins;
        const roster::Entry* me = roster::find(ESP.getEfuseMac());
        uint32_t& rotation = _composer.values().hopRotation;
        if (me != nullptr && bins > 0) {
            rotation = static_cast<uint32_t>(me->hopRotation % bins);
        } else {
            rotation = bins > 0 ? static_cast<uint32_t>(ESP.getEfuseMac() % bins) : 0U;
        }
        _nodeName = me != nullptr ? me->name : "unknown";
        serialOut().print("RB node name=");
        serialOut().print(_nodeName);
        serialOut().print(" rotation_source=");
        serialOut().println(me != nullptr ? "roster" : "mac");
    }
    // PAR-010 boot order: the fields hold the firmware defaults now (profile,
    // roster rotation), so capture them, then let valid stored values win
    // (decisions/2026-10-23-param-persistence-save-only.md) and apply once.
    _params.captureDefaults();
    loadStoredParams();
    if (_params.changedFromDefault("output.hop_rotation")) {
        serialOut().print("RB node rotation_source=stored rotation=");
        serialOut().println(_composer.values().hopRotation);
    }
    // The radio comes up after the stored values, so node.radio_enabled saved
    // as 0 keeps it off (PAR-013, step 11a).
    if (_switches.radioEnabled) {
        const bool up = _net.begin();
        serialOut().print("RB net started=");
        serialOut().print(up ? 1 : 0);
        serialOut().print(" channel=");
        serialOut().print(NET_CHANNEL);
        serialOut().print(" net_id=");
        serialOut().println(NET_ID);
    } else {
        serialOut().println("RB net started=0 reason=node.radio_enabled=0");
    }
    printSignalConfig();
    printHopGapCheck();
    printInspectionPlanCheck();
    _behavior.resetState();
    resetDetectionState();
    _audioSignal.resetStats();
    _audioSource.resetStats();
    _counters = NodeCounters{};
    // Boot: the mic warmup first (NodeBaseline::Config::micWarmupMs, the loop keeps running), then the floor is
    // rebased and the quiet gate starts (decisions/2026-10-26-node-boot-warmup-runs.md; it had been skipped since
    // begin(false)).
    _baseline.begin(timebase::nowMs());
    _rbLogMode = RbLogMode::Minimal;
    _rbLastWouldEmitHeardMs = 0;
    _rbLastWouldEmitDecision = ResonantBehavior::BehaviorDecision::None;
    _chirpOutput.begin();
    _led.begin(_ledPin);
    _behavior.seedIdleSchedule(timebase::nowMs());
    _silentApplied = _switches.silent;
    if (_switches.silent) {
        serialOut().println("RB silent=1 (output.silent: listening, never sounding)");
    }
    _serialLine.reset();
    _netGateway.begin();
    _selfTest.begin();

    serialOut().print("RB PROFILE name=");
    serialOut().print(profileName());
    printProfileComposition(activeDetectionProfile());
    serialOut().println();
    printDetectionProfileDetails(activeDetectionProfile());
    serialOut().println();
    printBehaviorRuntimeDetails(_behavior.status(timebase::nowMs()));
    serialOut().println();
    serialOut().print("RB startup baseline=");
    serialOut().println(_baseline.stateName());
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
        _selfTest.noteChirpFinished(timebase::nowMs());
    }
    _behavior.resetState();
}

void Node::startRbQuietBaseline() {
    quiesceBehaviorForOutputsOff();
    _baseline.startListening(timebase::nowMs());
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
    // quiesceBehaviorForOutputsOff already reset Behavior (review 12.0 L11: a second reset here).
    resetDetectionState();
    _audioSignal.resetStats();
    _audioSource.resetStats();
    _counters = NodeCounters{};
}

// --- baseline / startup ---

bool Node::rbOutputsEnabled() const {
    if (_switches.silent) {
        return false;
    }
    return _baseline.outputsAllowed();
}

// output.silent changed (PARAM SET, LOAD, RESET, or a stored value at boot):
// stop a sounding emission and drop Behavior's state either way, so a pattern
// heard or a call queued before the change does not fire after it, and the
// idle call starts its schedule fresh.
void Node::applySilent() {
    if (_switches.silent == _silentApplied) {
        return;
    }
    _silentApplied = _switches.silent;
    quiesceBehaviorForOutputsOff();
    _behavior.seedIdleSchedule(timebase::nowMs());
    serialOut().print("RB silent=");
    serialOut().println(_switches.silent ? 1 : 0);
}

// The machine (NodeBaseline) says what happened this tick; the Node does the side effects and prints the RB lines.
void Node::updateRbBaselineState(unsigned long now) {
    const NodeBaseline::Tick tick = _baseline.update(now, static_cast<float>(_audioSignal.smoothedSignalMagnitude()));
    switch (tick.step) {
        case NodeBaseline::Step::None:
            break;

        case NodeBaseline::Step::WarmupDone: {
            // The warmed-up mic's floor (what begin(true) did with a blocking delay), then the quiet gate.
            const bool rebased = _audioSignal.rebase();
            serialOut().print("RB warmup done ms=");
            serialOut().print(tick.elapsedMs);
            serialOut().print(" rebase=");
            serialOut().print(rebased ? "ok" : "timeout");
            serialOut().print(" baseline=");
            serialOut().println(_audioSignal.baseline(), 1);
            startRbQuietBaseline();
        } break;

        case NodeBaseline::Step::WarmupWaiting:
            serialOut().print("RB warmup waiting ms=");
            serialOut().print(tick.elapsedMs);
            serialOut().print(" of ");
            serialOut().println(_baseline.config().micWarmupMs);
            break;

        case NodeBaseline::Step::QuietHeld:
            performRbRebase();
            _baseline.noteRebaseResult(NodeBaseline::RebaseResult::Done, timebase::nowMs());
            serialOut().print("RB rebase done quietMs=");
            serialOut().print(tick.elapsedMs);
            serialOut().print(" baseline=");
            serialOut().print(_audioSignal.baseline(), 1);
            serialOut().print(" smooth=");
            serialOut().println(_audioSignal.smoothedSignalMagnitude());
            break;

        case NodeBaseline::Step::RebaseWaiting:
            serialOut().print("RB rebase waiting smooth=");
            serialOut().print(tick.smooth, 1);
            serialOut().print(" quietMs=");
            serialOut().println(tick.elapsedMs);
            break;

        case NodeBaseline::Step::NoQuiet:
            _baseline.noteRebaseResult(NodeBaseline::RebaseResult::NoQuiet, timebase::nowMs());
            serialOut().print("RB rebase skipped reason=no_quiet smooth=");
            serialOut().println(tick.smooth, 1);
            break;

        case NodeBaseline::Step::Activated:
            serialOut().println("RB baseline active");
            break;
    }
}

// --- SELFTEST glue (NodeSelfTest runs the phases; step 11i N4) ---

void Node::selfTestRequestChirp(void* ctx) {
    static_cast<Node*>(ctx)->_behavior.requestTestChirp();
}

void Node::selfTestCancelChirp(void* ctx) {
    static_cast<Node*>(ctx)->_behavior.cancelTestChirp();
}

void Node::updateSelfTest(unsigned long now) {
    NodeSelfTest::Inputs in;
    in.outputsEnabled = rbOutputsEnabled();
    in.silent = _switches.silent;
    in.chirpActive = _chirpOutput.isActive();
    in.lastOwnChirpEndMs = _lastOwnChirpEndMs;
    in.toneHz = _chirpOutput.toneHz();
    in.suppressTailMs = _behavior.config().detectionSuppressTailMsOwnEmit;
    if (_selfTest.update(now, in) == NodeSelfTest::Request::RunHardware) {
        runSelfTestHardware(now);
    }
}

// The blocking checks on the Node's audio source and tone output; NodeSelfTest
// prints quiet_boot from the baseline machine's results and counts the tally.
void Node::runSelfTestHardware(unsigned long now) {
    // Nothing of ours may sound while the mic floor is measured.
    if (_soundOutput.busy() || _chirpOutput.isActive()) {
        _soundOutput.stop();
        _behavior.notifyChirpFinished(now);
        _selfTest.noteChirpFinished(now);
    }

    selftest::Tally& tally = _selfTest.tally();
    const selftest::Result mic = selftest::runMicCheck(serialOut(), tally, _i2sSource);
    selftest::Result amp = selftest::Result::Skip;
    if (mic != selftest::Result::Pass) {
        selftest::skip(serialOut(), tally, "quiet_boot", "mic_failed");
        selftest::skip(serialOut(), tally, "amp", "mic_failed");
    } else {
        _selfTest.printQuietBoot(_audioSignal.baseline());
#if defined(BOARD_PIEZO)
        const char* path = "piezo";
#else
        const char* path = "i2s";
#endif
        amp = selftest::runAmpCheck(serialOut(), tally, _i2sSource, _chirpOutput.toneOutput(), _chirpOutput.toneHz(),
                                    path);
    }
    restoreAfterSelfTestHardware();
    _selfTest.hardwareDone(mic, amp, timebase::nowMs());
}

void Node::restoreAfterSelfTestHardware() {
    // The tone is off; put the output device back on the chirp frequency.
    _soundOutput.stop();
    // Several seconds of blocking checks: a HeardPattern from before them
    // must not fire after Settle (issue #31 R2-3). Config is kept.
    _behavior.resetState();
    _chirpOutput.setToneHz(_chirpOutput.toneHz());

    // The checks read several seconds of audio that never reached the
    // detection chain: restart it on fresh audio (same reset as a rebase,
    // without touching the AudioSignal baseline or the Behavior config), plus
    // a fresh band stream on the chirp tone the checks may have moved.
    resetDetectionState();
    _freqBandStream.resetState();
    _freqBandStream.setSampleRateHz(_audioSource.sampleRateHz());
    _freqBandStream.setTargetFrequencyHz(_chirpOutput.toneHz());

    _baseline.resettle(timebase::nowMs() + NodeSelfTest::kSettleAfterHardwareMs);
    serialOut().print("SELFTEST_RESTORE detection=reset baseline=");
    serialOut().println(_baseline.stateName());
}

// --- band and hop composition ---

void Node::applyBandParams() {
    _band.applyTo(_freqBandStream);  // an empty or inverted band means "single target at the chirp tone"
}

// The hop train's grid is the detector's (NodeSignalComposer::gridOf reads FreqBandStream as applied).
// Call after applyBandParams().
NodeSignalComposer::BandGrid Node::bandGrid() const {
    return NodeSignalComposer::gridOf(_freqBandStream);
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

    // Boot wiring: the chirp tone and the detector's single target start aligned;
    // applyBandParams() replaces the target with the band when detection.band_* is set.
    _chirpOutput.setToneHz(runtime::kDefaultChirpFrequencyHz);
    _freqBandStream.setTargetFrequencyHz(_chirpOutput.toneHz());
    _audioSignal.setBaselineTrackingQuietThreshold(20);
}

// --- main update loop ---

void Node::update() {
    const unsigned long now = timebase::nowMs();
    ota::confirmRunningImage(serialOut());
    const bool selfChirpSuppressed = _behavior.behaviorSuppressed(now);

    pollSerialCommands();
    pollNet();
    // The outputs-off gate (output.silent, the baseline machine) as Behavior's block reason (step 11i N6): set here
    // for the verdicts of this tick's samples, again below after the baseline step.
    _behavior.setOutputsEnabled(rbOutputsEnabled());

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

        const uint32_t sampleRateHz =
            _audioSource.sampleRateHz() > 0 ? _audioSource.sampleRateHz() : runtime::kDefaultAudioI2SSampleRateHz;
        for (uint16_t i = 0; i < block.sampleCount; ++i) {
            const uint64_t sampleTimeUs64 =
                block.approxStartUs64 + timebase::sampleOffsetUs(static_cast<uint32_t>(i), sampleRateHz);
            AudioSamplePacket audioSamplePacket;
            _audioSignal.update(static_cast<int>(block.samples[i]), sampleTimeUs64, audioSamplePacket,
                                block.overflowBeforeBlock);
            const bool ownEmitSuppressed = _behavior.ownEmitDetectionSuppressed(audioSamplePacket.timeMs);
            _lastSampleTimeMs = audioSamplePacket.timeMs;
            if (ownEmitSuppressed) {
                ++_suppressedSampleCount;
            }
            if (!ownEmitSuppressed) {
                _freqBandStream.observeCenteredSample(audioSamplePacket.baselineCorrectedValue,
                                                      audioSamplePacket.timeMs);
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
    _behavior.setOutputsEnabled(rbOutputsEnabled());  // outputs off: update() and shouldStartChirp() do nothing
    applyBandParams();  // PARAM SET detection.band_* takes effect here (two compares when unchanged)

    _behavior.update(now);
    if (_behavior.restsEntered() != _rbLastRestsEntered) {
        // A Behavior reset puts the count back to 0: resync without a line (review 12.0 L7).
        const bool newRest = _behavior.restsEntered() > _rbLastRestsEntered;
        _rbLastRestsEntered = _behavior.restsEntered();
        if (newRest && rbShouldLogDetail()) {
            const BehaviorStatus b = _behavior.status(now);
            serialOut().print("RB rest start reason=");
            serialOut().print(b.lastRestReasonName);
            serialOut().print(" now=");
            serialOut().print(now);
            serialOut().print(" until=");
            serialOut().print(b.restUntilMs);
            serialOut().print(" field_patterns=");
            serialOut().println(_detection.fieldState().recentPatternCount);
        }
    }
    const bool behaviorWouldEmit = _behavior.takeWouldEmit();
    const bool shouldLogWouldEmit = behaviorWouldEmit && rbShouldLogDetail() &&
        (_behavior.lastHeardMs() != _rbLastWouldEmitHeardMs || _behavior.lastDecision() != _rbLastWouldEmitDecision);
    if (shouldLogWouldEmit) {
        const BehaviorStatus b = _behavior.status(now);
        serialOut().print("BEH verdict=");
        serialOut().print(b.lastVerdictTypeName);
        serialOut().print(" heard=");
        serialOut().print(b.lastHeardMs);
        serialOut().print(" now=");
        serialOut().print(now);
        serialOut().print(" decision=");
        serialOut().print(b.lastDecisionName);
        serialOut().print(" block=");
        serialOut().print(b.lastBlockReasonName);
        serialOut().print(" waitMs=");
        serialOut().print(b.waitRemainingMs);
        serialOut().print(" refractoryMs=");
        serialOut().print(b.refractoryRemainingMs);
        serialOut().print(" behaviorSuppressMs=");
        serialOut().print(b.behaviorSuppressRemainingMs);
        serialOut().print(" outputBusy=");
        serialOut().print(b.outputBusy ? 1 : 0);
        serialOut().print(" wouldEmit=");
        serialOut().println(1);
        _rbLastWouldEmitHeardMs = _behavior.lastHeardMs();
        _rbLastWouldEmitDecision = _behavior.lastDecision();
    }

    if (sawI2SSample) {
        const int level = _audioSignal.smoothedSignalMagnitude();
        if (level > _rbLevelMaxSinceStatus) {
            _rbLevelMaxSinceStatus = level;
        }
    }

    if (_behavior.shouldStartChirp()) {
        startEmission(now);
    }
    updateEmissions(now);

    _led.update(now);

    if (_selfTest.active()) {
        updateSelfTest(timebase::nowMs());
    }
}

// Behavior asked for a chirp: start the emission it asked for (step 10 item 9:
// out of update(), unchanged). A heard pattern is answered with the network
// signal (the hop train); the idle call is an announcement tone, then the call.
void Node::startEmission(unsigned long now) {
    const auto chirpPattern = _behavior.chirpPattern();
    const char* sourceName = _behavior.chirpRequestSourceName();
    if (rbShouldLogDetail()) {
        const BehaviorStatus b = _behavior.status(now);
        serialOut().print("BEH emit start now=");
        serialOut().print(now);
        serialOut().print(" source=");
        serialOut().print(sourceName);
        serialOut().print(" state=");
        serialOut().print(b.stateName);
        serialOut().print(" pattern=");
        serialOut().println(chirpPatternName(chirpPattern));
        serialOut().print("RB emit accepted source=");
        serialOut().print(sourceName);
        serialOut().print(" state=");
        serialOut().print(b.stateName);
        serialOut().print(" pattern=");
        serialOut().println(chirpPatternName(chirpPattern));
    }
    _led.flash(now, 2);
    // The request from Behavior's ask, the band grid and output.* (NodeSignalComposer, step 11i N5): a heard
    // pattern is answered with the hop train, the idle call is announcement, gap, then the call. SoundOutput plays
    // it (step 10f); own-emit suppression covers its span.
    OutputRequest request = _composer.compose(bandGrid(), chirpPattern,
                                              _behavior.signalShape() == ResonantBehavior::SignalShape::NetworkSignal);
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
    if (!started) {
        // Nothing sounds: end the emission at once so Behavior does not wait in Chirping, and book nothing (no own-emit
        // budget, no started count, no self-test window that would never close; review D1.2, 2026-10-26).
        serialOut().print("RB emit rejected reason=");
        serialOut().print(outputRejectName(_soundOutput.status().lastReject));
        serialOut().println(" fallback=none");
        const unsigned long finishedMs = timebase::nowMs();
        _lastOwnChirpEndMs = finishedMs;
        _behavior.notifyChirpFinished(finishedMs);
        return;
    }
    _behavior.notifyChirpStarted(now, emitSpanMs);
    ++_counters.chirpStarted;
    _selfTest.noteChirpStarted(now, sourceName);
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
        _selfTest.noteChirpFinished(finishedMs);  // review 12.0 L9: was the loop-start now
    }
}

// --- detection frame -> Behavior -> SoundOutput ---

void Node::processDetectionFrame(const AudioSamplePacket& audioSamplePacket, unsigned long now,
                                 bool selfChirpSuppressed, bool& sawPatternThisLoop) {
    const auto liveFrequencyMeasurementPacket = captureFrequencyMeasurementPacket(audioSamplePacket);
    _detection.observeFrame(audioSamplePacket, liveFrequencyMeasurementPacket, audioSamplePacket.timeMs);

    detection::OccurrenceVerdict verdict;
    while (_detection.popOccurrenceVerdict(verdict)) {
        // Outputs off (Settle, ListenForQuiet, silent): Behavior notes the
        // pattern as heard and decides nothing (setOutputsEnabled, step 11i N6).
        const bool behaviorSeesVerdict = _behavior.outputsEnabled();
        const auto behaviorDecision = _behavior.handleOccurrenceVerdict(verdict, _detection.fieldState(), now);
        if (behaviorDecision == ResonantBehavior::BehaviorDecision::ConsumedPattern) {
            sawPatternThisLoop = true;
        }

        _counters.noteVerdict(verdict);
        if (verdict.accepted) {
            _led.flash(now, 1);
        }
        if (_selfTest.active()) {
            _selfTest.observeVerdict(verdict, now, _behavior.config().detectionSuppressTailMsOwnEmit);
        }

        if (rbShouldLogDetail()) {
            const BehaviorStatus b = _behavior.status(now);
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
            serialOut().print(behaviorSeesVerdict && b.behaviorEligible ? 1 : 0);
            serialOut().print(" decision=");
            serialOut().print(behaviorSeesVerdict ? b.lastDecisionName : "outputs_off");
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

detection::FrequencyBandMeasurementPacket Node::captureFrequencyMeasurementPacket(
    const AudioSamplePacket& audioSamplePacket) const {
    return detection::buildFrequencyMeasurementPacket(_freqBandStream, audioSamplePacket);
}
