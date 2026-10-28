#pragma once

/*
The Emitter's PULSES bounds (DET-011), shared with the Analyzer, which refuses a
SEQ run outside them before it starts (otherwise every trial is rejected on the
Emitter).
*/
namespace emitter_limits {

constexpr unsigned long kPulsesMaxCount = 50;
constexpr unsigned long kPulsesMaxPulseMs = 1000;
constexpr unsigned long kPulsesMaxGapMs = 2000;
constexpr unsigned long kPulsesMaxRampMs = 50;

}  // namespace emitter_limits
