#pragma once

#include "BehaviorProfile.h"

/*
BehaviorStatus (step 11i N6, review A9, docs/refactors/node-split.md)

One snapshot of Behavior's state at `nowMs`, filled by ResonantBehavior::status(now) and printed by the Node
(RB STATUS, RB summary's behavior line, the BEH verdict and RB rest start lines, the boot "RB behavior" block,
RB CALL). The Node prints these values; it does not re-derive them. Names are Behavior's own (state, decisions,
verdict type); the decisions themselves stay in ResonantBehavior.

Plain data, no logic: what `status()` copies is exactly what the getters return.
*/
struct BehaviorStatus {
    unsigned long nowMs = 0;  // the time the time-dependent fields below were taken at

    // State machine.
    const char* stateName = "";
    bool idle = true;              // state == Idle
    bool outputsEnabled = true;    // setOutputsEnabled(): the Node's outputs-off gate as Behavior sees it
    bool outputBusy = false;       // Chirping
    bool behaviorEligible = false;

    // The gate timing as configured (the behavior.* params).
    BehaviorGateConfig config = {};

    // The last verdict and what Behavior made of it.
    const char* lastDecisionName = "";
    const char* lastBlockReasonName = "";
    const char* lastVerdictTypeName = "";
    unsigned long lastHeardMs = 0;
    unsigned long lastEmitMs = 0;

    // Deadlines and what is left of them at nowMs.
    unsigned long waitUntilMs = 0;
    unsigned long refractoryUntilMs = 0;
    unsigned long waitRemainingMs = 0;
    unsigned long refractoryRemainingMs = 0;
    unsigned long behaviorSuppressRemainingMs = 0;
    bool ownEmitSuppressArmed = false;
    unsigned long ownEmitSuppressUntilMs = 0;

    // Rest (busy network).
    bool resting = false;  // at nowMs
    unsigned long restUntilMs = 0;
    unsigned long restsEntered = 0;
    const char* lastRestReasonName = "";
    unsigned long blockedResting = 0;

    // Counters since the last resetState().
    unsigned long patternsReceived = 0;
    unsigned long patternsIgnoredInvalid = 0;
    unsigned long blockedOutputBusy = 0;
    unsigned long blockedRefractory = 0;
    unsigned long blockedWaiting = 0;
    unsigned long blockedSelfSuppressed = 0;
    unsigned long wouldEmitCount = 0;
    unsigned long emittedCount = 0;
};
