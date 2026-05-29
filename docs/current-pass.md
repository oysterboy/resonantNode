# Codex Instruction — Analyzer Memory / Timing Pressure Cleanup

## Scope

This pass is limited to Analyzer memory/timing pressure and output gating.

Do **not** include broader output architecture work in this pass. Specifically, do not implement the Derived StageStatus Chain, result vocabulary cleanup, summary consistency cleanup, evidence namespace refactor, or Pattern-stage output redesign here.

This pass should only address:

1. Output mode ownership
2. Quiet mode semantics
3. Duplicate Analyzer FeatureHistory update
4. Avoiding unnecessary live-state/report work
5. Bounded diagnostic storage

## Status

done

## Implemented

- removed the analyzer-side duplicate `FeatureHistory` update
- kept `SEQ_TRIAL`/`SEQ_SOURCE`/`SEQ_INSPECT`/`SEQ_PATTERN` behind the output config
- made quiet mode stay compact without stage/debug dumps
- stopped the verdict printer from triggering inspect output as a side effect
- kept summary output and bounded diagnostics intact

---

## 1. Fix output mode ownership

### Problem

`MODE`, `WHEN`, and `VERBOSITY` must consistently control Analyzer output, but compact modes may still accidentally print verbose, stage, or debug blocks.

### Required behavior

Use this contract:

```text
MODE      = which line families may print
WHEN      = which trials get extra/stage output
VERBOSITY = how many fields each printed line contains
```

Ensure all Analyzer print paths obey this contract.

### Implementation instruction

Audit all SEQ print functions and make sure each one checks the output config before printing:

```text
SEQ_TRIAL
SEQ_SOURCE
SEQ_INSPECT
SEQ_PATTERN
SEQ_EXPLAIN / old verbose dump
SEQ_SUMMARY
```

Rules:

```text
mode=compact
  may print compact SEQ_TRIAL
  must not print verbose/stage/debug blocks

mode=source
  may print SEQ_TRIAL + SEQ_SOURCE according to WHEN

mode=inspect
  may print SEQ_TRIAL + SEQ_INSPECT according to WHEN

mode=pattern
  may print SEQ_TRIAL + SEQ_PATTERN according to WHEN

mode=explain
  may print all relevant stage lines / old verbose dump according to WHEN and VERBOSITY

mode=quiet
  see quiet semantics below
```

Do not let any old diagnostic function bypass the config.

### Acceptance criteria

- `mode=compact verbosity=0` does not print verbose diagnostic blocks.
- `mode=source` does not accidentally print inspect/pattern/deep blocks.
- `mode=inspect` does not accidentally print source/pattern/deep blocks unless explicitly intended.
- Old verbose output only appears under `mode=explain` and/or high verbosity.

---

## 2. Clarify quiet mode semantics

### Problem

Quiet mode may still print compact per-trial results, but it should not print verbose, stage, or debug blocks.

### Required behavior

For this project, `quiet` does **not** necessarily mean fully silent.

Define it as:

```text
quiet = compact per-trial output allowed
quiet = no verbose/stage/debug blocks
quiet = no unnecessary diagnostic dumps
```

If a fully silent mode is needed later, add a separate mode such as `silent` or `off`. Do not change that in this pass unless already trivial.

### Implementation instruction

Make sure `mode=quiet` suppresses:

```text
SEQ_SOURCE
SEQ_INSPECT
SEQ_PATTERN
SEQ_EXPLAIN
old verbose diagnostic dumps
large multi-line measurement blocks
```

but may allow compact final trial output such as:

```text
#12 ----------------
result=expected dt=52ms confidence=1.00
```

or a single compact `SEQ_TRIAL` line.

### Acceptance criteria

- `mode=quiet` does not print verbose diagnostic blocks.
- `mode=quiet` may still print compact end-of-trial results.
- `mode=quiet` output is clearly smaller than `mode=compact` or `mode=source`.
- No stage/debug output appears in quiet mode.

---

## 3. Remove duplicated Analyzer FeatureHistory update

### Problem

FeatureHistory should be updated by the normal DetectionRuntime pipeline only.

Analyzer-side FeatureHistory updates can duplicate work and create timing pressure.

Current pattern to look for:

```cpp
if (_sequenceFeatureHistory != nullptr) {
    detection::FeatureExtractor::observeFrame(frame, *_sequenceFeatureHistory);
}
```

while the normal detection pipeline also updates FeatureHistory inside `DetectionRuntime::observeFrame()`.

### Required behavior

For normal SEQ operation:

```text
DetectionRuntime owns and updates FeatureHistory.
Inspectors consume FeatureHistory through the normal detection pipeline.
Analyzer does not maintain/update a parallel FeatureHistory.
```

### Implementation instruction

Remove the Analyzer-side FeatureHistory update entirely if nothing currently needs it.

If a special diagnostic still needs `_sequenceFeatureHistory`, gate it behind an explicit helper:

```cpp
bool AnalyzerApp::sequenceAuxFeatureHistoryNeeded() const {
    if (!_sequenceTest.active) return false;

    // Only enable for features that explicitly read _sequenceFeatureHistory.
    if (_sequenceTest.sampleDumpEnabled) return true;

    if (_sequenceTest.outputConfig.mode == SeqOutputMode::Explain &&
        _sequenceTest.outputConfig.verbosity > 1) {
        return true;
    }

    return false;
}
```

Then use:

```cpp
if (_sequenceFeatureHistory != nullptr && sequenceAuxFeatureHistoryNeeded()) {
    detection::FeatureExtractor::observeFrame(frame, *_sequenceFeatureHistory);
}
```

If no current diagnostic reads `_sequenceFeatureHistory`, prefer deleting the allocation and update path for now.

### Acceptance criteria

- During normal SEQ runs, only `DetectionRuntime::observeFrame()` updates FeatureHistory.
- Analyzer does not duplicate FeatureHistory updates in the hot loop.
- Inspector behavior remains unchanged because it uses the normal pipeline FeatureHistory.
- No detection thresholds or profile behavior are changed.

---

## 4. Avoid unnecessary live-state / report work

### Problem

Analyzer output should not trigger unnecessary live-state reads, copies, captures, or report work in compact modes.

### Required behavior

Compact modes should do the minimum necessary work:

```text
build final trial result
update summary counters
print compact output if configured
```

Avoid extra capture/copy work unless a selected mode actually needs it.

### Implementation instruction

Review the trial finalization path and guard expensive optional work behind output config checks.

Look for calls like:

```text
captureDiagnostics
flushSequenceSampleHistory
printSequenceSampleDump
printSequenceCandidateLogs
printSequenceDiagnostics
printSequencePattern
printSequenceExplain
large measurement block formatting
```

Make sure they only run when the active mode/verbosity requires them.

Compact/quiet paths should skip:

```text
sample dumps
candidate logs
deep diagnostics
large frame counters
large multi-line measurement blocks
extra live runtime captures
```

Important: stage/trial printers should use the finalized report snapshot where possible, not mutable live detector/runtime state after reset. However, do not implement the full StageStatus Chain in this pass.

### Acceptance criteria

- Compact trial runs do not call sample dump / candidate log / explain dump functions.
- Compact trial runs do not perform unnecessary diagnostic capture.
- Finalization time is reduced in compact and quiet modes.
- Output still reports correct final result and summary counters.

---

## 5. Review and bound per-trial diagnostic storage

### Problem

Analyzer has had memory pressure issues. Diagnostic storage must be bounded and predictable.

### Required behavior

Diagnostics must avoid:

```text
dynamic allocation in hot paths
unbounded vectors/lists
large per-trial buffers enabled by default
raw frame logs in normal modes
String accumulation
```

Use fixed-size, bounded data structures.

### Implementation instruction

Audit Analyzer and detector diagnostics for:

```text
std::vector
String
heap allocation
large arrays
per-trial buffers
candidate logs
sample history
raw dumps
```

Keep or introduce these rules:

```text
- no unbounded diagnostic storage
- no heap growth during SEQ run
- fixed-size ring buffers only
- large buffers only when explicitly enabled
- diagnostic counters preferred over full logs
```

If reject/candidate diagnostics already exist or are added later, they must be bounded:

```cpp
static constexpr uint8_t kRejectLogCapacity = 4; // or 8 max
```

or use aggregate counters + one best selected candidate:

```text
rejects.count
rejects.reason_counts
selected_reject.*
```

For this pass, do not implement the full reject-candidate log unless already started. Just ensure current diagnostic storage is bounded and safe.

### Acceptance criteria

- No unbounded diagnostic storage is active during normal SEQ runs.
- No new heap allocation is introduced in hot-loop diagnostics.
- Large sample/candidate buffers are disabled unless explicitly requested.
- Memory usage is predictable across long SEQ runs.

---

## Do not change in this pass

Do **not** change:

```text
Frequency thresholds
Duration gates
Support gates
PatternRules
Detection profile behavior
Frequency computation strategy
Derived StageStatus Chain
Result vocabulary
Summary reason taxonomy
Evidence namespace design
Pattern output design
```

This pass is only about reducing Analyzer memory/timing pressure and making output gating predictable.

---

## Suggested verification

Run the same acoustic setup with:

```text
SEQ MODE quiet
SEQ WHEN miss
SEQ VERBOSITY 0
```

Expected:

```text
compact output only
no verbose/stage/debug blocks
no Analyzer duplicate FeatureHistory update
bounded diagnostics only
```

Then run:

```text
SEQ MODE compact
SEQ WHEN miss
SEQ VERBOSITY 0
```

Expected:

```text
compact trial output
no deep diagnostic blocks
no duplicate FeatureHistory update
```

Then run:

```text
SEQ MODE source
SEQ WHEN miss
SEQ VERBOSITY 1
```

Expected:

```text
source-stage output only as configured
no inspect/pattern/deep blocks unless requested
bounded diagnostic storage
```

Compare miss rates between quiet/trial/source modes. If compact modes improve significantly, Analyzer output/diagnostic pressure was affecting detection timing.

---

## Core rule

```text
Keep normal detection pipeline work normal.
Remove Analyzer-side duplicated work.
Make output gating predictable.
Keep diagnostics bounded.
Do not mix this with broader Analyzer output architecture refactors.
```
