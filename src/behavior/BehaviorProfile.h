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
    uint32_t restAfterFieldPatterns = 4;
    uint32_t ownEmitBudget = 6;
    uint32_t ownEmitBudgetWindowMs = 10000;
    uint32_t restMs = 5000;  // 5 s since 2026-10-22 (was 10 s), decisions/2026-10-22-network-pacing.md
};
