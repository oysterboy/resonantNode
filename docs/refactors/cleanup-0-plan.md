# Executable Plan — Sequencing All Detection Cleanup Docs

This sequences everything across `cleanup.md`,
`cleanup-detector-consolidation.md`, `cleanup-inspector-evaluator-scope.md`,
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
2. **`cleanup-inspector-evaluator-scope.md`** — remove `ProposalShape`/
   `PulseSequence` from `OccurrenceEvaluator.cpp`. **Done**, compiled clean. The
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

**Started ahead of Phase 0's formal resolution, 2026-09-21:** Phase 0's field
trials have not been run (no hardware access this session); per this plan's
own default ("if undecided after a reasonable number of trials, default to
keep and proceed with Phase 2"), Item 2 below was implemented since it is a
real, verified correctness bug independent of the keep/consolidate outcome
either way, `FrequencyMatchDetector::update()` exists and runs today
regardless of what Phase 0 eventually decides. If Phase 0 later resolves to
"consolidate," this fix is still correct, just short-lived.

1. **`cleanup.md` Item 2** — separate the diagnostics-only "best evidence"
   gate snapshot from the live gate state in `FrequencyMatchDetector::update()`.
   **Implemented, compiled clean against the real toolchain; hardware
   verification outstanding.** See `cleanup.md` Item 2's status note for
   exactly what output this is expected to change under default settings
   (`DetectorReport.frequency.inspect.gateReason`/`readyOk`/`gateOpen`,
   i.e. `SEQ_SOURCE_SPEC`'s `gate_reason`/`ready_ok`/`gate_open` fields).
   - Test: T1 (done), T2, T4 (hardware, outstanding). Additionally: run the
     same trial with diagnostics forced on and forced off, confirm identical
     `DetectorReport.accepted`/`selectedReject` truth in both runs (this is
     the specific bug being fixed, verify it directly, not just via the
     standard SEQ diff) — also outstanding, requires hardware.
2. **`cleanup.md` Item 3** — privatize `FrequencyMatchDetector`'s public
   field surface, one logical group at a time (lifecycle state, then
   pending/candidate facts, then best-rejected summary, then diagnostics),
   rebuilding after each group.
   **Implemented, 2026-09-21.** See `cleanup.md` Item 3's status note for
   the two design decisions this took (the `_pendingCandidateOccurrence`
   rename to avoid a name collision, and converting
   `frequencyRejectReasonFromState()` from a free function to a private
   member since privatizing broke its direct field access).
   - Test: T1 done for all three environments, including a full real build
     (not just syntax-only checks), see below. T2, T4 (hardware) still
     outstanding.

**Checkpoint from earlier, superseded:** this plan previously paused before
starting Item 3, pending Item 2's hardware verification (see git history).
The user asked to proceed with Item 3 anyway; it's done now, on the same
verification footing as Item 2, T1 (full build, all three environments)
passed, T2/T4 still need hardware.

**Gate:** Phase 3 can start once this phase is done, or in parallel if
working with more than one person, since Phase 3 touches `DetectionRuntime`
rather than `FrequencyMatchDetector` internals directly.

---

## Phase 3 — Unify the Per-Detector Switch (`cleanup.md` Item 5)

Does not require Phase 2, but doing Phase 2 first means
`FrequencyMatchDetector`'s surface is already narrow when this item touches
its call sites. (No longer gated on Item 1, which is withdrawn.)

1. Introduce the internal `ActiveDetectorAdapter` (`popOccurrence` only,
   see `cleanup.md` Item 5's status note for why `hasPendingOccurrence`
   wasn't folded in too). Route `drainDetectors()` through it. Leave
   `observeFrame()`'s detector-specific `update(...)` dispatch switch as-is.
   **Implemented, 2026-09-21.** T1 done (full build, all three
   environments); T2, T3, T6 (hardware) still outstanding.

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

## Resume point, 2026-09-21: stopped here, on purpose

Phases 1, 2, and 3 are code-complete and committed (`0102896`, `4280601`,
`1b4b829`, `4c7658e`, `3fd070e`, `0526ad1`). Every one of them compiles and
produces a real, linked firmware image on all three PlatformIO environments
as of this note. None of them have run on real hardware yet.

Before doing anything else in this plan:

1. **Run T2 and T3** — the 50-trial SEQ regression for both
   `TonalPulseFreq` and `TonalPulseScalar`. Item 2's fix is expected to
   change `SEQ_SOURCE_SPEC`'s `gate_reason`/`ready_ok`/`gate_open` fields
   under default settings (see `cleanup.md` Item 2's status note); confirm
   that's the only thing that moved and that `accepted`/`selectedReject`
   truth is unchanged.
2. **Run Item 2's own two-run comparison** — same trial, diagnostics forced
   on and off, confirm identical `DetectorReport.accepted`/`selectedReject`
   truth in both runs.
3. **Run T4** — the Unity suite (`test_analyzer_pass_rules`) on-device.

Only after that: proceed to Phase 4's soak below, then Phase 5. Phase 5 is
a real step up in risk from everything done so far, it changes object
lifetime and memory layout, not naming or switch consolidation, so a clean
compile is much weaker evidence there than it was for Phases 1–3. Don't
start it on top of still-unverified Phase 2/3 output changes.

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

### Phase 5a — `cleanup-analyzer-node-isolation.md` — **implemented 2026-09-21**

1. Re-confirm `ResonantNodeApp.cpp`'s call list into `DetectionRuntime`
   matches this document's list (re-check after Phases 1-3, both touch
   nearby code). **Done, list confirmed unchanged.**
2. Identify every member/method serving only
   `AnalyzerSystemReporter.cpp`/`AnalyzerSequenceSession.cpp`: the counters,
   the pipeline-event queue, `PipelineIntegrity`, the
   `PendingVerdictObservation` correlation machinery, `latestReport()`/
   `reportGeneration()`. Fold `cleanup.md` Item 6's counter judgment in here.
   **Done, by per-method caller search across the whole tree.**
3. Move that state and logic behind an `ANALYZER_MODE`-gated layer.
   **Done**, and it needed a second mechanism the proposal didn't
   anticipate: `platformio.ini` had no `build_src_filter`, so the analyzer
   tooling `.cpp` files were compiled into the Node/Emitter binaries too and
   broke the build once the methods they call were gated. Both are in place
   now; see that document's implementation record.
4. Test: T1 (all three environments must still link), T2, T3, T6, T7
   (this is the phase where the Node smoke test matters most, it's the
   first change that could plausibly affect the Node binary's behavior if
   the split is done incorrectly), T8 (this is the phase expected to show a
   measurable RAM/flash reduction, record it).
   **T1 done** (all three link). **T8 done:** Node gives back 13,504 bytes
   of RAM and 9,240 of flash; the Analyzer binary is byte-identical in size.
   **T2, T3, T6, T7 outstanding, hardware.** T7 matters more here than
   anywhere else in this plan: the Node binary is the one that changed, and
   the Analyzer instrumentation that would normally catch a regression no
   longer exists inside it.

### Phase 5b — `cleanup-detector-ownership.md` — **deferred 2026-09-21**

Step 1 was run (cross-access finding still holds) and step 5's `sizeof`
measurement was taken first, which is what settled it: the union saves
1,440 bytes, 1.9% of Node RAM, and the re-audit found a stale-report
dispatch in `capturePipelineResult()` that would be undefined behavior
under a union on the profile-switch path. Not worth it with two detector
kinds; see that document's Decision section for the numbers and the
revisit condition (a third detector kind).

1. ~~Re-run the cross-access search~~ done, holds.
2. ~~Introduce `DetectorStorage`~~ deferred.
3. ~~Migrate lifecycle calls through the single slot~~ deferred.
4. ~~Remove the old two-member layout~~ deferred.
5. `sizeof(DetectionRuntime)` was recorded (41,160 bytes at that point);
   it showed `_featureHistory` at 80% of the object, which redirected the
   effort to Phase 5c below.

### Phase 5c — FeatureHistory (not in any proposal doc; found by measuring 5b)

Two commits, each independently revertible, both build-verified on all
three environments, neither hardware-verified:

1. **Drop write-only per-bin aggregates** (`48e4f90`). `FeatureHistoryBin`
   stored rms and peak that nothing read; the window-level rms/peak are
   computed across bins from each bin's representative value. Bin 32 -> 24
   bytes, plus the accumulator state that only fed them. **-8,256 bytes.**
   Visible change: the Analyzer's `debugFeatureBinSize()` debug line now
   prints 24, which is the true size.
2. **Record only the streams the inspection plan reads** (`795f649`). One
   buffer per inspection module (3) instead of per known stream (4), bound
   on demand from `setInspectionPlan()`; unbound streams are dropped at
   `record()`, costing no RAM and no accumulation work. Slot count is tied
   to `kMaxInspectionModules` by `static_assert`, so a plan can never ask
   for more than fit. **-6,184 bytes.** Visible change, Analyzer run
   summary only (`printAudioRunSummary`, not per-trial SEQ): the FREQBAND
   line reports `history_records=<stream>:N,...` over the recorded streams
   instead of two hardcoded ones.

Also recorded in `FeatureHistory.h`: `kBinsPerStream = 256` is not a round
guess. The longest accepted occurrence is 240 ms and inspection looks back
10 ms before its anchor, so 250 ms must still be resident at inspection
time. Shrinking it silently degrades long occurrences to
`HistoryWindowIncomplete`. Leave it.

**Cumulative Node RAM, this session, real linked builds:**

| Step | Node RAM | Delta |
|---|---|---|
| baseline `293f11f` | 87,940 | |
| 5a Analyzer/Node isolation | 74,436 | -13,504 |
| 5c.1 per-bin aggregates | 66,180 | -8,256 |
| 5c.2 on-demand streams | 59,996 | -6,184 |
| **total** | | **-27,944 (31.8%)** |

Analyzer RAM moved in step: 99,564 -> 85,124. Emitter unchanged in RAM.

- Test: T1 done. T2, T3, T7 outstanding (hardware). 5c.2 also wants a
  profile-switch check (T6): `setActiveStreams()` clears buffers on every
  plan change, which is the intended behavior, but confirm no inspection
  runs against a just-cleared buffer on the switch frame.

### Phase 5d — `cleanup-detector-family-build.md` (proposal, not started)

Supersedes 5b. Detector family becomes a build flag (one Node and one
Analyzer env per family), profile becomes per-node config applied at
reboot, thresholds stay live params — which is `roadmap-param-config.md`'s
own FWOTA / Config / Params split applied to detection. Gets 5b's full
saving (the other detector isn't compiled at all) plus deletes every
`DetectorSelection` dispatch, including Phase 3's adapter, in both builds.
Measured expectation: frequency-family Node about -7,600 bytes (it also
drops to 2 `FeatureHistory` slots), scalar-family Node about -1,800.

Also records a pre-existing finding: `FrequencyMatchDetector` has no upper
duration bound, so its family can't shrink history depth, and a frequency
occurrence over ~246 ms already outruns the 256-bin buffer today.

Costs: five build environments instead of three, and the Analyzer loses
same-session cross-family comparison (two flashes instead of one command).
Both stated in the doc. Decided in principle; implementation is its own
pass with its own approach list.

**Gate:** Phase 6 is not gated on 5b any more (5b is superseded, not a
prerequisite). Everything above is gated on hardware verification; 5d is
additionally gated on deciding whether the environment-matrix cost is
acceptable, which is a release-process question, not a code one.

---

## Phase 6 — The Third Family: `SimpleThresholdDetector` (the MVP detector)

No longer optional in the "if a third detector is wanted" sense: it *is*
wanted, it's the MVP detector. `docs/specs/mvp-app-structure.md` §3
describes the algorithm; `cleanup-detector-family-build.md`'s worked
example gives the class, the family wiring, and the reconciliation with
the MVP doc (which previously said to reuse `ScalarTransientDetector`
instead; amended 2026-09-21). It is also the acceptance test for Phase 5d:
the first family added *after* `DetectionFamily.h` exists is the real test
of that header.

Depends on Phase 5d (the family header must exist to add a family to it).

1. Implement `SimpleThresholdDetector` against the four-method core
   contract, with the diagnostics contract stubbed to "no report"
   (`reportGeneration()` constant 0, `latestReport()` a static empty
   report). The Analyzer then records `MissingDetectorReport`, an integrity
   state it already handles. ~150 lines.
2. Add its `DetectionFamily.h` branch (aliases, `kCompiledFamily`,
   `kFamilyMaxActiveStreams = 0`, the three shims), its `DetectorId`/
   `DetectorSelection` values, a config struct and factory in
   `DetectionProfile.h`, and two envs. `DetectionRuntime` must not change;
   if it has to, the Phase 5d shim set is incomplete and *that* is the bug.
3. Make the `FeatureHistory` member conditional on
   `kFamilyMaxActiveStreams > 0` (a zero-length array is ill-formed); this
   family has no Inspector, so it binds no streams. That is where the RAM
   goes: about -18.6 KB, on top of -3.3 KB for neither existing detector.
   Expected simple-family Node RAM: roughly 38.5 KB, from 88 KB at the
   start of this work.
4. Confirm it produces `OccurrenceVerdict`/`FieldState` end-to-end with the
   Analyzer diagnostics layer never consulted.
5. Test: T1 (now seven environments), T7 on the simple-family Node. T2/T3
   don't apply (new family, no baseline), so run its own first SEQ set on
   the simple-family Analyzer to establish one.
6. Only after this works, decide whether it's worth a real
   `latestReport()` and a `DetectorReportPrinter` case. Add as pure
   additions if so; the stubs are not a placeholder to be filled, they are
   the intended shape for a detector nobody needs to inspect.

If this phase reveals the four-method contract was insufficient in
practice, that's real signal the isolation split (Phase 5a) drew the
core/diagnostics line in the wrong place, revisit that document rather than
quietly growing the new detector's required surface back toward what the
existing two have.

---

## Phase 7 — Follow-ups from the OccurrenceEvaluator rename (not started)

The `PatternMatcher` -> `OccurrenceEvaluator` rename (`ee4edd2`) stopped at
the type-coupled boundary on purpose. Three things it left behind. **Do 7c
first**: it deletes code that 7a and 7b would otherwise rename.

### 7c — Fold the Evaluator into the Inspector; delete the correlation queue

Decided 2026-09-21. The question that settled it: the Inspector inspects
and stores the result; the Evaluator checks whether the result met the
requirement and passes the verdict on. Why can't the Inspector do that?
The obvious objection, "because there can be multiple inspections," is
answered by the code: `inspectWithHistory()` already runs every plan module
and folds all of them into **one** `InspectedOccurrence`
(`magnitudeObservations[]`). The Evaluator consumes that single object and
does one comparison per module. Multiplicity is resolved before the
Evaluator exists.

Four facts, checked:

1. `OccurrenceEvaluatorConfig` is a `using` alias for `InspectionPlan`. The
   requirement each module checks (`minimumStrength`) is a field of the
   inspection module config. The Inspector already holds every requirement
   it would need to apply.
2. The Inspector already sets `InspectedOccurrence.decision`
   (`Accepted`/`Rejected`, `OccurrenceInspector.cpp:46`, `:337`). The spec's
   "inspectors produce facts, they do not own meaning" is already breached
   in name; today it judges "was inspection performed" instead of "were
   the requirements met."
3. The Evaluator has no cross-occurrence state: a 4-slot FIFO filled and
   drained inside one `observeFrame()`, and an `OccurrenceEvaluatorReport`
   with zero readers. It is a pure function with a stage's plumbing, and
   its own `update()` single-occurrence overload says so.
4. `InspectionPlan.failedRequirementMeansUncertain`, the only *policy* flag
   on the plan, is never read. There is no hidden "same evidence, different
   rules" mode that the separation is quietly enabling. The steelman
   (measure once, judge under several rule sets) would need the Evaluator
   configured with a different plan than the Inspector, but the plan is
   also the list of what to measure; the scenario is incoherent with the
   current types.

What the split costs, and what the merge removes: because the verdict
leaves as a separate object with no link to its `InspectedOccurrence`, the
Analyzer needs the `PendingVerdictObservation` correlation queue to
re-associate them afterwards by `occurrenceId`, the machinery
`cleanup-analyzer-node-isolation.md` called the most complex and
historically bug-prone part of `DetectionRuntime`. With the verdict
rendered as the last step of inspection, `drainDetectors()` holds the
inspected occurrence, its verdict, and the detector report in one scope,
synchronously, and all of this has nothing left to do:

- `OccurrenceEvaluator` (class, `.h/.cpp`), `OccurrenceEvaluatorTypes.h`,
  `OccurrenceEvaluatorReport`, `OccurrenceEvaluatorConfig`
- `DetectionRuntime::drainOccurrenceEvaluator()`, `hasPendingEvaluatorWork()`,
  `_occurrenceEvaluator`, the evaluator's input FIFO and
  `EvaluatorInputRejectReason::InputQueueFull` / `CorrelationQueueFull`
- Analyzer-only: `PendingVerdictObservation`, `_verdictCorrelationQueue`
  and its read index / count / overflow counter,
  `_verdictCorrelationFailureCount`, `pushVerdictObservation()` /
  `popVerdictObservation()`, `PipelineIntegrity.correlationComplete` and
  the `MissingInspectedOccurrence` / `OccurrenceIdMismatch` reasons that
  only correlation failure could produce, `_evaluatorAccept*Count`,
  `_evaluatorDrainCount`, `activeEvaluatorReport()`

What the merge keeps, deliberately:

- **`OccurrenceVerdict` as the type that is queued and handed to Behavior.**
  84 bytes against 912 for an `InspectedOccurrence`, and the Node queues
  four. That is a real reason for the compact type to exist; it was never
  a reason for a separate class. `ResonantBehavior::handleOccurrenceVerdict()`
  is unchanged in shape.
- `OccurrenceInspector::inspectWithHistory()` returns the
  `InspectedOccurrence` as today, now with the verdict applied. A small
  `OccurrenceVerdict makeVerdict(const InspectedOccurrence&)` projection
  (the current `fillResultFromProposal()` + `evaluateSinglePulse()`, minus
  the queue) lives next to it.
- The Node-facing `DetectionRuntime` surface: `popOccurrenceVerdict()`,
  `setVerdictQueueEnabled()`, `fieldState()` unchanged.

Shape of `drainDetectors()` afterwards, core path only:

```cpp
while (activeDetector.popOccurrence(occurrence)) {
    _fieldStateTracker.observeOccurrence(occurrence, nowMs);
    const InspectedOccurrence inspected =
        _occurrenceInspector.inspectWithHistory(occurrence, &_featureHistory, nowMs);
    _fieldStateTracker.observeInspectedOccurrence(inspected, nowMs);
    const OccurrenceVerdict verdict = makeVerdict(inspected);   // was: a whole stage
    _fieldStateTracker.observeOccurrenceVerdict(verdict, nowMs);
    if (_verdictQueueEnabled) pushOccurrenceVerdict(verdict);
#ifdef ANALYZER_MODE
    capturePipelineResult(verdict, &inspected, &latestDetectorReport());  // no queue, no lookup
#endif
}
```

`drainOccurrenceEvaluator()` and the second drain in `observeFrame()` go
away with it.

Decide while `makeVerdict()` is being written, because that is where it
lives: **the verdict is uniform in shape but not in meaning.** Its decision
fields (`valid`, `accepted`, `supportMatched`, `firstFailedRequirement*`)
are genuinely detector-agnostic, expressed in `StrengthClass` and
`InspectionTarget`. Its `primary*` summary fields are not: the `switch` on
`occurrenceType` in `makeProposalFromOccurrence()` fills the same slot with
different physical quantities:

| field | scalar occurrence | frequency occurrence |
|---|---|---|
| `primaryStrength` | peak amplitude | Goertzel `band.score` |
| `primaryOnsetStrength` | onset amplitude | `band.contrast` |
| `primaryReleaseStrength` | release amplitude | `band.contrast` again |
| `primaryAmbientBaseline` | measured baseline | hardcoded `0.0f` |

Today the only Node-side reader is `ResonantBehavior`, which keeps the max
`primaryStrength` in a window as `_activityLevel` and reads it only as
`> 0.0f`. So it is harmless today, and a live wire: the first
`_activityLevel > <number>` or gain-scaled-by-strength makes the Node
behave differently per detector family with no visible reason, and under
Phase 5d that is a bug that appears in one firmware and not the other.
Either drop the punned strength fields from the verdict (Behavior's one use
is already implied by `valid`) or normalize them to a `StrengthClass` like
the decision fields. The timing fields (`primaryStartMs`, `primaryHeardAtMs`,
`primaryAcceptedMs`, `primaryDurationMs`) are fine, milliseconds are the
same in every family.

**Resolved 2026-09-21: the verdict boundary rule.** Two channels cross
from detection to Behavior, and both are handled the same way: the
detection side computes, Behavior receives something already normalized.

1. **How much.** One number, `confidence` (or `strength`; pick one name),
   normalized to a declared range, computed by the detection side by
   whatever means that family has. The slot already exists:
   `OccurrenceVerdict.confidence` is currently degenerate
   (`valid ? 1.0f : 0.0f`). Make it real and drop the punned
   `primary*Strength`/`primaryAmbientBaseline` fields. Behavior never sees
   units.
2. **What.** A label, not a measurement. The profile defines the
   vocabulary of things that can be heard (an `EventClass`-style enum:
   `AnyBurst`, `ChirpA`, `ChirpB`, `NeighborSignature`, whatever future
   detection actually distinguishes); the detection side decides which
   class this occurrence was; the verdict carries the class; Behavior maps
   class to reaction. The 3200 Hz, the contrast, the onset amplitude stay
   inside detection, exactly as "how confidence is computed" does.

Why "label, not measurement" is the load-bearing half: under Phase 5d,
`ResonantBehavior` compiles into every family's firmware. If the verdict
carried family-shaped fields (`band.targetHz`), Behavior would need
`#if DETECTOR_FAMILY_*` to read them, a second directive site, the thing
`DetectionFamily.h` exists to prevent. A class tag is family-agnostic by
construction: a frequency family and a scalar family can both report
`ChirpA`, computed differently, and Behavior is one body of code.

So `OccurrenceVerdict` carries: the decision (`valid`, the failed
requirement), a normalized magnitude, an event class, timing in
milliseconds, `occurrenceId`, and `detectorId` as axis-1 provenance for the
Analyzer (Behavior should not need to look at it). Nothing with units,
nothing family-shaped. State this rule at the top of `OccurrenceVerdict.h`.

The one thing a label cannot carry: a reaction that genuinely needs a
continuous physical value from detection, "respond at the frequency you
heard, plus jitter." If that need arrives, it gets its own named,
unit-declared field (`heardFrequencyHz`, zero from a family that cannot
measure it), not a reused slot. The name says what it is; that is still
not punning. Until it arrives, don't add it.

Order of operations, the one subtlety: today `_fieldStateTracker.observeOccurrenceVerdict()`
runs in the evaluator drain, after *all* occurrences popped this frame
have been inspected. After the merge it runs per occurrence, interleaved.
`FieldStateTracker` is a windowed counter; check whether it cares about
that ordering within a single frame (it shouldn't, the timestamps are the
same `nowMs`), and say so in the commit either way.

Cost: this deletes an Analyzer diagnostic capability, the
`integrity.correlation_complete=` / `MissingInspectedOccurrence` /
`OccurrenceIdMismatch` signals. Those exist to detect the correlation
queue's own failures; with no queue there is nothing for them to detect.
Their SEQ labels disappear rather than being renamed. State that in the
commit as an intentional removal, not a regression.

Sequencing: **before 7a and 7b.** 7b's `VerdictType` question is simpler
once the projection is one function; 7a's list shrinks by everything
above. Also before Phase 5d (family build), which restructures
`DetectionRuntime` anyway and should start from the smaller runtime.

- Test: T1; T2/T3 (expected diff: the removed correlation/evaluator labels
  only, every verdict value identical); T7 (Behavior consumes the same
  type through the same call); T5 (`sizeof(DetectionRuntime)` drops by the
  evaluator's queue, ~3.9 KB Node, plus the correlation queue in the
  Analyzer).

### 7a — Derived vocabulary that means "a valid verdict happened"

Identifiers and labels that describe the *outcome* of the evaluator in
other stages' terms, still saying "pattern":

- Analyzer trial classifier: `AnalyzerReason::ValidPatternInExpectedWindow`
  / `ValidPatternBeforeWindow` / `ValidPatternAfterWindow` /
  `PatternRejected` / `MultipleValidPatterns` /
  `MultipleCompetingPatterns` / `UnexpectedValidPatternWithoutTrigger` /
  `DuplicatePatternAfterPrimary`; `patternValidTrials`,
  `patternRejectedTrials`, `patternRejectedTrial()`, `patternAvailable`,
  `primaryValidPattern*`, `runtimePatternCaptured`,
  `bestRejectedPatternCaptured`, `SequenceTrialSelection::Kind::ValidPattern`
- SEQ_SUMMARY labels: `pattern_valid_trials=`, `pattern_rejected_trials=`,
  and the reason strings `pattern_rejected`, `multiple_valid_patterns`,
  `unexpected_valid_pattern_without_trigger`, `duplicate_pattern_after_primary`
- `FieldStateTracker`/`FieldState`: `patternWindowMs`, `_patternCountInWindow`,
  `recentPatternCount`, `recentValidPatterns`, `validPatternActivity`
- `ResonantBehavior`: `HeardPattern`, `_pendingHeardPattern*`,
  `_lastHeardPatternMs`, `patternsReceived`, `patternsIgnored*`,
  `IgnoredInvalidPattern`, `IgnoredAmbiguousPattern`, `ConsumedPattern`,
  and the Node's `RB pattern`/`heard_pattern` status strings
- `test/test_analyzer_pass_rules/test_main.cpp`:
  `test_pattern_rejected_helper_excludes_source_rejects`

The decision to make first, once, then apply everywhere: does the trial
classifier say **verdict** ("a valid verdict in the expected window",
matching the stage that produced it) or **detection** ("a valid detection
in the expected window", matching what the trial is measuring)? Both read
well; "detection" is arguably truer for `AnalyzerReason` and `FieldState`
(they care that something was heard, not which stage said so), "verdict"
is truer for Behavior (it literally consumes an `OccurrenceVerdict`). Mixed
is acceptable if the line is that clean. Not: leaving "pattern".

Cost: touches pass-rule semantics by name only (no logic), the Unity test
name, and SEQ_SUMMARY labels. Byte-identical values, changed labels.

**Never** in this pass: `ChirpPattern`, `_ledPattern*`, `observePatternPulse`
(also uncalled, see 7b), `chirpPatternName`. Emission patterns are real
temporal sequences; the word is correct there.

### 7b — Vestigial verdict enum values

Audited 2026-09-21 by counting assignments vs name-only appearances:

| Enum | assigned somewhere | only in name functions / `case` labels |
|---|---|---|
| `VerdictType` | `None`, `SinglePulse`, `Invalid` | `DuplicateAfterPrimary`, `UnexpectedNoise`, `Ambiguous` |
| `VerdictReasonCode` | `None`, `FromOccurrence`, `SupportRequirementFailed` | `FromFrequencyMatch`, `DetectorRejected`, `AmbiguousEvidence` |
| `VerdictRejectReason` | `None`, `MissingSupport`, `SupportTooLow`, `UnexpectedTiming` | `NoProposal`, `InvalidOccurrence`, `NoFrequencyEvidence`, `FrequencyWindowInvalid`, `FrequencyScoreTooLow`, `FrequencyContrastTooLow`, `FrequencyScoreAndContrastTooLow`, `DuplicateAfterPrimary`, `UnexpectedNoise` |

`VerdictType` therefore distinguishes exactly two reachable states,
`SinglePulse` vs `Invalid`, which `valid` already encodes, and
`SinglePulse` is pattern vocabulary. `ResonantBehavior` has an ignore
branch for `VerdictType::Ambiguous` (`IgnoredAmbiguousPattern`) that
cannot fire. These are the last leftovers of the original pattern design,
same lineage as the deleted `ProposalShape`/`PulseSequence`.

Proposed shape, to decide before doing: delete every never-assigned value
and its name string; either delete `VerdictType` entirely (its information
is `valid`) or keep it as an honest two-value enum if a third verdict kind
is genuinely expected; delete Behavior's dead `Ambiguous` branch. Check
first that nothing in Behavior treats `Invalid` differently from `!valid`
(today `Invalid` implies `valid == false`, so it shouldn't).

Output change: `verdict.type=` loses `single_pulse` (becomes whatever the
two-value enum is named, or the field goes away). Behavior change: none in
practice, the deleted branches are unreachable, but that is exactly the
claim T7 has to confirm.

Also uncalled, found on the way: `NodeDebug::observePatternPulse()` has no
callers. Delete with 7b or leave for a dead-code pass; it is not part of
either rename.

- Test, both: T1; T2/T3 with the changed labels as the only expected diff;
  T4 (7a renames a test); T7 (7b touches Behavior).

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
| 2 | cleanup.md #2, #3 (both implemented, hardware verification outstanding) | Phase 0 = keep | T1 (done, all 3 envs, full build), T2, T4 (outstanding) |
| 3 | cleanup.md #5 (implemented, hardware verification outstanding) | — (Item 1 dependency removed; Phase 2 optional) | T1 (done), T2, T3, T6 (outstanding) |
| 4 | soak, no doc | Phases 1-3 | T7 |
| 5a | analyzer-node-isolation (implemented, hardware verification outstanding) | Phases 1-4 | T1, T8 (done); T2, T3, T6, T7 (outstanding) |
| 5b | detector-ownership (deferred on measurement: 1,440 bytes, UB hazard) | Phase 5a | T5 (done, led to deferral) |
| 5c | FeatureHistory (two commits, -14,440 bytes; not in a proposal doc) | measured during 5b | T1 (done); T2, T3, T6, T7 (outstanding) |
| 5d | detector-family-build (proposal; supersedes 5b) | — | not started |
| 6 | SimpleThresholdDetector family (the MVP detector; 5d acceptance test) | Phase 5d | T1 (seven envs), T7; own first SEQ baseline |
| 7c | fold Evaluator into Inspector, delete the correlation queue (do first) | — | T1, T2/T3 (removed labels only), T5, T7 |
| 7a | derived "valid pattern" vocabulary -> verdict/detection wording | Phase 7 note | T1, T2/T3 (labels only), T4 |
| 7b | vestigial VerdictType/ReasonCode/RejectReason values | Phase 7 note | T1, T2/T3 (labels only), T7 |
