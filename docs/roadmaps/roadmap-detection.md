# Roadmap - Detection and Analyzer

Status: active roadmap.
Scope: detection pipeline cleanup and analyzer follow-up work.
Purpose: keep detection and analyzer work together where the same runtime
facts are shared.

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
Detection produces facts.
Analyzer reports and classifies trials.
makeVerdict(), the last step of inspection, decides pattern meaning.
Behavior consumes OccurrenceVerdict and FieldState.
Clean analyzer output should read canonical runtime contracts only.
```

Landed items from this area now live in `docs/roadmaps/roadmap archive/roadmap-changelog.md`.

## Current code state

As of 2026-10-29 (step 12.3; was 2026-10-26):

```text
[REMOVED] The old analyzer legacy compatibility (the 2026-05 DetectionDiagnostics) left src.
[LANDED] DetectionDiagnostics, new shape (step 11e item 7, eda1397): the Analyzer's side
          channel out of DetectionRuntime, Null variant on the Node, Null-vs-diag replay
          diff in CI (myspec 5.5).
[LANDED] Inspection: OccurrenceInspector with two module kinds (MagnitudeFeatureStrength,
          BinShape); the shipped TonalPulseScalar plan runs three modules (contrast, amp,
          shape; shape required since a33f293, step 11b / DET-015).
[LANDED] Band bank in FreqBandStream, 2950-3950 Hz (DET-013, step 5b; the band stays after
          the 11h sweep) and BinTrack for the shape inspector (DET-015, step 11b); the
          shape knobs are detection.shape_* params (step 11g B2).
[LANDED] One detector family, ScalarTransientDetector (step 7d); family-as-build-flag is
          step 9, not built.
[LANDED] OccurrenceEvaluator folded into inspection 2026-10-22 (step 8, cleanup-0-plan.md
          Phase 7c): makeVerdict() renders the single-occurrence verdict;
          OccurrenceVerdict stays the Behavior-facing type. The Evaluator
          wording further down (ANA-002, the pattern-stage notes) predates it.
[REMOVED] Frequency reason handling (string-backed): FrequencyMatch removed 2026-10-22 (step 7d).
```

## Implementation order

### DET-006 - move detector-specific Analyzer output detail into detection-side report printer/name helpers

Status: TODO

Goal: adding a new detector should require changes in:

detector class
detector config/profile wiring
detector report detail definition/printer

but not in Analyzer core report assembly.

### DET-012 - lower edge of the network band

Status: RESCOPED 2026-10-15 (was "lower chirp frequencies"; step 5b). 2026-10-29: measured with the band stream in
the 11h sweep (E004): the band stays at 2950-3950 (decisions/2026-10-28-band-stays-after-the-sweep.md); a wider band
2950-4700 is BEH-009 (step 17).

Below 1 kHz is wanted for aesthetics, not detection: it moves to the aesthetic layer of a two-layer signal (OUT-006, `docs/decisions/2026-10-15-signal-variation-inside-a-shared-band.md`). What stays here: where the network band's lower edge can sit. 2026-10-14 single tones: speech/music valid 0/0 at 3200, 4/1 at 2800, 3/4 at 2600, 3/4 at 2000, 1/30 at 1500 Hz (50 windows each, `bench/sessions/2026-10-14-lowfreq-90cm-noise`, `-midfreq-90cm-noise`). So 2500-3000 needs DET-014 first; otherwise the band sits around 3000-3600. Re-measure with the band stream of DET-013, not a single bin.

Depends on step 5a item 2b (DC blocker, landed bfbe0c8).

### DET-014 - tonality inspector

Status: TODO, carried over (roadmap-0-steps.md "Carried over", 2026-10-29; not needed in 5b: OBS did not get worse)

Peak-bin power vs total window energy over the occurrence. Pays for the noise a wider band admits; the first candidate before a lower band edge (DET-012) is tried. Data path: per-occurrence accumulation in the detector first, a `FeatureHistory` slot only if needed (3 slots, ~5 KB each).

Research: docs/research/frequency-drift-detection.md (Inspectors that help)

### DET-018 - "valid pattern" vocabulary and vestigial verdict enum values

Status: DEFERRED (from the archived cleanup plan's Phase 7a / 7b, 2026-10-25); 7b done 2026-10-26 (step 11e item 3,
PR #64), 7a open. 7b landed in step 11e item 3
(docs/refactors/archive/review-fixes-2026-10-25.md; enumerators assigned nowhere, Behavior's dead Ambiguous branch);
7a (the SEQ labels, a tools update) stays here.

```text
7a: identifiers and SEQ labels that call a valid verdict a "pattern" (AnalyzerReason::ValidPattern*,
pattern_valid_trials=, ...) renamed to verdict / detection wording; SEQ label changes reach
tools/bench and bench/index.csv, so they ship with a tools update.
7b: enum values assigned nowhere (VerdictType Duplicate/Unexpected/Ambiguous, the Freq-era reason and
reject codes) removed, with Behavior's dead ignore branch.
Detail and the audit tables: docs/refactors/archive/cleanup-0-plan.md, "Phase 7".
Pick up with DET-016 (the verdict boundary rule touches the same names).
```

### DET-019 - the amp class is the first gate to fail as the level drops

Status: PLANNED (2026-10-30, from lab E005; a tuning pass, not a side change)

```text
E005 (level ladder at 79 cm): 20 / 20 down to -3 dB, 11 / 20 at -6 dB, and every reject at -6 dB is the amp
inspection (observed weak, medium required) while the shape is still strong. Then coverage, then duration.
The amp medium limit (detection.amp_medium, SCALAR_AMP_MEDIUM_THRESHOLD) therefore sets the level margin.
Lowering it once (2026-10-15, to 1500) let music through (18 / 50); since then the shape rule (DET-015) rejects
steady and non-rising sound, so the trade may have moved. Question: with shape in place, does a lower amp medium
buy margin without noise acceptance? Inputs: the 47 h soak's margin distribution
(bench/sessions/2026-10-29-step14-soak47, cycles.csv) and its noise plays. Its own pass, logged like LOG-001.
```

### DET-016 - verdict boundary rule: normalized confidence, event class, no unit-bearing fields

Status: DEFERRED (owner, 2026-10-22: a later roadmap item, not part of step 8)

The verdict carries the decision, one normalized magnitude (`confidence`), an
event class and timing; no `primary*Strength` / `primaryAmbientBaseline`. Rule
and design: docs/refactors/archive/cleanup-0-plan.md Phase 7c ("Resolved 2026-09-21:
the verdict boundary rule" and the step 8 plan's Part B, which maps the readers:
Behavior's activity level, the Node RB summary `avg_strength`, the Analyzer's
trial strength, the replay lines, the behavior suites). Open before it starts:
what `confidence` means, the `EventClass` vocabulary (it may grow with
DET-015's shape facts), and whether it waits for a second detector family. The
mismatch it was written against (one slot holding Freq and Scalar quantities)
is gone since step 7d. Sequenced in roadmap-0-steps.md step 11; since 2026-10-29 in its "Carried over" list.

### DET-003 - inspection target / payload split

Status: TODO

```text
Keep one simple selector for what must be inspected for acceptance.
Keep source-specific payload fields inside the module config or payload type.
Do not split the current path unless a second payload shape is actually needed.
```

### DET-004 - field state for detection and reporting

Status: TODO

```text
Track actual use of field state for detection and reporting.
Keep the field-state view separate from detector truth.
```

2026-10-22 review of what FieldState holds:

- Behavior reads only `recentPatternCount` (the rest rule); RB STATUS and
  the analyzer also print `quiet`/`dense` and the counts.
- `noiseFloor` is occurrence count / window ms and `avgAmbientLevel` copies
  it: no audio level is measured (audit 1.2). Cleanup: delete both or
  rename to an occurrence rate. No tuning involved; can land any time.
- Candidate additions, each only if step 6's counts show a need (decided
  with step 11's "FieldState gates or goes"):
  - real ambient level: running mean of mic-frame RMS/envelope from the
    feature stream, in its own domain (myspec 5.8 "where explicitly
    configured"; not rescaled from Strength16). Use: louder reply or rest
    in a loud room. Most likely useful one.
  - recent occurrences per hop bin: only if nodes pick a free bin; hop
    rotation is fixed (MAC % 5) today, so nothing would read it.
  - not here: avg occurrence strength/contrast are detector quality, they
    belong in DetectorReport/analyzer.

### DET-005 - detector reason-model parity

Status: TODO

```text
Make frequency detector internal reason handling as explicit and typed as the
scalar reject handling, or document the asymmetry if string-based reasoning is
still intentional.
```

### ANA-001 - analyzer stage vocabulary cleanup

Status: TODO

```text
Keep clean SEQ_TRIAL / SEQ_SOURCE / SEQ_INSPECT / SEQ_EXPLAIN / SEQ_SUMMARY
on canonical detector-report and inspected-occurrence facts.
Do not rebuild detector truth in AnalyzerRuntime.
Keep the analyzer display layer on canonical report fields.
```

### ANA-010 - SEQ report settle against the longest occurrence

Status: PLANNED (2026-10-30, review 14.1; low)

```text
With max duration 450 an occurrence that starts at the trial window's end gives its verdict at end + 460 ms,
inside the 500 ms report settle by 40 ms (it was 190 at 300). A user settle= under 460 now loses such verdicts.
Fix when it bites, or with the next Analyzer change: settle default 600, or a check at SEQ start that warns when
settle < max duration + release debounce.
```

### ANA-002 - multi-occurrence pattern proposals

Status: DEFERRED

```text
Allow patterns made from groups of occurrences, not only one occurrence at a
time.
Keep competing hypotheses private to OccurrenceEvaluator.
Keep OccurrenceVerdict compact and behavior-facing.
Expose only compact explanation facts through OccurrenceEvaluatorReport.
```

## Current focus

```text
Order: roadmap-0-steps.md. DET-008 (the cleanup pass) is steps 1, 8, 9;
DET-007 is step 5 (decided on D-AMP); ANA-003 is step 10. The rest is
unsequenced.
Now (2026-10-26): DET-015 done (11b); step 11g batches B (detection: Occurrence.band,
RawSampleHistory, the shape knobs as params) and C (Analyzer, with A8's TrialState split).
Now (2026-10-29, 12.3): 11g done (2026-10-28), the band stays after the 11h sweep; detection work next is
measurement (steps 13-15); DET-014, DET-016, DET-018 7a, DET-003..006 are in roadmap-0-steps.md "Carried over".
```

## Spec candidates

```text
DetectorReport is the detector-stage truth.
makeVerdict() (the end of inspection) is the public pattern-stage boundary.
AnalyzerReport stays on canonical trial classification plus scoped details.
Clean analyzer output should not read retired legacy diagnostics.
```

## Non-goals now

```text
Threshold retuning.
Behavior rewrite.
Output rewrite.
New command system.
Pattern helper types as public architecture boundaries.
```
