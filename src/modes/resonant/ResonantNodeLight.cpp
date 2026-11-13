#include "ResonantNodeLight.h"

#include <math.h>

namespace {

float clamp01(float x) {
    return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x);
}

// Elapsed ms from `from` to `now`, wrap-safe; 0 when `from` lies ahead.
unsigned long since(unsigned long now, unsigned long from) {
    const long d = static_cast<long>(now - from);
    return d > 0 ? static_cast<unsigned long>(d) : 0UL;
}

}  // namespace

float NodeLight::mixHue(float a, float b, float t) {
    float d = b - a;
    while (d > 180.0f) {
        d -= 360.0f;
    }
    while (d < -180.0f) {
        d += 360.0f;
    }
    float h = a + d * clamp01(t);
    while (h < 0.0f) {
        h += 360.0f;
    }
    while (h >= 360.0f) {
        h -= 360.0f;
    }
    return h;
}

float NodeLight::hueFor(uint32_t low, uint32_t high, uint32_t baseCount, long base) const {
    float t = 0.0f;
    if (baseCount > 1 && base > 0) {
        t = clamp01(static_cast<float>(base) / static_cast<float>(baseCount - 1));
    }
    return mixHue(static_cast<float>(low), static_cast<float>(high), t);
}

float NodeLight::idleBright(const Input& in, unsigned long sinceMs) const {
    const float top = static_cast<float>(_values.idleTopPct) / 100.0f;
    if (_values.idleMode == 0 && in.idleCallCanCome && in.idleAtMs != 0) {
        // The rise starts when idle starts, or later when the idle wait was scheduled after that.
        const unsigned long start = static_cast<long>(in.idleFromMs - _enteredMs) > 0 ? in.idleFromMs : _enteredMs;
        const long span = static_cast<long>(in.idleAtMs - start);
        if (span <= 0) {
            return top;
        }
        return top * clamp01(static_cast<float>(since(in.nowMs, start)) / static_cast<float>(span));
    }
    const float period = static_cast<float>(_values.breatheMs > 0 ? _values.breatheMs : 1U);
    const float phase = static_cast<float>(sinceMs % (_values.breatheMs > 0 ? _values.breatheMs : 1U)) / period;
    return top * (0.5f - 0.5f * cosf(6.2831853f * phase));
}

NodeLight::Hsb NodeLight::update(const Input& in) {
    if (!_started) {
        _started = true;
        _phase = in.phase;
        _enteredMs = in.nowMs;
        _lastMs = in.nowMs;
        _brightAtEntry = 0.0f;
        _blend = in.phase == Phase::Idle ? 0.0f : 1.0f;
        _last = {};
    }
    if (in.phase != _phase) {
        _phase = in.phase;
        _enteredMs = in.nowMs;
        _brightAtEntry = _last.bright;
    }
    unsigned long dt = since(in.nowMs, _lastMs);
    if (dt > 1000UL) {
        dt = 1000UL;  // a stalled loop does not jump the fade
    }
    _lastMs = in.nowMs;

    // Colour: A while idle, B otherwise, faded over fade_ms.
    const float target = _phase == Phase::Idle ? 0.0f : 1.0f;
    const float step = _values.fadeMs == 0 ? 1.0f : static_cast<float>(dt) / static_cast<float>(_values.fadeMs);
    _blend = _blend < target ? (_blend + step > target ? target : _blend + step)
                             : (_blend - step < target ? target : _blend - step);

    const long ownBase = static_cast<long>(in.ownBase);
    const long bBase = _values.hueSource == 1 && in.heardBase >= 0 ? in.heardBase : ownBase;
    const float hueA = hueFor(_values.hueALow, _values.hueAHigh, in.baseCount, ownBase);
    const float hueB = hueFor(_values.hueBLow, _values.hueBHigh, in.baseCount, bBase);
    const float satA = static_cast<float>(_values.satAPct) / 100.0f;
    const float satB = static_cast<float>(_values.satBPct) / 100.0f;

    // Brightness over the phase.
    const unsigned long inPhase = since(in.nowMs, _enteredMs);
    const float fallPerMs = _values.fallMs == 0 ? 1.0f : 1.0f / static_cast<float>(_values.fallMs);
    const float falling = clamp01(_brightAtEntry - fallPerMs * static_cast<float>(inPhase));
    float bright = 0.0f;
    switch (_phase) {
        case Phase::Idle: {
            const float rise = idleBright(in, inPhase);
            bright = falling > rise ? falling : rise;  // the fall from before meets the rise
            break;
        }
        case Phase::Heard: {
            const float top = static_cast<float>(_values.heardTopPct) / 100.0f;
            const long span = static_cast<long>(in.answerAtMs - _enteredMs);
            const float t =
                span <= 0 ? 1.0f : clamp01(static_cast<float>(inPhase) / static_cast<float>(span));
            bright = _brightAtEntry + (top - _brightAtEntry) * t;
            break;
        }
        case Phase::Emitting: {
            const float t = _values.emitRampMs == 0
                ? 1.0f
                : clamp01(static_cast<float>(inPhase) / static_cast<float>(_values.emitRampMs));
            bright = _brightAtEntry + (1.0f - _brightAtEntry) * t;
            break;
        }
        case Phase::Afterglow:
            bright = falling;
            break;
    }

    _last.hueDeg = mixHue(hueA, hueB, _blend);
    _last.sat = satA + (satB - satA) * _blend;
    _last.bright = _values.enabled ? clamp01(bright) : 0.0f;
    return _last;
}
