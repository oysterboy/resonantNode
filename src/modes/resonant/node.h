#pragma once

#include <stdint.h>
#include <stddef.h>

#include "../../detection/DetectionRuntime.h"
#include "../../detection/DetectionProfile.h"
#include "../../behavior/BehaviorProfile.h"
#include "../../hal/AudioSourceI2S.h"
#include "../../hal/PiezoToneOutputBTL.h"
#include "../../hal/PiezoToneOutput.h"
#include "../../io/AudioSignal.h"
#include "../../detection/features/FreqBandStream.h"
#include "../../io/ChirpOutput.h"
#include "../../behavior/ResonantBehavior.h"
#include "../../detection/patterns/PatternResult.h"
#include "node_debug.h"

/*
Node

Owns orchestration for the Resonant node.
Coordinates hardware setup, occurrence flow, detection runtime, behavior, chirp output,
serial commands, logging, and summaries.

Does not implement behavior decisions, classify patterns, or generate waveforms.
*/

class Node {
public:
    using PatternResult = detection::PatternResult;

    enum class RbLogMode {
        Off,
        Full,
        Minimal,
    };

    Node(int inputPin, int ledPin, int chirpPin, int chirpBtlPin);

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

    // Hardware wiring.
    int _ledPin;
    AudioSourceI2S _i2sSource;
    AudioSource& _audioSource;
    PiezoToneOutput _toneOutput;
    PiezoToneOutputBTL _toneOutputBTL;
    ChirpOutput _chirpOutput;

    // Occurrence / detection / behavior pipeline.
    AudioSignal _audioSignal;
    FreqBandStream _freqBandStream;
    detection::DetectionRuntime _detection;
    detection::DetectionProfile _activeDetectionProfile = detection::makeTonalPulseScalarProfile();
    ResonantBehavior _behavior;

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
};
