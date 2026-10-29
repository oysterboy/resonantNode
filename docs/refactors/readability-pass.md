# Step 12.4: the readability pass (docs/research/readability-measures.md, all of it)

Status: active (opened 2026-10-29). Step 12.4 (roadmap-0-steps.md, issue #83). Owner 2026-10-29: "12.4. Address the
whole readability findings." Integrates the other session's "12.5" (branch ccr-3cb2449c-sp2ed1, not merged; its issue
#83 is this step's issue).

## Goal

The code reads easier where the counts and the human read agree it is hard: state (the detector's 68 members,
Behavior's 46), the two Analyzer giants, the long reporters, the duplicated report structs, bare citations, missing
owns blocks. No behaviour and no output change anywhere.

Defaults for the note's open choices (owner may overrule): the reporters get a `LineWriter` (key=value pairs, same
bytes), not a table-driven format; citations: only the 11 bare ones are rewritten, plus a CLAUDE.md rule that new
comments state the reason and point to a decision file, not a step number; owns / does-not blocks for classes, not for
plain data structs.

## Items, in order (one commit each)

1. **12.4.1 the tracked set.** `tools/ci/readability.py`: the note's table A-G (functions over CCN 20, over 60 NLOC,
   over 5 params; members of the three largest classes; headers with an owns block; mean CCN modes vs the rest; lines
   over 120). lizard pinned like clang-format; printed in CI, never failing. The "before" goes into Progress, dated.
2. **12.4.2 detector state.** `ScalarTransientDetector`'s 68 members into the groups its comments name (live gate,
   candidate lifecycle, occurrence emission, detector report, reject summary), each reset by `= {}`.
3. **12.4.3 Behavior state.** `ResonantBehavior`'s 46 members: timing deadlines, counters, pending request, rest.
4. **12.4.4 the report builder.** `buildSequenceAnalyzerReport` (301 NLOC, 10 parameters) into one function per
   report section.
5. **12.4.5 RAW capture.** `runRawTrigger` (620 NLOC, CCN 171) into setup / capture / dump stages with one RAII buffer
   in place of the manual malloc / free pairs.
6. **12.4.6 the reporters.** printSystemHealth, printRbStatus, printDetectorReportGenericLine, printAudioRunSummary,
   printSequenceInspectCanonical through a `LineWriter`.
7. **12.4.7 the report twins.** `AcceptedOccurrenceSummary` / `SelectedRejectSummary` (22 identical fields) as one
   `OccurrenceSummary` plus the reject-only fields.
8. **12.4.8 citations.** The 11 bare `(step N)` / `(issue #N)` comments state their reason; the CLAUDE.md rule.
9. **12.4.9 owns blocks** for the classes that lack one.
10. The tracked set again; the after beside the before.

## Gates

Every item: gate.sh fully green (every env, host suites, T2h byte-identical, replay Null == diag, include direction),
test_lists.py, clang_format.py --check. Printed lines byte-identical: test_report_printers pins SEQ_SOURCE_CORE /
SPEC; for 12.4.4-12.4.7 also on the boards (remote, over WiFi): tools/bench/analyzerab.py against
bench/sessions/2026-10-29-step12-sanity/ab_ba34561 (seqcmp: same keys, same results), RAW captures before / after
(12.4.5: same header, same columns, same row count), tools/bench/nodecheck.py 27 / 27 (12.4.6: RB STATUS keys).

## Don't touch

Thresholds, detection behaviour, Behavior's decisions, line formats, the NET format, FirmwareOta, the secrets header.

## Progress

- 2026-10-29: opened.
- 2026-10-29: 12.4.1 done. tools/ci/readability.py (lizard 1.24.3, CI format job, prints only). Before, at c4a56bd:
  A 34, B 46, C 13, D 68 ScalarTransientDetector / 46 ResonantBehavior / 36 Node (classes only; the note's 30 for
  AnalyzerApp is fourth-ish, the Node's 36 did not spread as far as the note assumed), E 21 of 61, F 6.3 / 3.7, G 12.
  The note's counts reproduce exactly (A-C, E-G); the tail starts runRawTrigger 620 / 171, buildSequenceAnalyzerReport
  302 / 103.
- 2026-10-29: 12.4.2 done. ScalarTransientDetector's 68 members are 16: _config, GateState _gate, PeakState _peak,
  CandidateFacts _facts (with mean / matchedMean / rms, the three copies of each folded), OccurrenceTrack _track, the
  two id counters, Emission _emission, AcceptedState _accepted, RejectState _reject, the report detail / frozen report
  / two generations, LastReject _lastReject, Counts _counts. Every reset is `= {}` except the two that clear only part
  of a group (captureAcceptedOccurrence keeps the reject's occurrence id, the update's per-call flags). Gate green, T2h
  byte-identical.
- 2026-10-29: 12.4.3 done. ResonantBehavior's 46 members are 13: _config, RestState _rest, _emitTimes, _state,
  _activityLevel, _lastFieldState, HeardInput _heard, Timing _timing (moments and deadlines), LastDecision _last,
  IdleSchedule _idle, ChirpRequest _request, _outputsEnabled, Counters _counts. resetState is ten lines; it keeps
  _idle.scheduledFromMs, _outputsEnabled and _config as before. Gate green (test_behavior_* among the 132), T2h
  byte-identical.
- 2026-10-29: 12.4.4 done (09e747e). buildSequenceAnalyzerReport is a 24-line list of section fills over one
  SequenceReportInputs (selection, verdict / inspection / detector report, pipeline evidence, overflow delta, artifact
  reason, buffer overrun, profile); 10 parameters to 4 (the dropped six were fields it already held). The field
  section's never-taken null branch is gone (the field state is a reference).
- 2026-10-29: 12.4.5 done (3e5c757). runRawTrigger: plan, allocate, claim the Emitter, capture, print, over one
  AnalyzerApp::RawCaptureRun (defined in the .cpp); RawRows<Row> frees on every way out; storeRawRow /
  copyRawPreWindow templates replace five copies. No function in the file over CCN 15. B is 46 (the new printRawBegin
  and printRawRows are long print sequences; 12.4.6's ground).
- 2026-10-29: board check for 12.4.4 + 12.4.5 together on the Analyzer (OTA, remote):
  bench/sessions/2026-10-29-step12.4-readability. SEQ A/B a9b7e99 vs 3e5c757: the same trial results, line counts and
  key sets per line type; RAW set (six cases, every mode): the same keys, headers and row counts. 09e747e alone was not
  built separately (its tree differs from 3e5c757 only by the RAW file and the header's RAW block).

