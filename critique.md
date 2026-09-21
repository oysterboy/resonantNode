# ResonantNode — Independent Critique

**Method note:** This critique was written from the actual source under `src/`
(≈19,000 lines of C++), reasoned about directly, not derived from conformance
to `docs/specs/myspec.md` or any other in-repo document. Where a doc is cited
below, it is cited as *evidence of a problem* (a stale claim, an admitted but
un-diagnosed symptom), never as the standard the code is measured against.
The standard applied throughout is: does the code do what it claims to do,
is it no more complex than the problem requires, and can it be trusted.

---

## 1. A confirmed correctness bug in the "stable" detector path

`FrequencyMatchDetector` — the detector behind `TonalPulseFreq`, the profile
`docs/roadmaps/implementation-status.md` and `myspec.md` both call the
"stable active" / "main runtime profile" — never refreshes its cached
`DetectorReport` when it **accepts** an occurrence.

Evidence:

- `FrequencyMatchDetector::freezeReport()` (`src/detection/detectors/frequency/FrequencyMatchDetector.cpp:523`)
  is the only place `_latestReport` is rebuilt and `_reportGeneration` is
  bumped (besides the full reset in `clearFrozenReport()`).
- It is called from exactly one place in the whole detector:
  `recordRejectedPending()` (`FrequencyMatchDetector.cpp:272-276`), which is
  itself only reached from `closePending()`'s `if (!accepted)` branch
  (`FrequencyMatchDetector.cpp:351-353`).
- The accept path — `capturePendingOccurrence()` in
  `src/detection/detectors/frequency/FrequencyMatchOccurrence.cpp:3-44` —
  populates `_acceptedOccurrence` / `_acceptedDetail` and never calls
  `freezeReport()` or touches `_reportGeneration`.

Contrast with `ScalarTransientDetector`, which does this correctly: it calls
`freezeReport()` from **both** its reject path
(`ScalarTransientOccurrence.cpp:161`) and its accept path
(`ScalarTransientOccurrence.cpp:204`, inside `capturePendingOccurrence()`).
The two sibling detectors that are supposed to share one outward contract
(`DetectorReport`, per the report boundary both are meant to honor) do not
behave the same way at the one moment that matters most — acceptance.

**Why this matters:** `DetectionRuntime::captureLatestDetectorReportIfChanged()`
(`src/detection/DetectionRuntime.cpp:137-158`) only pulls a fresh
`DetectorReport` when `reportGeneration()` has changed. Because
`FrequencyMatchDetector` never bumps its generation on accept, the runtime's
cached `_detectorReport` — and everything downstream that reads it
(`DetectionRuntime::drainDetectors()`'s mismatch check at
`DetectionRuntime.cpp:494-498`, the pipeline-event correlation records, and
`SEQ_SOURCE`/`SEQ_EXPLAIN` analyzer output) — can and will show stale detector
state (an empty or previous-reject `DetectorReport`) for a trial that the
detector actually just accepted. The `_detectorReportMismatchCount`
diagnostic counter this codebase built specifically to catch this class of
problem will fire on essentially the first accepted frequency occurrence
after a reset or after a prior reject cycle.

**Corroboration from the repo's own status tracking:**
`docs/roadmaps/implementation-status.md` lists, as a "planned" item:
"Detector/report consistency — Investigate clean-summary acceptance
mismatches without retuning thresholds." The team has observed the symptom
(acceptance mismatches in the clean summary output) without having found the
cause. The code above is a direct, mechanical explanation for that symptom.
This is worth fixing before any further diagnostics work is layered on top
of `SEQ_SOURCE`/`SEQ_EXPLAIN`, because right now those tools are reporting
false mismatches for the one profile the project considers production-stable.

The practical blast radius is bounded to reporting/diagnostics: `PatternResult`
(which drives `ResonantBehavior`, i.e. actual sound-reaction behavior) is
built from `InspectedOccurrence`/`Occurrence`, not from `DetectorReport`, so
the node's actual react-to-sound behavior is not affected. But a large
fraction of this codebase's bulk (see §3) exists specifically to produce
trustworthy `DetectorReport`-derived diagnostics, and for the default profile
that machinery is currently lying to itself.

---

## 2. The project's own documentation is not a reliable source of truth

This is relevant background for why this critique deliberately avoided
grounding itself in `myspec.md`: the in-repo docs disagree with the code.

- `docs/roadmaps/implementation-status.md` lists "Params/commands/config" as
  **deferred**, with the note "`ParamRegistry`, `CommandRouter`, persistent
  config, remote params, and typed tuning structs are not implemented yet."
  But `src/param/ParamRegistry.h`/`.cpp` exist, are wired into
  `Node::registerDetectionParams()` and `Node::handleParamCommand()`
  (`src/modes/resonant/ResonantNodeApp.cpp:498-508, 993-1071`), and are
  reachable at runtime via the `PARAM LIST|GET|SET|DUMP` serial commands. The
  git history confirms this landed the day before the doc that still calls it
  unimplemented (`052b03e Param: add lightweight flat ParamRegistry...` and
  `72e8dc1 Docs: import VEKTOR Core Spec v1.1, align param/VEKTOR roadmaps to
  it`, both 2026-09-20).
- `myspec.md` §1 frames the firmware as "the first reusable VEKTOR Node
  firmware reference architecture" and closes with "serving as the first
  reusable firmware architecture for future VEKTOR nodes." Nothing in `src/`
  implements any part of the VEKTOR protocol described in
  `docs/specs/vektor-spec.md`: there is no CMD/STATE/EVENT/ACK/ERR framing, no
  AXIS/LAMP/SCALAR/SENSOR/SYSTEM resource model, no transport layer, no hub
  client. The constructors in `main.cpp` hardwire specific GPIO pin numbers
  for one physical board (`Node app(34, 2, 25, 26)`); nothing about the
  current code is generic across node types. "Reusable reference
  architecture" is aspirational language, not a description of what exists.

Neither of these is fatal on its own, but together they establish that this
repo's documentation lags or overstates the code often enough that it should
not be trusted as ground truth by a reader (or a future coding agent) without
independent verification — which is exactly the instruction this critique was
given.

---

## 3. The runtime is architected for a scale of problem it doesn't have

`DetectionRuntime` (`src/detection/DetectionRuntime.h/.cpp`, ~745 + 270
lines) coordinates a pipeline that, at any given time, runs **exactly one**
active detector (`_detectorSelection` is a two-way switch) producing **at
most one** occurrence at a time on a single-core embedded loop that processes
samples serially. For that problem, the runtime maintains:

- two parallel bounded queues (`_pipelineEventQueue`, `_patternInspectedQueue`)
  plus a `_resultQueue`, each with its own overflow counter;
- a hand-rolled correlation mechanism (`popPatternObservation()`,
  `DetectionRuntime.cpp:713-740`) that linearly scans a 4-slot ring buffer to
  match a `PatternResult` back to the `InspectedOccurrence`/`DetectorReport`
  that produced it, purely because the pattern-result path and the
  detector-report path are threaded through the runtime as two independently
  drained data flows that can (per the comment at `DetectionRuntime.cpp:468-476`)
  legitimately diverge from each other;
- generation counters (`_lastObservedScalarReportGeneration`,
  `_lastObservedFrequencyReportGeneration`, `_lastEmittedAcceptedOccurrenceId`,
  `_lastEmittedAcceptedReportGeneration`, `_lastEmittedSelectedRejectOccurrenceId`,
  `_lastEmittedSelectedRejectReportGeneration`, ...) to deduplicate re-emission
  of events across drains;
- roughly twenty separate `uint32_t`/`unsigned long` counters
  (`_observeFrameCount`, `_freshDetectorInputCount`, `_detectorDrainCount`,
  `_patternDrainCount`, `_detectorReportRefreshCount`,
  `_noFreshFrequencySkipCount`, `_detectorOccurrencePoppedCount`,
  `_detectorValidOccurrencePoppedCount`, `_patternAcceptAttemptCount`,
  `_patternAcceptSuccessCount`, `_patternAcceptRejectCount`,
  `_patternResultProducedCount`, `_patternEventPushedCount`,
  `_patternEventDroppedCount`, ...) exposed as public accessors for a system
  that has one input and one output per iteration.

Since a single detector is active at a time and both `drainDetectors()`
branches (`DetectionRuntime.cpp:478-549`) do the same nine steps in the same
order against `_frequencyDetector` vs. `_scalarDetector`, this queueing and
correlation machinery exists to guard against a race between two data paths
that, in the current architecture, are driven from the same call
(`observeFrame()`) on the same thread, in a fixed order, once per sample.
There is no concurrency here to protect against. The correlation-queue
divergence the code defends against (comment at `DetectionRuntime.cpp:468-476`:
"the correlation queue... can legitimately diverge from what the matcher
itself still has queued... for example, when `pushPatternObservation()` fails
while `acceptOccurrence()` already succeeded") is a failure mode the code
itself introduces by using two separate small ring buffers to carry two
halves of what is conceptually one event, rather than carrying them together.
Widening the queue slot to hold both pieces of data as one struct (which
`PendingPatternObservation` at `DetectionRuntime.h:71-74` almost already is)
and pushing/popping it as a unit would remove the entire correlation-failure
class instead of instrumenting it.

This complexity has a real cost beyond readability: it is precisely the kind
of code where the bug in §1 hides. A stale-cache bug across two independently
generation-tracked report objects, correlated through a separately-tracked
event queue, with twenty diagnostic counters layered on top to *notice* when
the correlation fails — is much harder to see by inspection than a bug in a
straight-line, single-detector, single-report pipeline would have been. The
machinery built to catch inconsistency became complex enough to itself be a
source of inconsistency risk, and in §1 it demonstrably didn't catch the one
bug it exists to catch (the "planned" status of that roadmap item confirms
nobody has traced it yet, despite the counters being right there).

---

## 4. Documentation and diagnostics dwarf the functional code, without tests to match

- `docs/**/*.md`: **112 files, ~50,000 lines.**
- `src/**/*.{cpp,h}`: **~19,000 lines** total, of which `src/detection/analyzer/`
  (reporting/diagnostics tooling: `AnalyzerSeqReporter`, `AnalyzerText`,
  `AnalyzerSampleDump`, `AnalyzerRawCapture`, `AnalyzerRuntimeReporter`,
  `AnalyzerSystemReporter`, `AnalyzerTrialCapture`, `AnalyzerTrialClassifier`,
  `AnalyzerSequenceSession`, `AnalyzerCommands`, `AnalyzerPassRules`) alone is
  **~4,000 lines** — more than the entire scalar+frequency detector
  implementations combined.
- **1,842** `Serial.print*` call sites across `src/`.
- Git history: of the last 20 commits, the plurality are tagged `Docs:`,
  `...Cleanup:`, `...Refactor:`, or `...Rename:` rather than feature or fix
  work (`e0302ad`, `72e8dc1`, `4c7658e`, `4280601`, `0102896`, `847b1ef`,
  `1f75daf`, `4b356b5`, `876424e`, `60cf71c`, ...).

Meanwhile, `test/` contains a **single** test file
(`test/test_analyzer_pass_rules/test_main.cpp`, 244 lines) exercising only
`ScalarTransientDetector` and `FeatureHistory`'s quantile math. There is no
test coverage for:

- `FrequencyMatchDetector` — the detector with the bug in §1, and the one
  behind the project's own "stable" profile;
- `PatternMatcher` — the single place a `PatternResult.valid` decision is made;
- `DetectionRuntime` — the queueing/correlation/generation-tracking
  machinery in §3, which is exactly the kind of code (stateful, order- and
  timing-dependent, several interacting ring buffers) that most benefits from
  and most needs unit tests;
- `ResonantBehavior` — the actual reaction/output state machine;
- `FieldStateTracker`, `ChirpOutput`, `ParamRegistry`, `AudioSignal`.

For a project that has spent enormous effort narrating its own architecture
(50k lines of docs, dozens of "cleanup pass" and "refactor pass" documents
under `docs/refactors/`) and instrumenting its own runtime with dozens of
diagnostic counters (§3), the near-total absence of automated tests on the
one component (`FrequencyMatchDetector`) that both carries the confirmed bug
and backs the "stable" profile is the most consequential gap in the project.
Prose describing intended architecture and print statements describing
runtime behavior are both weaker verification tools than a unit test that
fails on `git push`; this project has invested heavily in the former two and
almost not at all in the latter.

---

## 5. Duplication and drift within the code itself

- `Node::applyActiveDetectionProfile()`
  (`src/modes/resonant/ResonantNodeApp.cpp:1124-1137`) calls
  `_detection.setInspectionPlan(detectionProfile.inspectionPlan);` twice in a
  row (lines 1130 and 1131, identical arguments). Harmless at runtime, but
  it's the kind of copy-paste leftover that a test or a reviewer should have
  caught, and in a codebase this document-heavy it's a small tell that the
  prose review process isn't substituting for a code review process.
- `DetectionRuntime::drainDetectors()`
  (`src/detection/DetectionRuntime.cpp:478-549`) has two `switch` branches
  (`FrequencyMatch` / `ScalarTransient`) that are ~30 lines each and
  structurally identical line-for-line except for which detector member is
  addressed. This is a textbook case for a small template/generic helper;
  as written, any future change to the accept/inspect/correlate sequence has
  to be made twice and kept in sync by hand — which is exactly the kind of
  drift that produced the asymmetry in §1 between the two detectors'
  `freezeReport()` discipline.
- The serial command surface duplicates its own configuration story. `RB
  PARAM` and `RB BEHAV` (`ResonantNodeApp.cpp:695-819`) are a bespoke,
  hand-parsed token protocol living alongside the newer, more principled
  `PARAM LIST|GET|SET|DUMP` surface backed by `ParamRegistry`
  (`ResonantNodeApp.cpp:993-1071`). The code's own comment acknowledges this
  ("Kept separate from the ad hoc `RB PARAM` / `RB BEHAV` commands until those
  are deliberately migrated", `ResonantNodeApp.cpp:990-992`), which is fine as
  a statement of intent, but today it means there are two different ways to
  change the same running parameters, with different validation (the ad hoc
  path does no range checking at all — `strtoul`/assignment straight into the
  behavior fields — while `ParamRegistry::applyValue` enforces min/max).
- The ad hoc parser leans on manually-computed string offsets to strip
  prefixes, e.g. `token + (startsWithTokenIgnoreCase(token,
  "behaviorSuppressSelfChirpMs=") ? 28 : 18)` and `token + (...  ?  31 :  22)`
  (`ResonantNodeApp.cpp:766-771`). These offsets are currently correct
  (verified by counting: `"suppressSelfChirp="` is 18 chars,
  `"behaviorSuppressSelfChirpMs="` is 28), but nothing enforces that a future
  rename of either alias keeps its offset in sync — a one-character typo in
  the alias string silently shifts what gets parsed, with no compiler
  diagnostic and no test to catch it. `ParamRegistry`'s path-based lookup
  doesn't have this failure mode; the older `RB PARAM`/`RB BEHAV` path does,
  and it's the one still handling the primary tuning knobs
  (`waitAfterHeardMs`, `refractoryAfterEmitMs`, etc.) end to end.

---

## 6. What's actually solid

To be fair to the parts of the code that don't fit the pattern above:

- `ResonantBehavior` (`src/behavior/ResonantBehavior.cpp/.h`) is a clean,
  legible, bounded state machine. Four states, clear transition guards,
  no dynamic allocation, no hidden coupling to detector internals — it
  reads like code written to be understood, not just to be exhaustive.
- `AudioSourceI2S`/`AudioSignal` handle the actual hardware-facing,
  timing-sensitive work (I2S decode, block refill, baseline tracking) with
  fixed-size buffers, no dynamic allocation in the hot path, and sensible
  non-blocking reads (`i2s_read(..., 0)` timeout). This is the part of the
  system where correctness bugs would be hardest to debug (DMA timing, sample
  framing) and it's also the part that looks the most carefully reasoned
  about.
- `FieldStateTracker` is proportionate to its job: it's a small, simple
  rolling-window counter, and it looks like one. It's a useful point of
  comparison against `DetectionRuntime` in §3 — this is roughly what the
  rest of the pipeline's bookkeeping should look like relative to what it
  actually needs to track.
- `FeatureHistory`'s bounded ring-buffer-of-bins design
  (`src/detection/features/FeatureHistory.cpp/.h`) is a reasonable,
  bounded-memory way to give inspectors a retrospective window without
  unbounded raw sample storage, and the quantile/trimmed-mean math is at
  least exercised by the one test file that exists.

---

## 7. Bottom line

The node's core sensing and reaction loop (I2S capture → baseline → behavior
state machine → chirp output) is reasonably sound. The layer built on top of
it to observe and explain that loop — `DetectionRuntime`'s pipeline-event
correlation machinery and the ~4,000-line `analyzer/` reporting subsystem —
is disproportionately large relative to what it's observing, is undertested
relative to how stateful and timing-dependent it is, and currently contains
a real, traceable defect (§1) in exactly the profile the project calls
stable, a defect the project's own status doc admits noticing symptoms of
but hasn't traced. The project has spent far more effort narrating its
architecture in Markdown than verifying it in tests, and at least one of
those two documents (`implementation-status.md`) is itself already stale
against the code it describes. The most valuable next steps are not more
architecture prose: they are (a) fix `FrequencyMatchDetector::update()`/
`capturePendingOccurrence()` to freeze its report on accept the way
`ScalarTransientDetector` does, and (b) add unit tests for
`FrequencyMatchDetector`, `PatternMatcher`, and `DetectionRuntime` before
any further diagnostic layers are added on top of them.
