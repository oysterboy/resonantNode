#pragma once

#include <stdint.h>
#include <stddef.h>

#include "../../detection/DetectionRuntime.h"
#include "../../detection/DetectionProfile.h"
#include "../../detection/inspector/FrequencyEvidenceEvaluation.h"
#include "../../behavior/BehaviorProfile.h"
#include "../../hal/AudioSourceI2S.h"
#include "../../hal/PiezoToneOutputBTL.h"
#include "../../hal/PiezoToneOutput.h"
#include "../../io/AudioSignal.h"
#include "../../detection/detectors/AmpTransientDetector.h"
#include "../../detection/legacy/AmpCandidateBuilder.h"
#include "../../detection/features/FreqBandStream.h"
#include "../../io/ChirpOutput.h"
#include "../../behavior/ResonantBehavior.h"
#include "../../detection/patterns/PatternPayload.h"
#include "node_debug.h"

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
- enums and construction
- hardware / signal / behavior members
- logging and runtime counters
- baseline and startup state
- lifecycle / parameter setup
- loop orchestration
- serial command handling
- logging / summary helpers
*/

class Node {
public:
    using FrequencyEvidence = detection::FrequencyEvidence;
    using PatternResult = detection::PatternResult;

    enum class RbLogMode {
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
    void configureSharedParameters();
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
    const BehaviorProfile& activeBehaviorProfile() const;
    void applyActiveProfiles();
    void processLegacyAmpFrame(const AudioSignalFrame& frame, unsigned long now, bool selfChirpSuppressed, bool& sawPatternThisLoop);
    void drainLegacyAmpCandidates(unsigned long now, bool selfChirpSuppressed, bool& sawPatternThisLoop);
    void processModernFrame(const AudioSignalFrame& frame, unsigned long now, bool selfChirpSuppressed, bool& sawPatternThisLoop);
    FrequencyEvidence captureFrequencyEvidence() const;
    void logCandidate(const DetectorCandidate& candidate, const PatternResult& patternResult, const FrequencyEvidence* liveFrequencyEvidence, unsigned long candidateNumber, long gapMs, unsigned long queueDepthBeforeDrain, unsigned long behaviorLagMs, const char* candidateClass, const char* action, const char* stateName, const char* gateName);
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

    // Signal / detection / behavior pipeline.
    AmpTransientDetector _audioOnsetDetector;
    AudioSignal _audioSignal;
    // Legacy AMP path only. Modern detection flows through DetectionRuntime.
    AmpCandidateBuilder _ampCandidateBuilder;
    FreqBandStream _freqBandStream;
    detection::DetectionRuntime _detection;
    FrequencyEvidenceEvaluation::Values _frequencyEvidenceTuning = {};
    ResonantBehavior _behavior;

    // Debug / logging support.
    NodeDebug _debug;
    char _serialLineBuffer[96] = {};
    size_t _serialLineLength = 0;
    unsigned long _rbCandidateCount = 0;
    unsigned long _rbActionCount = 0;
    unsigned long _rbOverflowCandidates = 0;
    unsigned long _rbLastCandidateMs = 0;
    bool _rbHaveLastCandidateMs = false;
    unsigned long _rbStrengthSumScaled = 0;
    unsigned long _rbDurationSumMs = 0;
    unsigned long _rbLastLoggedOnsetRejectCount = 0;
    unsigned long _rbLastLoggedTransientRejectCount = 0;
    bool _rbDetectOnly = false;
    RbLogMode _rbLogMode = RbLogMode::Minimal;
    detection::DetectionProfileKind _profileKind = detection::DetectionProfileKind::FreqAmp;
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
