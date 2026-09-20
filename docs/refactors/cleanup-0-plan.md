# Executable Plan — Sequencing All Detection Cleanup Docs

This sequences everything across `cleanup.md`,
`cleanup-detector-consolidation.md`, `cleanup-inspector-pattern-scope.md`,
`cleanup-analyzer-node-isolation.md`, and `cleanup-detector-ownership.md`
into one ordered, testable execution plan. It does not repeat each item's
full detail, those live in their own documents, this states the order,
the dependencies, the fork points, and what to run at each gate before
moving on.

Working assumption for this plan, per the most recent decision: proceed as
if `FrequencyMatchDetector` is kept. Phase 0 below still resolves that
formally with field data; if it resolves the other way, jump to the "If
Phase 0 resolves to consolidate" section instead of Phase 2.

---

## Standing Test Battery

Defined once here, referenced by number at each phase gate. Do not skip a
referenced test to save time, every phase below states exactly which of
these apply.

- **T1 — Build all three environments.** `esp32dev`, `esp32dev-emitter`,
  `esp32dev-analyzer` must all compile and link. This is the only check that
  would catch a change to shared code breaking a binary you didn't have in
  mind while editing.
- **T2 — SEQ regression, TonalPulseFreq.** Run the 50-trial
  `TonalPulseFreq` SEQ test. Diff `SEQ_TRIAL`/`SEQ_SOURCE`/`SEQ_INSPECT`/
  `SEQ_EXPLAIN`/`SEQ_SUMMARY` output against the pre-phase baseline. Must be
  byte-identical unless the phase explicitly says otherwise (only Phase 0's
  outcome is allowed to change behavior; every other phase is a pure
  refactor).
- **T3 — SEQ regression, TonalPulseScalar.** Same as T2, for the 50-trial
  `TonalPulseScalar` test.
- **T4 — Unity test suite.** Run `test_analyzer_pass_rules` (the one
  existing automated test target; it pulls `ScalarTransientDetector`,
  `FeatureHistory`, and `AnalyzerPassRules` directly into the test build).
  This requires the physical ESP32 target to execute, `pio test` runs
  on-device here, there is no native/host test environment configured. If
  hardware isn't available for a given change, at minimum confirm the test
  target still compiles.
- **T5 — Struct size check.** For any change to `Occurrence`, `DetectorReport`,
  or their detail members, compile a `sizeof()` probe against the real
  `xtensa-esp32-elf-g++` toolchain (not host g++, alignment differs) and
  record before/after sizes in the commit notes.
- **T6 — Profile-switch soak.** Run one profile, switch to the other
  mid-session, switch back, for at least a few dozen trials each way.
  Confirms no state leaks or goes stale across `setDetectorSelection()`
  calls. Required for any change touching `DetectionRuntime`'s detector
  storage or lifecycle.
- **T7 — Node smoke test.** There is no automated test that exercises
  `ResonantNodeApp`'s runtime path (SEQ tests run in Analyzer mode only).
  For any change to code shared with the Node build (chiefly
  `DetectionRuntime`), flash `env:esp32dev` and manually confirm the node
  still detects and responds normally. This is a real gap, not a formality,
  call it out in commit notes rather than skipping it silently.
- **T8 — Binary size measurement.** Record the Node (`env:esp32dev`) build's
  RAM/flash usage before and after. Only meaningful once the isolation
  proposal (Phase 5b) lands, since that's the only phase expected to move
  the number.

---

## Phase 0 — Decision: Keep or Consolidate `FrequencyMatchDetector`

Doc: `cleanup-detector-consolidation.md`.

This is a field-data exercise, not a code change, and it runs on a
different clock than everything else here. Start it now, in parallel with
Phase 1; do not let it block Phase 1's work, but do not start Phase 2 until
it resolves.

1. Run the matched-condition comparison trials described in that document
   (`10/20/40/60cm` ladder, both profiles, recording accept rate,
   pattern-valid rate, and rejection-class distribution at each distance).
2. Decide: keep both detectors, or consolidate to one.
3. If undecided after a reasonable number of trials, default to "keep" and
   proceed with Phase 2 as planned, revisit later. Do not let this decision
   block the rest of the plan indefinitely.

**Gate:** Phase 2 does not start until this resolves to "keep." If it
resolves to "consolidate," skip to "If Phase 0 resolves to consolidate"
below instead of Phase 2.

**Test:** none code-side; this phase's output is a decision, recorded in
`cleanup-detector-consolidation.md`'s own comparison table.

---

## Phase 1 — Safe, Independent Items (start immediately, any order)

These have no dependencies on each other or on Phase 0's outcome. Suggested
serial order below is by size (fastest win first), not by necessity.

**Status as of 2026-09-20: items 1 and 2 done, item 3 withdrawn/blocked. See
correction below.**

1. **`cleanup.md` Item 4** — remove the duplicated dead helper functions
   (`frequencyRejectReasonFromState`, `frequencyRejectClassFromReason`) from
   `FrequencyMatchDetector.cpp`/`FrequencyMatchOccurrence.cpp`. **Done.**
   Compiled clean against the real toolchain.
   - Test: T1, T4 (compile only is sufficient, the removed code was
     unreachable). Hardware-side T4 run still outstanding.
2. **`cleanup-inspector-pattern-scope.md`** — remove `ProposalShape`/
   `PulseSequence` from `PatternMatcher.cpp`. **Done**, compiled clean. The
   `InspectionModuleKind` half of this item was reviewed and declined (it
   contradicted that document's own Non-Goals and reached into
   `ResonantNodeApp.cpp`'s display code for no behavioral benefit); see that
   document for the reasoning.
   - Test: T1 done. T2, T3 (hardware SEQ runs) still outstanding.
3. ~~**`cleanup.md` Item 1**~~ — **withdrawn, do not implement.** While
   preparing this item, direct evidence showed `Occurrence.scalar`/
   `.frequency` are both genuinely populated and read for the same
   occurrence by both stable profiles, not a detector-exclusive pair a union
   could safely choose between. Implementing the originally planned union
   would have silently dropped real evidence on every occurrence. See
   `cleanup.md` Item 1 for the full evidence trail. The `DetectorReport`
   half of the same item is a separate, unverified question, not scheduled
   until it gets its own read-site check.

**Gate:** Phase 1 is as complete as it's going to get without hardware
access for T2/T3/T4. Phase 2 and Phase 3 do not actually depend on the
withdrawn Item 1 in any way that survives this correction, see their
updated notes below.

**Out-of-band, 2026-09-21: naming rename, not tracked as a numbered item
above.** While investigating Item 1's `Occurrence` correction, the
`.scalar`/`.frequency` field names were found to collide with
`DetectorId::ScalarTransient`/`FrequencyMatch` and invite exactly the wrong
"detector-exclusive" mental model the correction above describes. Renamed
`Occurrence.scalar`/`.frequency` to `.magnitude`/`.band`
(`MagnitudeOccurrenceDetail`/`FrequencyBandOccurrenceDetail`), then extended
the same fix to the rest of the same naming collision:
`ScalarInspectionMode`/`Basis`/`Note`/`Anchor` -> `MagnitudeInspection*`,
`ScalarFeatureInspectionConfig` -> `MagnitudeFeatureInspectionConfig`,
`ScalarWindow.h` -> `MagnitudeWindow.h`, and their call sites across
`InspectorTypes.h`, `OccurrenceInspector.{h,cpp}`, `InspectedOccurrence.h`,
`InspectionNames.h`, `AnalyzerReportTypes.h`, `AnalyzerSeqReporter.cpp`,
`DetectionProfile.h`, `AnalyzerModeApp.cpp`, `ResonantNodeApp.cpp`, and
`test_main.cpp`. `DetectorId::ScalarTransient`/`FrequencyMatch`,
`OccurrenceType::Scalar`/`Frequency`, `ScalarTransientDetector`/`Config`,
and `DetectorReport.scalar`/`.frequency` are untouched, those are the
axis-1 provenance names this rename disambiguates against and are already
accurate. Compiled clean against the real toolchain for every touched file
except `AnalyzerModeApp.cpp`/`ResonantNodeApp.cpp` (verified by exhaustive
grep instead, see commit `0102896`). T2/T3/T4 hardware runs remain
outstanding, same as the rest of Phase 1.

---

## Phase 2 — FrequencyMatchDetector Fixes (gated on Phase 0 = "keep")

Doc: `cleanup.md` Items 2 and 3. Item 1 is withdrawn and is no longer a
prerequisite; Item 3 still says to re-check for external field access after
Item 2 lands, that part is unaffected by Item 1's withdrawal.

1. **`cleanup.md` Item 2** — separate the diagnostics-only "best evidence"
   gate snapshot from the live gate state in `FrequencyMatchDetector::update()`.
   - Test: T1, T2, T4. Additionally: run the same trial with diagnostics
     forced on and forced off, confirm identical `DetectorReport.accepted`/
     `selectedReject` truth in both runs (this is the specific bug being
     fixed, verify it directly, not just via the standard SEQ diff).
2. **`cleanup.md` Item 3** — privatize `FrequencyMatchDetector`'s public
   field surface, one logical group at a time (lifecycle state, then
   pending/candidate facts, then best-rejected summary, then diagnostics),
   rebuilding after each group.
   - Test: T1 after each group; T2, T4 after the full item is complete.

**Gate:** Phase 3 can start once this phase is done, or in parallel if
working with more than one person, since Phase 3 touches `DetectionRuntime`
rather than `FrequencyMatchDetector` internals directly.

---

## Phase 3 — Unify the Per-Detector Switch (`cleanup.md` Item 5)

Does not require Phase 2, but doing Phase 2 first means
`FrequencyMatchDetector`'s surface is already narrow when this item touches
its call sites. (No longer gated on Item 1, which is withdrawn.)

1. Introduce the internal `ActiveDetectorAdapter` (`hasPendingOccurrence`,
   `popOccurrence` only, see the corrected sketch in `cleanup.md` Item 5).
   Route `drainDetectors()` through it. Leave `observeFrame()`'s
   detector-specific `update(...)` dispatch switch as-is.
   - Test: T1, T2, T3, T6.

**Note:** this item is deliberately kept even though Phase 5b (ownership
proposal) will later replace the adapter with a fuller single-slot model.
Doing it now is a smaller, independently verifiable step; skipping straight
to Phase 5 is possible but riskier, since it combines two structural changes
(dispatch unification and object lifetime) into one.

**Skip note on `cleanup.md` Item 6 (diagnostic counter audit):** do not do
this as a standalone step. Phase 5b (isolation) relocates the entire
counter set into the diagnostics layer anyway; auditing them first and then
moving them is duplicated effort. Fold Item 6's "keep/consolidate/remove"
judgment into Phase 5b's own inventory step instead.

---

## Phase 4 — Let Phase 1–3 Soak

Not a code phase. Run the Node build in normal use for a period before
starting the bigger structural proposals in Phase 5. Both `cleanup-detector-ownership.md`
and `cleanup-analyzer-node-isolation.md` are explicitly higher-risk than
anything above, they change object lifetime and code location, not just
struct layout or dead-code removal. Confirm Phases 1–3 are stable under
real use first.

**Test:** T7, informally, over normal operation rather than a single
scripted run.

---

## Phase 5 — The Two Big Structural Proposals

Do these sequentially, not simultaneously, even though they're
independent proposals. Recommended order: isolation first, then ownership.
Reasoning: isolation removes the correlation-queue machinery entirely from
the core, which shrinks what the ownership change has to reason about next.
Swapping the order is possible if preferred, neither hard-depends on the
other.

### Phase 5a — `cleanup-analyzer-node-isolation.md`

1. Re-confirm `ResonantNodeApp.cpp`'s call list into `DetectionRuntime`
   matches this document's list (re-check after Phases 1-3, both touch
   nearby code).
2. Identify every member/method serving only
   `AnalyzerSystemReporter.cpp`/`AnalyzerSequenceSession.cpp`: the counters,
   the pipeline-event queue, `PipelineIntegrity`, the
   `PendingPatternObservation` correlation machinery, `latestReport()`/
   `reportGeneration()`. Fold `cleanup.md` Item 6's counter judgment in here.
3. Move that state and logic behind an `ANALYZER_MODE`-gated layer.
4. Test: T1 (all three environments must still link), T2, T3, T6, T7
   (this is the phase where the Node smoke test matters most, it's the
   first change that could plausibly affect the Node binary's behavior if
   the split is done incorrectly), T8 (this is the phase expected to show a
   measurable RAM/flash reduction, record it).

### Phase 5b — `cleanup-detector-ownership.md`

1. Re-run the `_scalarDetector.`/`_frequencyDetector.` cross-access search
   to confirm the zero-cross-access finding still holds after Phase 5a.
2. Introduce `DetectorStorage` (tagged union of the two detector objects),
   route `setDetectorSelection()` through placement-new construction.
3. Migrate `resetState()`, `popOccurrence()`, `hasPendingOccurrence()`
   through the single active slot. (`latestReport()`/`reportGeneration()`
   now live only in the Phase 5a diagnostics layer, which reads whichever
   detector is currently active by the same selection state.)
4. Remove the old two-member layout.
5. Test: T1, T2, T3, T4, T6, T7, T5 (record `sizeof(DetectionRuntime)`
   before/after this phase specifically, this is the phase expected to
   roughly halve its resident detector-state size).

**Gate:** both must be individually verified complete (all listed tests
passing) before starting Phase 6.

---

## Phase 6 — Optional: Add the Third Detector as an Acceptance Check

Not required, but if a third detector is actually wanted, building it now is
the strongest proof the plan achieved its goal. Reference:
`cleanup-analyzer-node-isolation.md`'s worked example
(`SimpleThresholdDetector`).

1. Implement the new detector against the four-method core contract
   (`resetState`, `update`, `hasPendingOccurrence`, `popOccurrence`) only.
   Do not implement `latestReport()`/`reportGeneration()` yet.
2. Wire it into `DetectorStorage` (Phase 5b), add its `DetectorId`/
   `DetectorSelection` value, a config struct in `DetectionProfile.h`, and
   one profile factory.
3. Confirm it works end-to-end producing `PatternResult`/`FieldState`
   without touching the Analyzer diagnostics layer at all.
4. Test: T1, T6, T7. T2/T3 don't apply (new profile, no baseline to diff
   against), instead run its own first set of SEQ trials to establish a
   baseline for future changes to it.
5. Only after this works, decide whether the new detector is worth
   exposing to Analyzer diagnostics (`latestReport()`/`reportGeneration()`,
   a `DetectorReportPrinter` case). If yes, add them as pure additions.

If this phase reveals the four-method contract was insufficient in
practice, that's real signal the isolation split (Phase 5a) drew the
core/diagnostics line in the wrong place, revisit that document rather than
quietly growing the new detector's required surface back toward what the
existing two have.

---

## If Phase 0 Resolves to "Consolidate"

Skip Phase 2 entirely. Follow `cleanup-detector-consolidation.md`'s "If the
decision is: consolidate" section instead:

1. Do not delete `FrequencyMatchDetector` until the replacement profile is
   confirmed as the one actually used going forward.
2. Item 1 is withdrawn regardless of this branch, it does not apply here
   either. `Occurrence.frequency` does not become fully unused even after
   consolidation: `TonalPulseScalar` already writes
   `frequency.scoreStrength`/`contrastQuality`/`targetBandStrength` via its
   `Contrast`-target inspection module today, on a `ScalarTransientDetector`
   -only profile. Only `FrequencyBandOccurrenceDetail`'s
   `FrequencyMatchDetector`-native fields (`score`, `contrast`,
   `measurement`) become removable.
3. Re-scope Phase 5b: `DetectionRuntime` simplifies to a single
   `ScalarTransientDetector` member, no union, no per-detector dispatch,
   full stop, unless a third detector (Phase 6) is still planned.
4. Remove `DetectorId::FrequencyMatch`, `OccurrenceType::Frequency`, and
   `FrequencyBandOccurrenceDetail`'s detector-native fields (`score`, `contrast`,
   `measurement`) and their printers, keeping the struct's Inspector-owned
   fields, verified against `TonalPulseScalar`'s own baseline, not
   `TonalPulseFreq`'s (which no longer exists to compare against).
5. Correct `docs/specs/myspec.md`, which currently names `TonalPulseFreq`
   the stable path.
6. Test: T1, T3 (T2 no longer applies, that profile is gone), T4, T6, T7.

---

## Summary Table

| Phase | Doc | Depends on | Key tests |
|---|---|---|---|
| 0 | consolidation | — (parallel track) | field trials only |
| 1 | cleanup.md #4 (done), inspector-pattern-scope (done), ~~cleanup.md #1~~ (withdrawn), Magnitude/Band rename (done, out-of-band) | — | T1 (done), T2-T4 (hardware, outstanding) |
| 2 | cleanup.md #2, #3 | Phase 0 = keep | T1, T2, T4 |
| 3 | cleanup.md #5 | — (Item 1 dependency removed; Phase 2 optional) | T1, T2, T3, T6 |
| 4 | soak, no doc | Phases 1-3 | T7 |
| 5a | analyzer-node-isolation | Phases 1-4 | T1, T2, T3, T6, T7, T8 |
| 5b | detector-ownership | Phase 5a | T1-T7, T5 |
| 6 | third detector (optional) | Phase 5b | T1, T6, T7 |
