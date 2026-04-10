#include "ResonantBehavior.h"

void ResonantBehavior::update(float inputLevel) {
    const float exciteGain = 0.1f;
    const float decay = 0.90f;
    const float sig_threshold = 50.0f;   // important

    // only excite on meaningful input
    if (inputLevel > sig_threshold) {
        _activity += inputLevel * exciteGain;
    }

    // decay always
    _activity*= decay;

    // clamp
    if (_activity > 1.0f) {
        _activity = 1.0f;
    }
    if (_activity < 0.001f) {
        _activity = 0.0f;
    }
}

float ResonantBehavior::activity() const {
    return _activity;
}

bool ResonantBehavior::isActive() const {
    return _activity > 0.3f;
}