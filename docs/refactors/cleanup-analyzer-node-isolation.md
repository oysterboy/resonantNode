# Architecture Proposal — Analyzer Diagnostics Must Not Cost the Node

Status: **implemented 2026-09-21** (Phase 5a). Hardware verification still
outstanding, see "Implementation record" below.
Related to: `docs/refactors/cleanup.md` and
`docs/refactors/cleanup-detector-ownership.md`.

---

## Implementation record (2026-09-21)

Implemented with two complementary mechanisms, both build-time:

1. **`#ifdef ANALYZER_MODE` inside `DetectionRuntime.h`/`.cpp`** for the
   diagnostics state and logic: the pipeline-event queue, the counter set,
   `PipelineIntegrity`, the `PendingPatternObservation` correlation queue,
   `capturePipelineResult()`, `drainDetectorReportEvents()`,
   `captureLatestDetectorReportIfChanged()`, and every diagnostics accessor.
   The core keeps exactly the surface `ResonantNodeApp` calls, re-confirmed
   by search at implementation time: `resetState`, the seven profile
   setters, `observeFrame`, `popPatternResult`, `fieldState`.
2. **`build_src_filter` in `platformio.ini`** excluding `detection/analyzer/`
   and `modes/analyzer/` from `env:esp32dev` and `env:esp32dev-emitter`.

Mechanism 2 was not anticipated by this document and turned out to be
required, not optional. `platformio.ini` had no source filter at all, so
every Analyzer tooling `.cpp` was being compiled into the Node and Emitter
binaries already — a larger instance of the same problem this document
describes. That also made mechanism 1 impossible on its own: those files
call the methods being gated, so the Node build failed to compile until the
files themselves stopped being part of it. Checked before adding the
filter: nothing outside those two directories references the analyzer
translation units, no analyzer code references `modes/resonant` or
`modes/emitter`, and `AnalyzerPassRules.h` (which `ScalarTransientDetector`
does depend on) is header-only, so it is unaffected by a source filter and
remains available in every build.

### Measured result (step 6 of the approach below)

Real linked builds, before at `293f11f` (measured in a clean worktree) and
after:

| Build | RAM before | RAM after | Flash before | Flash after |
|---|---|---|---|---|
| `esp32dev` (Node) | 87,940 | **74,436** | 365,817 | **356,577** |
| `esp32dev-emitter` | 21,832 | 21,832 | 284,757 | **282,709** |
| `esp32dev-analyzer` | 99,564 | 99,564 | 403,277 | 403,277 |

The Node build gives back **13,504 bytes of RAM** (15.4% of its previous
usage) and 9,240 bytes of flash. Emitter RAM is unchanged as expected, it
never instantiated `DetectionRuntime`; only its flash drops, from no longer
compiling the analyzer tooling. The Analyzer build is byte-identical in
both figures.

### Verification

- All three environments compile and link (step 4).
- The Analyzer-mode translation unit was preprocessed before and after and
  diffed: it is identical apart from statement ordering and one removed
  dead local (`const bool eventPushed = capturePipelineResult(...)` in
  `drainPatternMatcher()`, which was assigned and never read). Combined
  with the identical Analyzer binary size, SEQ output is expected to be
  unchanged, but this is inference from the build, **not** a hardware run.
- **Not done: step 5.** The Node behavior path and the 50-trial SEQ tests
  for both profiles have not been run on hardware. This is the phase where
  T7 (Node smoke test) matters most, per `cleanup-0-plan.md`, precisely
  because the Node binary is the one that changed and the Analyzer
  instrumentation that would normally verify it no longer exists inside it.

## Relationship to the other cleanup docs

This is a second bigger, riskier structural proposal, alongside
`cleanup-detector-ownership.md`. It changes where code lives and how it's
built, not just struct layout or dead-code removal. Recommended sequencing:
finish `cleanup.md` first, in particular Item 3 (`FrequencyMatchDetector`
encapsulation) and Item 6 (diagnostic counter audit), since both shrink or
clarify the exact state this proposal needs to relocate.
`cleanup-inspector-pattern-scope.md` is independent and can land at any
time, before or after this proposal.

Correction, 2026-09-20: this used to also list Item 1 (the `Occurrence`/
`DetectorReport` tagged union) as a prerequisite. Item 1's `Occurrence` half
was withdrawn, it was based on a false premise (see `cleanup.md` Item 1 for
the evidence), so it is not a prerequisite for anything. Its `DetectorReport`
half is unverified and not scheduled. Neither blocks this proposal, which
does not depend on either struct's detail-payload shape.

Non-negotiable constraint carried over from that discussion: the Node-facing
API must not change shape. `ResonantNodeApp`/`ResonantBehavior` must
continue to consume exactly what they consume today, the profile setters,
`observeFrame()`, `popPatternResult()`, and `fieldState()`, with
`PatternResult`, `FieldState`, and `DetectionProfile` unchanged in shape.

---

## Problem

`DetectionRuntime` is shared, undifferentiated, between two build targets
that are otherwise already fully separate binaries:

```text
src/app/main.cpp selects exactly one of:
  ResonantNodeApp   (default build, env:esp32dev)
  EmitterApp        (EMITTER_MODE, env:esp32dev-emitter)
  AnalyzerModeApp   (ANALYZER_MODE, env:esp32dev-analyzer)
```

`EmitterApp` has no dependency on `DetectionRuntime` at all, confirming the
project already keeps mode-specific code out of binaries that don't need it.
`DetectionRuntime` is the exception: both `ResonantNodeApp` and
`AnalyzerModeApp` link against the same class, and that class carries
Analyzer-only diagnostic machinery unconditionally:

- the `_pipelineEventQueue` and `DetectionPipelineEvent`/`PipelineIntegrity`
  tracking
- roughly twenty free-standing counters
  (`observeFrameCount`, `patternAcceptAttemptCount`, and so on)
- `activeDetectorReport()`/`activePatternMatcherReport()`/
  `popPipelineEvent()`

Checked directly: `ResonantNodeApp.cpp` never calls any of the above. It
calls only the profile setters, `resetState()`, `observeFrame()`,
`popPatternResult()`, and `fieldState()`. Every one of the items above is
called only from `AnalyzerSystemReporter.cpp` and
`AnalyzerSequenceSession.cpp`.

So the production Node binary compiles in, constructs, and updates on every
frame a set of queues and counters that exist solely to serve a debug tool
it never runs. This is not merely unused code sitting in a shared header, it
is live state that gets written to on every `observeFrame()` call in
production, for a build that will never read it.

### Additional evidence: it's not just state, it's logic

Tracing what actually feeds `PatternResult` and `FieldState`, the values
Node consumes, versus what only feeds the diagnostic
`DetectionPipelineEvent`, found something bigger than the counters: a
meaningful piece of `DetectionRuntime`'s own *logic*, not just idle state,
exists solely to serve diagnostics.

`DetectionRuntime::drainPatternMatcher()` calls
`pushPatternResult(result)` (which feeds `popPatternResult()`, the thing
Node actually reads) and `_fieldStateTracker.observePatternResult(result, nowMs)`
using only the bare `PatternResult`. Neither depends on
`hasMatchedInspectedOccurrence` or the matched `DetectorReport`. But the
`PendingPatternObservation`/`_patternInspectedQueue`/`popPatternObservation`/
`pushPatternObservation` machinery, arguably the most complex and
historically bug-prone part of `DetectionRuntime` (it was the actual subject
of the pipeline-failure investigation in the archived `current-pass.md`
pass), exists solely to attach a matching `DetectorReport` and
`InspectedOccurrence` to the diagnostic `DetectionPipelineEvent` inside
`capturePipelineResult()`. It has no effect on `PatternResult` or
`FieldState`.

The same is true of `DetectorReport` itself: neither `PatternResult` nor
`FieldState` is ever built from it, anywhere. `DetectorReport` is a pure
diagnostics/reporting type. So `latestReport()`/`reportGeneration()` are not
needed by the Node-facing behavior path either, only by whatever reads
`DetectorReport` for display or correlation, which today is Analyzer alone.

### The minimal Node-required detector contract

Putting this together, once diagnostics are properly isolated, the contract
a detector must fulfill to participate in the actual behavior-producing
path (the one `ResonantNodeApp` depends on) is small:

```cpp
// Core, Node-required:
void resetState();
bool hasPendingOccurrence() const;
bool popOccurrence(detection::Occurrence& out);
void update(/* detector-specific input */);
```

Everything else a detector class has today, `latestReport()`,
`reportGeneration()`, `setDiagnosticsEnabled()`, the reject-summary reset
methods (spelled differently per detector today:
`resetRejectSummary()` for frequency, two separate methods for scalar), is
diagnostics-only. None of it is called from `ResonantNodeApp` today (checked
directly: `resetSourceRejectSummaries()` and `setDiagnosticsEnabled()` are
called only from `AnalyzerSequenceSession.cpp`/`AnalyzerCommands.cpp`), and
none of it is required for `PatternResult`/`FieldState` to be produced
correctly.

This sharpens the split proposed below: the "diagnostics layer" is not just
extra counters bolted on top of the core, it also owns the correlation
queue and the report-matching logic that exists only to serve it. The core
gets simpler, not just smaller, once that logic moves out.

A detector that wants to participate in Analyzer/SEQ tooling can still
implement the diagnostics-facing methods, but a minimal detector is not
required to: the diagnostics layer can use default no-op behavior (an empty
`DetectorReport{}`, a constant generation of `0`) for any detector that
doesn't bother implementing them, rather than requiring every detector to
hand-write reject-summary tracking under its own bespoke method name.

---

## Proposed Change

Split `DetectionRuntime` into two layers:

1. **A lean core** carrying exactly the surface `ResonantNodeApp` uses
   today: the profile setters, `resetState()`/`resetDetectors()`,
   `observeFrame()`, `popPatternResult()`, `fieldState()`, and whatever
   internal state (`_frequencyDetector`/`_scalarDetector`,
   `_occurrenceInspector`, `_patternMatcher`, `_fieldStateTracker`,
   `_featureHistory`, `_resultQueue`) is required to make those calls work.
   This is what both the Node and (indirectly) the core detection behavior
   depend on.
2. **An Analyzer-only diagnostics layer**, compiled in only under
   `ANALYZER_MODE`, adding the pipeline-event queue, `PipelineIntegrity`,
   the counters, the report-access methods (`latestReport()`,
   `reportGeneration()`), and, per the finding above, the
   `PendingPatternObservation` correlation queue and the report-matching
   logic in `capturePipelineResult()`. This layer observes the core's
   outputs, it does not change what the core computes; the core produces
   `PatternResult`/`FieldState` without any of this machinery running at
   all.

The mechanism (composition vs. a derived class vs. a build-time
`#if defined(ANALYZER_MODE)` block inside one file) is an implementation
choice to make at design time, not decided here. What matters
architecturally is the outcome: none of the diagnostics-layer code, data, or
state should exist in the compiled Node or Emitter binaries. Not merely
unreachable, absent.

### What this fixes

- The Node binary stops paying RAM for the pipeline-event queue,
  `PipelineIntegrity` structs, and the counter set.
- The Node binary stops spending CPU cycles on every `observeFrame()` call
  building `DetectionPipelineEvent`s and updating counters nobody reads.
- Makes explicit, and enforced by the build, the principle that Analyzer
  tooling must not affect the detector/Node runtime.

### What this does not change

- `PatternResult`, `FieldState`, and `DetectionProfile` field shapes.
- `ResonantNodeApp`'s call sites into `DetectionRuntime` — same method
  names, same signatures, same behavior.
- Detector internals, thresholds, or lifecycle logic.
- Anything covered by `cleanup-detector-ownership.md`; that proposal is
  about detector object lifetime, this one is about where diagnostic state
  lives. They can be implemented independently, in either order, though
  doing detector-ownership first would mean this proposal has less state to
  relocate.

---

## Worked Example: A Minimal Third Detector Against the Core Contract

This is what makes the split concrete: a genuinely simple, low-config
detector, written after this proposal and `cleanup-detector-ownership.md`
land, only has to satisfy the four-method core contract to work in
production. Sketch, not final API, and assuming it reuses
`OccurrenceType::Scalar`'s existing detail shape rather than introducing a
new one:

```cpp
namespace detection {

// A minimal example: fires once when a scalar value crosses a single
// threshold and stays above it for at least minHoldMs, no hysteresis,
// no carrier-quality gating, no candidate lifecycle beyond that.
class SimpleThresholdDetector {
public:
    void resetState();

    // Detector-specific input, as the spec allows.
    void update(float value, unsigned long nowMs);

    bool hasPendingOccurrence() const { return _pendingPresent; }
    bool popOccurrence(Occurrence& out);

    // Optional: only needed if this detector should be inspectable from
    // Analyzer/SEQ tooling. Safe to omit for a first version.
    // const DetectorReport& latestReport() const;
    // uint32_t reportGeneration() const;

    void setThreshold(float value) { _threshold = value; }
    void setMinHoldMs(unsigned long value) { _minHoldMs = value; }

private:
    float _threshold = 0.0f;
    unsigned long _minHoldMs = 0;
    bool _above = false;
    unsigned long _aboveSinceMs = 0;
    bool _pendingPresent = false;
    Occurrence _pending = {};
    unsigned long _nextOccurrenceId = 0;
};

} // namespace detection
```

That's the whole class for a first version: no reject-summary tracking, no
diagnostics toggle, no report freezing. It plugs in as its own build-time
family via `cleanup-detector-family-build.md`'s `DetectionFamily.h` (which
superseded the union-of-detectors idea this sketch originally assumed), the
same way the existing two do, just with a much shorter list of things it
has to get right. That document's worked example is the current, fuller
version of this sketch, sized to `mvp-app-structure.md` §3. If it later
earns a place in Analyzer/SEQ tooling, `latestReport()`/`reportGeneration()`
get added then, as pure additions, not as a precondition for it to work in
the Node at all.

The remaining wiring, a `DetectorId`/`DetectorSelection` value, a config
struct in `DetectionProfile.h`, a profile factory, a `DetectorReportPrinter`
case if it should show up in SEQ output, is unchanged by this proposal and
stays as cheap as it is today, since that wiring was never the expensive
part.

---

## Risks and Open Questions

- **Two build configurations to keep working.** Today `DetectionRuntime` is
  one class exercised by both the Node build and the Analyzer build in CI or
  local builds. After the split, both configurations (lean core alone, core
  plus diagnostics layer) need to keep compiling and passing their
  respective SEQ/behavior tests — a regression in the Analyzer-only layer
  should not be able to silently break the Node build, and vice versa.
- **Where the seam goes.** Some of today's `DetectionRuntime` internals
  (e.g., `captureLatestDetectorReportIfChanged()`, used by both
  `activeDetectorReport()` for Analyzer and internally by
  `drainDetectorReportEvents()` for the pipeline-event queue) currently
  serve both the core update path and the diagnostics path. These need to be
  disentangled carefully so the core's own correctness doesn't accidentally
  depend on diagnostics-layer bookkeeping.
- **Testing without the diagnostics layer.** Some existing verification
  approaches (in `cleanup.md` and elsewhere) rely on SEQ output and counters
  that only exist in the Analyzer build. Node-side behavior correctness will
  need its own verification path that doesn't depend on Analyzer-only
  instrumentation, since after this change that instrumentation won't exist
  in the Node binary to verify against.
- **Worth doing before or after `cleanup-detector-ownership.md`?** Both
  proposals touch `DetectionRuntime`'s internals substantially. Doing them
  as two separate, sequential passes is safer than combining them, but the
  order affects how much rework each one causes the other. This should be
  decided once both are closer to implementation, not now.

---

## Suggested Approach If Adopted

1. Confirm, by search, that `ResonantNodeApp.cpp`'s call list into
   `DetectionRuntime` is still exactly the list in this document (re-check
   after `cleanup.md` and `cleanup-inspector-pattern-scope.md` land, since
   both touch nearby code).
2. Identify every `DetectionRuntime` member and method that exists solely to
   serve `AnalyzerSystemReporter.cpp`/`AnalyzerSequenceSession.cpp`, using
   the counter/queue list in this document as the starting set.
3. Move that state and those methods behind an `ANALYZER_MODE`-gated layer,
   choosing the composition/inheritance/build-flag mechanism at that time.
4. Build all three environments (`esp32dev`, `esp32dev-emitter`,
   `esp32dev-analyzer`) and confirm each still compiles and links.
5. Run the Node's existing behavior/output path (manual or scripted) and the
   Analyzer's 50-trial SEQ tests for both profiles, confirming both are
   unchanged from baseline.
6. Measure and record the Node binary's RAM/flash size before and after, to
   quantify the actual savings from this change.
