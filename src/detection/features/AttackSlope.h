#pragma once

#include <stdint.h>

namespace detection {

/*
AttackSlope (ECHO-001, step 20.2)

How fast the level rose: the time from the level last sitting at or below floorLevel to it crossing onsetLevel
upward. A clap or a knock crosses in a millisecond or two, a voice swelling or a door scraping in tens
(docs/research/echo-room.md, "Audio quality first, a simpler detector"). Fed once per DMA block with the block's
envelope in Strength16 units; pure, host-tested (test_echo_detection).

Owns: the last-quiet time and whether the level is above the onset.
Does not: decide whether the rise makes a clap (makeClapVerdict does), or track the occurrence (the scalar detector).
*/
class AttackSlope {
public:
    void configure(float floorLevel, float onsetLevel) {
        _floor = floorLevel;
        _onset = onsetLevel;
    }

    void reset() {
        _haveQuiet = false;
        _above = false;
    }

    // The rise time in ms when this block crosses the onset upward, else -1. -1 too when the level was never quiet
    // since reset() (a sound already running at boot or after the own replay has no measurable attack).
    long update(float level, uint32_t nowMs) {
        long attack = -1;
        if (level <= _floor) {
            _quietMs = nowMs;
            _haveQuiet = true;
        }
        const bool above = level >= _onset;
        if (above && !_above && _haveQuiet) {
            attack = static_cast<long>(static_cast<uint32_t>(nowMs - _quietMs));
        }
        _above = above;
        return attack;
    }

private:
    float _floor = 0.0f;
    float _onset = 0.0f;
    uint32_t _quietMs = 0;
    bool _haveQuiet = false;
    bool _above = false;
};

}  // namespace detection
