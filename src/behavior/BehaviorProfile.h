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
    // 60 s +- 20 s since 2026-10-22 (was 20 s +- 5 s): with five nodes at 0.5 FS
    // every idle call wakes the whole group (decisions/2026-10-22-network-pacing.md).
    uint32_t idleTimeoutMs = 60000;
    uint32_t idleTimeVariationMs = 20000;
    uint32_t idleBlockedAfterHeardMs = 1000;
    uint32_t idleBlockedAfterOwnEmitMs = 500;

    // Rest when the network gets busy (decisions/2026-10-17-two-node-
    // exchange-chain-length.md, owner 2026-10-17): a hard pause, no answers
    // and no idle voice, for restMs. Two triggers, either one:
    //  - the field: this many valid network patterns heard in the field
    //    window (FieldStateConfig::patternWindowMs; only accepted, valid
    //    verdicts count, so speech/music/traffic do not silence a node);
    //  - the node's own budget: this many own emissions inside
    //    ownEmitBudgetWindowMs (a node that hears only one loud neighbour
    //    sees a thin field but keeps answering).
    // Same values on every node (no per-node variation, owner's choice).
    // 0 switches a trigger off.
    // Since BEH-008 (step 16) both triggers default to 0 (off): the level automaton below rests the node. They stay
    // as params for the reproducibility rule until retired.
    uint32_t restAfterFieldPatterns = 0;
    uint32_t ownEmitBudget = 0;
    uint32_t ownEmitBudgetWindowMs = 10000;
    // 8 s with the level automaton (BEH-008's first guess; 5 s since 2026-10-22 with the old triggers).
    uint32_t restMs = 8000;

    // The level automaton (BEH-008, decisions/2026-10-29-answers-follow-the-field-level-automaton.md): an integer
    // level, +1 per heard valid train (a chord's merged occurrence counts its trains by duration), +1 per own
    // emission, -1 per levelDecayMs of hearing nothing; p = (level + 1) / levelMax answers a heard train and keeps a
    // pending answer when another verdict arrives (else it yields); at levelMax the node rests restMs and the level
    // returns to 0. An answer waits waitAfterHeardMs plus a uniform 0..answerJitterMs. levelMax 0 switches the
    // automaton off (p = 1, no level rest: the behaviour before step 16).
    uint32_t levelMax = 6;
    uint32_t levelDecayMs = 2000;
    uint32_t answerJitterMs = 3000;  // 3 s, shrinking with the level (owner 2026-10-30, after native-fieldsim)
    // p = (level + levelOffset) / levelMax, the level counted before the draw (as tools/sim/fieldramp.py): offset 0
    // answers the first train heard with 1 / L (owner 2026-10-30: "1 in 6"; offset 1 would be 2 / L). Not a param.
    uint32_t levelOffset = 0;
    // The decision's named extra rule ("revisit when the ear wants each answer sooner"): the jitter window shrinks
    // with the level, jitter * (levelMax - level) / levelMax: a quiet node answers late, one near the top quickly
    // (owner 2026-10-30, after native-fieldsim). Not a param.
    bool answerJitterShrinks = true;
    // BEH-009 (decisions/2026-10-29-wider-band-base-bin-per-node.md): on each own answer the base bin moves one bin
    // toward the base last heard (the glide, owner 2026-10-29), and at the automaton's rest it scatters to a random
    // base. Off: the base stays where the Node seeded it. Not a param.
    bool baseGlide = true;
    // The scatter's range (the decision left it open): 0 = a random base anywhere; k = at most k bins from where it
    // was, never 0 bins (step 19.2 compares them). Not a param.
    uint32_t baseScatterBins = 0;
};
