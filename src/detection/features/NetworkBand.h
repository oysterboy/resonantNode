#pragma once

#include <stdint.h>

#include <ParamRegistry.h>

#include "../../app/ParamIds.h"
#include "../../app/RuntimeDefaults.h"
#include "FreqBandStream.h"

/*
NetworkBand (step 11i N7, review A10; DET-013)

The band the detector listens to, as the detection.band_* params set it: FreqBandStream's bank over lowHz..highHz
on its bin grid. An empty or inverted band (lowHz 0, highHz < lowHz) means "single target at the chirp tone".
Band 2950-4700 (8 bins, 2026-10-30; decisions/2026-10-29-wider-band-base-bin-per-node.md), from 2950-3950 so the
3,200 Hz chirp sits on a bin (owner 2026-10-16, decisions/2026-10-16-network-band-2950-3950.md). The Node's hop train reads the applied grid back from
FreqBandStream (NodeSignalComposer::gridOf), never from these values.
*/
struct NetworkBand {
    uint32_t lowHz = runtime::kDefaultNetworkBandLowHz;
    uint32_t highHz = runtime::kDefaultNetworkBandHighHz;

    // Into the stream when it differs from what the stream has applied (a few compares when unchanged). A band
    // wider than the bank is compared at the edge the stream clamps it to, else it never matched and the bank was
    // rebuilt every loop (review 12.0 M7).
    void applyTo(FreqBandStream& stream) const {
        const bool bandValid = lowHz != 0 && highHz >= lowHz;
        const unsigned long lo = bandValid ? lowHz : 0UL;
        const unsigned long hi = bandValid ? stream.clampedBandHighHz(lowHz, highHz) : 0UL;
        if (stream.bandLowHz() == lo && stream.bandHighHz() == hi) {
            return;
        }
        stream.setBand(lo, hi);
    }
};

namespace param {

// detection.band_low_hz / band_high_hz (0 = single target at the chirp tone).
inline void describeParams(ParamRegistry& registry, NetworkBand& band) {
    registry.addUInt32(ParamId::Detection_BandLowHz, ModuleId::Detection, "detection.band_low_hz", &band.lowHz, 0U,
                       runtime::kMaxToneHz);
    registry.addUInt32(ParamId::Detection_BandHighHz, ModuleId::Detection, "detection.band_high_hz", &band.highHz, 0U,
                       runtime::kMaxToneHz);
}

}  // namespace param
