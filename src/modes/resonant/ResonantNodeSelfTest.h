#pragma once

#include <stdint.h>

#include "../../detection/evaluation/OccurrenceVerdict.h"
#include "../../selftest/SelfTestReport.h"
#include "ResonantNodeBaseline.h"

/*
NodeSelfTest (NODE-015; step 11i N4, docs/refactors/node-split.md)

The Node's SELFTEST: the command, its phases and the runtime smoke checks.
It is handed what the Node already sees (verdicts popped from
DetectionRuntime, own chirp start / finish, a few values per tick in
`Inputs`) and asks for chirps through `Chirps` (Behavior's test chirp); it
reads the baseline machine's results for quiet_boot and never reaches into
the Node. The blocking hardware checks (mic, amp) need the Node's audio
source and tone output: `update()` returns `Request::RunHardware`, the Node
runs them (with `tally()` and `printQuietBoot()`) and hands the results back
with `hardwareDone()`. Phase sequence: ResonantNodeSelfTest.cpp.
*/
class NodeSelfTest {
public:
    // Behavior's test chirp: ask for one, or drop a request it never took.
    struct Chirps {
        void* ctx;
        void (*request)(void* ctx);
        void (*cancel)(void* ctx);
    };

    // What the Node hands in on every tick.
    struct Inputs {
        bool outputsEnabled = false;          // the Node's outputs gate (baseline and output.silent)
        bool chirpActive = false;             // ChirpOutput sounding
        unsigned long lastOwnChirpEndMs = 0;  // when the last own chirp stopped
        uint32_t toneHz = 0;                  // the chirp tone (SELFTEST_CHIRP line)
        uint32_t suppressTailMs = 0;          // behavior.detectionSuppressTailMsOwnEmit
    };

    enum class Request : uint8_t {
        None,
        RunHardware,  // run the mic / amp checks now, then hardwareDone()
    };

    // Settle after the hardware checks, same as the post-rebase settle.
    static constexpr unsigned long kSettleAfterHardwareMs = 500;

    NodeSelfTest(const NodeBaseline& baseline, const Chirps& chirps)
        : _baseline(baseline),
          _chirps(chirps) {}

    // Boot: no self-test running.
    void begin();

    // "SELFTEST ..." from the console or the radio.
    void handleCommand(const char* line, unsigned long now);
    bool active() const { return _st.phase != Phase::Idle; }
    Request update(unsigned long now, const Inputs& in);

    // The hardware block, run by the Node on Request::RunHardware.
    selftest::Tally& tally() { return _st.tally; }
    void printQuietBoot(float audioBaseline);
    void hardwareDone(selftest::Result mic, selftest::Result amp, unsigned long nowMs);

    void observeVerdict(const detection::OccurrenceVerdict& verdict, unsigned long now, uint32_t suppressTailMs);
    void noteChirpStarted(unsigned long now, const char* sourceName);
    void noteChirpFinished(unsigned long now);

private:
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

    struct State {
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

    void startChirps(Phase phase, unsigned long now);
    void driveChirps(unsigned long now, const Inputs& in);
    void finishOwnChirp(bool timedOut, uint32_t suppressTailMs);
    void finishExternal(unsigned long now);
    void finish();
    bool verdictIsOwn(unsigned long occurrenceStartMs, uint32_t suppressTailMs) const;

    const NodeBaseline& _baseline;
    Chirps _chirps;
    State _st;
};
