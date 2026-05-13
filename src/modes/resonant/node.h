#pragma once

#include <stdint.h>
#include <stddef.h>

#include "../../detection/DetectionPipeline.h"
#include "../../detection/FrequencyEvidenceEvaluation.h"
#include "../../hal/AudioSourceAnalog.h"
#include "../../hal/AudioSourceI2S.h"
#include "../../hal/PiezoToneOutputBTL.h"
#include "../../hal/PiezoToneOutput.h"
#include "../../io/AudioFrequencyDetector.h"
#include "../../io/AudioSignal.h"
#include "../../io/AudioOnsetDetector.h"
#include "../../io/ChirpOutput.h"
#include "../../behavior/ResonantBehavior.h"
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
    enum class AudioSourceKind {
        Analog,
        I2S
    };

    enum class RbLogMode {
        Full,
        Minimal,
    };

    Node(int inputPin,
         int ledPin,
         int chirpPin,
         AudioSourceKind sourceKind = AudioSourceKind::Analog);
    Node(int inputPin,
         int ledPin,
         int chirpPin,
         int chirpBtlPin,
         AudioSourceKind sourceKind = AudioSourceKind::Analog);

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
    void configureAnalogParameters();
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
    bool rbShouldLogDetail() const;
    const char* rbLogModeName() const;
    DetectionPipeline::FrequencyEvidence captureFrequencyEvidence() const;
    void logCandidate(const DetectorCandidate& candidate, const DetectionPipeline::PatternResult& patternResult, const DetectionPipeline::FrequencyEvidence* liveFrequencyEvidence, unsigned long candidateNumber, long gapMs, unsigned long queueDepthBeforeDrain, unsigned long behaviorLagMs, const char* candidateClass, const char* action, const char* stateName, const char* gateName);
    void printRbSummary() const;
    void printRbSignalSummary() const;
    void printRbDetectorSummary() const;
    void printRbBehaviorSummary() const;

    // Hardware wiring.
    int _ledPin;
    AudioSourceAnalog _analogSource;
    AudioSourceI2S _i2sSource;
    AudioSource& _audioSource;
    AudioSourceKind _sourceKind;
    PiezoToneOutput _toneOutput;
    PiezoToneOutputBTL _toneOutputBTL;
    ChirpOutput _chirpOutput;

    // Signal / detection / behavior pipeline.
    AudioSignal _audioSignal;
    AudioFrequencyDetector _audioFrequencyDetector;
    AudioOnsetDetector _audioOnsetDetector;
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
