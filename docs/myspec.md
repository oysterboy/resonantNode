# Resonant Node Refactor Spec (Current Branch)

## Goal

Refine the current chirp-listening node without changing the overall architecture.

Keep:

- HAL → raw hardware primitives
- IO → concrete device logic
- Behavior → state machine / decision logic
- Node → wiring / orchestration

Fix:

1. **Behavior currently depends on IO lifecycle indirectly**
2. **Parameters should be grouped and explicit**
3. **Comments should reflect the architectural thinking**
4. **Add structured debug output (event + value modes)**

Do **not** redesign signal processing right now.  
Do **not** introduce new DSP logic.  
Do **not** change overall behavior model unless required for lifecycle fix.

---

# Architecture

## HAL
Primitive hardware access only.

Examples:
- AnalogInHal
- DigitalOut / PWM primitives

Must NOT know:
- chirps
- behavior
- state machines

---

## IO
Concrete devices built on HAL.

Examples:
- LevelInput
- ChirpOutput

May know:
- signal conditioning
- waveform generation

Must NOT decide:
- when to chirp
- system logic

---

## Behavior
Owns autonomous logic.

Responsibilities:
- activity model
- state machine
- timing (heard delay, cooldown, idle timeout)
- action decisions

Must NOT know:
- pins
- LEDC / PWM
- MOSFET / hardware details

---

## Node
Thin glue layer.

Responsibilities:
- update input
- update behavior
- forward action requests to IO
- forward IO lifecycle events to behavior
- debug output

Must NOT contain:
- signal processing logic
- state machine logic
- waveform generation

---

# Problem 1: Behavior ↔ IO lifecycle coupling

## Current issue

Behavior enters `Chirping` but does not know when chirp finishes without Node detecting it.

Temporary solution:
- `_chirpWasActive` in Node
- edge detection
- `notifyChirpFinished()`

## Required fix

Make lifecycle explicit.

### IO should expose:

- `start()`
- `update()`
- `isActive()`
- optionally: `finished()`

### Behavior should expose:

- `shouldStartChirp()`
- `notifyChirpFinished(unsigned long now)`

### Node should:

```cpp
if (_behavior.shouldStartChirp()) {
    _chirpOutput.start();
}

_chirpOutput.update();

if (chirp finished) {
    _behavior.notifyChirpFinished(now);
}