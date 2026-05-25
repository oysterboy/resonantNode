#pragma once

/*
BehaviorGateConfig

Behavior-side timing and gating defaults.
These settings affect behavior eligibility, not pattern validity.
*/
struct BehaviorGateConfig {
    bool idleEnabled = true;

    unsigned long waitAfterHeardMs = 100;
    unsigned long refractoryAfterEmitMs = 0;
    unsigned long idleTimeoutMs = 20000;
    unsigned long idleTimeVariationMs = 10000;
    unsigned long idleBlockedAfterHeardMs = 3000;
    unsigned long idleBlockedAfterOwnEmitMs = 5000;
};
