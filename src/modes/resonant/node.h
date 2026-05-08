#pragma once

#include <stdint.h>
#include <stddef.h>

#include "../../behavior/ResonantBehavior.h"
#include "../../hal/AudioSourceAnalog.h"
#include "../../hal/AudioSourceI2S.h"
#include "../../detection/DetectionPipeline.h"
#include "../../detection/FrequencyLoggingTuning.h"
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

- updates input, behavior, and output
- forwards action requests and lifecycle events
- owns debug output

Does NOT:
- implement state logic
- generate waveforms
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

    int _ledPin;

    AudioSourceAnalog _analogSource;
    AudioSourceI2S _i2sSource;
    AudioSource& _audioSource;
    AudioSourceKind _sourceKind;
    AudioSignal _audioSignal;
    AudioFrequencyDetector _audioFrequencyDetector;
    AudioOnsetDetector _audioOnsetDetector;
    FrequencyLoggingTuning::Values _frequencyLoggingTuning = {};
    ResonantBehavior _behavior;
    PiezoToneOutput _toneOutput;
    PiezoToneOutputBTL _toneOutputBTL;
    ChirpOutput _chirpOutput;

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
    bool _rbDetectOnly = false;
    RbLogMode _rbLogMode = RbLogMode::Minimal;
    bool _wasSelfChirpSuppressed = false;
    unsigned long _rbLastWouldEmitHeardMs = 0;
    ResonantBehavior::BehaviorDecision _rbLastWouldEmitDecision = ResonantBehavior::BehaviorDecision::None;
    RBBaselineState _rbBaselineState = RBBaselineState::Boot;
    unsigned long _rbBaselineStateStartedMs = 0;
    unsigned long _rbBaselineQuietSinceMs = 0;
    unsigned long _rbBaselineLastLogMs = 0;
    unsigned long _rbBaselineSettleUntilMs = 0;
};
