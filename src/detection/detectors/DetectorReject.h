#pragma once

namespace detection {

/*
DetectorRejectClass

Minimal canonical reject bucket for detector-stage reporting.
Detector-specific strings and richer typed details stay in detector-owned
detail structs.
*/
enum class DetectorRejectClass {
    None,
    Threshold,
    Timing,
    Strength,
    Quality,
    Cooldown,
    State,
    Window,
    Unknown,
};

} // namespace detection
