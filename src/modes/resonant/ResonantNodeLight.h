#pragma once

#include <stdint.h>

#include <ParamRegistry.h>

#include "../../app/ParamIds.h"

/*
NodeLight

The Node's light effects (OUT-007, decisions/2026-11-13-light-colour-model-and-states.md): what the strip shows,
composed from Behavior's state the way NodeSignalComposer composes the sound. The state picks colour A (idle) or B
(heard, emitting, afterglow) and the brightness over time; the base bins set the hues. Out comes hue, saturation
and a perceived brightness 0..1; the driver (src/hal/PixelStrip) turns that into bytes under the cap.

  idle       A, B rises from 0 to idle_top, reaching it when the idle call is due (idle_mode 0); a slow breath
             instead when idle_mode is 1 or no call can come (call off, outputs off, resting)
  heard      fade A -> B (fade_ms), B from where it is to heard_top when the answer starts
  emitting   B, ramp to 1.0 in emit_ramp_ms
  afterglow  B, falls to 0 (1.0 -> 0 takes fall_ms); idle takes over at the same rate, so a yield ramps down too

Pure: no Arduino, no device; host-tested in test/test_node_light. The light.* params bind to `values()`.
Owns: the light.* effect values and the composition of one colour per update.
Does not: decide anything (Behavior), know bytes, currents or the cap (the driver), or drive a pin.
*/
class NodeLight {
public:
    enum class Phase : uint8_t {
        Idle,
        Heard,
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
        uint32_t baseCount = 0;  // the base bins (BEH-009); 0 or 1: one hue per colour
        uint32_t ownBase = 0;
        long heardBase = -1;  // -1: none heard
    };

    struct Hsb {
        float hueDeg = 0.0f;
        float sat = 0.0f;
        float bright = 0.0f;
    };

    // light.*: first guesses for the room, set there by eye. Hues in degrees, the low end of each range at the
    // lowest base. hueSource 0: both colours from the node's own base; 1: colour B from the base last heard.
    struct Values {
        bool enabled = true;
        uint32_t maxMa = 500;  // the driver holds it at the build cap (LIGHT_MAX_MA) if set above
        uint32_t idleMode = 0;
        uint32_t hueSource = 0;
        uint32_t idleTopPct = 50;
        uint32_t heardTopPct = 50;
        uint32_t fadeMs = 100;
        uint32_t emitRampMs = 150;
        uint32_t fallMs = 1500;
        uint32_t breatheMs = 6000;
        uint32_t hueALow = 190;
        uint32_t hueAHigh = 250;
        uint32_t hueBLow = 20;
        uint32_t hueBHigh = 50;
        uint32_t satAPct = 80;
        uint32_t satBPct = 100;
    };

    Values& values() { return _values; }
    const Values& values() const { return _values; }

    // The colour at in.nowMs; call it often (the Node: every 20 ms). The first call starts dark.
    Hsb update(const Input& in);
    // Back to dark, as at boot (LIGHT auto after a manual LIGHT).
    void reset() { _started = false; }
    const Hsb& last() const { return _last; }

    // A hue on the way from a to b by t (0..1), the short way round.
    static float mixHue(float a, float b, float t);

private:
    float hueFor(uint32_t low, uint32_t high, uint32_t baseCount, long base) const;
    float idleBright(const Input& in, unsigned long sinceMs) const;

    Values _values;
    bool _started = false;
    Phase _phase = Phase::Idle;
    unsigned long _enteredMs = 0;
    unsigned long _lastMs = 0;
    float _brightAtEntry = 0.0f;
    float _blend = 0.0f;  // 0: colour A, 1: colour B
    Hsb _last;
};

namespace param {

// light.*: the Node's light effects (NodeLight::Values). light.max_ma lowers the driver's build-time cap.
inline void describeParams(ParamRegistry& registry, NodeLight::Values& v) {
    registry.addBool(ParamId::Light_Enabled, ModuleId::Light, "light.enabled", &v.enabled);
    registry.addUInt32(ParamId::Light_MaxMa, ModuleId::Light, "light.max_ma", &v.maxMa, 0U, 2000U);
    registry.addUInt32(ParamId::Light_IdleMode, ModuleId::Light, "light.idle_mode", &v.idleMode, 0U, 1U);
    registry.addUInt32(ParamId::Light_HueSource, ModuleId::Light, "light.hue_source", &v.hueSource, 0U, 1U);
    registry.addUInt32(ParamId::Light_IdleTopPct, ModuleId::Light, "light.idle_top_pct", &v.idleTopPct, 0U, 100U);
    registry.addUInt32(ParamId::Light_HeardTopPct, ModuleId::Light, "light.heard_top_pct", &v.heardTopPct, 0U, 100U);
    registry.addUInt32(ParamId::Light_FadeMs, ModuleId::Light, "light.fade_ms", &v.fadeMs, 0U, 5000U);
    registry.addUInt32(ParamId::Light_EmitRampMs, ModuleId::Light, "light.emit_ramp_ms", &v.emitRampMs, 0U, 5000U);
    registry.addUInt32(ParamId::Light_FallMs, ModuleId::Light, "light.fall_ms", &v.fallMs, 0U, 30000U);
    registry.addUInt32(ParamId::Light_BreatheMs, ModuleId::Light, "light.breathe_ms", &v.breatheMs, 500U, 60000U);
    registry.addUInt32(ParamId::Light_HueALow, ModuleId::Light, "light.hue_a_low", &v.hueALow, 0U, 359U);
    registry.addUInt32(ParamId::Light_HueAHigh, ModuleId::Light, "light.hue_a_high", &v.hueAHigh, 0U, 359U);
    registry.addUInt32(ParamId::Light_HueBLow, ModuleId::Light, "light.hue_b_low", &v.hueBLow, 0U, 359U);
    registry.addUInt32(ParamId::Light_HueBHigh, ModuleId::Light, "light.hue_b_high", &v.hueBHigh, 0U, 359U);
    registry.addUInt32(ParamId::Light_SatAPct, ModuleId::Light, "light.sat_a_pct", &v.satAPct, 0U, 100U);
    registry.addUInt32(ParamId::Light_SatBPct, ModuleId::Light, "light.sat_b_pct", &v.satBPct, 0U, 100U);
}

}  // namespace param
