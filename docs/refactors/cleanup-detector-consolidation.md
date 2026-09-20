# Decision Doc — Consolidate to One Detector Family?

Status: decision pending field validation. Not a code change, not yet.
Related to: `docs/refactors/cleanup.md`,
`docs/refactors/cleanup-detector-ownership.md`,
`docs/refactors/cleanup-analyzer-node-isolation.md`,
`docs/refactors/cleanup-inspector-pattern-scope.md`.

## Why this sits above the other cleanup docs

This decision must be made before Items 2 and 3 in `cleanup.md` are worked on.
Both of those items improve `FrequencyMatchDetector`'s own code (fixing its
diagnostics-state bug, privatizing its public fields). If
`FrequencyMatchDetector` is going to be removed rather than kept, that work
would be wasted effort. Do not start `cleanup.md` Items 2 or 3 until this
decision is made.

---

## The question

`ScalarTransientDetector` already observes `FeatureStreamId::FrequencyTarget`
and `FrequencyContrast`, that is exactly what the `TonalPulseScalar` profile
does today. The one thing `FrequencyMatchDetector` uniquely provides is a
**joint per-sample gate**: its attack/release logic requires score and
contrast to clear their thresholds together, at the detector level, on every
sample (`attackOk = attackScoreOk && attackContrastOk`, see
`FrequencyMatchCriteria::evaluate()`).

`TonalPulseScalar`'s current profile does not replicate that jointly. It
splits the same two facts across two stages instead:

- `ScalarTransientDetector` gates on score alone, observing
  `FrequencyTarget`.
- Contrast becomes a separate Inspector/Pattern support requirement (an
  `InspectionModuleConfig` targeting `InspectionTarget::Contrast`, observing
  `FrequencyContrast`), checked as a windowed aggregate, not per-sample.

If that split proves acoustically equivalent, or better, to the joint gate,
`FrequencyMatchDetector` is not just simplifiable, it is removable, along
with `DetectorId::FrequencyMatch`, `OccurrenceType::Frequency`,
`FrequencyBandOccurrenceDetail`, `FrequencyMatchDetectorReportDetail`,
`FrequencyMatchPrinter`, `FrequencyMatchCriteria`, and the rest of
`src/detection/detectors/frequency/`.

---

## What is already structurally proven, and what is not

**Already proven, by reading the code, no field test needed:**

- Node-facing API stability: `PatternMatcher::makePatternProposalFromOccurrence()`
  already switches on `OccurrenceType`, not `DetectorId`, and both `Scalar`
  and `Frequency` already funnel into the identical `evaluateSinglePulse()`
  path. Removing `OccurrenceType::Frequency` entirely would not change
  `PatternResult`, `FieldState`, or anything `ResonantNodeApp`/
  `ResonantBehavior` consume.
- If this consolidation happens, `cleanup-detector-ownership.md`'s
  union-of-two-detector-objects proposal becomes unnecessary: with one
  detector family, `DetectionRuntime` just holds one
  `ScalarTransientDetector`, no union, no adapter, no per-detector switch
  needed at all in `observeFrame()`/`drainDetectors()`.
- **Correction, 2026-09-20:** this used to also claim `cleanup.md` Item 1
  (a tagged union between `Occurrence.scalar`/`.frequency`) would become
  unnecessary after consolidation, "no always carry both waste to begin
  with." That's wrong. `Occurrence.scalar`/`.frequency` are not a
  detector-exclusive pair, they're separate evidence namespaces (Amp-domain
  vs. Frequency-domain) that `OccurrenceInspector` populates based on which
  `InspectionTarget`s a profile configures, independent of which detector
  produced the occurrence. `TonalPulseScalar` already configures a
  `Contrast`-target module today, on a `ScalarTransientDetector`-only
  profile, so `.frequency.contrastQuality` would still be written even in a
  fully consolidated, single-detector-family world. See `cleanup.md` Item 1
  for the full evidence; Item 1's `Occurrence` half is withdrawn regardless
  of what this decision resolves to. Consolidation would only remove
  `FrequencyBandOccurrenceDetail`'s detector-native fields specific to
  `FrequencyMatchDetector` itself (`score`, `contrast`, `measurement`), not
  the Inspector-written `scoreStrength`/`contrastQuality`/`targetBandStrength`
  fields in the same struct, which stay load-bearing either way.

**Not proven, and not provable from source code alone:**

- Whether the split (detector-level score gate + Inspector/Pattern-level
  contrast requirement) actually performs as well acoustically as the joint
  per-sample gate. This is a real detection-quality question, not a
  structural one.
- The most recent relevant field evidence is inconclusive in the direction
  that matters here: `docs/lab/260903_notes` (`exp001-01`/`exp001-03`)
  records `TonalPulseScalar`-style runs producing `amp_class=weak`
  rejections at the pattern stage at 20-40cm, and the most recent commit
  (`exp001 - distance ladder, inconclusive`) explicitly labels that
  distance-ladder investigation unresolved. This does not mean the split
  approach is worse, it means it has not yet been shown to be equal-or-better
  under the conditions that matter (range, orientation, reflections).

---

## What a real comparison needs

Run matched trials under both profiles, same physical setup, same distance,
same emitter configuration, alternating or interleaving `TonalPulseFreq` and
`TonalPulseScalar` runs so acoustic conditions are as close to identical as
practical. For each distance/condition pair, record:

```text
expected trials
detector-accepted count
pattern-valid count
amp_class / support-class distribution at rejection
duration range of accepted occurrences
strength range of accepted occurrences
false-accept count (if any unexpected-source runs are included)
```

Do this across the same distance ladder already being explored
(`10/20/40/60cm`, per `exp001-03`), since that is exactly where the current
open question (weak-class rejection at range) lives.

## Decision criteria

- If `TonalPulseScalar` (or a retuned variant of it) matches or exceeds
  `TonalPulseFreq`'s accept rate and pattern-valid rate at every tested
  distance, with no new failure mode introduced, that is sufficient grounds
  to proceed with consolidation.
- If `TonalPulseScalar` is worse at any distance that matters for the
  intended use case, do not consolidate. Keep `FrequencyMatchDetector` and
  proceed with `cleanup.md` Items 2 and 3 to improve its code quality
  instead.
- A partial result (better at short range, worse at long range, or vice
  versa) is a real possible outcome and does not have to resolve to a single
  answer immediately. It is fine to leave this decision open and proceed
  with `cleanup.md` Items 4, 5, and 6 in the meantime, since none of those
  depend on this decision, only Items 2 and 3 do. (Item 1 is withdrawn/
  unscheduled regardless of this decision; Item 7 is deferred/optional
  regardless.)

---

## If the decision is: consolidate

1. Do not delete `FrequencyMatchDetector` until `TonalPulseScalar` (or its
   successor profile) is confirmed as the profile actually used going
   forward.
2. Skip `cleanup.md` Items 2 and 3 entirely; that code is going away.
3. `cleanup.md` Item 1's `Occurrence`/`DetectorReport` union plan does not
   apply here regardless of this decision, it was withdrawn as a false
   premise (see that item). Consolidation does not change that: even with
   only `ScalarTransientDetector`, `TonalPulseScalar` already writes
   `FrequencyBandOccurrenceDetail`'s Inspector-owned fields
   (`scoreStrength`/`contrastQuality`/`targetBandStrength`) via its
   `Contrast`-target inspection module, so that part of `.frequency` stays
   needed either way. Only `FrequencyBandOccurrenceDetail`'s
   `FrequencyMatchDetector`-native fields (`score`, `contrast`,
   `measurement`) become removable if `FrequencyMatchDetector` is deleted.
4. Re-scope `cleanup-detector-ownership.md`: the union-of-two-objects design
   is no longer needed; `DetectionRuntime` simplifies to a single
   `ScalarTransientDetector` member with no per-detector dispatch.
5. Remove `DetectorId::FrequencyMatch`, `OccurrenceType::Frequency`, and
   `FrequencyBandOccurrenceDetail`'s detector-native fields (`score`, `contrast`,
   `measurement`) and their printers, but keep the struct's Inspector-owned
   fields (`scoreStrength`, `contrastQuality`, `targetBandStrength`), which
   remain in active use by `TonalPulseScalar`'s `Contrast`-target module.
   Follow the same identical-SEQ-output verification discipline as the
   other cleanup items, using `TonalPulseScalar`'s own 50-trial baseline as
   the reference, not `TonalPulseFreq`'s.
6. Update `docs/specs/myspec.md`: it currently names `TonalPulseFreq` the
   "Stable active profile" and describes `TonalPulseScalar` as "not the
   primary stable path." If consolidation happens, that section is now
   wrong and should be corrected, not left as historical residue.

## If the decision is: keep FrequencyMatchDetector

1. Proceed with `cleanup.md` in its existing order, including Items 2 and 3.
2. Revisit this document if acoustic conditions or requirements change
   enough that the comparison is worth re-running later.

---

## Non-Goals

- No deletion of `FrequencyMatchDetector` or `OccurrenceType::Frequency`
  before comparison data exists.
- No retuning of `TonalPulseFreq`'s or `TonalPulseScalar`'s thresholds as
  part of any other cleanup item while this decision is open, since that
  would confound the comparison this document asks for.
- No treating `TonalPulseScalar`'s current tuning as final. If the
  comparison shows it underperforms, retuning it before concluding it loses
  to `FrequencyMatchDetector` is reasonable and expected.
