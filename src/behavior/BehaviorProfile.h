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
};
