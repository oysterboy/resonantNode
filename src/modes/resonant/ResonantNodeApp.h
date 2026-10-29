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
#include "ResonantNodeLed.h"
#include "ResonantNodeCounters.h"
#include "ResonantNodeBaseline.h"
#include "ResonantNodeNetGateway.h"
#include "ResonantNodeSelfTest.h"
#include "ResonantNodeSignalComposer.h"
#include "ResonantNodeParams.h"

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
    void configureParameters();
    void configureI2SParameters();
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
    void resetDetectionState();
    void performRbRebase();
    void quiesceBehaviorForOutputsOff();
    void updateRbBaselineState(unsigned long now);
    bool rbOutputsEnabled() const;
    void pollSerialCommands();
    // The text dispatch (step 11i N8, ResonantNodeCommands.cpp): kCommands, verb -> handler, first match wins.
    void handleSerialLine(const char* line);
    struct Command {
        enum class Match : uint8_t {
            Word,   // the verb, then the end or a space (text_parse::startsWithWordIgnoreCase)
            Exact,  // the whole line, any case
        };
        const char* verb;
        Match match;
        void (Node::*handler)(const char* line);
    };
    static const Command kCommands[];
    void handleOtaCommand(const char* line);
    void printHelp(const char* line);
    void handleSelfTestCommand(const char* line);
    void refuseRbParam(const char* line);
    void refuseRbBehav(const char* line);
    void handleRebaseForceCommand(const char* line);
    void handleRebaseCommand(const char* line);
    void handleCallCommand(const char* line);
    void handleSummaryCommand(const char* line);
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
    void processDetectionFrame(const AudioSamplePacket& audioSamplePacket, unsigned long now, bool selfChirpSuppressed,
                               bool& sawPatternThisLoop);
    detection::FrequencyBandMeasurementPacket captureFrequencyMeasurementPacket(
        const AudioSamplePacket& audioSamplePacket) const;
    void printRbSummary() const;
    void printRbSignalSummary() const;
    void printRbBehaviorSummary() const;

    // SELFTEST (NODE-015): NodeSelfTest (ResonantNodeSelfTest.h/.cpp) runs the
    // command and its phases; the Node hands it verdicts, chirp start / finish
    // and per-tick inputs, and runs the blocking hardware checks when asked
    // (they need the audio source and the tone output).
    void updateSelfTest(unsigned long now);
    void runSelfTestHardware(unsigned long now);
    void restoreAfterSelfTestHardware();
    static void selfTestRequestChirp(void* ctx);
    static void selfTestCancelChirp(void* ctx);

    // Network band and the node's hop signal (BEH-005 / OUT-005 / DET-013).
    void applyBandParams();              // band params -> FreqBandStream, when they differ
    NodeSignalComposer::BandGrid bandGrid() const;  // FreqBandStream's applied grid (call after applyBandParams())
    void printHopShape(const OutputRequest& request);
    void printSignalConfig();
    void printHopGapCheck();  // RB hop_check: output.hop_* against detection.scalar_* (step 5h item 2)
    void printInspectionPlanCheck();  // RB plan_check: detection.scalar_* against the plan and history (step 8b item 2)
    // Behavior's chirp request into a SoundOutput request; a request that does not start ends the emission at once.
    void startEmission(unsigned long now);
    void updateEmissions(unsigned long now);  // the output tick; the emission's end goes to Behavior and SELFTEST
    void applyLiveDetectionParams();  // PARAM SET detection.*: in place, no reset (step 10 item 2)
    void loadInspectionLimitsFromProfile();  // detection.contrast_medium / amp_medium / shape_* from the plan

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
    NetGateway _netGateway{_net};  // reply tee and reply reassembly (step 11i N3)
    const char* _nodeName = "unknown";  // from the roster (app/NodeRoster.h)

    // Occurrence / detection / behavior pipeline.
    AudioSignal _audioSignal;
    FreqBandStream _freqBandStream;
    detection::DetectionRuntime _detection;
    detection::DetectionProfile _activeDetectionProfile = detection::makeTonalPulseScalarProfile();
    ResonantBehavior _behavior;
    NodeParams _params;  // registry + NVS store; each owner describes its params (step 11i N7)

    // Network band the detector listens to (detection.band_*, applied to the
    // FreqBandStream bank; 0 = single target at the chirp tone). Band
    // 2950-3950 so the 3,200 Hz chirp sits on a bin (2950/3200/3450/3700/3950),
    // owner 2026-10-16. decisions/2026-10-16-network-band-2950-3950.md
    NetworkBand _band;  // detection.band_* (step 11i N7: NetworkBand::applyTo)
    // The node's network signal and idle call (output.hop_*, output.idle_announce_*) on the band's grid
    // -> OutputRequest (step 11i N5).
    NodeSignalComposer _composer;
    // output.silent and node.radio_enabled. silent holds the outputs-off gate
    // shut (rbOutputsEnabled()), so detection runs and verdicts are counted,
    // but Behavior never fires: no answer, no idle call, no RB CALL.
    // _silentApplied is what the loop last acted on, so a change quiesces
    // Behavior once. radioEnabled is read at boot.
    NodeSwitches _switches;
    bool _silentApplied = false;

    // For RB STATUS (2026-10-17): the newest audio sample's time on the ms
    // axis and how many samples the own-emit suppression has dropped.
    unsigned long _lastSampleTimeMs = 0;
    unsigned long _suppressedSampleCount = 0;
    unsigned long _rbLastRestsEntered = 0;  // logs "RB rest start" once per rest

    // Debug / logging support.
    NodeLed _led;
    // 256 like the Analyzer's USB line; a longer line is refused, never run
    // cut (issue #31; app/SerialLine.h). Also the command scratch size.
    serial_line::LineReader<serial_line::kCommandLineMax> _serialLine;
    NodeCounters _counters;  // RB tallies since boot or rebase (step 11i N1)
    int _rbLevelMaxSinceStatus = 0;  // loudest smoothed level since the last RB STATUS (room notes, 10c)
    RbLogMode _rbLogMode = RbLogMode::Minimal;
    detection::DetectionProfileKind _profileKind = detection::DetectionProfileKind::TonalPulseScalar;
    BehaviorGateConfig _activeBehaviorProfile = {};  // the behavior.* params bind into it (step 10 item 4)
    // detection.contrast_medium / amp_medium / shape_* (step 10 item 7, step 11g B2): the active plan's limits.
    detection::InspectionLimits _inspectionLimits = {};
    // When the last own chirp stopped; SELFTEST Settle waits out its echo.
    unsigned long _lastOwnChirpEndMs = 0;
    unsigned long _rbLastWouldEmitHeardMs = 0;
    ResonantBehavior::BehaviorDecision _rbLastWouldEmitDecision = ResonantBehavior::BehaviorDecision::None;

    // Baseline / startup state.
    // The quiet-gated rebase machine, its timers and results (step 11i N2).
    NodeBaseline _baseline;

    // SELFTEST and its phases (step 11i N4); asks Behavior for test chirps through the callbacks.
    NodeSelfTest _selfTest{_baseline,
                           NodeSelfTest::Chirps{this, &Node::selfTestRequestChirp, &Node::selfTestCancelChirp}};
};
