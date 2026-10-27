# Step 11g batch C: the Analyzer's run / trial split (A8, D1.4, D2.6, D2.8)

Status: active (opened 2026-10-27). Step 11g, batch C (roadmap-0-steps.md, issue #67). Owner order 2026-10-27:
11h's 24 h sweep holds the boards, batch C is written and host-gated meanwhile, its bench A/B runs after 11h.

## Goal

The Analyzer (`src/modes/analyzer/`, ~5,800 lines, one `AnalyzerApp` across sixteen files) says what a trial was,
once, from state that has one owner:

- a run's settings copied once at `SEQ start` (`RunConfig`), one trial's state reset by assignment
  (`TrialState{}`), the run's tallies (`RunTotals`); one `SeqOutputConfig`, not two;
- the trial classified at one site, fed the verdict's validity, so the host-tested classifier decides what the
  board prints; no synthesized Behavior fact (`behaviorEligible`) in Analyzer output;
- the SEQ commands safe while a run is active;
- no write-only fields, and no Analyzer presentation compiled into `src/detection/`.

Source: docs/research/architecture-readability-review-2026-10-25.md A8 (structure), D1.4 (SEQ commands), D2.6
(write-only fields), D2.8 (printers in detection/).

## Items, in order (one commit each, the host gate after each)

1. **C1, D1.4, SEQ commands.** `SEQ start` while a run is active is refused with a reason (as SELFTEST does);
   `SEQ STOP` clears the pending start as well; `SEQ STATUS` reads one config. Behavior change on purpose,
   operator-facing: the guide's SEQ section in the same commit.
2. **C2, D2.8, printers.** `DetectorReportPrinter.cpp` and `ScalarTransientPrinter.cpp` move to
   `modes/analyzer/` (they read only `DetectorReport`); the detector's `EVT transient` line moves to the Analyzer's
   reporter. The printed lines stay byte-identical (`test_report_printers` pins SEQ_SOURCE_CORE / SPEC; it moves
   with them). The Node and Emitter binaries lose the printers' code.
3. **C3, D2.6, write-only fields.** Delete what is assigned and never read (the list in D2.6, re-checked by grep
   first), outside the structs C4 rewrites anyway.
4. **C4, A8, the split.** `RunConfig` / `TrialState` / `RunTotals` replace `SequenceTest`'s top level and
   `PendingSequenceStart`'s copy; struct bodies in their own header, out of `AnalyzerModeApp.h`; one
   `SeqOutputConfig`; the `mutable` members reduced to what a const printer truly caches. No printed field changes.
5. **C5, A8, one classification.** `finalizeSequenceTrial`'s ladder and `classifySequenceTrial()` become one site
   fed the verdict validity; `behaviorEligible` in Analyzer output becomes `verdict_valid` (or goes: a decision
   file in the same commit). This is the one item that may change printed values (a trial the two ladders
   classified differently); each difference is listed in the commit.

## Standing gate (after every item)

- Every env builds (`gate.sh`), host suites pass, T2h byte-identical against the current reference, replay
  Null == diag, include direction strict. `test_analyzer_pass_rules` (now host and board) passes.
- Tool parsers unchanged unless the item says so: `SEQ_SUMMARY` keys (`expected_trials`, `rejected_trials`,
  `miss_trials`, `avg_strength`, ...), `SEQ_TRIAL reject_reason=`, `SEQ_EXPLAIN failed_label=`, SEQ_SOURCE_CORE /
  SPEC (seqcmp, benchlib, soakcycle, bandsweep, test_report_printers).

## Bench gate (after 11h; batch C's close)

An Analyzer A/B on the bench: the same SEQ set (hop10, chirp 3200 / 100, chirp 3450 / 150, a `pulses_bad` run, a
`SEQ start` sent during a run, `SEQ start` + `SEQ STOP` in one buffer) on the build before C1 (42f1eef, flashed
for 11h; its sweep logs are the before for hop10 / chirp150) and on the build after C5, compared with seqcmp:
identical except what C1 and C5 list.

## Don't touch without saying so

Detection thresholds and the plan (tuning is its own pass); the detector and inspector (C2 moves printers only);
SEQ line names and the keys the tools parse; the Emitter protocol; the Node.

## Output audit, 2026-10-27 (what the bench tools read; input to C2, C3, C5)

Measured over the 214 committed sessions on branch `bench` with `tools/bench/keycensus.py` (keys per line type,
distinct values, constants) and a read of every parser in `tools/bench/`, `tools/logging/` and `test/`.

**What the tools parse.** Four line types, these keys only:

| Line | Parsed by | Keys read |
|---|---|---|
| `SEQ_SUMMARY` | benchlib, seqcmp, bandsweep, soakcycle, cellrun | `expected_trials`, `rejected_trials`, `miss_trials`, `unexpected_trials`, `avg_strength`, `buffer_overrun_trials`, `profile`, `detector`; the rest of `benchlib.SUMMARY_FIELDS` is copied into index.csv and read by nothing |
| `SEQ_TRIAL` | benchlib, soakcycle, bandsweep, inspectstats | `trial`, `result`, `reject_reason` |
| `SEQ_EXPLAIN` | soakcycle, bandsweep | `failed_label` |
| `SEQ_INSPECT` | inspectstats | `inspect.label`, `inspect.valid`, `inspect.peak`, `inspect.p75`, `inspect.p90` |

`SEQ_SOURCE_CORE` / `SEQ_SOURCE_SPEC`: pinned by `test_report_printers`, key-set-diffed by seqcmp, no value read by
any tool; lab notes cite `carrier.*` and `accepted.confidence` by hand. `SEQ_TRIAL` keys cited by hand in docs/lab,
docs/research, bench notes: `amp_class`, `contrast_class`, `coverage_ms`, `island_max_ms`, `gap_max_ms`,
`dt_anchor`, `buffer_overrun`, `src_total`, `source_strength`, `verdict_confidence`, `emit_marker_seen`.

**Printed, read by nothing.**

- `SEQ_HISTDBG` (15,380 lines in the bench, every trial, every mode): issue #26 instrumentation, closed 2026-10-09;
  no tool, no test, cited only by archived pass docs.
- `SEQ_TIMING` (3,066 lines, diagnostics on): no tool, no doc, no lab note.
- bare `SEQ_SOURCE` (9,241 lines): benchlib counts the line type and reads no key; its content is a subset of
  `SEQ_SOURCE_CORE` printed one line earlier.
- `EVT transient success` in `ScalarTransientReport.cpp`: gated on `AUDIO_VERBOSE_DEBUG` (0, no env sets it), zero
  occurrences in any log; its five counters (`_onsetRejectedCount`, `_transientRejected*Count`, `_peakAcceptedCount`)
  have no other reader. C2 planned to move it; the evidence says delete it with the counters.
- `SIZE` boot lines, `SEQ_PROF`, `SEQ_BENCH_*`: old logs only; the last two are no longer in src.

In a current detail-mode log (`2026-10-27-step11h-checks/pos500_r01_hop10.log`) `SEQ_INSPECT` and the three SOURCE
lines are 71% of the bytes; the parsed keys live in `SEQ_TRIAL` + `SEQ_SUMMARY`, 5%.

**Constant over all 934 summaries / 16,500 trials.** `early_trials`, `duplicate_trials`, `ambiguous_trials`,
`too_dense_trials` (0; `late` fires in 3%); `avg_conf`, `verdict_confidence`, `accepted.confidence`,
`reject.confidence`, `pattern_confidence` (0.00 / 1.00: `verdict.confidence = valid ? 1 : 0`,
`VerdictProjection.cpp:85`, a boolean printed as a float five times); `dt_anchor` (`command`);
`coverage.future_unavailable`, `integrity.queue_overflow`, `inspect.integrity_queue_overflow` (0);
`detail.scalar.*.normalized`, `detail.scalar.reject.baseline`, `.reject.lift`, `.aggregate.max_rejected_lift`
(hard-coded 0, `ScalarTransientOccurrence.cpp:165-216`); `detail.scalar.inspect.opened / released /
min_strength_pass / carrier_quality_required` (1, tautological on an accepted occurrence).

**Write-only at HEAD (re-check of D2.6; `bestPending*` and `duplicateFirst/LastDtMs` are already gone).**
Analyzer: `TrialDiagnostics.onsetSeen`, `acceptedPatternStrength`, `emitSeen`, `emitDoneSeen`, `emitTrialId`,
`emitDoneDtMs`, `ambientBaselineSum`, `duplicatePatternStrength`, `duplicateOriginWindow` (never touched),
`SequenceTest.progressLineStarted`, `windowStartOffsetMs` (constant 0), `liveFrequencyMeasurementPacket`; the
`duplicate*` block feeds only `dup=` (nonzero in 1% of trials). Detection: `DetectionRuntime::_detectorSelection`,
`FrequencyBandMeasurementPacket.matched / targetHz / ageSamples / confidence`, `FeatureHistory::hasSamples /
latestValue`, the commented-out `history.record` lines in `FeatureExtractor.h`, the detector stat counters above.
No Node or Behavior code reads any scalar detail field (`lift`, `normalized`, `coverage*`, `island*`): they exist
for the Analyzer printers only.

**Consequences for the items.** C2 (landed be2e705) deleted the EVT line and its counters rather than moving
them; still open from this audit: drop bare `SEQ_SOURCE` and `SEQ_HISTDBG` (`SEQ_TIMING` is the owner's call, never
cited), with C3 or C5. C3: the list above replaces D2.6's.
C5: confidence collapses to the verdict's `valid`; the five confidence keys and the four zero buckets can leave the
printers, with `benchlib.SUMMARY_FIELDS` and its stale `SEQ_DETAIL` entry trimmed in the same commit (seqcmp's key-set
diff flags them against old logs; its docstring allows a key change, never a value change). Caveats: committed
sessions only (11h's sweep logs are not on `bench` yet); the field scan is a regex over `src/modes/analyzer/`, a
read through a reference alias would not show.

## Progress

- 2026-10-27: opened. Before-state for the bench A/B: 11h's sweep logs on 42f1eef
  (bench/sessions/2026-10-27-step11h-sweep24, pos*_hop10.log / chirp150.log, mode=detail verbose=1).
- 2026-10-27, C1 done: SEQ start refused while a run is active or queued (`ERR SEQ start refused reason=run_active`),
  SEQ STOP clears the queued start, SEQ STATUS prints one config (the running one, else the next start's), SEQ MODE
  quiet during a run also clears the running config's diagnostics flag. The tools that wait for SEQ_SUMMARY
  (benchlib.run_commands, soakcycle, bandsweep) send SEQ STOP when it never comes, so a hung run cannot block the
  next start. Host gate green, T2h byte-identical.
- 2026-10-27, C2 done: DetectorReportPrinter / ScalarTransientPrinter moved to `src/modes/analyzer/report/` (lines
  unchanged: test_report_printers passes); test_shipped_profile no longer includes them. The `EVT transient` line
  was compiled out everywhere (`AUDIO_VERBOSE_DEBUG` 0, review D1.9), so it was deleted with its flag chain
  (`setDiagnosticsEnabled` in the detector and DetectionRuntime, the Analyzer's five calls, the stats fields) rather
  than moved; SEQ DIAG keeps gating the Analyzer's own diagnostics. Node flash -248 B (the linker had dropped most).
- 2026-10-27: output audit written (section above, `tools/bench/keycensus.py` added): what the tools read, what
  is printed for nobody, what is constant, the write-only list re-checked at HEAD. No code changed.
