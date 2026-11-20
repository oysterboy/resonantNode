#pragma once

#include <stddef.h>
#include <stdint.h>

#include <ParamRegistry.h>

#include "../../app/ParamIds.h"
#include "../../app/RuntimeDefaults.h"
#include "../../detection/HopGapContract.h"
#include "../../output/OutputRequest.h"
#include "../../output/TrillShape.h"

class FreqBandStream;

/*
NodeSignalComposer

The Node's half of an emission (myspec 6.2 / 7.1, decisions/2026-10-23-soundoutput-executes-node-composes.md):
Behavior names the kind of emission, this part turns it into one OutputRequest from the band grid and the
output.* hop / idle-call values it owns. SoundOutput executes the request.

The grid is the detector's: `gridOf()` reads FreqBandStream's applied band edges, bin count and spacing, never
recomputes them, so the train is played on the bins the detector listens on. The band itself stays detection's
(detection.band_*, applied by the Node to FreqBandStream).

Pure: no Arduino calls, no printing, no device; it builds on the host (test/test_node_composer). The output.hop_*
and output.idle_announce_* params bind to `values()` (param::describeParams below, registered by NodeParams).
Owns: the output.* hop / idle-call values and the composition of one OutputRequest.
Does not: play it (SoundOutput), choose when (Behavior) or own the band (detection.band_*).
*/
class NodeSignalComposer {
public:
    // The band's bins as FreqBandStream applied them (0 bins: single-target mode, no train).
    struct BandGrid {
        uint32_t lowHz = 0;
        uint32_t spacingHz = 0;
        size_t bins = 0;
    };

    // output.hop_* and output.idle_announce_*: the node's network signal and its idle call. The train's shape is
    // runtime::kDefaultHop* (decisions/2026-11-05-short-glide-signal.md); hopRotation seeds the train's base bin,
    // where it starts climbing (from the roster or the MAC at boot, PARAM SET output.hop_rotation overrides).
    // Behavior owns the base from there (the glide, the scatter; BEH-009) and the Node passes it in; no wrap with
    // the 8-bin band. Idle call (chosen 2026-10-17): announcement tone below the band, a gap, then the node's call
    // (its hop train); 0 Hz = plain idle chirp pattern.
    // decisions/2026-10-16-hop-acceptance-contrast-gate-not-amp.md
    struct Values {
        bool hopEnabled = true;
        uint32_t hopRotation = 0;
        uint32_t hopPulseMs = runtime::kDefaultHopPulseMs;
        uint32_t hopGapMs = runtime::kDefaultHopGapMs;
        uint32_t hopSteps = runtime::kDefaultHopSteps;
        // Pulses on the base bin before the climb, each as long as a step and followed by hopTrillGapMs: a cricket's
        // trill, with the climb still crossing the band's bins, which a fixed tone alone would lose in the room's
        // comb (docs/research/room-comb-and-margin.md). 0 = the plain train. The climb keeps hopGapMs (0: the glide).
        uint32_t hopTrillPulses = 0;
        uint32_t hopTrillGapMs = 15;
        // A burst of high-passed noise at the start of the pulses, so people can place the call
        // (docs/research/broadband-localizable-call.md): broadbandLevel percent of the tone's peak, 0 = off (the
        // output is then today's); broadbandScope 0 = every pulse, 1 = the trill's only; the burst lies above
        // broadbandLowHz, which keeps it out of the detection band. The tone gives up 20 log10(1 + level) dB of
        // headroom for it (output/BroadbandBurst.h).
        uint32_t broadbandLevel = 0;
        uint32_t broadbandScope = 0;
        uint32_t broadbandLowHz = 6500;
        // The trill's pulses shaped like a field cricket's (24.4.1, output/TrillShape.h): a fade in and out of
        // trillFadeMs, a pitch fall of trillDroopHz across each pulse, the first trillSwellPulses growing from half
        // level. Each 0 = off; all 0 = the plain trill.
        uint32_t trillFadeMs = 0;
        uint32_t trillDroopHz = 0;
        uint32_t trillSwellPulses = 0;
        // Which pulses the shape reaches (trill::kScope*): 0 the trill's, 1 the rise's steps too, 2 the rise's steps
        // the fall only (they stay gapless).
        uint32_t trillShapeScope = 0;
        // The trill's pulse lengths (output/TrillShape.h, trill::Length): the last pulse trillPulseEndMs long, the
        // ones between stepping from hopPulseMs to it, each varied at random by up to trillPulseJitterMs. Each 0 =
        // off. The call's length changes with them: a preset keeps it inside the detector's maximum.
        uint32_t trillPulseEndMs = 0;
        uint32_t trillPulseJitterMs = 0;
        uint32_t idleAnnounceHz = 300;
        uint32_t idleAnnounceMs = 500;
        // 0: the announcement is one steady tone; else it glides from idleAnnounceHz to this over idleAnnounceMs
        // (OUT-008: the rise played backwards, below every listening window so no node detects it).
        uint32_t idleAnnounceEndHz = 0;
        uint32_t idleAnnounceLevelPct = 100;  // the announcement's level, % of the call's
    };

    static constexpr unsigned long kIdleAnnounceGapMs = 200;

    static BandGrid gridOf(const FreqBandStream& band);

    Values& values() { return _values; }
    const Values& values() const { return _values; }

    // The highest base bin a train of hopSteps fits under (bins - steps; 3 on the 8-bin band with 5 steps), and a
    // base lowered to that.
    size_t maxBaseBin(const BandGrid& grid) const;
    size_t clampBaseBin(const BandGrid& grid, uint32_t base) const;
    // The train's bins on the grid, climbing from baseBin: hopSteps of them (at most capacity and the grid's bins),
    // 0 when the grid has no bins.
    size_t hopFrequencies(const BandGrid& grid, uint32_t baseBin, uint32_t* out, size_t capacity) const;
    // The node's network signal into request's hop fields (BEH-005): the trill's pulses on the base bin, then the
    // climb. The trill is shortened until the train fits maxTrainMs. False when the grid has no bins or the pulse
    // is 0 (request.hopSteps is then 0).
    bool composeHopTrain(const BandGrid& grid, uint32_t baseBin, OutputRequest& request,
                         unsigned long maxTrainMs) const;
    // The train as the detector's contract reads it (trill and climb, before a span limit trims the trill).
    detection::HopTrainShape trainShape(unsigned long climbSteps) const {
        detection::HopTrainShape shape;
        shape.steps = climbSteps;
        shape.pulseMs = _values.hopPulseMs;
        shape.gapMs = _values.hopGapMs;
        shape.trillPulses = _values.hopTrillPulses;
        shape.trillGapMs = _values.hopTrillGapMs;
        return shape;
    }
    // The request for what Behavior asked: a heard pattern is answered with the hop train (networkSignal), the
    // idle call is the announcement tone, a gap, then the hop train (or the plain chirp when the hop is off);
    // everything else is the plain pattern.
    OutputRequest compose(const BandGrid& grid, ChirpPattern pattern, bool networkSignal, uint32_t baseBin) const;

private:
    Values _values;
};

namespace param {

// output.hop_* and output.idle_announce_*. Bound so the longest emission fits OutputLimits.maxSpanMs (6000): 8 steps
// of 250 + 7 gaps of 250 = 3750 ms, and the idle call adds announce (<= 2000) + its 200 ms gap = 5950 (review D1.2,
// 2026-10-26: 1000 / 2000 let a legal SET turn every answer into a chirp; test/test_node_composer).
// output.idle_announce_hz: 0 (no announcement) or a tone; 1..99 plays at OutputLimits.minHz (100), see compose.
inline void describeParams(ParamRegistry& registry, NodeSignalComposer::Values& hop) {
    registry.addBool(ParamId::Output_HopEnabled, ModuleId::Output, "output.hop_enabled", &hop.hopEnabled);
    registry.addUInt32(ParamId::Output_HopRotation, ModuleId::Output, "output.hop_rotation", &hop.hopRotation, 0U, 15U);
    registry.addUInt32(ParamId::Output_HopPulseMs, ModuleId::Output, "output.hop_pulse_ms", &hop.hopPulseMs, 1U, 250U);
    registry.addUInt32(ParamId::Output_HopGapMs, ModuleId::Output, "output.hop_gap_ms", &hop.hopGapMs, 0U, 250U);
    registry.addUInt32(ParamId::Output_HopSteps, ModuleId::Output, "output.hop_steps", &hop.hopSteps, 1U, 8U);
    // Up to 12 (runtime::kMaxTrillPulses); a trill that would push a call past OutputLimits.maxSpanMs is shortened
    // by the composer, never rejected.
    registry.addUInt32(ParamId::Output_HopTrillPulses, ModuleId::Output, "output.hop_trill_pulses", &hop.hopTrillPulses,
                       0U, static_cast<uint32_t>(runtime::kMaxTrillPulses));
    registry.addUInt32(ParamId::Output_HopTrillGapMs, ModuleId::Output, "output.hop_trill_gap_ms", &hop.hopTrillGapMs,
                       0U, 250U);
    registry.addUInt32(ParamId::Output_BroadbandLevel, ModuleId::Output, "output.broadband_level", &hop.broadbandLevel,
                       0U, 100U);
    registry.addUInt32(ParamId::Output_BroadbandScope, ModuleId::Output, "output.broadband_scope", &hop.broadbandScope,
                       0U, 1U);
    registry.addUInt32(ParamId::Output_BroadbandLowHz, ModuleId::Output, "output.broadband_low_hz", &hop.broadbandLowHz,
                       1000U, runtime::kMaxToneHz);
    // The fade is held to half a pulse and the fall to trill::kMaxDroopHz by the device, so the ranges stay plain.
    registry.addUInt32(ParamId::Output_TrillFadeMs, ModuleId::Output, "output.trill_fade_ms", &hop.trillFadeMs, 0U,
                       50U);
    registry.addUInt32(ParamId::Output_TrillDroopHz, ModuleId::Output, "output.trill_droop_hz", &hop.trillDroopHz, 0U,
                       400U);
    registry.addUInt32(ParamId::Output_TrillSwellPulses, ModuleId::Output, "output.trill_swell_pulses",
                       &hop.trillSwellPulses, 0U, static_cast<uint32_t>(runtime::kMaxTrillPulses));
    registry.addUInt32(ParamId::Output_TrillShapeScope, ModuleId::Output, "output.trill_shape_scope",
                       &hop.trillShapeScope, 0U, trill::kScopeMax);
    registry.addUInt32(ParamId::Output_TrillPulseEndMs, ModuleId::Output, "output.trill_pulse_end_ms",
                       &hop.trillPulseEndMs, 0U, trill::kMaxPulseMs);
    registry.addUInt32(ParamId::Output_TrillPulseJitterMs, ModuleId::Output, "output.trill_pulse_jitter_ms",
                       &hop.trillPulseJitterMs, 0U, 20U);
    registry.addUInt32(ParamId::Output_IdleAnnounceHz, ModuleId::Output, "output.idle_announce_hz", &hop.idleAnnounceHz,
                       0U, runtime::kMaxToneHz);
    registry.addUInt32(ParamId::Output_IdleAnnounceMs, ModuleId::Output, "output.idle_announce_ms", &hop.idleAnnounceMs,
                       20U, 2000U);
    registry.addUInt32(ParamId::Output_IdleAnnounceEndHz, ModuleId::Output, "output.idle_announce_end_hz",
                       &hop.idleAnnounceEndHz, 0U, runtime::kMaxToneHz);
    registry.addUInt32(ParamId::Output_IdleAnnounceLevelPct, ModuleId::Output, "output.idle_announce_level_pct",
                       &hop.idleAnnounceLevelPct, 0U, 100U);
}

}  // namespace param
