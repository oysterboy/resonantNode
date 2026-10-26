#pragma once

#include <Arduino.h>

#include "../detection/features/FreqBandStream.h"

/*
The FreqBandStream's runtime fields as ` freq.window_samples=... freq.packet_age_ms=...`, one copy for the
Node's RB STATUS and the Analyzer's FREQBAND line (they were byte-identical in both, review B7, step 11f). No
leading label, no newline: the callers frame it.
*/
inline void printFreqStatusFields(Print& out, const FreqBandStream& stream, unsigned long sampleRateHz) {
    const unsigned long windowSizeSamples = stream.windowSizeSamples();
    const unsigned long frequencyUpdateEverySamples = stream.frequencyUpdateEverySamples();
    const unsigned long ageSamples = stream.lastPacketAgeSamples();
    const float windowMs = sampleRateHz > 0
        ? (static_cast<float>(windowSizeSamples) * 1000.0f) / static_cast<float>(sampleRateHz)
        : 0.0f;
    const float updateStepMs = sampleRateHz > 0
        ? (static_cast<float>(frequencyUpdateEverySamples) * 1000.0f) / static_cast<float>(sampleRateHz)
        : 0.0f;
    const float ageMs = sampleRateHz > 0
        ? (static_cast<float>(ageSamples) * 1000.0f) / static_cast<float>(sampleRateHz)
        : 0.0f;
    out.print(" freq.window_samples=");
    out.print(windowSizeSamples);
    out.print(" freq.window_ms=");
    out.print(windowMs, 2);
    out.print(" freq.update_every_samples=");
    out.print(frequencyUpdateEverySamples);
    out.print(" freq.update_period_ms=");
    out.print(updateStepMs, 3);
    out.print(" freq.target_hz=");
    out.print(stream.targetFrequencyHz());
    out.print(" freq.produced_fresh_packet=");
    out.print(stream.producedFreshPacketOnLastObserve() ? 1 : 0);
    out.print(" freq.packet_age_samples=");
    out.print(ageSamples);
    out.print(" freq.packet_age_ms=");
    out.print(ageMs, 3);
}
