#pragma once

#include "SignalCandidate.h"

namespace detection {

enum class SignalDecision {
    None,
    Accepted,
    Rejected
};

struct InspectedSignal {
    SignalCandidate signal = {};
    SignalDecision decision = SignalDecision::None;

    bool accepted = false;
    bool rejected = false;

    const char* reason = "none";
};

} // namespace detection
