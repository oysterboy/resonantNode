#pragma once

/*
BehaviorGateConfig

Behavior-side timing and gating defaults.
These settings affect behavior eligibility, not pattern validity.
*/
struct BehaviorGateConfig {
    bool idleEnabled = true;

    unsigned long waitAfterHeardMs = 200;
    unsigned long refractoryAfterEmitMs = 0;
    unsigned long behaviorSuppressSelfChirpMs = 100;
    unsigned long detectionSuppressTailMsOwnEmit = 0;
    unsigned long idleTimeoutMs = 20000;
    unsigned long idleTimeVariationMs = 5000;
    unsigned long idleBlockedAfterHeardMs = 1000;
    unsigned long idleBlockedAfterOwnEmitMs = 500;
};
