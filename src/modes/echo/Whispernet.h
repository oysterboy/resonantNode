#pragma once

#include <stddef.h>
#include <stdint.h>

#include <ParamRegistry.h>

#include "../../audio/BandBalance.h"
#include "../../audio/BandFlatness.h"
#include "../../audio/EchoCanceller.h"
#include "../../audio/GatedDelayLine.h"
#include "../../audio/RoomLevel.h"
#include "../../behavior/ResonantBehavior.h"
#include "../../behavior/WhisperGate.h"
#include "../../detection/EchoProfiles.h"
#include "../../detection/EnvelopeRuntime.h"
#include "../../hal/DuplexStream.h"

/*
Whispernet (ECHO-002, step 20.3): the echo shell's second mode (esp32dev-whisper, 24 kHz). A whisper heard is
whispered back by the room's nodes, each after its own delay D (400 ms .. 2 s, from the chip id;
decisions/2026-11-01-whispernet-delay-per-node.md).

Per sample the mic goes into the delay line (GatedDelayLine, 2 s) and what it plays into the FIFO the I2S writer
streams to the amp (silence while the gate is shut). Per block the envelope goes through EnvelopeRuntime with
Whispernet's plan (LevelWindow with the loud bound, Modulation) judged while the phrase runs (provisional verdicts,
300 ms then every 100 ms); WhisperGate turns them into the gate's openings and closings, and the first passing verdict
of a phrase goes to Behavior, whose answer decides whether that phrase is passed. The replay is Behavior's emission
from its opening until the line has played out its closing. Suppression is off: the node listens while it plays and
hears its own replay as another whisper (Behavior's level and rest end the chain; the loud bound stops a howl).

Owns: the delay line, the FIFO, the gate rule, Whispernet's gate values and counters, the emission's bookkeeping.
Does not: decide to answer (Behavior), judge a phrase (EnvelopeRuntime's inspector and makeVerdict), own the I2S port,
the radio or the console (EchoApp).
*/
class Whispernet {
public:
    struct Values {
        // Strength16 levels from the desk, 2026-11-01 (E5, the synthetic set through the lab PC's speaker): the quiet
        // room reads 40-100, a whisper (-35 dBFS file) 160-570 (level P90), speech (-20 / -25 dBFS) 1,100-4,400.
        // Live whispers the same day: 300-860 from 0.5-1 m, ~150-200 from 2 m; in the afternoon the room sat at
        // 150-260, so the thresholds stand at 2.5x the room: 2 m whispers are lost. Not the replay tool's scale (its
        // files map to the mic's full scale); WHISPER GATE sets them live.
        // With adaptive on (2026-11-04) these are the minimums and their ratios: the thresholds stand at roomFactor x the
        // room's level (RoomLevel), never under these; release and floor keep their ratio to the onset.
        // The detection chain's values and defaults (detection/EchoProfiles.h; the host replay starts from the same).
        detection::WhisperChainValues chain;
        // The spectrum's flatness a whisper lies within (BandFlatness, the median of the phrase's loud blocks): the
        // whispers on the desk read 0.16-0.49 (median ~0.3), speech 0.04-0.41 (median ~0.07), rustling and
        // typing 0.19-0.76 (median ~0.45) (2026-11-01, bench/sessions/2026-11-01-step20.3-whisper-desk). A first
        // range, under test; flatmin=0 flat=1 turn the check off in effect.
        // 2026-11-05 from the labelled recordings (BandBalance; research/whispernet.md), off by default until heard:
        // levelBand measures the level (onset, release, the room) on 1.5-4.5 kHz instead of the full band;
        // timbreCheck 1 makes the timbre module read the low band's excess over the room (whisper <= lowMaxDb) in
        // place of the flatness range.
        bool levelBand = false;
        uint32_t delayMs = 1000;  // D, set per node at begin()
        // The replay's level over the mic's: the amp plays the line's int16 at full scale, so gain 1 is the mic's digital
        // level, inaudible at a metre; 16 still was, 64 sounded about right by ear (desk, 2026-11-01).
        float gain = 64.0f;
        bool enabled = true;
        // Timing, set on the desk 2026-11-01.
        uint32_t preMs = 200;  // passed before the phrase's start: its soft beginning under the onset
        uint32_t holdMs = 300;  // the line kept open after the gate shuts: the whisper's tail
        uint32_t tailMs = 300;  // not listening this long after the own playback (the room's ring-down)
        uint32_t rampMs = 40;  // the gain's fade at each edge (10 sounded cut off)
        // Whispernet's Behavior (the echo room's values to start; found slow for whispers).
        uint32_t levelMax = 4;
        uint32_t levelOffset = 2;
        uint32_t levelDecayMs = 3000;
        uint32_t restMs = 4000;
        // The thresholds follow the room (RoomLevel): the onset at roomFactor x the room's quiet level, learned while no
        // phrase runs and the node does not play; quick down, slow up.
        bool adaptive = true;
        float roomFactor = 2.5f;
        uint32_t roomRiseMs = 10000;
        uint32_t roomFallMs = 500;
        // Echo cancellation (2026-11-04): the node's own output taken out of its mic (EchoCanceller) before anything
        // else hears it. With listenWhilePlaying the node stops going deaf during its replay once the cancellation
        // reaches listenErleDb.
        // On E5 with the lockstep output: ~10-11 dB of the own output taken out, the direct sound at tap 32
        // (2026-11-04). Off by default: while the node goes deaf during its replay it gains little; on with
        // listenWhilePlaying.
        bool aec = false;
        uint32_t aecTaps = 128;  // 5 ms at 24 kHz: the direct sound and the first reflections (192 dropped buffers)
        float aecMu = 0.2f;
        // The speaker's delay behind the mic, samples: the TX queue (DuplexStream::kQueueFrames, 512) and 554 more on
        // the way (mic and amp pipelines, the air): 1066 measured on E5 four times alike (WHISPER AEC CAL, 2026-11-04).
        // The canceller's auto-centring moves from here.
        uint32_t aecDelay = static_cast<uint32_t>(DuplexStream::kQueueFrames) + 554U;
        bool listenWhilePlaying = false;
        float listenErleDb = 15.0f;
    };
    struct Counts {
        unsigned long phrases = 0;  // final verdicts
        unsigned long whispers = 0;  // of which valid
        unsigned long checks = 0;  // provisional verdicts
        unsigned long asked = 0;  // phrases put to Behavior
        unsigned long answered = 0;
        unsigned long opens = 0;
        unsigned long gatedMs = 0;
        unsigned long replays = 0;  // Behavior emissions
        unsigned long deafs = 0;  // times detection paused for the own playback
        unsigned long deafMs = 0;  // and for how long in all
        unsigned long cuts = 0;  // phrases still running when the own playback began (cut there)
    };

    // Allocates the 2 s line at rateHz, D from the seed (the chip id). False when out of heap.
    bool begin(uint32_t rateHz, uint64_t seed);
    // The lockstep output (myspec 7.3): each block's output goes there, and it keeps what it played for the canceller.
    void attachDuplex(DuplexStream& duplex);
    // Before an OTA: the line freed (~96 KB), else the download stalls with 16 KB as the largest free block (E5 on the
    // desk, 2026-11-01). resume() after a failed update allocates it again.
    void release();
    bool resume();
    // Before release(): an emission that runs is closed (Behavior told it finished, the playback window and the
    // pending answer dropped). release() ends the line without shutLine(), so without this the finish test in
    // runBehavior() never passes and Behavior stays Chirping, every later verdict OutputBusy (review 2026-11-05 H1).
    void closeEmission(ResonantBehavior& behavior, unsigned long now);
    // Whispernet's detector config, plan and Behavior config into the shell's runtime and Behavior.
    void configure(detection::EnvelopeRuntime& runtime, ResonantBehavior& behavior);

    // One mic sample (24-bit) without the node's own echo (what it played, the delay back, from the DuplexStream).
    int32_t cancel(int32_t mic);
    // The sample the level follows: the whisper band's with level_band on, else as heard. Feeds the low band too.
    int32_t forLevel(int32_t s) {
        const float y = _balance.push(static_cast<float>(s));
        return _values.levelBand ? static_cast<int32_t>(y) : s;
    }
    // One mic sample (int16) in, the delay line's sample into this block's output.
    void pushSample(int16_t in) {
        int16_t out = _line.process(in);
        if (_calNoiseLeft > 0) {  // WHISPER AEC CAL: a burst of noise in place of the line
            --_calNoiseLeft;
            _calRng = _calRng * 1664525U + 1013904223U;
            out = static_cast<int16_t>(static_cast<int32_t>(_calRng >> 16) % 301 - 150);  // x gain 64: ~-14 dBFS
        }
        if (_outCount < kMaxBlock) {
            _out[_outCount++] = out;
        }
        _flat.push(static_cast<float>(in));
    }
    // The block's output, for DuplexStream::writeBlock after the block's samples.
    const int16_t* blockOutput() const { return _out; }
    size_t blockOutputCount() const { return _outCount; }
    // After a block's samples: its flatness into the runtime's history when the block is loud (above the onset), so
    // the phrase's timbre is read from its sound, not from the gaps between syllables.
    void endBlock(detection::EnvelopeRuntime& runtime, float level, uint32_t blockMs);
    // Before a block's samples: its first sample's line index and its time (ms -> line index for the gate).
    // ... and the block's capture time and RX index, which align the played samples to it (the echo cancellation).
    void beginBlock(uint32_t blockMs, uint64_t blockUs = 0, uint64_t rxIndex = 0, uint16_t samples = 0);
    // A judged occurrence from the runtime: provisional ones drive the gate, final ones end the phrase.
    void handleResult(const detection::EnvelopeRuntime::Result& result, ResonantBehavior& behavior, unsigned long now);
    // After a block's results: a phrase the detector dropped without a verdict ends here.
    void afterBlock(const detection::EnvelopeRuntime& runtime, uint32_t blockMs);
    // Behavior's emission: started when it asks, finished once the closed gate's last sample has played.
    void runBehavior(ResonantBehavior& behavior, unsigned long now);

    // whisper.* into the registry (ECHO-003): every field of Values.
    void describeParams(param::ParamRegistry& registry);
    // The values into the line, the gate, the stream and the runtime (after a PARAM or WHISPER GATE write).
    void applyValues(detection::EnvelopeRuntime& runtime, ResonantBehavior& behavior);
    // WHISPER GATE [onset=] [release=] [floor=] [loud=] [mod=] [flat=] [delay=MS] [gain=] [on=0|1]; prints them.
    void handleGateCommand(const char* line, detection::EnvelopeRuntime& runtime, ResonantBehavior& behavior);
    void printStatus();
    // WHISPER REC <n>|now: record the mic around the next n phrases that pass a check (300 ms before the phrase's
    // start to 1.2 s after the check), or the last 1.5 s now; printed as WHISPER_REC lines (base64 int16) by
    // dumpRecording(), which the shell calls outside the audio path. Diagnostic: the loop stalls ~8 s per dump.
    void handleRecCommand(const char* line);
    bool recordingDue(unsigned long now) const;
    void dumpRecording();
    // WHISPER AEC CAL: the board plays 0.4 s of noise through the lockstep output and, 0.25 s in, prints the mic (the
    // line, raw with whisper.aec 0) and what was written for the same mic indices as WHISPER_CAL lines: the PC finds
    // the speaker's delay behind the mic by cross-correlation (tools/bench/aeccal.py). Diagnostic; audible.
    void armCalibration() {
        _calArmed = true;
        _calNoiseLeft = _rateHz * 4U / 10U;
        _calStartMs = _blockMs;
    }
    // WHISPER AEC NOISE <ms>: the same noise, longer and without the dump (the canceller learns on it; WHISPER STATUS
    // then shows what it takes out). Audible.
    void playNoise(uint32_t ms) { _calNoiseLeft = _rateHz / 1000U * (ms > 20000U ? 20000U : ms); }
    bool calibrationDue(unsigned long now) const;
    void dumpCalibration();
    const Values& values() const { return _values; }
    bool ready() const { return _line.capacity() > 0; }
    // The node is playing its own replay (or its tail) at blockMs: the shell does not feed detection meanwhile and
    // restarts it after (observed 2026-11-01: at an audible gain the node answered itself).
    bool inputSuppressed(uint32_t blockMs);
    // Detection pauses for the own playback: the gate shuts now (the line holds no own output).
    void suppressNow();
    // The shell's deaf state per block (the counters deafs / deafMs).
    void noteDeaf(bool deaf, uint32_t blockMs) {
        if (deaf && !_deaf) {
            ++_counts.deafs;
            _deafSinceMs = blockMs;
        } else if (!deaf && _deaf) {
            _counts.deafMs += blockMs - _deafSinceMs;
        }
        _deaf = deaf;
    }
    size_t bytes() const { return _line.capacity() * sizeof(int16_t); }

private:
    void apply(const WhisperGate::Step& step);
    uint64_t lineIndexAtMs(unsigned long ms) const;

    uint32_t _rateHz = 24000;
    uint64_t _seed = 0;
    Values _values;
    Counts _counts;
    GatedDelayLine _line;
    BandFlatness _flat;
    BandBalance _balance;
    RoomLevel _lowRoom;  // the low band's quiet level, learned with the room's
    RoomLevel _room;
    EchoCanceller _aec;
    // The canceller's delay (samples) as the auto-centring has moved it, from the param's value.
    uint32_t _aecDelayNow = 0;
    uint32_t _aecDelaySet = 0;
    uint32_t _centreShifts = 0;
    uint32_t _nextCentreCheckMs = 0;
    uint64_t _rxCursor = 0;  // the mic index of the next sample
    static constexpr size_t kMaxBlock = 512;
    int16_t _out[kMaxBlock] = {};
    size_t _outCount = 0;
    uint64_t _rxEnd = 0;  // the last block's end (RX index) and its time, for the calibration
    uint64_t _rxEndUs = 0;
    bool _calArmed = false;
    bool _deaf = false;
    uint32_t _deafSinceMs = 0;
    uint32_t _calNoiseLeft = 0;
    uint32_t _calRng = 12345;
    uint32_t _calStartMs = 0;
    float _onsetApplied = 0.0f;
    uint32_t _lastRoomMs = 0;  // the onset the runtime holds now (configure / retune)
    uint32_t _lastLowMs = 0;
    float effectiveOnset() const;
    void retune(detection::EnvelopeRuntime& runtime);
    void buildConfig(detection::ScalarTransientConfig& d, detection::InspectionPlan& plan) const;
    WhisperGate _gate;
    DuplexStream* _duplex = nullptr;
    uint64_t _blockIndex = 0;
    uint32_t _blockMs = 0;
    unsigned long _openedAtMs = 0;  // the gate's last opening (from), for gatedMs
    bool _emitting = false;  // Behavior's emission runs
    bool _answerPending = false;  // a phrase was answered, Behavior's chirp not started yet
    unsigned long _playedOutAtMs = 0;  // the closed gate's last sample plays then
    // The line held open a little past the gate's closing (the whisper's tail), then shut.
    bool _holdPending = false;
    unsigned long _holdUntilMs = 0;
    // The own playback: from the first passed sample's turn (its time + D) to the last's plus a tail, the node does not
    // listen (it would hear its own replay as a new whisper, louder than the first at an audible gain).
    bool _playArmed = false;
    unsigned long _playFromMs = 0;
    unsigned long _playUntilMs = 0;
    void shutLine();
    // WHISPER REC
    unsigned long _recArmed = 0;
    bool _recPending = false;
    unsigned long _recDueMs = 0;
    uint64_t _recFrom = 0;
    unsigned long _recSeq = 0;
    char _recNote[96] = {};
};
