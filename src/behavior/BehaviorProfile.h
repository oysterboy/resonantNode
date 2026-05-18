#pragma once

struct BehaviorProfile {
    bool detectionOnly = false;
    bool requireTonalForBehavior = true;
    bool idleEnabled = true;

    unsigned long waitAfterTransientMs = 100;
    unsigned long refractoryAfterEmitMs = 0;
    unsigned long idleTimeoutMs = 20000;
    unsigned long idleTimeVariationMs = 10000;
    unsigned long idleBlockedAfterHeardMs = 3000;
    unsigned long idleBlockedAfterOwnEmitMs = 5000;
};
