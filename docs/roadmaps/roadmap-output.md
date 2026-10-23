# Roadmap - Output

Status: future-supporting roadmap.
Scope: SoundOutput / ChirpOutput boundary, later OutputStatus and OutputProfile.
Purpose: keep the current output path understandable while the behavior/output
split stays simple.

---

## Status legend

```text
[LANDED]    Verified in current code.
[PARTIAL]   Present, but not yet in the intended final shape.
[TODO]      Next implementation step.
[DEFERRED]  Intentionally later.
[REMOVED]   No longer part of the active plan.
```

## Architecture goal

```text
Behavior requests output.
SoundOutput / ChirpOutput executes output.
OutputStatus reports availability.
OutputProfile later defines available emitted sound shapes.
```

Landed items from this area now live in `docs/roadmaps/roadmap archive/roadmap-changelog.md`.

## Current code state

```text
[PARTIAL] Behavior has outputBusy state, but no stable OutputStatus object.
[TODO] OutputStatus is not landed.
[TODO] OutputRequest is not landed.
[TODO] OutputProfile / ChirpProfile is not landed.
[TODO] OutputDispatcher is not landed.
```

## Implementation order

### OUT-001 - name the current output path

Status: TODO

```text
Document the current output path as the ChirpOutput path.
Keep output generation unchanged for 5-node tests.
Expose busy / last-start / last-finished only if already cheap.
Keep behavior as the owner of the response decision.
```

### OUT-002 - minimal OutputStatus

Status: DONE 2026-10-23 (step 10f; decisions/2026-10-23-soundoutput-executes-node-composes.md). `OutputStatus` in
src/output/SoundOutput.h: phase, started / done / rejected, last reject, last start / end, span; RB STATUS
`output.*`. The original note:

```text
Add OutputStatus only when behavior/status needs it.
Fields may include busy, lastStartMs, lastDoneMs, currentPattern.
```

### OUT-003 - OutputRequest

Status: DONE 2026-10-23, with one difference to the note below: the Node composes the request from Behavior's
decision and the output.* params (the hop grid comes from the detection band, which Behavior does not know).
`SoundOutput` executes it, sequences the idle call and checks `OutputLimits`. Host test test_sound_output.
The original note:

```text
Behavior emits OutputRequest / BehaviorAction.
Output executes request and validates physical limits.
```

### OUT-004 - OutputProfile

Status: DEFERRED

```text
Add OutputProfile / ChirpProfile only after behavior variations prove needed.
Connect later to ResonantProgram.
```

### OUT-005 - chirp shapes: hopping pulses and in-chirp sweep

Status: in code (step 5b), bench pending. Hop: Emitter `PULSES ... hop=` (2026-10-15, bench-checked),
Node hop train from Behavior's `SignalShape::NetworkSignal` (155211e, compile only). Sweep: `CHIRP sweep=END_HZ`
on the Emitter and `SEQ start sweep=` (9664032, compile only; `I2sToneOutput::setSweep`, phase-continuous).
Design rule from the 2026-10-16 battery: hop steps on the 250 Hz band-bank bins, gaps <= 10 ms, 25 ms x 5.
Sweep on hardware 2026-10-16 (bench/sessions/2026-10-16-sweep-70cm): detected as one occurrence, rejected by the
contrast inspector in 30 of 30 trials (P75 29,000-30,500 while gliding between bins). With the current contrast the
detection-layer shape is the hop; the sweep stays an aesthetic-layer / DET-015 option.

```text
PULSES takes a frequency per pulse (hop train); a tone can ramp its
frequency inside one chirp, phase-continuous (sweep: start/end Hz).
Emitter first, as SEQ trial options, so the Analyzer can drive them;
then ChirpOutput on the Node, with the shape chosen by Behavior (BEH-005).
No OutputRequest needed: parameters on the current ChirpOutput path.
Every shape stays inside the network band
(docs/decisions/2026-10-15-signal-variation-inside-a-shared-band.md).
Source: docs/research/frequency-drift-detection.md (Sequence, step 2).
```

### OUT-006 - two-layer signal: aesthetic layer + detection layer

Status: TODO (step 8a)

```text
Renderer sums two oscillators: an aesthetic layer free in frequency
(owner: interesting below 1 kHz) and a detection layer in the band.
Level budget shared under full scale; speaker resonance near 800 Hz
helps the low layer. Detection layer lasts at least the detector's
minimum duration or forms a pulse train. Bench first: leak of the low
layer into the band (Hann window in DET-013 if needed). Ultrasound as
detection layer considered and dropped 2026-10-15.
Source: docs/research/frequency-drift-detection.md (Two-layer signal).
```

## Current focus

```text
Order: roadmap-0-steps.md. OUT-001 folds into NODE-001 (step 6);
OUT-002/003 done in step 10f (2026-10-23); OUT-004 is deferred (step 11). OUT-005 is step 5b, OUT-006 step 8a. The D-AMP output (a sine ToneOutput
over I2S) is NODE-009, step 2: hardware, not output architecture.
```

## Spec candidates

```text
OutputProfile defines what can be emitted.
Behavior may request concrete values or variants.
SoundOutput executes requested values and may validate limits.
SoundOutput does not decide behavior modulation.
```

## Non-goals now

```text
OutputDispatcher.
OutputProfile.
Advanced synthesizer architecture.
Multi-channel audio engine.
Generic scheduler.
Behavior-owned waveform generation.
Output refactor before the 5-node field trial (NODE-008), unless status
visibility needs it.
```
