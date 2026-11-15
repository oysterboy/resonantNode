#pragma once

#include <stdint.h>

#include <ParamRegistry.h>

#include "../../app/ParamIds.h"

/*
NodeLight

The Node's light effects (OUT-007, decisions/2026-11-15-node-light-one-colour-and-floor.md): what the strip shows,
composed from Behavior's state the way NodeSignalComposer composes the sound. One colour per node, its hue set by
the node's own base bin; the state sets only the brightness, and the emission is the biggest gesture. The light never
goes below the floor. Out comes hue, saturation and a perceived brightness 0..1; the driver (src/hal/PixelStrip)
turns that into bytes under the cap.

  idle       floor -> idle_top, reaching it when the idle call is due (idle_mode 0); a slow breath between the two
             instead when idle_mode is 1 or no call can come (call off, outputs off, resting)
  heard      a quick lift to heard_lift (heard_lift_ms), then building to heard_top by the moment the answer starts
  announce   (the idle call's announcement tone and its gap) the level held: the gesture belongs to the call
  emitting   ramp to 1.0 in emit_ramp_ms, held while it sounds
  afterglow  1.0 back to the floor over glow_ms, fast at first and a long tail; idle takes over under it
  yield      (heard, then idle without answering) back down over yield_ms, the same shape

Pure: no Arduino, no device; host-tested in test/test_node_light. The light.* params bind to values().
Owns: the light.* effect values and the composition of one colour per update.
Does not: decide anything (Behavior), know bytes, currents or the cap (the driver), or drive a pin.
*/
class NodeLight {
public:
    enum class Phase : uint8_t {
        Idle,
        Heard,
        Announce,
        Emitting,
        Afterglow,
    };

    // What the Node reads from Behavior for one update.
    struct Input {
        unsigned long nowMs = 0;
        Phase phase = Phase::Idle;
        unsigned long answerAtMs = 0;  // Heard: when the answer starts (Behavior's waitUntilMs)
        bool idleCallCanCome = false;  // the idle call is on and nothing holds it (outputs on, not resting)
        unsigned long idleFromMs = 0;  // the idle wait running: from, and when the call is due
        unsigned long idleAtMs = 0;
        uint32_t baseCount = 0;  // the base bins (BEH-009); 0 or 1: the hue at hue_low
        uint32_t ownBase = 0;
    };

    struct Hsb {
        float hueDeg = 0.0f;
        float sat = 0.0f;
        float bright = 0.0f;
    };

    // light.*: first guesses for the room, set there by eye (chosen 2026-11-15, the decision above). Hues in degrees:
    // hue_low at the lowest base, hue_high at the highest, the short way round between (20 -> 220 passes through
    // magenta and violet, not green). Levels in percent of the perceived brightness.
    struct Values {
        bool enabled = true;
        uint32_t maxMa = 500;  // the driver holds it at or below the build cap (LIGHT_MAX_MA)
        uint32_t idleMode = 0;
        uint32_t floorPct = 35;  // never darker: below about 0.3 the strip has only a few byte steps left
        uint32_t idleTopPct = 45;
        uint32_t heardLiftPct = 55;
        uint32_t heardLiftMs = 200;
        uint32_t heardTopPct = 65;
        uint32_t emitRampMs = 80;
        uint32_t glowMs = 2000;
        uint32_t yieldMs = 1500;
        uint32_t breatheMs = 6000;
        uint32_t hueLow = 20;
        uint32_t hueHigh = 220;
        uint32_t satPct = 80;
    };

    Values& values() { return _values; }
    const Values& values() const { return _values; }

    // The colour at in.nowMs; call it often (the Node: every 20 ms). The first call starts at the floor.
    Hsb update(const Input& in);
    // Back to the start, as at boot (LIGHT auto after a manual LIGHT).
    void reset() { _started = false; }
    const Hsb& last() const { return _last; }

    // A hue on the way from a to b by t (0..1), the short way round.
    static float mixHue(float a, float b, float t);

private:
    float idleBright(const Input& in, unsigned long sinceMs, float floor) const;
    float heardBright(const Input& in, unsigned long sinceMs) const;
    // The falling envelope (afterglow, yield): from its start level down to the floor over its time, (1 - t)^2.
    float fall(unsigned long nowMs, float floor) const;
    void startFall(unsigned long nowMs, uint32_t durationMs);

    Values _values;
    bool _started = false;
    Phase _phase = Phase::Idle;
    unsigned long _enteredMs = 0;
    float _brightAtEntry = 0.0f;
    float _fallFrom = 0.0f;
    unsigned long _fallStartMs = 0;
    uint32_t _fallMs = 0;  // 0: no fall running
    Hsb _last;
};

namespace param {

// light.*: the Node's light effects (NodeLight::Values). light.max_ma sits at or below the driver's build-time cap.
inline void describeParams(ParamRegistry& registry, NodeLight::Values& v) {
    registry.addBool(ParamId::Light_Enabled, ModuleId::Light, "light.enabled", &v.enabled);
    registry.addUInt32(ParamId::Light_MaxMa, ModuleId::Light, "light.max_ma", &v.maxMa, 0U, 2000U);
    registry.addUInt32(ParamId::Light_IdleMode, ModuleId::Light, "light.idle_mode", &v.idleMode, 0U, 1U);
    registry.addUInt32(ParamId::Light_FloorPct, ModuleId::Light, "light.floor_pct", &v.floorPct, 0U, 100U);
    registry.addUInt32(ParamId::Light_IdleTopPct, ModuleId::Light, "light.idle_top_pct", &v.idleTopPct, 0U, 100U);
    registry.addUInt32(ParamId::Light_HeardLiftPct, ModuleId::Light, "light.heard_lift_pct", &v.heardLiftPct, 0U, 100U);
    registry.addUInt32(ParamId::Light_HeardLiftMs, ModuleId::Light, "light.heard_lift_ms", &v.heardLiftMs, 0U, 5000U);
    registry.addUInt32(ParamId::Light_HeardTopPct, ModuleId::Light, "light.heard_top_pct", &v.heardTopPct, 0U, 100U);
    registry.addUInt32(ParamId::Light_EmitRampMs, ModuleId::Light, "light.emit_ramp_ms", &v.emitRampMs, 0U, 5000U);
    registry.addUInt32(ParamId::Light_GlowMs, ModuleId::Light, "light.glow_ms", &v.glowMs, 0U, 30000U);
    registry.addUInt32(ParamId::Light_YieldMs, ModuleId::Light, "light.yield_ms", &v.yieldMs, 0U, 30000U);
    registry.addUInt32(ParamId::Light_BreatheMs, ModuleId::Light, "light.breathe_ms", &v.breatheMs, 500U, 60000U);
    registry.addUInt32(ParamId::Light_HueLow, ModuleId::Light, "light.hue_low", &v.hueLow, 0U, 359U);
    registry.addUInt32(ParamId::Light_HueHigh, ModuleId::Light, "light.hue_high", &v.hueHigh, 0U, 359U);
    registry.addUInt32(ParamId::Light_SatPct, ModuleId::Light, "light.sat_pct", &v.satPct, 0U, 100U);
}

}  // namespace param
