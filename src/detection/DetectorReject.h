#pragma once

namespace detection {

/*
DetectorRejectClass

Minimal canonical reject bucket for detector-stage reporting.
Detector-specific strings and richer typed details stay in migration-era
structures until later passes define the full report payload.
*/
enum class DetectorRejectClass {
    None,
    Threshold,
    Timing,
    Strength,
    Cooldown,
    State,
    Window,
    Unknown,
};

} // namespace detection
