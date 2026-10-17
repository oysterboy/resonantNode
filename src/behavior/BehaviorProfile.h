#pragma once

/*
BehaviorGateConfig

Behavior-side timing and gating defaults.
These settings affect behavior eligibility, not pattern validity.
*/
struct BehaviorGateConfig {
    bool idleEnabled = true;

    unsigned long waitAfterHeardMs = 100;
    unsigned long refractoryAfterEmitMs = 400;
    unsigned long behaviorSuppressSelfChirpMs = 100;
    // After toneOff the own chirp is still heard for 33-54 ms on D-AMP (TX
    // queue + room; #19 latency measurement), so detection stays off a bit
    // longer than that.
    unsigned long detectionSuppressTailMsOwnEmit = 60;
    unsigned long idleTimeoutMs = 20000;
    unsigned long idleTimeVariationMs = 5000;
    unsigned long idleBlockedAfterHeardMs = 1000;
    unsigned long idleBlockedAfterOwnEmitMs = 500;

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
    unsigned long restAfterFieldPatterns = 4;
    unsigned long ownEmitBudget = 6;
    unsigned long ownEmitBudgetWindowMs = 10000;
    unsigned long restMs = 10000;
};
