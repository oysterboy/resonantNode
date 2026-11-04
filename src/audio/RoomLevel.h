#pragma once

#include <stdint.h>

/*
RoomLevel (ECHO-002, 2026-11-04)

The room's quiet level, from the envelope: it follows a drop quickly (fallMs) and a rise slowly (riseMs), so a
whisper, a clap or a voice barely lifts it while the afternoon's louder room does over a few seconds; while not
learning (a phrase is open, the node plays) it holds. Whispernet's thresholds stand on it: on the desk
(2026-11-01) the quiet room read 40-100 in the morning and 150-260 in the afternoon, and fixed thresholds either
lost the 2 m whispers or answered the room. Pure, host-tested (test_room_level).

Owns: the estimate.
Does not: decide the thresholds (Whispernet), or measure the envelope (EnvelopeFollower).
*/
class RoomLevel {
public:
    void setTimes(uint32_t riseMs, uint32_t fallMs) {
        _riseMs = riseMs > 0 ? riseMs : 1;
        _fallMs = fallMs > 0 ? fallMs : 1;
    }
    void reset(float level) {
        _value = level;
        _started = true;
    }

    // One block's level and length; learning false holds the estimate.
    void update(float level, uint32_t blockMs, bool learning) {
        if (!_started) {
            reset(level);
            return;
        }
        if (!learning) {
            return;
        }
        const uint32_t tau = level > _value ? _riseMs : _fallMs;
        float k = static_cast<float>(blockMs) / static_cast<float>(tau);
        k = k > 1.0f ? 1.0f : k;
        _value += (level - _value) * k;
    }

    float value() const { return _value; }
    bool started() const { return _started; }

private:
    uint32_t _riseMs = 10000;
    uint32_t _fallMs = 500;
    float _value = 0.0f;
    bool _started = false;
};
