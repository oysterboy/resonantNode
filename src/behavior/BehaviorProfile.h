#pragma once

#include <stdint.h>

/*
BehaviorGateConfig

Behavior-side timing and gating defaults.
These settings affect behavior eligibility, not pattern validity.
The behavior.* params bind to the Node's copy (app/ParamBindings.h), so the
millisecond and count fields are uint32_t, the registry's unsigned width on
every build.
*/
struct BehaviorGateConfig {
    bool idleEnabled = true;

    uint32_t waitAfterHeardMs = 100;
    uint32_t refractoryAfterEmitMs = 400;
    uint32_t behaviorSuppressSelfChirpMs = 100;
    // After toneOff the own chirp is still heard for 33-54 ms on D-AMP (TX
    // queue + room; #19 latency measurement), so detection stays off a bit
    // longer than that.
    uint32_t detectionSuppressTailMsOwnEmit = 60;
    // 60 s +- 20 s: every idle call wakes the whole group, so the interval scales with the number of nodes (measured
    // with five nodes at 0.5 FS, decisions/2026-10-22-network-pacing.md).
    uint32_t idleTimeoutMs = 60000;
    uint32_t idleTimeVariationMs = 20000;
    uint32_t idleBlockedAfterHeardMs = 1000;
    uint32_t idleBlockedAfterOwnEmitMs = 500;

    // The pause at the top of the level (decisions/2026-10-17-two-node-exchange-chain-length.md): a hard pause, no
    // answers and no idle voice, for restMs. The two older triggers (patterns heard in the field window, own
    // emissions in a window) were retired 2026-11-06 (decisions/2026-11-06-retire-the-old-rest-triggers.md): the
    // level automaton below does that job.
    // 8 s with the level automaton (BEH-008's first guess).
    uint32_t restMs = 8000;

    // The level automaton (BEH-008, decisions/2026-10-29-answers-follow-the-field-level-automaton.md): an integer
    // level, +1 per heard valid train (a chord's merged occurrence counts its trains by duration), +1 per own
    // emission, -1 per levelDecayMs of hearing nothing; p = (level + 1) / levelMax answers a heard train and keeps a
    // pending answer when another verdict arrives (else it yields); at levelMax the node rests restMs and the level
    // returns to 0. An answer waits waitAfterHeardMs plus a uniform 0..answerJitterMs. levelMax 0 switches the
    // automaton off (p = 1, or a constant startChancePct; no level rest, no jitter).
    uint32_t levelMax = 6;
    uint32_t levelDecayMs = 2000;
    uint32_t answerJitterMs = 3000;  // 3 s, shrinking with the level (chosen 2026-10-30 after native-fieldsim)
    // p = (level + levelOffset) / levelMax, the level counted before the draw (as tools/sim/fieldramp.py): offset 0
    // answers the first train heard with 1 / L (chosen 2026-10-30: "1 in 6"; offset 1 would be 2 / L). Not a param
    // since 2026-11-06 (decisions/2026-11-06-param-review-owner-answers.md): every mode sets startChancePct.
    uint32_t levelOffset = 0;
    // The chance in % of answering the first train in a quiet field, rising evenly to 100 % at levelMax: the start
    // set apart from the length (decisions/2026-11-05-start-chance-and-chain-length.md). 0 keeps
    // the rule above; with levelMax 0 it is a constant chance.
    uint32_t startChancePct = 0;
    // Where the ramp ends: the chance reaches 100 % at this share of the way from level 1 to the top (100 = at the
    // level before the top, 50 = half way, then a plateau of certain answers). Only with startChancePct > 0.
    uint32_t plateauPct = 100;
    // The decision's named extra rule ("revisit when the ear wants each answer sooner"): the jitter window shrinks
    // with the level, jitter * (levelMax - level) / levelMax: a quiet node answers late, one near the top quickly
    // (chosen 2026-10-30 after native-fieldsim). behavior.spread_shrinks.
    bool answerJitterShrinks = true;
    // BEH-009 (decisions/2026-10-29-wider-band-base-bin-per-node.md): on each own answer the base bin moves one bin
    // toward the base last heard (the glide), and at the automaton's rest it scatters to a random
    // base. Off: the base stays where the Node seeded it. Not a param.
    bool baseGlide = true;
    // The scatter's range: 0 = a random base anywhere; k = 1..k bins from where it was (a band per node
    // needs 1, decisions/2026-10-31-bands-per-node.md).
    uint32_t baseScatterBins = 0;
    // A band per node (decisions/2026-10-31-bands-per-node.md): the base is the position of the node's own window on
    // bandRangeLowHz..HighHz (the Node applies the window to the detector), and a node that heard nothing since its
    // last idle call wanders to a random position before the next. Off: the shared band with base bins
    // (decisions/2026-10-29-wider-band-base-bin-per-node.md).
    bool bandPerNode = false;
    // The range picked by ear (decisions/2026-11-05-short-glide-signal.md).
    uint32_t bandRangeLowHz = 2000;
    uint32_t bandRangeHighHz = 6500;
};

// What the Node ships with: the field's settings of the 2026-11-05 overnight run, confirmed by the owner as the
// defaults on 2026-11-09 (bench/sessions/2026-11-05-night-e0, 2026-11-05-radio-log-lively). One window for every
// node, a long chain with a high start chance, no fixed wait. The struct's own defaults stay for the host tests,
// the field simulation and the echo modes, which set what they need.
inline BehaviorGateConfig shippedNodeBehavior() {
    BehaviorGateConfig b;
    b.waitAfterHeardMs = 0;
    b.refractoryAfterEmitMs = 200;
    b.idleTimeoutMs = 30000;
    b.idleTimeVariationMs = 10000;
    b.levelMax = 200;
    b.startChancePct = 85;
    b.answerJitterMs = 1000;
    b.bandPerNode = true;
    b.bandRangeLowHz = 2500;
    b.bandRangeHighHz = 3500;
    return b;
}
