#pragma once

#include <stddef.h>
#include <stdint.h>

#include <ParamRegistry.h>

#include "../../app/ParamIds.h"
#include "../../app/RuntimeDefaults.h"
#include "../../output/OutputRequest.h"

class FreqBandStream;

/*
NodeSignalComposer (step 11i N5, docs/refactors/node-split.md)

The Node's half of an emission (myspec 6.2 / 7.1, decisions/2026-10-23-soundoutput-executes-node-composes.md):
Behavior names the kind of emission, this part turns it into one OutputRequest from the band grid and the
output.* hop / idle-call values it owns. SoundOutput executes the request.

The grid is the detector's: `gridOf()` reads FreqBandStream's applied band edges, bin count and spacing, never
recomputes them, so the train is played on the bins the detector listens on. The band itself stays detection's
(detection.band_*, applied by the Node to FreqBandStream).

Pure: no Arduino calls, no printing, no device; it builds on the host (test/test_node_composer). The output.hop_*
and output.idle_announce_* params bind to `values()` (param::describeParams below, registered by NodeParams).
*/
class NodeSignalComposer {
public:
    // The band's bins as FreqBandStream applied them (0 bins: single-target mode, no train).
    struct BandGrid {
        uint32_t lowHz = 0;
        uint32_t spacingHz = 0;
        size_t bins = 0;
    };

    // output.hop_* and output.idle_announce_*: the node's network signal and its idle call. Shape from the
    // 2026-10-16 battery (25 ms x 5, gap 10); the rotation is the train's start bin (from the roster or the MAC
    // at boot, PARAM SET output.hop_rotation overrides). Idle call (owner 2026-10-17): announcement tone below
    // the band, a gap, then the node's call (its hop train); 0 Hz = plain idle chirp pattern.
    // decisions/2026-10-16-hop-acceptance-contrast-gate-not-amp.md
    struct Values {
        bool hopEnabled = true;
        uint32_t hopRotation = 0;
        uint32_t hopPulseMs = runtime::kDefaultHopPulseMs;
        uint32_t hopGapMs = runtime::kDefaultHopGapMs;
        uint32_t hopSteps = runtime::kDefaultHopSteps;
        uint32_t idleAnnounceHz = 300;
        uint32_t idleAnnounceMs = 500;
    };

    static constexpr unsigned long kIdleAnnounceGapMs = 200;

    static BandGrid gridOf(const FreqBandStream& band);

    Values& values() { return _values; }
    const Values& values() const { return _values; }

    // The train's bins on the grid, rotated per node: hopSteps of them (at most capacity), 0 when the grid has
    // no bins.
    size_t hopFrequencies(const BandGrid& grid, uint32_t* out, size_t capacity) const;
    // The node's network signal into request's hop fields (BEH-005). False when the grid has no bins or the
    // pulse is 0 (request.hopSteps is then 0).
    bool composeHopTrain(const BandGrid& grid, OutputRequest& request) const;
    // The request for what Behavior asked: a heard pattern is answered with the hop train (networkSignal), the
    // idle call is the announcement tone, a gap, then the hop train (or the plain chirp when the hop is off);
    // everything else is the plain pattern.
    OutputRequest compose(const BandGrid& grid, ChirpPattern pattern, bool networkSignal) const;

private:
    Values _values;
};

namespace param {

// output.hop_* and output.idle_announce_*. Bound so the longest emission fits OutputLimits.maxSpanMs (6000): 8 steps
// of 250 + 7 gaps of 250 = 3750 ms, and the idle call adds announce (<= 2000) + its 200 ms gap = 5950 (review D1.2,
// 2026-10-26: 1000 / 2000 let a legal SET turn every answer into a chirp; test/test_node_composer).
inline void describeParams(ParamRegistry& registry, NodeSignalComposer::Values& hop) {
    registry.addBool(ParamId::Output_HopEnabled, ModuleId::Output, "output.hop_enabled", &hop.hopEnabled);
    registry.addUInt32(ParamId::Output_HopRotation, ModuleId::Output, "output.hop_rotation", &hop.hopRotation, 0U, 15U);
    registry.addUInt32(ParamId::Output_HopPulseMs, ModuleId::Output, "output.hop_pulse_ms", &hop.hopPulseMs, 1U, 250U);
    registry.addUInt32(ParamId::Output_HopGapMs, ModuleId::Output, "output.hop_gap_ms", &hop.hopGapMs, 0U, 250U);
    registry.addUInt32(ParamId::Output_HopSteps, ModuleId::Output, "output.hop_steps", &hop.hopSteps, 1U, 8U);
    registry.addUInt32(ParamId::Output_IdleAnnounceHz, ModuleId::Output, "output.idle_announce_hz", &hop.idleAnnounceHz, 0U,
                       runtime::kMaxToneHz);
    registry.addUInt32(ParamId::Output_IdleAnnounceMs, ModuleId::Output, "output.idle_announce_ms", &hop.idleAnnounceMs, 20U,
                       2000U);
}

}  // namespace param
