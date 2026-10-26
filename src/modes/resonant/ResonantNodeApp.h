#pragma once

#include <stdint.h>
#include <stddef.h>

#include "../../detection/DetectionRuntime.h"
#include "../../detection/DetectionProfile.h"
#include "../../behavior/BehaviorProfile.h"
#include "../../hal/AudioSourceI2S.h"
#if defined(BOARD_PIEZO)
#include "../../hal/PiezoToneOutputBTL.h"
#include "../../hal/PiezoToneOutput.h"
#else
#include "../../hal/I2sToneOutput.h"
#endif
#include "../../audio/AudioSignal.h"
#include "../../detection/features/FreqBandStream.h"
#include "../../output/ChirpOutput.h"
#include "../../app/SerialLine.h"
#include "../../output/SoundOutput.h"
#include "../../net/NodeNet.h"
#include "../../behavior/ResonantBehavior.h"
#include "../../detection/evaluation/OccurrenceVerdict.h"
#include <ParamRegistry.h>
#include "../../app/ParamIds.h"
#include "../../app/NvsParamStore.h"
#include "../../selftest/SelfTestReport.h"
#include "ResonantNodeDebug.h"

/*
Node

Owns orchestration for the Resonant node.
Coordinates hardware setup, occurrence flow, detection runtime, behavior, chirp output,
serial commands, logging, and summaries.

Does not implement behavior decisions, classify patterns, or generate waveforms.
*/

class Node {
public:
    using OccurrenceVerdict = detection::OccurrenceVerdict;

    enum class RbLogMode {
        Off,
        Full,
        Minimal,
    };

    // ledPin < 0: no status LED. chirpPin / chirpBtlPin drive the piezo
    // (BOARD_PIEZO only; the D-AMP chirp goes out over the I2S port).
    Node(int ledPin, int chirpPin, int chirpBtlPin);

    void begin();
    void update();

private:
    enum class RBBaselineState {
        Boot,
        ListenForQuiet,
        Settle,
        Active,
        FailedNoQuiet,
    };

    void configureParameters();
    void configureI2SParameters();
    void registerDetectionParams();
    void handleParamCommand(const char* line);
    void applyParamModule(param::ModuleId module);
    void loadStoredParams();
    // Remote params over the radio (PAR-013, step 11a).
    void handleNetCommand(const char* line);
    void pollNet();
    void runRemoteCommand(const NodeNet::Received& received);
    void printNetStatus();
    void applySilent();
    void startRbQuietBaseline();
    void resetRbCounters();
    void resetDetectionState();
    void performRbRebase();
    void quiesceBehaviorForOutputsOff();
    void updateRbBaselineState(unsigned long now);
    bool rbOutputsEnabled() const;
    const char* rbBaselineStateName() const;
    void pollSerialCommands();
    void handleSerialLine(const char* line);
    void handleDebugCommand(const char* line);
    void handleLogCommand(const char* line);
    void printRbStatus(const char* line);
    void handleProfileCommand(const char* line);
    bool rbShouldLogDetail() const;
    const char* rbLogModeName() const;
    bool setProfileFromName(const char* name);
    const char* profileName() const;
    const detection::DetectionProfile& activeDetectionProfile() const;
    const BehaviorGateConfig& activeBehaviorProfile() const;
    void applyActiveDetectionProfile();
    void applyActiveBehaviorGateConfig();
    void applyActiveProfiles();
    void processDetectionFrame(const AudioSamplePacket& audioSamplePacket, unsigned long now, bool selfChirpSuppressed, bool& sawPatternThisLoop);
    detection::FrequencyBandMeasurementPacket captureFrequencyMeasurementPacket(const AudioSamplePacket& audioSamplePacket) const;
    void printRbSummary() const;
    void printRbSignalSummary() const;
    void printRbBehaviorSummary() const;

    // SELFTEST (NODE-015, ResonantNodeSelfTest.cpp). The hardware checks
    // block; the runtime smoke checks run as phases of the normal loop and
    // only watch its public outputs (verdicts popped from DetectionRuntime,
    // chirp start/finish), never detector internals.
    enum class RbRebaseResult : uint8_t {
        Pending,
        Done,
        NoQuiet,
    };
    struct SelfTestState {
        enum class Phase : uint8_t {
            Idle,
            QuietBoot,
            Hardware,
            Settle,
            OwnChirp,
            OwnChirpTail,
            External,
            ExternalTail,
        };
        static constexpr uint8_t kWindows = 8;

        Phase phase = Phase::Idle;
        selftest::Command command;
        selftest::Tally tally;
        unsigned long phaseStartMs = 0;
        unsigned long deadlineMs = 0;
        uint8_t chirpsRequested = 0;
        uint8_t chirpsStarted = 0;
        uint8_t chirpsFinished = 0;
        bool waitingForChirp = false;
        bool selfTestChirpSounding = false;
        // Hardware results; checks that need a working mic / amp SKIP on a
        // failure instead of passing vacuously.
        selftest::Result micResult = selftest::Result::Skip;
        selftest::Result ampResult = selftest::Result::Skip;
        unsigned long nextChirpAtMs = 0;
        // Own chirps (any source) seen while a SELFTEST runs: start / finish
        // times (0 = still sounding), a small ring.
        unsigned long chirpStartMs[kWindows] = {};
        unsigned long chirpEndMs[kWindows] = {};
        uint8_t windowNext = 0;
        uint8_t windowCount = 0;
        unsigned long ownVerdicts = 0;
        unsigned long ownValid = 0;
        unsigned long otherVerdicts = 0;
        unsigned long otherValid = 0;
        unsigned long firstOtherValidMs = 0;
        unsigned long responses = 0;
    };

    void handleSelfTestCommand(const char* line);
    void updateSelfTest(unsigned long now);
    void runSelfTestHardware(unsigned long now);
    void restoreAfterSelfTestHardware();
    void printSelfTestQuietBoot();
    void startSelfTestChirps(SelfTestState::Phase phase, unsigned long now);
    void driveSelfTestChirps(unsigned long now);
    void finishSelfTestOwnChirp(bool timedOut);
    void finishSelfTestExternal(unsigned long now);
    void finishSelfTest();
    bool selfTestVerdictIsOwn(unsigned long occurrenceStartMs) const;
    void selfTestObserveVerdict(const OccurrenceVerdict& verdict, unsigned long now);
    void selfTestNoteChirpStarted(unsigned long now, const char* sourceName);
    void selfTestNoteChirpFinished(unsigned long now);
    void noteRebaseResult(RbRebaseResult result);

    // Network band and the node's hop signal (BEH-005 / OUT-005 / DET-013).
    void registerOutputParams();
    void applyBandParams();              // band params -> FreqBandStream, when they differ
    size_t bandBinCount() const;         // 250 Hz bins from band_low to band_high
    size_t hopFrequencies(uint32_t* out, size_t capacity) const;  // the train's bins, rotated per node
    bool composeHopTrain(OutputRequest& request) const;  // the node's train from output.* and the band; false = no bins
    void printHopShape(const OutputRequest& request);
    void printSignalConfig();
    void printHopGapCheck();  // RB hop_check: output.hop_* against detection.scalar_* (step 5h item 2)
    void printInspectionPlanCheck();  // RB plan_check: detection.scalar_* against the plan and history (step 8b item 2)
    void startEmission(unsigned long now, bool sawPatternThisLoop, bool selfChirpSuppressed);  // step 10 item 9
    void updateEmissions(unsigned long now);                                                     // step 10 item 9
    void applyLiveDetectionParams();  // PARAM SET detection.*: in place, no reset (step 10 item 2)
    void registerBehaviorParams();    // behavior.* on the registry (PAR-006, step 10 item 4)
    void loadInspectionLimitsFromProfile();  // detection.contrast_medium / amp_medium from the plan (item 7)

    // Hardware wiring.
    int _ledPin;
    AudioSourceI2S _i2sSource;
    AudioSource& _audioSource;
#if defined(BOARD_PIEZO)
    PiezoToneOutput _toneOutput;
    PiezoToneOutputBTL _toneOutputBTL;
#else
    I2sToneOutput _toneOutput;
#endif
    ChirpOutput _chirpOutput;
    // Executes the Node's emissions and keeps their status (OUT-002/003, step 10f).
    SoundOutput _soundOutput{_chirpOutput};
    NodeNet _net;
    bool _radioEnabled = true;          // node.radio_enabled, read at boot
    const char* _nodeName = "unknown";  // from the roster (app/NodeRoster.h)

    // Occurrence / detection / behavior pipeline.
    AudioSignal _audioSignal;
    FreqBandStream _freqBandStream;
    detection::DetectionRuntime _detection;
    detection::DetectionProfile _activeDetectionProfile = detection::makeTonalPulseScalarProfile();
    ResonantBehavior _behavior;
    param::ParamRegistry _paramRegistry;
    NvsParamStore _paramStore;

    // Network band the detector listens to (FreqBandStream bank; 0 = single
    // target at the chirp tone) and the node's network signal: a hop train
    // over the band's 250 Hz bins, started on bin `rotation` (from the MAC
    // at boot, PARAM SET output.hop_rotation overrides). Shape from the
    // 2026-10-16 battery (25 ms x 5, gap 10); band 2950-3950 so the 3,200 Hz
    // chirp sits on a bin (2950/3200/3450/3700/3950), owner 2026-10-16.
    // decisions/2026-10-16-hop-acceptance-contrast-gate-not-amp.md,
    // decisions/2026-10-16-network-band-2950-3950.md
    uint32_t _bandLowHz = runtime::kDefaultNetworkBandLowHz;
    uint32_t _bandHighHz = runtime::kDefaultNetworkBandHighHz;
    bool _hopEnabled = true;
    uint32_t _hopRotation = 0;
    uint32_t _hopPulseMs = runtime::kDefaultHopPulseMs;
    uint32_t _hopGapMs = runtime::kDefaultHopGapMs;
    uint32_t _hopSteps = runtime::kDefaultHopSteps;
    // Idle call (owner 2026-10-17): announcement tone below the band, a gap,
    // then the node's call (its hop train). 0 Hz = plain idle chirp pattern.
    uint32_t _idleAnnounceHz = 300;
    uint32_t _idleAnnounceMs = 500;
    // output.silent: the outputs-off gate held shut (rbOutputsEnabled()), so
    // detection runs and verdicts are counted, but Behavior never fires: no
    // answer, no idle call, no RB CALL. _silentApplied is what the loop last
    // acted on, so a change quiesces Behavior once.
    bool _silent = false;
    bool _silentApplied = false;
    static constexpr unsigned long kIdleAnnounceGapMs = 200;

    // For RB STATUS (2026-10-17): the newest audio sample's time on the ms
    // axis and how many samples the own-emit suppression has dropped.
    unsigned long _lastSampleTimeMs = 0;
    unsigned long _suppressedSampleCount = 0;
    unsigned long _rbLastRestsEntered = 0;  // logs "RB rest start" once per rest

    // Debug / logging support.
    NodeDebug _debug;
    // 256 like the Analyzer's USB line; a longer line is refused, never run
    // cut (issue #31; app/SerialLine.h). Also the command scratch size.
    static constexpr size_t kSerialLineCapacity = 256;
    serial_line::LineReader<kSerialLineCapacity - 1> _serialLine;
    unsigned long _rbPendingCount = 0;
    unsigned long _rbPatternAcceptedCount = 0;
    unsigned long _rbValidPatternCount = 0;
    unsigned long _rbChirpStartedCount = 0;
    int _rbLevelMaxSinceStatus = 0;  // loudest smoothed level since the last RB STATUS (room notes, 10c)
    unsigned long _rbOverflowPending = 0;
    uint64_t _rbStrengthSumScaled = 0; // Strength16 * 100 per verdict; uint32 overflowed after ~1,300 (issue #31)
    uint64_t _rbValidStrengthSum = 0;  // valid verdicts only, whole Strength16 units: RB STATUS heard.* (10c drift)
    unsigned long _rbDurationSumMs = 0;
    RbLogMode _rbLogMode = RbLogMode::Minimal;
    detection::DetectionProfileKind _profileKind = detection::DetectionProfileKind::TonalPulseScalar;
    BehaviorGateConfig _activeBehaviorProfile = {};  // the behavior.* params bind into it (step 10 item 4)
    float _contrastMediumThreshold = 0.0f;  // detection.contrast_medium (step 10 item 7)
    float _ampMediumThreshold = 0.0f;       // detection.amp_medium
    // When the last own chirp stopped; SELFTEST Settle waits out its echo.
    unsigned long _lastOwnChirpEndMs = 0;
    unsigned long _rbLastWouldEmitHeardMs = 0;
    ResonantBehavior::BehaviorDecision _rbLastWouldEmitDecision = ResonantBehavior::BehaviorDecision::None;

    // Baseline / startup state.
    RBBaselineState _rbBaselineState = RBBaselineState::Boot;
    unsigned long _rbBaselineStateStartedMs = 0;
    unsigned long _rbBaselineQuietSinceMs = 0;
    unsigned long _rbBaselineLastLogMs = 0;
    unsigned long _rbBaselineSettleUntilMs = 0;
    // Outcome of the quiet-gated rebase: the one at boot and the latest one
    // (a later "RB rebase" repeats it); "RB rebase force" is counted apart.
    RbRebaseResult _rbBootRebaseResult = RbRebaseResult::Pending;
    RbRebaseResult _rbLastRebaseResult = RbRebaseResult::Pending;
    unsigned long _rbLastRebaseAtMs = 0;
    unsigned long _rbQuietRebaseCount = 0;
    unsigned long _rbForcedRebaseCount = 0;
    float _rbLastRebaseSmooth = 0.0f;

    SelfTestState _selfTest;
};
