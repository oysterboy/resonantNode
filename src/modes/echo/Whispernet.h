#pragma once

#include <stddef.h>
#include <stdint.h>

#include <ParamRegistry.h>

#include "../../audio/BandFlatness.h"
#include "../../audio/GatedDelayLine.h"
#include "../../audio/SampleFifo.h"
#include "../../behavior/ResonantBehavior.h"
#include "../../behavior/WhisperGate.h"
#include "../../detection/EnvelopeRuntime.h"
#include "../../hal/I2sToneOutput.h"

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
        // The owner's whispers: 300-860 from 0.5-1 m, ~150-200 from 2 m; in the afternoon the room sat at 150-260, so
        // the thresholds went up (2.5x the room): 2 m whispers are lost. Not the replay tool's scale (its files map to
        // the mic's full scale); WHISPER GATE sets them live.
        float onset = 400.0f;  // the detector's onset: a phrase starts
        float release = 280.0f;  // and its release
        float floor = 300.0f;  // the level window's low bound
        float loud = 900.0f;  // the loud bound: above it never a whisper (speech, a clap, the own loop growing)
        float modulation = 0.3f;  // the modulation index a whisper reaches at least (Medium)
        // The spectrum's flatness a whisper lies within (BandFlatness, the median of the phrase's loud blocks): the
        // owner's whispers on the desk read 0.16-0.49 (median ~0.3), speech 0.04-0.41 (median ~0.07), rustling and
        // typing 0.19-0.76 (median ~0.45) (2026-11-01, bench/sessions/2026-11-01-step20.3-whisper-desk). A first
        // range, under test; flatmin=0 flat=1 turn the check off in effect.
        float flatMin = 0.15f;
        float flatMax = 0.40f;
        uint32_t delayMs = 1000;  // D, set per node at begin()
        // The replay's level over the mic's: the amp plays the line's int16 at full scale, so gain 1 is the mic's digital
        // level, inaudible at a metre; 16 still was, 64 sounded about right to the owner (desk, 2026-11-01).
        float gain = 64.0f;
        bool enabled = true;
        // Timing, set on the desk 2026-11-01 (params since 2026-11-04).
        uint32_t preMs = 200;  // passed before the phrase's start: its soft beginning under the onset
        uint32_t holdMs = 300;  // the line kept open after the gate shuts: the whisper's tail
        uint32_t tailMs = 300;  // not listening this long after the own playback (the room's ring-down)
        uint32_t rampMs = 40;  // the gain's fade at each edge (10 sounded cut off)
        uint32_t firstCheckMs = 300;  // a running phrase is first judged this far in
        uint32_t checkEveryMs = 100;  // and again this often
        uint32_t minPhraseMs = 300;  // shorter is no phrase
        uint32_t gapMs = 300;  // syllable gaps up to this are bridged (the detector's release debounce)
        // Whispernet's Behavior (the echo room's values to start; the owner found them slow for whispers).
        uint32_t levelMax = 4;
        uint32_t levelOffset = 2;
        uint32_t levelDecayMs = 3000;
        uint32_t restMs = 4000;
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
    };

    // Allocates the 2 s line and a 100 ms FIFO at rateHz, D from the seed (the chip id). False when out of heap.
    bool begin(uint32_t rateHz, uint64_t seed);
    // The writer streams the FIFO from here on.
    void attachOutput(I2sToneOutput& output);
    // Before an OTA: the stream stopped and the line and FIFO freed (~100 KB), else the download stalls with 16 KB as
    // the largest free block (E5 on the desk, 2026-11-01). resume() after a failed update allocates them again.
    void release();
    bool resume();
    // Whispernet's detector config, plan and Behavior config into the shell's runtime and Behavior.
    void configure(detection::EnvelopeRuntime& runtime, ResonantBehavior& behavior);

    // One mic sample (int16) in, the delay line's sample out to the FIFO.
    void pushSample(int16_t in) {
        _fifo.push(_line.process(in));
        _flat.push(static_cast<float>(in));
    }
    // After a block's samples: its flatness into the runtime's history when the block is loud (above the onset), so
    // the phrase's timbre is read from its sound, not from the gaps between syllables.
    void endBlock(detection::EnvelopeRuntime& runtime, float level, uint32_t blockMs);
    // Before a block's samples: its first sample's line index and its time (ms -> line index for the gate).
    void beginBlock(uint32_t blockMs) {
        _blockIndex = _line.written();
        _blockMs = blockMs;
    }
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
    const Values& values() const { return _values; }
    bool ready() const { return _line.capacity() > 0 && _fifo.capacity() > 0; }
    // The node is playing its own replay (or its tail) at blockMs: the shell does not feed detection meanwhile and
    // restarts it after (owner 2026-11-01: at an audible gain the node answered itself).
    bool inputSuppressed(uint32_t blockMs);
    // Detection pauses for the own playback: the gate shuts now (the line holds no own output).
    void suppressNow();
    size_t bytes() const { return _line.capacity() * sizeof(int16_t) + _fifo.capacity() * sizeof(int16_t); }

private:
    void apply(const WhisperGate::Step& step);
    uint64_t lineIndexAtMs(unsigned long ms) const;

    uint32_t _rateHz = 24000;
    uint64_t _seed = 0;
    Values _values;
    Counts _counts;
    GatedDelayLine _line;
    BandFlatness _flat;
    SampleFifo _fifo;
    WhisperGate _gate;
    I2sToneOutput* _output = nullptr;
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
