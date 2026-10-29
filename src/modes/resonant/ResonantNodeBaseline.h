#pragma once

#include <stdint.h>

#include "../../app/RuntimeDefaults.h"

/*
NodeBaseline (step 11i N2, docs/refactors/node-split.md)

The Node's quiet-gated rebase machine: at boot the mic warmup, then listen for
a quiet hold, rebase, settle, outputs on (or give up after a timeout and run
on the old floor). `RB rebase` starts the quiet gate again, `RB rebase force`
and SELFTEST's restore go straight to Settle.

Pure: time and the smoothed level are handed in, no Arduino calls, no audio,
no printing; it builds on the host (test/test_node_baseline). `update()`
returns what happened this tick; the Node performs the side effects (audio
rebase, Behavior quiesce, detection reset) and prints the RB lines. Two steps
are requests the Node completes: `WarmupDone` (rebase the audio, then
`startListening()`) and `QuietHeld` (the machine is in Settle already; the
Node rebases, then `noteRebaseResult(Done, ...)`).
*/
class NodeBaseline {
public:
    enum class State : uint8_t {
        Boot,
        ListenForQuiet,
        Settle,
        Active,
        FailedNoQuiet,
    };

    enum class RebaseResult : uint8_t {
        Pending,
        Done,
        NoQuiet,
    };

    struct Config {
        // Smoothed signal magnitude under which the startup baseline counts as
        // quiet. The D-AMP mic's floor sits at ~230-250 in a quiet room (#20,
        // 2026-10-09), so the piezo-era 20 was never reached there.
#if defined(BOARD_PIEZO)
        int quietThreshold = 20;
#else
        int quietThreshold = 400;
#endif
        unsigned long micWarmupMs = runtime::kDefaultAudioSignalStartupWarmupMs;
        unsigned long quietHoldMs = 1000;
        // 20 s, not 8 (owner 2026-10-29, step 12.1.1): boots in a noisy moment (all boards rebooting together after an
        // OTA) missed 1 s below the threshold within 8 s; the threshold itself stays.
        unsigned long timeoutMs = 20000;
        // Settle after a rebase before outputs come back (quiet-gated and forced).
        unsigned long settleMs = 500;
        unsigned long logEveryMs = 1000;
    };

    enum class Step : uint8_t {
        None,
        WarmupWaiting,  // log line due: elapsedMs of the warmup
        WarmupDone,  // warmup over: rebase the audio, then startListening()
        RebaseWaiting,  // log line due: smooth, elapsedMs = quiet so far
        QuietHeld,  // quiet hold reached, now in Settle: rebase, then noteRebaseResult(Done)
        NoQuiet,  // timeout without a quiet hold, now FailedNoQuiet: noteRebaseResult(NoQuiet)
        Activated,  // Settle over, now Active
    };

    struct Tick {
        Step step = Step::None;
        unsigned long elapsedMs = 0;
        float smooth = 0.0f;
    };

    NodeBaseline() = default;
    explicit NodeBaseline(const Config& config)
        : _config(config) {}

    // Boot state from `now`, results and counts cleared.
    void begin(unsigned long now);
    Tick update(unsigned long now, float smooth);

    // The quiet gate from `now` (after the warmup, and `RB rebase`).
    void startListening(unsigned long now);
    // A forced rebase was done at `now`: Settle, counted apart.
    void noteForcedRebase(unsigned long now);
    // Settle again until `untilMs` when outputs were on or settling (SELFTEST restore).
    void resettle(unsigned long untilMs);
    // Outcome of a quiet-gated rebase (Done or NoQuiet) at `atMs`.
    void noteRebaseResult(RebaseResult result, unsigned long atMs);

    State state() const { return _state; }
    const char* stateName() const;
    // Active or FailedNoQuiet: the baseline lets outputs on (the Node adds output.silent).
    bool outputsAllowed() const { return _state == State::Active || _state == State::FailedNoQuiet; }

    RebaseResult bootResult() const { return _bootResult; }
    RebaseResult lastResult() const { return _lastResult; }
    unsigned long lastRebaseAtMs() const { return _lastRebaseAtMs; }
    float lastRebaseSmooth() const { return _lastRebaseSmooth; }
    unsigned long quietRebaseCount() const { return _quietRebaseCount; }
    unsigned long forcedRebaseCount() const { return _forcedRebaseCount; }
    const Config& config() const { return _config; }

private:
    Config _config;
    State _state = State::Boot;
    unsigned long _stateStartedMs = 0;
    unsigned long _quietSinceMs = 0;
    unsigned long _lastLogMs = 0;
    unsigned long _settleUntilMs = 0;
    // Outcome of the quiet-gated rebase: the one at boot and the latest one
    // (a later "RB rebase" repeats it); "RB rebase force" is counted apart.
    RebaseResult _bootResult = RebaseResult::Pending;
    RebaseResult _lastResult = RebaseResult::Pending;
    unsigned long _lastRebaseAtMs = 0;
    unsigned long _quietRebaseCount = 0;
    unsigned long _forcedRebaseCount = 0;
    float _lastRebaseSmooth = 0.0f;
};
