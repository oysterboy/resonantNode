# Roadmap — Detection

Status: active roadmap. Scope: future detection work only.

The landed detection architecture belongs in `myspec.md`.

---

## Architecture Goal

Detection should remain a layered pipeline:

```text
AudioSignal / FeatureStreams
→ OccurrenceSources
→ Occurrences
→ OccurrenceInspector
→ InspectedOccurrences
→ PatternAssembler
→ PatternCandidates
→ PatternRules
→ PatternResults
```

Behavior consumes PatternResults, not detector internals.

---

## Spec Candidates

These are stable rules that should later be considered for `myspec.md`:

```text
Occurrence = bounded source-level acoustic happening from one evidence path.
PatternCandidate remains the assembled pattern-level candidate.
Behavior consumes PatternResult, not Occurrence or detector internals.
OccurrenceSource owns its source-specific detector wiring unless per-source config becomes necessary.
```

---

## MVP Guardrail

MVP does not mean throwaway.

Each minimum viable pass should be the smallest useful slice that still follows the intended architecture direction.

Avoid two extremes:

```text
too large:
    empty frameworks, generic registries, factories, unused abstractions

too small / wrong:
    hacks in Node, duplicated logic, temporary APIs, shortcuts that must be removed immediately
```

A good MVP:

```text
uses real current modules
solves a real near-term problem
keeps ownership in the right subsystem
can be extended without rewriting the same boundary
does not create compatibility sediment
```

If the quickest implementation would put logic in the wrong owner, prefer a slightly larger but correctly placed slice.

Rule:

```text
Build the smallest slice you can keep.
```


---

## Current Status

Landed enough for spec:

```text
DetectionProfile v1
DetectionRuntime
FeatureStreams / FeatureHistory
OccurrenceSources after rename
Occurrences after rename
OccurrenceInspector after rename
InspectedOccurrences after rename
PatternAssembler v0
PatternCandidate
PatternRules v0
PatternResult
FieldStateTracker / FieldState v0
FrequencyWindowProbe / raw-history candidate-window features
```

Current target rename:

```text
SignalCandidate        → Occurrence
SignalEmitter          → OccurrenceSource
SignalInspector        → OccurrenceInspector
InspectedSignal        → InspectedOccurrence
FreqAmp profile        → TonalPulse profile
```

---

## Implementation Order

```text
1. Naming cleanup: Occurrence + TonalPulse.
2. Run 5-node TonalPulse detection tests.
3. Use findings to decide whether DetectionProfile cleanup is needed.
4. Only then mature PulseSequence / pulsed chirp grouping.
5. Only later add different detection/pattern families.
```

---

## Minimum Viable First Pass

Goal:

```text
Make current landed detection architecture readable.
```

Do:

```text
SignalCandidate        → Occurrence
SignalEmitter          → OccurrenceSource
SignalInspector        → OccurrenceInspector
InspectedSignal        → InspectedOccurrence
FreqAmp profile        → TonalPulse profile
```

Also:

```text
- update includes/logs/docs
- remove decorative global signalDetector/signalEmitter fields if they are only metadata
- compile
```

Do not:

```text
- change PatternRules
- change thresholds
- change behavior
- add OccurrenceSourceConfig
- add CandidateCorrelator
- mature PulseSequence
```

Success:

```text
Active code and docs no longer use old SignalCandidate / SignalEmitter / FreqAmp names, and behavior is unchanged.
```

---

## Active Future Work

```text
DetectionProfile cleanup / stronger boundaries
profile-specific configuration and switching
full PatternProfile composition if needed
CandidateCorrelator / relation facts
mature PulseSequence / pulsed chirp grouping
```

Important guardrail:

```text
Do not introduce OccurrenceSourceConfig until there are at least two real source/detector combinations that require independent configuration.
```

---

## Possible Extension Profiles / Pattern Families

After current TonalPulse testing:

```text
continuous tonal chirp trajectory
glass chime / resonant decay
woodblock / knock
white-noise / broadband
```

---

## Later Cross-Cutting Work

```text
dense-field ambiguity handling
family matching / profile identity matching
VEKTOR pattern configuration / DESCRIBE exposure
mature FieldState interpretation
final pattern vocabulary cleanup
```

---

## Non-Goals for the First Pass

```text
new detection features
new profiles
candidate correlation
source/detector config framework
legacy compatibility wrappers
behavior changes
```

---

## One-Line Strategy

```text
Rename and test current TonalPulse detection first; add pulsed chirp or different detection only after the 5-node tests show what is needed.
```
