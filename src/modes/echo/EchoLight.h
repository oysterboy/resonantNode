#pragma once

#include <math.h>
#include <stdint.h>

#include <ParamRegistry.h>

#include "../../app/ParamIds.h"

/*
EchoLight

The echo shell's light (decisions/2026-11-13-light-colour-model-and-states.md): one hue and saturation per mode, a
steady glow, and for Whispernet the brightness following what it plays. update() takes the loudest output sample
since the last call (0..1 of full scale, after the gain); the brightness jumps up to it and falls back to the glow in
release_ms. The level reads in dB (-48 dBFS dark, 0 dBFS full), so a whisper's replay shows, not only a loud one.
The echo room hands in 0: its glow stays steady.

Pure, header-only; host-tested in test/test_node_light.
Owns: the light.* values of the echo modes and one colour per update.
Does not: know bytes, currents or the cap (src/hal/PixelStrip), or what plays (Whispernet, the echo room's clip).
*/
class EchoLight {
public:
    struct Values {
        bool enabled = true;
        uint32_t maxMa = 500;  // the driver holds it at the build cap (LIGHT_MAX_MA) if set above
        uint32_t hue = 35;
        uint32_t satPct = 70;
        uint32_t glowPct = 30;
        uint32_t releaseMs = 300;
    };

    struct Hsb {
        float hueDeg = 0.0f;
        float sat = 0.0f;
        float bright = 0.0f;
    };

    Values& values() { return _values; }
    const Values& values() const { return _values; }

    Hsb update(unsigned long nowMs, float playedPeak) {
        const unsigned long dt = _started ? (nowMs - _lastMs > 1000UL ? 1000UL : nowMs - _lastMs) : 0UL;
        _started = true;
        _lastMs = nowMs;
        float target = 0.0f;
        if (playedPeak > 1.0f / 256.0f) {
            target = (20.0f * log10f(playedPeak > 1.0f ? 1.0f : playedPeak) + 48.0f) / 48.0f;
        }
        const float fall = _values.releaseMs == 0 ? 1.0f : static_cast<float>(dt) / static_cast<float>(_values.releaseMs);
        _env = target > _env ? target : (_env - fall > target ? _env - fall : target);
        const float glow = static_cast<float>(_values.glowPct) / 100.0f;
        Hsb c;
        c.hueDeg = static_cast<float>(_values.hue);
        c.sat = static_cast<float>(_values.satPct) / 100.0f;
        c.bright = _values.enabled ? glow + (1.0f - glow) * _env : 0.0f;
        return c;
    }

    // light.* of the echo modes; release_ms only where something plays through the light (Whispernet).
    void describeParams(param::ParamRegistry& registry, bool followsPlayback) {
        using param::ModuleId;
        using param::ParamId;
        registry.addBool(ParamId::Light_Enabled, ModuleId::Light, "light.enabled", &_values.enabled);
        registry.addUInt32(ParamId::Light_MaxMa, ModuleId::Light, "light.max_ma", &_values.maxMa, 0U, 2000U);
        registry.addUInt32(ParamId::Light_ModeHue, ModuleId::Light, "light.hue", &_values.hue, 0U, 359U);
        registry.addUInt32(ParamId::Light_ModeSatPct, ModuleId::Light, "light.sat_pct", &_values.satPct, 0U, 100U);
        registry.addUInt32(ParamId::Light_GlowPct, ModuleId::Light, "light.glow_pct", &_values.glowPct, 0U, 100U);
        if (followsPlayback) {
            registry.addUInt32(ParamId::Light_ReleaseMs, ModuleId::Light, "light.release_ms", &_values.releaseMs, 0U,
                               5000U);
        }
    }

private:
    Values _values;
    bool _started = false;
    unsigned long _lastMs = 0;
    float _env = 0.0f;
};
