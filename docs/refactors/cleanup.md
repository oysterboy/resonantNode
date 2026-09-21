# Codex Pass — Detector Layer Cleanup

## Goal

Simplify the Detector layer (`DetectionRuntime`, `ScalarTransientDetector`,
`FrequencyMatchDetector`, `DetectorReport`, `Occurrence`) without changing
detection behavior. This pass follows up on the architecture review of
`docs/specs/myspec.md` vs. the current Detector code.

Items are ordered by urgency, not by file or by detector. Do each item in
order. Do not start a lower item before the one above it is verified.

Do not change detector thresholds, profile tuning, TonalPulse semantics, or
pattern/inspection behavior in any item below unless the item explicitly says
so. Every item must produce byte-for-byte identical SEQ_TRIAL/SEQ_SUMMARY
output on an unchanged 50-trial run unless noted otherwise.

A note on scope: `ScalarTransientDetector` and `FrequencyMatchDetector` are
explicitly allowed to diverge internally per the spec ("Generic outward
contract. Specialized detector internals."). Nothing in this pass asks the
two detectors' lifecycle, threshold, or gating logic to converge. Item 7
originally proposed merging bookkeeping between the two detectors and was
downgraded after review found real, deliberate differences between them (see
Item 7 for the corrected reasoning) — it is kept last and marked optional.

A second correction, found while implementing rather than while reviewing:
Item 1's `Occurrence` half was withdrawn on 2026-09-20 after direct evidence
showed `.scalar`/`.frequency` are both genuinely load-bearing on the same
occurrence for both stable profiles, not a detector-exclusive pair. See
Item 1 for the full evidence. Its `DetectorReport` half is unverified and
not scheduled until re-checked the same way. Both `cleanup-analyzer-node-isolation.md`
and `cleanup-detector-consolidation.md` referenced the original (wrong)
`Occurrence` union plan and have been corrected to match.

---

## Evidence base

Two existing lab notes independently confirm the RAM/stack risk targeted by
Item 1:

- `docs/roadmaps/notes` ("MEMORY STACK ANALYSER") measures
  `DetectionRuntime::resetDetectionState()` at a 1552-byte stack frame and
  both detector `resetState()` calls at 416 bytes each, nested directly under
  it, and names `resetDetectionState()` nesting as one of the two strongest
  candidates to reduce.
- `docs/lab/260903_notes` (`exp001-04`, `exp001-05/-06`) records a real
  `Stack canary watchpoint triggered (loopTask)` crash after sequence
  completion, with stack margin observed as low as 44 words, on the same
  reset/sequence-start path.

Item 1 below reduces the size of the structs copied and reset repeatedly on
that path. It does not by itself prove the crash is fixed; treat it as a
contributing fix, and keep `ARDUINO_LOOP_STACK_SIZE` at its current increased
value until the crash is independently reproduced as gone.

---

# Item 1 — Collapse always-both detail payloads (Occurrence: withdrawn, DetectorReport: needs re-verification)

Naming update (2026-09-21): the fields discussed throughout this item as
`Occurrence.scalar`/`.frequency` have since been renamed to
`Occurrence.magnitude`/`.band` (types `MagnitudeOccurrenceDetail`/
`FrequencyBandOccurrenceDetail`), specifically because their old names
collided with `DetectorId::ScalarTransient`/`FrequencyMatch` and invited the
exact wrong "detector-exclusive" mental model this item's correction below
describes. Every mention of `.scalar`/`.frequency` below and in the other
cleanup docs refers to what is now `.magnitude`/`.band`; not rewritten
throughout since it's a pure rename with no change to the underlying
finding. `DetectorReport.scalar`/`.frequency` were not renamed, those names
are accurate for that type.

## Correction (2026-09-20): the `Occurrence` half of this item is wrong, do not implement it

While preparing to implement this item, I found direct evidence that
`Occurrence.scalar` and `Occurrence.frequency` are not "whichever detector
produced this" alternatives, they are two independently-used **evidence
namespaces** (Amp-domain and Frequency-domain) that `OccurrenceInspector`
populates based on each configured `InspectionTarget`, regardless of which
detector produced the occurrence, and that `OccurrenceEvaluator` reads from
*both* namespaces for a *single* occurrence:

- `OccurrenceInspector::annotateScalarFeatureStrength()` switches on
  `InspectionTarget`, not on `DetectorId`/`OccurrenceType`:
  `InspectionTarget::Amp` writes `occurrence.scalar.*`;
  `TargetScore`/`Contrast`/`TargetBand` write `occurrence.frequency.*`.
- Both current stable profiles configure inspection modules spanning both
  namespaces on every occurrence they produce: `TonalPulseFreq` uses
  Amp + TargetScore + Contrast together; `TonalPulseScalar` uses
  Amp + Contrast together.
- `OccurrenceEvaluator::makePatternProposalFromOccurrence()` reads
  `source.scalar.strengthClass` **and**
  `source.frequency.scoreStrength`/`contrastQuality`/`targetBandStrength`
  in both its `OccurrenceType::Frequency` and `OccurrenceType::Scalar`
  branches, unconditionally, for the same occurrence.
- `FrequencyMatchDetector::capturePendingOccurrence()` writes its own
  `.frequency.score`/`.contrast` **and** a basic AMP reading into
  `.scalar.value`/`.baseline`/`.lift` on the same occurrence.

A tagged union between `.scalar` and `.frequency` would silently drop
whichever one lost the union race, on every occurrence, for both stable
profiles. This is not a storage-layout change, it would be a real detection
regression. Withdrawn. Do not implement a union for `Occurrence`.

The measured 72-byte-per-instance figure from the original analysis was
real as a `sizeof()` fact, but the "only one is ever meaningfully used"
premise behind proposing a union was false. The two namespaces are both
load-bearing.

## DetectorReport: plausibly still valid, but unverified, do not implement without redoing this check

`DetectorReport.scalar`/`.frequency` are a different pair of types
(`ScalarDetectorReportDetail`/`FrequencyMatchDetectorReportDetail`) from
`Occurrence`'s, and initial spot-checking suggests they may genuinely be
exclusive to whichever detector is active: `DetectorReportPrinter.cpp`
dispatches on `report->detectorId` before reading either, and
`FrequencyMatchDetector`/`ScalarTransientDetector` each only ever populate
their own `.frequency`/`.scalar` sub-struct in their own `buildReport()`.

But at least one read site does not gate on `detectorId` before reading:
`AnalyzerSeqReporter.cpp` reads `detector.scalar.inspect.carrierQualityRequired`
unconditionally whenever a `DetectorReport*` is non-null, regardless of
`detectorId`. That happens to be harmless today only because the
unpopulated struct defaults to `false`/`"none"` rather than something
misleading, not because the code was written to be union-safe.

Before doing anything with `DetectorReport`, redo the same
read-site-by-read-site check that caught the `Occurrence` problem: find
every place that reads `.scalar` or `.frequency` off a `DetectorReport`,
and confirm every single one either checks `detectorId` first or is
provably safe to read at its zero-value default. Do not assume the printer
dispatch pattern extends to every consumer just because it holds for the
ones already checked. This item stays open, not urgent, and not currently
scheduled, until that check is done.

## Intermediate Verification 1

Not applicable, no code change is made under this item until the
`DetectorReport` re-verification above is done and shows the union is
actually safe there. This item is currently a documentation-only correction,
not a pass to execute. Proceed to Item 2 directly; it does not depend on
this item.

---

# Item 2 — Fix diagnostics-enabled state mutation in FrequencyMatchDetector::update() (implemented 2026-09-21, hardware verification outstanding)

**Status:** code change landed. `attackScoreOk`/`attackContrastOk`/`attackOk`/
`releaseScoreOk`/`releaseContrastOk`/`releaseOk`/`evidenceOk` are now set
exactly once per call, from live `gates`, and are never touched by the
`_diagnosticsEnabled` block. `gateReason` is now also set exactly once from
live evidence (a 2-tier `no_frequency_evidence`/`freq_score_too_low`
reason, computed unconditionally); the previous 3-tier
`live_window_not_ready`/`no_frequency_evidence`/`freq_score_too_low`
classification depended on `bestEvidence`'s cross-call persistence, which is
inherently diagnostics-only state, so it has no live equivalent and was not
reproduced. The diagnostics block keeps its own "best evidence so far"
tracking (`bestEvidence`/`bestEval`, both already diagnostics-scoped) for
`wouldPendingReason` and the `diagnosticsScoreOkCount`/`ContrastOkCount`/
`BothOkCount`/`MatchedCount` aggregate counters, unchanged in behavior.

**This changes canonical output.** `DetectorReport.frequency.inspect.gateReason`/
`readyOk`/`gateOpen` (printed as `SEQ_SOURCE_SPEC`'s
`detail.frequency.inspect.gate_reason`/`ready_ok`/`gate_open`) previously
reflected the diagnostics-only best-evidence re-evaluation whenever
`_diagnosticsEnabled` was true, which defaults to `true`
(`AnalyzerModeApp.h`'s `diagnosticsEnabled = true`). After this fix they
always reflect the live per-sample gate state instead. This is the bug
being fixed, not a regression, but it means `TonalPulseFreq` SEQ trials are
**not** expected to be byte-identical on these three fields under default
settings; `accepted`/`selectedReject` truth and every other field should be
unaffected. Verify with T2 and this item's own Intermediate Verification 2
(the two-run diagnostics-forced-on/off comparison) before trusting this,
compilation alone does not confirm the new live reason classification
matches real trial behavior. Not yet run against hardware.

## Problem

In `FrequencyMatchDetector::update()`
(`src/detection/detectors/frequency/FrequencyMatchDetector.cpp`), the
lifecycle decision for the current call uses `attackScoreOk`, `attackOk`,
`releaseScoreOk`, `releaseOk`, and `gateReason` computed from live evidence
earlier in the function. Later in the same function, a block gated by
`if (_diagnosticsEnabled)` re-evaluates `FrequencyMatchCriteria::evaluate()`
against a separately tracked "best evidence so far" snapshot and overwrites
those same member fields.

This does not corrupt the decision already made in the current call, but it
means the detector's persisted gate state between calls differs depending on
whether diagnostics is enabled, coupling a debug-only feature to detector
state that report-building and reject-summary code (`updateBestRejectedPending`,
`buildReport`) may read on a later call.

## Required Change

Introduce a separate, explicitly-named set of fields for the
diagnostics-only "best evidence" gate snapshot (for example
`diagnosticsBestAttackScoreOk`, `diagnosticsBestGateReason`, or a small
`FrequencyDiagnosticsSnapshot` struct). The diagnostics block must write only
to these new fields. `attackScoreOk`, `attackOk`, `releaseScoreOk`,
`releaseOk`, and `gateReason` must be written exactly once per `update()`
call, from live evidence, regardless of `_diagnosticsEnabled`.

Update any diagnostic print path that reads the "best evidence" gate state to
read the new fields instead.

## Intermediate Verification 2

1. Run the same trial with `_diagnosticsEnabled` forced `true` and forced
   `false` across two runs with identical input (use a captured/replayed RAW
   feature log if available, or two consecutive runs of the same physical
   trial setup).
2. Confirm `DetectorReport.accepted`/`selectedReject` truth for the trial is
   identical in both runs.
3. Confirm diagnostics-only output changes only the diagnostic line, not
   canonical SEQ_SOURCE/SEQ_INSPECT fields.

---

# Item 3 — Bring FrequencyMatchDetector's public surface in line with ScalarTransientDetector (implemented 2026-09-21, T2-T4 hardware verification outstanding)

**Status:** implemented. Confirmed via search that no file outside
`src/detection/detectors/frequency/*.cpp` reads any `FrequencyMatchDetector`
field directly (only `DetectionRuntime._frequencyDetector`'s method calls:
`resetState`/`resetRejectSummary`/`setDiagnosticsEnabled`/
`resetDiagnosticsSummary`/`update`/`latestReport`/`reportGeneration`/
`popOccurrence`/`hasPendingOccurrence`). All ~70 fields moved to `private`,
`_`-prefixed, in the four groups this item names (lifecycle/config state,
candidate/occurrence/report state, best-rejected summary, diagnostics),
rebuilding after each group. Two design notes:

- `pendingOccurrence` (the in-progress candidate built during `update()`)
  is renamed `_pendingCandidateOccurrence`, not `_pendingOccurrence`,
  because that name was already taken by the existing private
  `_pendingOccurrence` (the captured, ready-to-pop snapshot). These remain
  two distinct objects, unchanged from before this item; Item 3 is
  encapsulation only, not a merge of the two.
- `frequencyRejectReasonFromState()` was a free function in
  `FrequencyMatchReport.cpp`'s anonymous namespace that read detector state
  directly; since it isn't a member, privatizing the fields it read broke
  it. Converted to a private member function
  (`FrequencyMatchDetector::frequencyRejectReasonFromState() const`) rather
  than adding a public accessor or a `friend` declaration (friending a
  same-named anonymous-namespace function is unreliable — the anonymous
  namespace makes it a distinct entity from a global-scope friend
  declaration). `frequencyRejectClassFromReason()` needed no change, it
  only takes a reason string, not a detector reference.

Verification: compiled clean against the real toolchain after each of the
four groups (syntax-only checks plus a final full real build of all three
PlatformIO environments — `esp32dev`, `esp32dev-emitter`,
`esp32dev-analyzer` — all linked and produced a firmware image, satisfying
T1). No field, public or private, was added to compensate. T2 and T4
(hardware SEQ/Unity runs) are still outstanding, same as Item 2.

## Problem

`FrequencyMatchDetector.h` exposes roughly 70 raw public fields (`pendingState`,
`gateReason`, `bestPeakScore`, `evidencePresent`, and so on), justified by a
comment claiming external code reads them directly. No code outside the
class currently reads them (verified by search at review time). This is the
detector backing the stable production profile (`TonalPulseFreq`), so it is
the highest-value target for the encapsulation pattern `ScalarTransientDetector`
already uses: private `_`-prefixed members, a narrow public method surface
(`update`, `buildReport`, `latestReport`, `reportGeneration`, `popOccurrence`,
`hasPendingOccurrence`, plus the `set*`/`reset*` methods it needs).

This item is about encapsulation of one detector's own internals, not about
detector-to-detector divergence — it does not ask `FrequencyMatchDetector` to
look more like `ScalarTransientDetector` internally, only to stop exposing
state nothing outside the class reads.

## Required Change

1. Confirm via search that no file outside
   `src/detection/detectors/frequency/*.cpp` reads any `FrequencyMatchDetector`
   field directly. Re-run this check after Item 1 and Item 2, since both
   touch this file.
2. Move all fields not required by the public contract above to `private`,
   prefixed with `_` to match `ScalarTransientDetector` convention.
3. Keep the three `.cpp` files (`FrequencyMatchDetector.cpp`,
   `FrequencyMatchOccurrence.cpp`, `FrequencyMatchReport.cpp`) working against
   the now-private members; they are already part of the same class and can
   access private members directly.
4. Do this incrementally, one logical group of fields at a time (lifecycle
   state, then pending/candidate facts, then best-rejected summary, then
   diagnostics), rebuilding after each group.

## Intermediate Verification 3

1. Build succeeds after each field group is privatized.
2. Run the 50-trial `TonalPulseFreq` SEQ test; output identical to baseline.
3. Confirm no new public field was added to compensate — if a field is
   needed publicly, it should be exposed through `DetectorReport`, not
   through the detector instance.

---

# Item 4 — Remove duplicated dead helper functions

## Problem

`frequencyRejectReasonFromState` is defined identically in three files:
`FrequencyMatchDetector.cpp`, `FrequencyMatchOccurrence.cpp`, and
`FrequencyMatchReport.cpp`. Only the copy in `FrequencyMatchReport.cpp` is
ever called. `frequencyRejectClassFromReason` is defined identically in
`FrequencyMatchDetector.cpp` and `FrequencyMatchReport.cpp`; only the
`FrequencyMatchReport.cpp` copy is called.

This is dead code duplicated within one detector's own files, not divergence
between the two detectors.

## Required Change

Delete the unused copies in `FrequencyMatchDetector.cpp` and
`FrequencyMatchOccurrence.cpp`. Keep the single definitions in
`FrequencyMatchReport.cpp` (or move them to a shared internal header if a
second call site appears later — not needed today).

## Intermediate Verification 4

Build succeeds; no behavior change is possible from this item since the
removed code was unreachable. A clean compile with no unused-function
warnings is sufficient verification.

---

# Item 5 — Unify the per-detector switch statements in DetectionRuntime (implemented 2026-09-21, T2/T3/T6 hardware verification outstanding)

**Status:** implemented, with two deliberate deviations from the sketch
below:

- `ActiveDetectorAdapter` exposes only `popOccurrence()`, not
  `hasPendingOccurrence()`. `hasPendingDetectorOutput()`'s existing 6-line
  switch already matched the shape this item targets, but routing it
  through the adapter would need the adapter to hand out `const`-safe
  access too (that method is `const`, `popOccurrence()` isn't), and that
  method wasn't named as unify-worthy duplication in the Problem statement
  (it's small, single-branch-per-case, not the ~30-line repeat
  `drainDetectors()` had). Left it switch-based rather than add
  const/non-const complexity to the adapter for a case this item didn't
  ask for.
- The adapter is a lightweight value constructed fresh inside
  `drainDetectors()` from `_detectorSelection` and references to both
  detector members (`ActiveDetectorAdapter activeDetector(_detectorSelection,
  _frequencyDetector, _scalarDetector);`), not a persistent member bound
  once in `setDetectorSelection()`. Both detectors are fixed-address
  members of `DetectionRuntime` for its whole lifetime, so there's no
  lifecycle to manage; a persistent bound adapter member would need to be
  re-bound on every `setDetectorSelection()` call for no behavioral
  benefit over constructing the (2-pointer-sized) view on the stack each
  time `drainDetectors()` runs.

`observeFrame()`'s `update(...)` dispatch switch is untouched, as
specified. `latestReport()` inside `drainDetectors()`'s loop stays
switch-based (a ternary on `_detectorSelection`, not routed through the
adapter), per this item's own note that report access is diagnostics-only
and out of scope here.

Verified: compiled clean against the real toolchain, then a full real
build of all three PlatformIO environments (all linked, all produced a
firmware image) — satisfies T1. T2, T3 (hardware SEQ regression on both
profiles) and T6 (profile-switch soak) are still outstanding.

## Problem

`DetectionRuntime::observeFrame()` and `DetectionRuntime::drainDetectors()`
each branch on `_detectorSelection` with two cases that are structurally
identical, differing only in which detector's `update`/`popOccurrence`/
`latestReport` is called. `drainDetectors()` in particular repeats about 30
lines of field-state/inspector/pattern-matcher wiring per branch.

This duplication lives in the coordinator, not in either detector, and it
already operates on an interface (`popOccurrence`, `hasPendingOccurrence`,
`latestReport`, `reportGeneration`) that both detectors already implement
identically today. Removing it does not ask either detector's internals to
converge.

## Required Change

Introduce a minimal internal adapter used only inside `DetectionRuntime`,
not a public `IDetector` interface and not a change to either detector's
public contract:

```cpp
struct ActiveDetectorAdapter {
    bool hasPendingOccurrence() const;
    bool popOccurrence(detection::Occurrence& out);
};
```

`latestReport()`/`reportGeneration()` are deliberately not part of this
adapter. `docs/refactors/cleanup-analyzer-node-isolation.md` found that
neither `OccurrenceVerdict` nor `FieldState` is ever built from `DetectorReport`,
so report access is a diagnostics-only concern, not part of the core drain
path this item unifies. `drainDetectorReportEvents()`'s use of
`latestReport()`/`reportGeneration()` stays switch-based (or moves to the
diagnostics layer entirely, per that document) rather than going through
this adapter.

Implement it as a small class or a pair of free functions holding a pointer
to whichever detector is active, selected once in `setDetectorSelection()`.
Route `observeFrame()`'s detector-specific `update(...)` calls (which have
genuinely different signatures per detector and should stay specialized)
through the existing switch, but replace the duplicated drain/report-capture
logic in `drainDetectors()` with one code path against the adapter.

Do not force `ScalarTransientDetector::update()` and
`FrequencyMatchDetector::update()` to share a signature. The spec explicitly
allows their `update()` inputs to remain specialized.

## Intermediate Verification 5

1. Run both 50-trial SEQ tests (`TonalPulseFreq`, `TonalPulseScalar`);
   output identical to baseline.
2. Confirm `drainDetectors()` has one drain loop body, not two.

---

# Item 6 — Review DetectionRuntime diagnostic counter load

## Problem

`DetectionRuntime` carries about twenty free-standing diagnostic counters
plus a `PipelineIntegrity` struct per event. The counters are consumed by
exactly one debug line (`AnalyzerSystemReporter.cpp`'s `SEQ REPORT` output)
and one overflow-count read (`AnalyzerSequenceSession.cpp`). This is
reasonable for an actively-tuned device, but it should be revisited
periodically rather than left to grow unbounded.

## Required Change (this pass: audit only, no code change required)

Produce a short table in the commit notes listing each counter in
`DetectionRuntime.h`, its single consumer, and a recommendation: keep,
consolidate into a single struct, or remove. Do not remove any counter in
this pass unless it has zero consumers — if a zero-consumer counter is
found, delete it as part of this item and note it in Intermediate
Verification 6.

## Intermediate Verification 6

If any counter was deleted: build succeeds, and the `SEQ REPORT` line
compiles and prints unchanged for all remaining counters.

---

# Item 7 — (Optional, lowest priority) Shared candidate-coverage tracking

## Problem, and why this is downgraded

`ScalarTransientDetector::updateCandidateFacts/resetCandidateFacts/finalizeCandidateFacts`
and `FrequencyMatchDetector::updatePendingFacts/resetPendingFacts/finalizePendingFacts`
compute a structurally similar family of statistics: peak, mean, rms,
coverage time above attack/release thresholds, island count, gap count,
longest island, largest gap.

This was originally written up as straightforward duplication of one
algorithm. On closer review that framing was wrong, and this item is kept
only for completeness:

- `ScalarTransientDetector` operates on `audioSamplePacket.timeUs`,
  microsecond per-sample timing, because it reacts to raw audio directly.
  `FrequencyMatchDetector` operates on millisecond evidence-window
  timestamps, because frequency evidence arrives in coarser measurement
  packets. The unit difference is a correct reflection of each detector's
  actual input resolution, not an inconsistency to fix.
- `ScalarTransientDetector` tracks an additional "matched mean strength"
  (mean filtered to samples above the release threshold) that
  `FrequencyMatchDetector` does not, because scalar's `requireMinStrength`/
  `minMatchedMeanStrength` gating needs it and frequency's gating does not.
- `FrequencyMatchDetector` tracks two peak dimensions (score and contrast,
  with a tie-break rule between them) where scalar tracks one.

The two detectors are explicitly allowed to diverge internally per the spec
("Generic outward contract. Specialized detector internals."), and here they
already have diverged in real, load-bearing ways. A shared tracker would
need a time-unit parameter, an optional matched-mean feature, and a
pluggable peak comparator, which risks becoming a worse abstraction than the
current duplication.

## Required Change

None for this pass. Do not extract a shared tracker now.

Revisit only if a third detector needs the same family of statistics, or if
the two detectors' coverage/island/gap bookkeeping turns out to disagree on
a case where it should agree (a real bug found in the field), whichever
comes first.

## Intermediate Verification 7

None required; no code changes are made under this item in this pass.

---

# Item 8 — Collapse the verdict correlation queue into a single carried unit

## Problem

`DetectionRuntime::drainDetectors()` and `drainOccurrenceEvaluator()` push two
different pieces of what is conceptually one event into two independent
bookkeeping structures and re-associate them later by `occurrenceId`:

- `drainDetectors()` builds a `PendingVerdictObservation` (already a bundle
  of `InspectedOccurrence` + `DetectorReport`) and pushes it onto the
  `_verdictCorrelationQueue` ring buffer via `pushVerdictObservation()`
  (`DetectionRuntime.cpp:774`), at the point where `observation.detectorReport`
  is filled from a `_detectorSelection == DetectorSelection::FrequencyMatch
  ? ... : ...` ternary — the one piece of per-detector access Item 5
  deliberately left switch-based, since report access is diagnostics-only
  (see the adapter's own comment).
- `_occurrenceEvaluator.acceptOccurrence(inspected)` separately queues the
  `InspectedOccurrence` inside `OccurrenceEvaluator`'s own internal queue.
- `drainOccurrenceEvaluator()` later pops an `OccurrenceVerdict` from
  `_occurrenceEvaluator` and calls
  `popVerdictObservation(result.occurrenceId, matchedObservation)`
  (`DetectionRuntime.cpp:786`), a linear scan over up to
  `kResultQueueCapacity` (4) entries matching by ID, to reunite the result
  with the observation that produced it.

`cleanup-analyzer-node-isolation.md` already identified this exact
machinery as "arguably the most complex and historically bug-prone part of
`DetectionRuntime`" and traced that neither `OccurrenceVerdict` nor
`FieldState` (the two things `ResonantNodeApp`/`ResonantBehavior` actually
consume) depend on the result of this correlation at all —
`pushOccurrenceVerdict()` and `_fieldStateTracker.observeOccurrenceVerdict()`
both run off the bare `OccurrenceVerdict`, before correlation happens. The
entire mechanism exists to attach a `DetectorReport`/`InspectedOccurrence` to
the diagnostic `DetectionPipelineEvent` built in `capturePipelineResult()`.

Update, 2026-09-21: since this item was first written, that same document's
Phase 5a landed — both sides of this queue (`pushVerdictObservation()`,
`popVerdictObservation()`, and the `PendingVerdictObservation` construction
in `drainDetectors()`) are now entirely inside `#ifdef ANALYZER_MODE`. That
sharpens this item rather than obsoleting it: it is now provably a
zero-Node-impact simplification of Analyzer-only diagnostics machinery, not
a change with any reach into the Node's compiled behavior path.

`hasPendingEvaluatorWork()`'s own comment (`DetectionRuntime.cpp:550-557`)
already documents that this two-structure split can legitimately desync
("the correlation queue... can legitimately diverge from what the matcher
itself still has queued... for example, when `pushVerdictObservation()`
fails while `acceptOccurrence()` already succeeded"), and
`_verdictCorrelationQueueOverflowCount`/`_verdictCorrelationFailureCount`
exist specifically to notice when it does. That is a symptom being
monitored, not a root cause being fixed: correlation can fail because the
two halves of one event are carried in two independently-sized,
independently-drained structures instead of one.

This is the same category of problem as the `FrequencyMatchDetector`
accept-path report-freeze gap fixed separately (Item 2/3 territory): two
things that must be kept in sync by hand (there, two detector code paths;
here, two queues) and, per the comment above, are already known to be able
to fall out of sync.

## Required Change

Scoped to `DetectionRuntime` only — no change to `OccurrenceEvaluator`'s
public contract, `OccurrenceVerdict`, or `FieldState` shape, and no
dependency on the larger Node/Analyzer build split proposed in
`cleanup-analyzer-node-isolation.md` (that proposal has already landed for
Phase 5a; this item is independent of anything still open in it). This item
is a smaller, immediately actionable step that stays useful on its own.

1. Carry the `PendingVerdictObservation` alongside the occurrence through
   the existing drain path instead of pushing it to a second structure keyed
   by ID — for example, by extending what
   `_occurrenceEvaluator.acceptOccurrence()` queues internally to hold the
   observation it already has in hand at push time, and returning it
   unchanged from `popOccurrenceVerdict()` alongside the `OccurrenceVerdict`.
2. Delete `_verdictCorrelationQueue`, `pushVerdictObservation()`,
   `popVerdictObservation()`, and
   `_verdictCorrelationQueueOverflowCount`/`_verdictCorrelationFailureCount`
   once nothing reads them.
3. `capturePipelineResult()` keeps building the same diagnostic
   `DetectionPipelineEvent` from the (now directly-available) observation;
   its output should be unchanged.
4. This item does not touch `ActiveDetectorAdapter` (Item 5). That adapter
   deliberately stops at `popOccurrence()`; `latestReport()` access for
   `observation.detectorReport` stays exactly where Item 5 left it (the
   `_detectorSelection == DetectorSelection::FrequencyMatch ? ... : ...`
   ternary), since Item 5's own reasoning for keeping report access
   switch-based (diagnostics-only, out of scope) applies here too. Don't
   fold report access into the adapter as a side effect of this item.

Item 5 has landed, so this item is unblocked. Do it against the current
unified `drainDetectors()` loop.

## Intermediate Verification 8

1. Run both 50-trial SEQ tests (`TonalPulseFreq`, `TonalPulseScalar`) on the
   Analyzer build; `SEQ_TRIAL`/`SEQ_SOURCE`/`SEQ_INSPECT`/`SEQ_EXPLAIN`/
   `SEQ_SUMMARY` output identical to baseline.
2. Confirm `_verdictCorrelationFailureCount` (or its replacement) cannot be
   nonzero by construction, not just by observation on this run — the
   structure split it was measuring should no longer exist.
3. Confirm `OccurrenceVerdict`/`FieldState` consumption in
   `ResonantNodeApp.cpp` is byte-for-byte unchanged; this item must not
   touch the Node-facing contract. Since the touched code is entirely
   `ANALYZER_MODE`-gated, the Node/Emitter binaries should not even need a
   rebuild to confirm this, only a re-read of the diff.

---

# Item 9 — Split Item 6's counter audit: measurement counters vs. correlation/dedup state

## Problem

Item 6 treats `DetectionRuntime`'s roughly twenty diagnostic fields as one
homogeneous group and asks for a single keep/consolidate/remove table. Two
different kinds of field are mixed together there:

- **Measurement counters** (`_observeFrameCount`, `_freshDetectorInputCount`,
  `_detectorDrainCount`, `_evaluatorDrainCount`, `_detectorOccurrencePoppedCount`,
  and similar): monotonic tallies read by `AnalyzerSystemReporter.cpp`'s
  `SEQ REPORT` line. These are legitimately a "keep, consolidate, or remove
  by usefulness" decision, per Item 6 as written.
- **Correlation/dedup-tracking state** (`_lastObservedScalarReportGeneration`,
  `_lastObservedFrequencyReportGeneration`, `_lastEmittedAcceptedOccurrenceId`,
  `_lastEmittedAcceptedReportGeneration`, `_lastEmittedSelectedRejectOccurrenceId`,
  `_lastEmittedSelectedRejectReportGeneration`): these are not measurements,
  they are the mechanism `captureLatestDetectorReportIfChanged()` and
  `capturePipelineResult()`/`drainDetectorReportEvents()` use to decide
  whether a `DetectorReport`/event has already been emitted. Their
  usefulness question isn't "is this counter worth keeping" but "does this
  bookkeeping stop being necessary once Item 8 removes the two-structure
  correlation it exists to guard against."

Folding both kinds into Item 6's single audit table risks a "keep" verdict
on the generation-tracking fields for the wrong reason (they have an
internal consumer, so they look load-bearing) when the actual question is
whether that consumer itself can go away.

## Required Change (this pass: audit only, no code change required, same as Item 6)

Run Item 6's audit as two separate tables instead of one:

1. Measurement counters, exactly as Item 6 specifies.
2. Correlation/dedup-tracking fields, with the recommendation column
   answering "does this field's sole purpose disappear once Item 8 lands,"
   not "is this field currently read by something." List
   `captureLatestDetectorReportIfChanged()`'s and `capturePipelineResult()`'s/
   `drainDetectorReportEvents()`'s generation comparisons explicitly as the
   consumers to check against.

Do not remove anything under this item; it is a classification pass that
feeds Item 8 and any future `cleanup-analyzer-node-isolation.md` work, the
same relationship Item 6 already has to that document's "Suggested
Approach" step 2.

## Intermediate Verification 9

Two short tables exist in the commit notes (measurement counters;
correlation/dedup-tracking fields), each field assigned to exactly one
table, no code change.

---

# Non-Goals

- No threshold tuning.
- No changes to carrier-quality rules, AMP class, or contrast class logic.
- No profile redesign or new `DetectionProfileKind`.
- No forced `IDetector` interface or type-erased detector graph — the spec
  explicitly defers this, and Item 5 stays internal to `DetectionRuntime`.
- No change to `OccurrenceEvaluator`, `FieldStateTracker`, or Analyzer
  classification logic beyond the read-site updates required by Item 1.
- No merging of `ScalarTransientDetector` and `FrequencyMatchDetector`
  lifecycle, threshold, or gating logic under any item in this pass.
- No silent behavior change hidden behind a refactor — every item requires
  identical SEQ output on the stated verification runs unless the item says
  otherwise.

---

# Suggested Commit Sequence

```text
DetectionFix: separate diagnostics gate snapshot from live gate state
DetectionCleanup: privatize FrequencyMatchDetector public field surface
DetectionCleanup: remove dead duplicated frequency reason helpers
DetectionCleanup: unify per-detector drain path in DetectionRuntime
DetectionCleanup: audit and trim DetectionRuntime diagnostic counters
DetectionCleanup: collapse verdict correlation queue in DetectionRuntime
DetectionDocs: split diagnostic-counter audit into measurement vs. dedup-state tables
```

Each commit must compile and pass its corresponding Intermediate
Verification before proceeding to the next item. Item 7 has no commit; it is
recorded as a deliberately deferred decision. Item 8 depended on Item 5
landing first; that dependency is satisfied. Item 9's commit is
documentation-only, same as Item 6.
