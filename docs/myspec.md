## Status

- [x] Behavior owns state machine
- [x] Chirp waveform isolated in IO
- [x] Node reduced to thin glue
- [x] Chirp lifecycle feedback made explicit (IO -> Node -> Behavior)
- [~] Parameter grouping (basic, may refine)
- [x] Comments aligned with architecture
- [x] Debug system added in Node (event mode + plotter-friendly value mode)
- [x] Behavior state exposed for debug (`stateName()` / `stateCode()`)

Notes:
- Lifecycle feedback is explicit via the IO interface
- Behavior logic remains unchanged
- Debug defaults are plotter-friendly (`_debugPlot = true`, `_debugEvents = false`)

# Resonant Node Refactor Spec

## Goal

Keep refining the chirp-listening node without changing the overall architecture.

Keep:

- HAL -> raw hardware primitives
- IO -> concrete device logic
- Behavior -> state machine / decision logic
- Node -> wiring / orchestration

Current focus:

1. **Parameters should be grouped and explicit**

Do not:

- redesign signal processing
- introduce new DSP logic
- change behavior logic unless explicitly required

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

# Next Steps

## Parameter cleanup

Continue improving parameter clarity while keeping logic unchanged.

Priorities:
- keep grouped headings clear
- prefer readable names over abbreviations
- keep timing and threshold intent obvious
- avoid introducing a large config struct in this pass

Possible follow-up:
- decide later whether parameters should become externally configurable
