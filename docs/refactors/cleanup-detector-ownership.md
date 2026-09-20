# Architecture Proposal — Single Active Detector Ownership

Status: proposal, not an active implementation pass.
Related to: `docs/refactors/cleanup.md` (this builds on and is complementary
to that pass; it does not replace it).

## Relationship to cleanup.md

This is a larger, riskier structural change than any item in `cleanup.md`.
`cleanup.md` fixes the shape of individual structs and dispatch code without
changing object lifetime or ownership. This proposal changes ownership: it
asks whether `DetectionRuntime` should hold two detector objects for the life
of the program, or one.

Recommended sequencing: complete `cleanup.md` first. Item 3
(FrequencyMatchDetector encapsulation) makes the change described here
easier and lower-risk once done. Item 5 (unify the per-detector switch in
`DetectionRuntime`) becomes largely redundant if this proposal is adopted,
since single-slot ownership removes the two branches it was written to
merge.

Correction, 2026-09-20: this used to also cite Item 1 (the `Occurrence`/
`DetectorReport` tagged union) as reducing detector-owned state. Item 1's
`Occurrence` half was withdrawn as a false premise; see `cleanup.md` Item 1.
It is unrelated to this proposal regardless: this document's `DetectorStorage`
union is a union of the two *detector objects* (`FrequencyMatchDetector`/
`ScalarTransientDetector`), a completely different thing from a union inside
the `Occurrence`/`DetectorReport` *data* types. That distinction was blurred
in the original wording; they do not depend on each other.

Do not start this proposal until `cleanup.md` is complete and its
verification runs pass.

---

## Problem

`DetectionRuntime` declares both detectors as permanent members:

```cpp
FrequencyMatchDetector _frequencyDetector;
ScalarTransientDetector _scalarDetector;
```

Both exist, fully constructed, for the entire lifetime of the program,
regardless of which one `_detectorSelection` currently names. Checking actual
usage (not just the code shape) confirms only one is ever meant to be live at
a time:

- `setDetectorSelection()` is called from exactly two places:
  `AnalyzerSequenceSession.cpp` (once per SEQ sequence) and
  `ResonantNodeApp.cpp` (once per operator profile command). It is never
  called inside the per-frame `observeFrame()` path.
- Every functional call in `DetectionRuntime` (`update`, `popOccurrence`,
  `hasPendingOccurrence`, `latestReport`) is already correctly gated behind a
  switch on `_detectorSelection`. Nothing reads the inactive detector's
  functional state.
- `resetState()` and `setDiagnosticsEnabled()` call into *both* detectors
  unconditionally, which only makes sense because both objects exist, not
  because anything needs both reset.
- `DetectionRuntime::scalarReportGeneration()` and
  `DetectionRuntime::frequencyReportGeneration()` are declared, defined, and
  never called from anywhere outside `DetectionRuntime.cpp` itself. They are
  leftover surface from a model where both detectors' state might be
  compared; nothing does that comparison.

The practical usage pattern is: pick one detector, run it exclusively for the
length of a run, occasionally swap to a different one between runs, driven by
an explicit profile decision. The current object model is built for a more
dynamic situation, two live detector pipelines coexisting with a runtime
router between them, that does not actually occur.

This is the upstream cause of several separate observations already in
`cleanup.md`:

- the double-detail `Occurrence`/`DetectorReport` structs exist partly
  because a report has to be generically routable without knowing which
  detector produced it — more important when two detectors could plausibly
  be live at once than when only one ever is
- the switch-statement duplication in `observeFrame()`/`drainDetectors()`
  exists because there are two permanent objects to dispatch between
  - the unconditional dual-reset calls and the two dead accessors are direct
  leftovers of the two-objects-forever model

---

## Proposed Change

Replace the two permanent member objects with storage for exactly one
detector, of whichever kind is currently selected. The other kind simply
does not exist while it is not selected.

```cpp
// Sketch, not final API.
union DetectorStorage {
    FrequencyMatchDetector frequency;
    ScalarTransientDetector scalar;
    DetectorStorage() {} // constructed explicitly by setDetectorSelection()
    ~DetectorStorage() {} // trivial: neither member owns a resource
};
```

`setDetectorSelection()` becomes the single place that placement-constructs
the newly selected detector into `DetectorStorage`. Both
`FrequencyMatchDetector` and `ScalarTransientDetector` are plain value types
today (no owned resources, no virtual functions, no dynamic allocation), so
this is safe: neither has meaningful teardown work, and reconstructing one in
place is not materially different in cost from what `resetState()` already
does on every profile switch today.

Everything that already has an identical shape across both detectors
(`popOccurrence`, `hasPendingOccurrence`, `resetState`) is called through one
path against "the current detector," selected once at construction time
rather than branched on every call. `observeFrame()`'s detector-specific
`update(...)` call, which genuinely has a different signature per detector,
keeps its switch — this proposal does not touch that boundary, and does not
ask `ScalarTransientDetector::update()` and `FrequencyMatchDetector::update()`
to converge on one signature.

Note: `latestReport()`/`reportGeneration()` are deliberately not part of
this adapter's required surface. `docs/refactors/cleanup-analyzer-node-isolation.md`
found that neither `PatternResult` nor `FieldState` is ever built from
`DetectorReport`, so report access is a diagnostics-only concern, not part
of the core dispatch this proposal unifies. See that document's "minimal
Node-required detector contract" for the full split.

### What this fixes beyond cleanup.md

- Removes roughly half of `DetectionRuntime`'s resident detector-state
  memory: today both detectors' full private state (pending/candidate
  bookkeeping, best-rejected summaries, diagnostics counters) is permanently
  allocated even though one side is always fully unused dead weight.
- Removes the switch-statement duplication in `drainDetectors()` and
  `observeFrame()`'s report-capture path as a natural side effect, rather
  than as a separate refactor.
- Removes the unconditional dual-reset calls in `resetState()` and
  `setDiagnosticsEnabled()`, since there is only one object to reset.
- Makes `scalarReportGeneration()`/`frequencyReportGeneration()` provably
  removable, since there would be exactly one active generation to expose.

### What this does not fix, and is not trying to

- Does not change the shape of `Occurrence` or `DetectorReport`. Note:
  `Occurrence.scalar`/`.frequency` are not a detector-exclusive pair that a
  future item could still collapse, they are both genuinely populated per
  occurrence today (see `cleanup.md` Item 1's correction). This proposal
  does not touch that either way, single- vs. dual-resident detector objects
  is orthogonal to what shape `Occurrence`/`DetectorReport` carry.
- Does not introduce a virtual `IDetector` interface or a type-erased
  detector graph. The spec explicitly defers that, and nothing here requires
  it: a union plus a switch on `_detectorSelection` for the handful of
  lifecycle calls is sufficient for two known detector kinds.
- Does not change detector-specific `update()` signatures, threshold
  semantics, or lifecycle logic in either detector.
- Does not remove the ability to switch profiles at runtime. Profile
  switching remains exactly as available as it is today; only its
  implementation cost changes, from "flip a selection flag" to
  "reconstruct the selected detector in place," which happens at the same
  call sites, at the same (low) frequency.

---

## Risks and Open Questions

- **Manual lifetime management.** Placement-new inside a union is more
  delicate than plain member objects. It must be audited carefully: nothing
  may hold a reference or pointer into the previous detector across a
  `setDetectorSelection()` call, and any queued data derived from the
  previous detector (pending `DetectorReport`/`Occurrence` copies already in
  `DetectionRuntime`'s queues) must be understood to survive independently
  of the detector object, since they are already plain copies, not
  references, today.
- **Diagnostics/reset semantics change.** Code that currently relies on
  being able to call `resetSourceRejectSummaries()` or similar on "the
  detector that isn't selected" (if any exists, none found in this review)
  would need to be found and re-checked. A full search for direct
  `_scalarDetector.`/`_frequencyDetector.` access outside the switch-gated
  paths should be redone immediately before implementation, since this
  proposal assumes today's zero-cross-access finding stays true.
- **Whether a union is the right mechanism at all**, versus a small manual
  vtable-free tagged dispatch, versus accepting real virtual dispatch (the
  Arduino framework this project already depends on uses virtual functions
  itself, e.g. `Stream`/`Print`, so this is not foreign to the target, but it
  is a style choice this project has otherwise avoided in favor of
  enum/switch dispatch). This should be decided before implementation
  begins, not discovered mid-pass.
- **Value of doing this at all before a third detector exists.** With only
  two detector kinds, the memory and dispatch savings are real but bounded.
  If a third detector kind is added later, the case for this change gets
  materially stronger. It may be reasonable to defer this proposal until
  that happens, rather than doing it speculatively now.

---

## Suggested Approach If Adopted

1. Re-run the cross-access search (`_scalarDetector.`/`_frequencyDetector.`
   outside switch-gated code) against the post-`cleanup.md` codebase to
   confirm the zero-cross-access finding still holds.
2. Introduce `DetectorStorage` and route `setDetectorSelection()` through it,
   without yet removing the old two-member layout — build both side by side
   behind a compile-time flag if useful for a safe A/B comparison run.
3. Migrate the lifecycle/report call paths (`resetState`,
   `setDiagnosticsEnabled`, `popOccurrence`, `hasPendingOccurrence`,
   `latestReport`, `reportGeneration`) to go through the single active
   detector.
4. Remove the old two-member layout, the dual-reset calls, and the two dead
   report-generation accessors.
5. Run both 50-trial SEQ tests (`TonalPulseFreq`, `TonalPulseScalar`) and a
   profile-switch sequence (run one profile, switch, run the other, switch
   back) and confirm identical output and no leaked state across switches.

This is deliberately not written as a phased implementation pass with
required changes, the way `cleanup.md` is. It is a decision to make before
committing to that level of detail.
