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

As of 2026-10-29 (step 12.3; was 2026-10-26):

```text
[LANDED] OutputRequest vocabulary (src/output/OutputRequest.h: ChirpPattern, OutputRequest,
          OutputLimits, OutputStatus), composed by the Node, executed by SoundOutput on
          ChirpOutput (OUT-003, step 10f).
[LANDED] OutputStatus in RB STATUS output.* (OUT-002, step 10f).
[LANDED] Behavior's outputBusy is read from its Chirping state, no separate mirror
          (review D2.9, step 11g batch D, done 2026-10-28; SoundOutput's phase is output.phase).
[LANDED] Hop train and in-chirp sweep (OUT-005); the Node sounds the hop train.
[TODO] OutputProfile / ChirpProfile is not landed (OUT-004, deferred).
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

### OUT-004 - OutputProfile

Status: DEFERRED
Research: docs/research/node-structure.md (proposed Node classes and files; its A7 cut became step 11i, done 2026-10-29; the later seams are still proposals).

```text
Add OutputProfile / ChirpProfile only after behavior variations prove needed.
Connect later to ResonantProgram.
```

### OUT-006 - two-layer signal: aesthetic layer + detection layer

Status: TODO; sequenced 2026-11-06 as the sound layer before the performance (roadmap-0-steps.md step 24,
2026-12-01 .. 12-06): the aesthetic layer only, detection unchanged (decisions/2026-11-06-sound-layer-before-the-
performance.md)

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
Now (2026-10-26): no output step is sequenced; OUT-006 waits on step 8a, OUT-004 on step 11.
Now (2026-10-29, 12.3): OUT-005 done (5b); OUT-004 and OUT-006 (8a, after step 19's decision) are in
roadmap-0-steps.md "After the performance".
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
