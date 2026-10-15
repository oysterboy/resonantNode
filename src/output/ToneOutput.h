#pragma once

#include <Arduino.h>

/*
ToneOutput

Minimal hardware output interface for tone-capable sound emitters.
Behavior and ChirpOutput call this interface; implementations own GPIO/PWM/I2S details.
*/
class ToneOutput {
public:
    virtual ~ToneOutput() = default;

    virtual void begin() = 0;
    virtual void setToneHz(uint32_t toneHz) = 0;
    virtual void toneOn() = 0;
    virtual void toneOff() = 0;

    // Optional: a train of `count` pulses (onMs of sound, rampMs edge ramps
    // inside it, gapMs of silence) timed by the device itself, for edges
    // finer than the caller's loop can place. Returns false when the device can't; the caller then
    // times the train with toneOn/toneOff. toneOff() cancels a running train.
    virtual bool startPulseTrain(uint32_t count, uint32_t onMs, uint32_t gapMs, uint32_t rampMs) {
        (void)count;
        (void)onMs;
        (void)gapMs;
        (void)rampMs;
        return false;
    }
    // Optional: a frequency per pulse for the NEXT startPulseTrain(), cycled
    // when the train has more pulses than entries (hop). n = 0 clears it.
    // A device that can't hop ignores it and plays the train at setToneHz().
    virtual void setPulseHop(const uint32_t* hz, size_t n) {
        (void)hz;
        (void)n;
    }
    // True from startPulseTrain() until the last pulse has ramped to silence.
    virtual bool pulseTrainActive() const { return false; }
};
