#pragma once

#include <Arduino.h>

/*
ToneOutput

Minimal hardware output interface for tone-capable sound emitters.
Behavior and ChirpOutput call this interface; implementations own GPIO/PWM/I2S details.
Does not: decide what or when to play (SoundOutput and ChirpOutput do).
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
    // Optional: a gain per pulse for the NEXT startPulseTrain() (1.0 = the device's level), cycled like the hop;
    // n = 0 clears it. For the Emitter's level ladder and per-hop attenuation (the margin test); a device that can't
    // scale ignores it and plays every pulse at its level.
    virtual void setPulseGains(const float* gain, size_t n) {
        (void)gain;
        (void)n;
    }
    // Optional: the first `pulses` pulses of the NEXT startPulseTrain() are each followed by gapMs instead of the
    // train's gap (the trill before a gapless climb); pulses = 0 clears it. A device that can't ignores it and
    // plays every gap at the train's gap.
    virtual void setPulseLeadGap(uint32_t pulses, uint32_t gapMs) {
        (void)pulses;
        (void)gapMs;
    }
    // Optional (step 24.1): a burst of high-passed noise at the start of the NEXT train's pulses, levelPct of the
    // tone's peak (0 = none, the device's output is then unchanged), on every pulse or on the trill's only, above
    // lowHz so it stays out of the detection band. A device that can't ignores it. Cleared by levelPct = 0.
    virtual void setPulseBroadband(uint32_t levelPct, bool trillOnly, uint32_t lowHz) {
        (void)levelPct;
        (void)trillOnly;
        (void)lowHz;
    }
    // Optional (24.4.1): the NEXT train's trill pulses shaped like a cricket's (output/TrillShape.h): fade, pitch
    // fall, swell; all 0 = the plain trill. A device that can't ignores it.
    virtual void setPulseTrillShape(uint32_t fadeMs, uint32_t droopHz, uint32_t swellPulses) {
        (void)fadeMs;
        (void)droopHz;
        (void)swellPulses;
    }
    // True from startPulseTrain() until the last pulse has ramped to silence.
    virtual bool pulseTrainActive() const { return false; }
    // Optional: a sweep inside a plain tone (OUT-005). From the next toneOn()
    // the frequency glides linearly from setToneHz() to endHz over durationMs,
    // phase-continuous, then holds endHz. endHz = 0 or durationMs = 0 clears
    // it. A device that can't sweep plays the steady tone. Pulse trains
    // ignore it (a hop is the train's own shape).
    virtual void setSweep(uint32_t endHz, uint32_t durationMs) {
        (void)endHz;
        (void)durationMs;
    }
    // Optional (ECHO-001): plays `count` int16 samples at the device's rate, scaled by gain, with short ramps in and
    // out; the caller keeps the samples untouched until clipActive() is false. toneOff() cuts it. False: the device
    // can't (the piezo LEDC output).
    virtual bool startClip(const int16_t* samples, size_t count, float gain) {
        (void)samples;
        (void)count;
        (void)gain;
        return false;
    }
    virtual bool clipActive() const { return false; }
};
