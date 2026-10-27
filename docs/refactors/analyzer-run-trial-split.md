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

## Progress

- 2026-10-27: opened. Before-state for the bench A/B: 11h's sweep logs on 42f1eef
  (bench/sessions/2026-10-27-step11h-sweep24, pos*_hop10.log / chirp150.log, mode=detail verbose=1).
- 2026-10-27, C1 done: SEQ start refused while a run is active or queued (`ERR SEQ start refused reason=run_active`),
  SEQ STOP clears the queued start, SEQ STATUS prints one config (the running one, else the next start's), SEQ MODE
  quiet during a run also clears the running config's diagnostics flag. The tools that wait for SEQ_SUMMARY
  (benchlib.run_commands, soakcycle, bandsweep) send SEQ STOP when it never comes, so a hung run cannot block the
  next start. Host gate green, T2h byte-identical.
