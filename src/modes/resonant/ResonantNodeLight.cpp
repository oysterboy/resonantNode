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

float pct(uint32_t p) {
    return clamp01(static_cast<float>(p) / 100.0f);
}

float maxOf(float a, float b) {
    return a > b ? a : b;
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

float NodeLight::idleBright(const Input& in, unsigned long sinceMs, float floor) const {
    const float top = maxOf(pct(_values.idleTopPct), floor);
    if (_values.idleMode == 0 && in.idleCallCanCome && in.idleAtMs != 0) {
        // The rise starts when idle starts, or later when the idle wait was scheduled after that.
        const unsigned long start = static_cast<long>(in.idleFromMs - _enteredMs) > 0 ? in.idleFromMs : _enteredMs;
        const long span = static_cast<long>(in.idleAtMs - start);
        if (span <= 0) {
            return top;
        }
        return floor + (top - floor) * clamp01(static_cast<float>(since(in.nowMs, start)) / static_cast<float>(span));
    }
    const uint32_t period = _values.breatheMs > 0 ? _values.breatheMs : 1U;
    const float phase = static_cast<float>(sinceMs % period) / static_cast<float>(period);
    return floor + (top - floor) * (0.5f - 0.5f * cosf(6.2831853f * phase));
}

float NodeLight::heardBright(const Input& in, unsigned long sinceMs) const {
    const float lift = pct(_values.heardLiftPct);
    const float top = maxOf(pct(_values.heardTopPct), lift);
    const float from = _brightAtEntry < lift ? _brightAtEntry : lift;
    if (sinceMs < _values.heardLiftMs) {
        return from + (lift - from) * static_cast<float>(sinceMs) / static_cast<float>(_values.heardLiftMs);
    }
    const unsigned long buildFrom = _enteredMs + _values.heardLiftMs;
    const long span = static_cast<long>(in.answerAtMs - buildFrom);
    if (span <= 0) {
        return top;
    }
    return lift + (top - lift) * clamp01(static_cast<float>(since(in.nowMs, buildFrom)) / static_cast<float>(span));
}

float NodeLight::fall(unsigned long nowMs, float floor) const {
    if (_fallMs == 0) {
        return floor;
    }
    const float t = clamp01(static_cast<float>(since(nowMs, _fallStartMs)) / static_cast<float>(_fallMs));
    const float rest = (1.0f - t) * (1.0f - t);
    return floor + (maxOf(_fallFrom, floor) - floor) * rest;
}

void NodeLight::startFall(unsigned long nowMs, uint32_t durationMs) {
    _fallFrom = _last.bright;
    _fallStartMs = nowMs;
    _fallMs = durationMs > 0 ? durationMs : 1U;
}

NodeLight::Hsb NodeLight::update(const Input& in) {
    const float floor = pct(_values.floorPct);
    if (!_started) {
        _started = true;
        _phase = in.phase;
        _enteredMs = in.nowMs;
        _brightAtEntry = floor;
        _fallMs = 0;
        _last = {};
        _last.bright = floor;
    }
    if (in.phase != _phase) {
        const Phase before = _phase;
        _phase = in.phase;
        _enteredMs = in.nowMs;
        _brightAtEntry = _last.bright;
        if (in.phase == Phase::Afterglow || (in.phase == Phase::Idle && before == Phase::Emitting)) {
            startFall(in.nowMs, _values.glowMs);
        } else if (in.phase == Phase::Idle && (before == Phase::Heard || before == Phase::Announce)) {
            startFall(in.nowMs, _values.yieldMs);
        } else if (in.phase == Phase::Emitting) {
            _fallMs = 0;
        }
        // Idle after the afterglow, and a heard during it, keep the fall running underneath.
    }

    const unsigned long inPhase = since(in.nowMs, _enteredMs);
    const float falling = fall(in.nowMs, floor);
    float bright = floor;
    switch (_phase) {
        case Phase::Idle:
            bright = maxOf(falling, idleBright(in, inPhase, floor));  // the fall from before meets the rise
            break;
        case Phase::Heard:
            bright = maxOf(falling, heardBright(in, inPhase));
            break;
        case Phase::Announce:
            bright = maxOf(falling, _brightAtEntry);
            break;
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

    float t = 0.0f;
    if (in.baseCount > 1) {
        t = clamp01(static_cast<float>(in.ownBase) / static_cast<float>(in.baseCount - 1));
    }
    _last.hueDeg = mixHue(static_cast<float>(_values.hueLow), static_cast<float>(_values.hueHigh), t);
    _last.sat = pct(_values.satPct);
    _last.bright = _values.enabled ? clamp01(maxOf(bright, floor)) : 0.0f;
    return _last;
}
