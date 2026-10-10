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
#include "../../behavior/ResonantBehavior.h"
#include "../../detection/evaluation/OccurrenceVerdict.h"
#include "../../param/ParamRegistry.h"
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
        Rebase,
        Settle,
        Active,
        FailedNoQuiet,
    };

    void configureParameters();
    void configureI2SParameters();
    void registerDetectionParams();
    void handleParamCommand(const char* line);
    void applyParamModule(param::ModuleId module);
    void startRbQuietBaseline();
    void resetRbCounters();
    void resetDetectionState();
    void performRbRebase();
    void updateRbBaselineState(unsigned long now);
    bool rbOutputsEnabled() const;
    const char* rbBaselineStateName() const;
    void pollSerialCommands();
    void handleSerialLine(const char* line);
    void handleDebugCommand(const char* line);
    void handleLogCommand(const char* line);
    void handleDetectCommand(const char* line);
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
    void printRbDetectorSummary() const;
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
    bool selfTestVerdictIsOwn(unsigned long heardMs) const;
    void selfTestObserveVerdict(const OccurrenceVerdict& verdict, unsigned long now);
    void selfTestNoteChirpStarted(unsigned long now, const char* sourceName);
    void selfTestNoteChirpFinished(unsigned long now);
    void noteRebaseResult(RbRebaseResult result);

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

    // Occurrence / detection / behavior pipeline.
    AudioSignal _audioSignal;
    FreqBandStream _freqBandStream;
    detection::DetectionRuntime _detection;
    detection::DetectionProfile _activeDetectionProfile = detection::makeTonalPulseScalarProfile();
    ResonantBehavior _behavior;
    param::ParamRegistry _paramRegistry;

    // Debug / logging support.
    NodeDebug _debug;
    char _serialLineBuffer[96] = {};
    size_t _serialLineLength = 0;
    unsigned long _rbPendingCount = 0;
    unsigned long _rbPatternAcceptedCount = 0;
    unsigned long _rbValidPatternCount = 0;
    unsigned long _rbChirpStartedCount = 0;
    unsigned long _rbOverflowPending = 0;
    unsigned long _rbLastPendingMs = 0;
    bool _rbHaveLastPendingMs = false;
    unsigned long _rbStrengthSumScaled = 0;
    unsigned long _rbDurationSumMs = 0;
    unsigned long _rbLastLoggedOnsetRejectCount = 0;
    unsigned long _rbLastLoggedTransientRejectCount = 0;
    RbLogMode _rbLogMode = RbLogMode::Minimal;
    detection::DetectionProfileKind _profileKind = detection::DetectionProfileKind::TonalPulseScalar;
    bool _wasSelfChirpSuppressed = false;
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
