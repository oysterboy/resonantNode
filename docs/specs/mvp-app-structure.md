# ResonantNode MVP App Structure — Detect Simple, Emit Simple

Status: descriptive spec / MVP reference.
Scope: the smallest useful slice of `myspec.md`'s runtime chain that still
detects a sound event with a very simple algorithm and emits sound in
response to a very simple own rule.
Relationship to `myspec.md`: this is not a different architecture. It is
`myspec.md`'s chain with every stage that isn't required for "detect one
kind of event, react to it" left out or reduced to a pass-through. Per
`changelog.md`'s guardrail: MVP means the smallest useful slice within the
intended architecture direction, not a throwaway parallel structure.

---

## 1. One-sentence definition

Listen for a loud-enough, short-enough sound; if one occurs and the node
isn't in cooldown, beep back.

---

## 2. Chain (MVP subset of the full runtime chain)

```text
AudioSourceI2S      mic samples in (HAL, reused as-is)
→ AudioSignal        baseline/centering (reused as-is)
→ AmpEnvelope         one scalar feature: rectified + smoothed amplitude
→ SimpleThresholdDetector   onset/hold/release over a fixed threshold
→ Occurrence          minimal: startMs, peakMs, endMs, strength
→ Behavior            a handful of if/else rules + a refractory timer
→ SoundOutput.emitBeep()   PiezoToneOutput (HAL, reused as-is)
```

Stages dropped from the full chain, and why they're safe to drop for MVP:

```text
FrequencyMatchDetector   not needed: no target-frequency matching, only "loud burst"
Inspector                no secondary evidence to gather; strength is already on Occurrence
PatternMatcher           single-occurrence pattern only; PatternResult.valid := true whenever
                         an Occurrence was accepted, no sequence/multi-pulse logic
FieldStateTracker        no acoustic-context-driven behavior yet; Behavior reads Occurrence directly
Analyzer/SEQ reporting   trial/pass classification is a test-harness concern, not runtime
```

Everything on the "dropped" list is an additive, backward-compatible layer
in the full architecture — none of it needs to be un-built to grow the MVP
back into the full chain later; they just aren't wired in yet.

---

## 3. Detection: the very simple algorithm

One scalar feature, one detector, no frequency analysis, no FFT/Goertzel.

```text
1. rectify each incoming centered PCM sample: |sample|
2. envelope = exponential moving average of the rectified sample
   envelope += (|sample| - envelope) >> k      // k = smoothing shift, e.g. 4-6
3. onset:   envelope crosses above onThreshold for >= minOnsetMs
4. release: envelope drops below offThreshold (offThreshold < onThreshold, hysteresis)
            or maxDurationMs elapses (safety cap)
5. accept:  onset..release duration is within [minDurationMs, maxDurationMs]
            -> emit Occurrence{ startMs, peakMs, endMs, strength=peakEnvelope }
6. reject:  too short (noise spike) or too long (continuous sound, not a burst)
7. cooldownMs after any accept/reject before a new onset can start
            (merges the tail of one physical event into one Occurrence)
```

This is exactly `ScalarTransientDetector`'s shape already in the codebase
(`src/detection/detectors/scalar/ScalarTransientDetector.h`), run over
`AmpEnvelope` alone, with `TargetBand`/frequency-domain inputs and the
Inspector step left disconnected. No new detector class is required for the
MVP — it's an existing detector, wired with one input and no downstream
inspection/pattern stages.

Tunable knobs (all already exist as config on the scalar detector):

```text
onThreshold / offThreshold   Strength16 (0..32767)
minOnsetMs                   debounce against 1-sample spikes
minDurationMs / maxDurationMs  shape gate: burst vs. continuous noise
cooldownMs                   merge window for one physical event
```

---

## 4. Emission: simple own rules

Behavior owns the reaction. For MVP this is a short, explicit rule list, not
a rules engine:

```text
On accepted Occurrence:
  if now - lastEmitMs < refractoryMs:
      ignore (self-suppression; don't react to your own echo or in rapid bursts)
  else:
      emitBeep(durationMs, centerFreqHz ± smallRandomJitterHz, gain)
      lastEmitMs = now
```

"Own rules" = whatever small policy the node should express, layered on top
of that base reaction, e.g.:

```text
- probability p < 1.0 of responding at all (node doesn't answer every time)
- duration/frequency drift so repeated responses aren't identical
- longer refractory after N responses in a row (avoid feedback loops with
  its own or a neighboring node's emission)
```

These rules live in `Behavior`, not in the detector and not in
`SoundOutput` — per `myspec.md` §6, `SoundOutput` only executes
`emitBeep(duration, frequency, amplitude)`; it does not decide when or how
to vary it. This keeps the MVP forward-compatible: growing from "one
if/else" to `ResonantBehavior`'s fuller policy later is additive, not a
rewrite.

---

## 5. Minimal file/module footprint

Reusing existing HAL and output code, the MVP-specific pieces are small:

```text
HAL (reused):        AudioSourceI2S, PiezoToneOutput
Signal (reused):     AudioSignal (baseline/centering)
Feature (reused):    AmpEnvelope producer
Detector (reused, single-input): ScalarTransientDetector
Occurrence (reused): generic core fields only (no typed detail needed)
Behavior (new/minimal): refractory timer + probability/jitter rules,
                     ~1 small class or even a function in main loop
Output (reused):     SoundOutput::emitBeep()
```

No new detector, no new HAL driver, no new output primitive — the MVP is a
*wiring reduction* of the existing `TonalPulseScalar`/`AmpExperimental`
profile shape (`myspec.md` §5.1), minus Inspector/PatternMatcher/FieldState,
plus a small explicit Behavior rule set instead of `ResonantBehavior`'s full
policy surface.

---

## 6. Non-goals (explicitly out of scope for MVP)

```text
frequency-selective detection (FrequencyMatchDetector / Goertzel)
multi-pulse / sequence pattern matching
field-state / acoustic-context-driven behavior
runtime param persistence (PARAM SET + SAVE/LOAD)
VEKTOR fleet exposure
analyzer/SEQ_TRIAL trial reporting
```

Each of these is a documented future roadmap item (`docs/roadmaps/`) and
slots into the same chain without restructuring it.
