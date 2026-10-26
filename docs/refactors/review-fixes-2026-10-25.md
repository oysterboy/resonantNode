# Step 11e: review fixes 2026-10-25 (pass doc)

Status: open 2026-10-25. Step 11e of docs/roadmaps/roadmap-0-steps.md, parallel to 11a; issue #63.
Source: docs/research/architecture-readability-review-2026-10-25.md (the findings, with evidence). This
doc holds the plan, the order, the gates and the progress; the research note stays as the reasoning.

Goal: the code matches the spec where it says it does, the duplicated and dead parts are gone, and the
next reader of DetectionRuntime, ScalarTransientDetector, the Analyzer console and the Node's param path
sees one shape per concept. The nodes' behavior does not change: the replay is byte-identical and the
Node's PARAM DUMP is byte-identical, except where an item below says "changes behavior" and why.

Surface: Claude Code on the web, no board. Everything here is compile plus host tests; the bench check
(Node smoke, Analyzer SEQ 10/10, a radio PARAM round trip) is the owner's when they are back, and is the
step's gate in roadmap-0-steps.md.

## Owner decisions 2026-10-25 (asked before the work started)

- Scope: items 1-6 of the research note's suggested order (below as items 1-9). The next Node cut and the
  Analyzer TrialState split are not in this pass (NODE-004 stays open).
- Spec: `myspec.md` 6.2 and 7.2 are rewritten around OutputRequest / OutputStatus as the boundary, folding
  decision 2026-10-23 in: Behavior decides the kind, the Node composes the shape from the band grid and the
  `output.*` params, SoundOutput executes. `ChirpPattern` leaves `ChirpOutput.h` for a device-free header.
- Analyzer params: the 14 scalar knobs move onto ParamRegistry with the Node's paths and grammar
  (`PARAM SET detection.scalar_* <value>`); the old `PARAM key=value` form is dropped, not aliased
  (decisions/2026-10-25-analyzer-params-on-registry-and-line-policy.md). PAR-003 lands here.
- Serial lines: one LineReader, one policy: a line over the buffer is refused with an ERR reply on every
  port (the Node's issue #31 rule), no longer run cut on the Analyzer and the Emitter.
- Roadmap: sub-step 11e, parallel to 11a, with its issue (#63). Delivery: one commit per item on the
  branch, a PR against main at the end.

## Ordered items

| # | item | roadmap | what | changes behavior? |
|---|---|---|---|---|
| 1 | Spec and the request vocabulary | OUT-002/003, decision 2026-10-23 | `myspec.md` 6.2 / 7.2 rewritten around OutputRequest / OutputStatus; `ChirpPattern` moves to `src/output/OutputRequest.h` (no device include); `ResonantBehavior.h` stops including `ChirpOutput.h` | no |
| 2 | Comment drift and file hygiene | review B9, B11 | stale defaults in `DetectionProfile.h`, "First Difference", tone level 0.3, the three `micros-wrap.md` pointers, `build.yml` "warning only", the post-split banners, RB BEHAV / RB DETECT / Freq leftovers; BOM off two headers; `#pragma once` plus guards; the literal control bytes in `test_net_frame` as escapes | no |
| 3 | Dead code and dead vocabulary | review A4, B10, A8 (fields); DET-018 7b | `AudioSignal` block path, `DetectorOccurrenceDetail`, curve callback; `RuntimeDefaults` unused aliases; unreachable decode branches; inspector's unreachable branch, empty `reset()`, unused `inspect()`; Node / Emitter / Analyzer unread members; the write-only `SequenceTest` fields; `evaluatorFirstEnabledRequirement` renamed; enumerators assigned nowhere removed (VerdictType, VerdictRejectReason, VerdictReasonCode, OccurrenceRejectReason, BehaviorDecision, TransientRejectReason, DetectorRejectClass, MagnitudeInspectionMode) with Behavior's dead Ambiguous branch. SEQ labels unchanged (7a stays in DET-018) | no (output identical) |
| 4 | RingQueue | review B1 | `src/app/RingQueue.h`, `template <class T, size_t N>`; used by DetectionRuntime (verdicts, pending inspections, pipeline events), ResonantBehavior (emit history), NodeNet (outbox) | no |
| 5 | Hold the config structs | review B3, A6 | `ScalarTransientDetector` holds a `ScalarTransientConfig` (13 setters and 13 member defaults go; `applyScalarTransientConfig` becomes one assignment); `ResonantBehavior` holds a `BehaviorGateConfig` (the idle-reschedule logic of `setIdleTimeMs` stays as the one setter with behavior) | no |
| 6 | Occurrence payload trim | review A5; spec 5.4 | `Occurrence.magnitude.evidence` removed; the Analyzer reads `InspectedOccurrence.magnitudeObservations[]` by target; sizes before / after recorded (host; xtensa when the toolchain is here) | no |
| 7 | DetectionDiagnostics | review A3 | the Analyzer-only counters, the pipeline event queue, the latest-result snapshot, the detector report capture and the history-incomplete snapshot move into `src/detection/DetectionDiagnostics.h`; the Node build gets the empty variant; `DetectionRuntime` keeps one `#ifdef` (which variant) and none in its bodies | no |
| 8 | One LineReader, one token parser | review B6 | `serial_line::LineReader<N>` in `src/app/SerialLine.h`, refuse policy; the seven readers use it. `tokenValue` and `parseUnsigned` in `src/app/TextParse.h`; hand-counted `token + N` offsets and bare `strtoul` replaced in the Analyzer, the Node and the Emitter | yes, console only: a too-long line is refused on every port; a non-numeric value is refused instead of read as 0 |
| 9 | describeParams, Analyzer on the registry | PAR-003; review A6 | `src/app/ParamBindings.h`: `bindScalarTransientParams()`, `bindBehaviorParams()` next to the ids; Node, Analyzer and host replay call them; the Node's PARAM LIST / GET / SET / DUMP handler becomes `src/app/ParamCommands` shared with the Analyzer; the Analyzer's key=value parser, the 14 diff blocks and `printParamStatus` go; `notes_manual.md` Analyzer PARAM section rewritten | yes, Analyzer console: new PARAM grammar (owner decision) |

Order: 1, 2, 3 first (doc and deletion, each byte-identical); then 4, 5, 6, 7 (detection, each byte-identical
on T2h); then 8, 9 (mode layer, the two owner-decided behavior changes). 5 before 7 (fewer members to move).
9 needs 8's `TextParse` helpers.

Open points, to ask before the item starts:
- item 9: `scalar_observed_stream` is an Analyzer-only knob of enum type; the registry has no enum type.
  Register it as a uint16 code, keep it as a `SEQ ... stream=` option only, or add an enum type to the
  library. Owner 2026-10-26: not a param; the profile sets it (`SEQ PROFILE`).
- item 7: whether the Analyzer reads diagnostics through `DetectionRuntime` forwarding methods (small
  Analyzer diff, Analyzer-only methods stay in the runtime header) or through `detection.diagnostics()`
  (the runtime header loses every Analyzer-only method).

Not in this pass (stays on the roadmap): the next Node cut (NODE-004: NodeParams, NetGateway, the baseline
state machine as a struct), the Analyzer TrialState / RunConfig split and its three-way classification
(review A8), the board-audio header for the piezo conditionals (review A11), SoundOutput's literal restores
and the I2S ramp knob (A12), `DetectorReport.source*` as Analyzer bookkeeping (A13), the `TimeMs` alias
(B8), FieldState density naming, the test gaps (B16). Threshold values are not touched.

## Standing checks (every commit)

- T1: every env builds. Here: `pio run -e <env>` for the eight ESP envs when the toolchain installs in the
  container; otherwise CI on the pushed branch (`.github/workflows/build.yml`) is T1.
- T2h: host replay byte-identical. `pio run -e native-replay`, then `program --verbose --dir test/fixtures`
  before the pass; the output's sha256 is recorded below and every item compares against it. The bench
  worktree is not checked out here, so the step-8 baseline file is the owner's comparison at the bench.
- T4 host: `pio test -e native-test`, `pio test -e native-test-behavior`.
- T5: `sizeof` probe for items 4-7 (host g++; xtensa when available), before / after in the progress line.
- PARAM: the Node's registered path list (grep of `_paramRegistry.add*` paths) unchanged by items 1-8; item 9
  adds the Analyzer's registrations and changes no Node path.
- Include direction: `python3 tools/ci/include_direction.py --strict`.
- `notes_manual.md` pointers fixed in the same commit as any renamed command, param path or config site.

## Progress

(one line per landed item: date, commit, what, the checks' result)

- 2026-10-25, item 1: myspec 6.2 / 7 rewritten (layering table, OutputRequest / OutputLimits / OutputStatus,
  the Node as composer per decision 2026-10-23, OUT-004 as the move point); `src/output/OutputRequest.h` holds
  `ChirpPattern`, `OutputRequest`, `OutputLimits`, `OutputReject`, `OutputPhase`, `OutputStatus` and one
  `chirpPatternName()` (the Node's and NodeDebug's copies gone); `ResonantBehavior.h` and `ResonantNodeDebug.h`
  include it, not `ChirpOutput.h`; BOM off `ResonantBehavior.h`. Host: 68 tests pass (g++ harness, PlatformIO's
  registry is blocked in this container), T2h byte-identical (sha c4bf73ec), include direction strict 0. T1 on CI.
- 2026-10-25, item 2: comment drift and hygiene. DetectionProfile.h (stale "default 20000 / 2500", "pattern
  rules", the duplicated minTransientPeakStrength line, the second include guard); First Difference named as the
  default in AudioSourceI2S / AudioSelfTest; tone level 0.3 in RuntimeDefaults / I2sToneOutput; the three
  micros-wrap.md and the step10-boundary-map / damp-bench-check pointers to archive/; build.yml and
  include_direction.py "warning only until ANA-003"; the Node's garbled Does-NOT line, the stacked banners after
  the split, the Transitional / RB BEHAV / RB DETECT leftovers (boot banner line now `RB PROFILE detail`); the
  three new Node TUs lose the pasted preamble's unused headers; Analyzer `build="pass-c"` is BUILD_VERSION (never
  printed); BOM off MagnitudeWindow.h; test_net_frame's raw control bytes are escapes (git diffs it again);
  FreqBandStream's public constants in the public section; test/ and lib/ READMEs say what is there. Host 68/68,
  T2h byte-identical, include direction strict.
- 2026-10-25, item 3a (detection, audio, app): AudioSignal loses the never-called block path, `lastBlock*`, the
  curve callback and `DetectorOccurrenceDetail`; `processSample` keeps the one parameter it reads; IRAM_ATTR only
  on the per-sample path; the quiet-threshold fallback is the 20 every mode sets. RuntimeDefaults' three unused
  aliases gone; the unreachable 16- / 8-bit I2S decodes refused with a static_assert. OccurrenceInspector: one
  entry point `inspectWithHistory` (the `inspect` / `inspectImpl` / `inspectAcceptedOccurrence*` chain and the
  empty `reset()` gone). FeatureHistory loses `copyWindowApproximateValues`. `evaluatorFirstEnabledRequirement`
  is `firstEnabledRequirement`. Vocabulary (DET-018 7b): VerdictType {None, SinglePulse}, VerdictReasonCode
  {None, FromOccurrence, SupportRequirementFailed}, VerdictRejectReason {None, MissingSupport, SupportTooLow},
  OccurrenceRejectReason {None, InvalidTiming, UnsupportedKind}; TransientRejectReason::PeakStillActive and the
  six BehaviorDecision values assigned nowhere removed, with Behavior's Ambiguous branches, its local
  verdictTypeName copy and the always-zero `ignoredAmbiguous=` field of RB behavior (no tool parses it). Kept on
  purpose: MagnitudeInspectionMode / Basis (implemented profile options, not dead) and DetectorRejectClass (the
  outward contract of 5.3). Host 68/68, T2h byte-identical, include direction strict.
- 2026-10-25, item 3b (modes): the Analyzer's write-only trial bookkeeping is gone (39 `SequenceTest` /
  `TrialDiagnostics` fields: the pending-sample array and its best / origin / class bookkeeping, the per-trial
  accepted / duplicate copies of verdict fields, the onset-reject counters, the legacy run tallies hits / misses /
  unexpected / duplicates / startupArtifacts, `diagMode` with its enum and mapper, `kPrintIntervalMs`), with
  `SequencePendingClass`, `sequenceOutputModeEnabled`, `kSequenceWarmupMs` and the unused `_inputPin` ctor
  argument. Emitter: `_outputPin` / `_outputBtlPin` members and `ControlSerialKind`. Node: the write-only
  `_rbLastPendingMs` pair and `RBBaselineState::Rebase`, which was set and overwritten two statements later so
  no observer could see it (`rbBaselineStateName` never printed REBASE). SEQ output unchanged: every removed
  field was assigned and never read (grep per field). Host 68/68, T2h byte-identical; the mode layer compiles
  only on CI (T1).
- 2026-10-25, item 3a follow-up: CI caught the Analyzer's OCCURRENCE summary reading the three `lastBlock*`
  accessors (my grep looked for `lastBlock()` only). It prints the same values from `latestFrame()` (one-sample
  blocks: start = sampleIndex, count = 1, micros = timeUs64). Lesson for the later items: grep the bare symbol
  stem across src/ and test/ before deleting a member.
- 2026-10-25, item 4: `src/app/RingQueue.h` (`RingQueue<T, N>`: push / pushOverwrite / emplace / pop / front /
  popFront / at / clear). DetectionRuntime's three rings (verdicts, pending inspections, Analyzer pipeline
  events), Behavior's own-emit history and NodeNet's heap outbox use it; the read-index / count pairs and the
  six push / pop bodies are gone. Host 68/68, T2h byte-identical; sizeof DetectionRuntime unchanged (68,024 host),
  ResonantBehavior 576 -> 568. NodeNet builds on CI.
- 2026-10-25, item 5: `ScalarTransientDetector` holds a `ScalarTransientConfig` (`setConfig` / `config()`; the
  struct in its own header `detectors/scalar/ScalarTransientConfig.h`, DetectionProfile.h includes it); the 13
  setters and the 13 members with their third default set are gone, `applyScalarTransientConfig` is one call.
  `ResonantBehavior` holds a `BehaviorGateConfig` (`configure()` / `config()`): 14 setters, 14 getters and 13
  members gone; the step 10e idle reschedule lives in `configure()`, the own-emit budget clamp too. Readers
  (RB STATUS, SELFTEST, the behavior tests) use `config().field`. The two on-device detector tests that relied
  on the detector's built-in 75 / 67.5 thresholds set them explicitly (`wideOpenConfig()`); syntax-checked on
  the host, run on a board when one is back. Host 68/68, T2h byte-identical.
- 2026-10-25, item 6: `Occurrence.magnitude.evidence` (a whole `MagnitudeInspectionObservation`, written by the
  inspector into its copy of the occurrence, the last module's winning) is gone, with the `MagnitudeEvidence`
  alias; the Analyzer's one reader takes the last module's observation from `InspectedOccurrence`, same value,
  same `available` rule. myspec 5.4's deferred "payload trimming" line now records the trim. Host sizes:
  Occurrence 504 -> 224, InspectedOccurrence 1,336 -> 1,056, DetectionPipelineEvent 2,120 -> 1,840,
  ScalarTransientDetector 2,048 -> 1,768, DetectionRuntime 68,024 -> 65,512 (xtensa: T5 at the bench).
  Host 68/68, T2h byte-identical.
- 2026-10-26, merge of origin/main (step 11a closed); two doc conflicts (roadmap status lines, decisions index).
- 2026-10-26, item 7: `src/detection/DetectionDiagnostics.h/.cpp`. The Analyzer-facing records
  (`DetectionPipelineEvent`, `SourceDiagnosticRecord`, `PipelineIntegrity`, `HistoryIncompleteSnapshot`) and
  the recording (counters, pipeline event queue, detector report capture, history-incomplete snapshot) move into
  `DetectionDiagnostics`; the Node build holds `NullDetectionDiagnostics` (inline no-ops, an empty `ReportSlot`).
  `DetectionRuntime` calls the hooks unconditionally and has no `#if` left (was 21 in the .cpp, 6 in the .h); the
  one build decision is the `ActiveDetectionDiagnostics` alias. The Analyzer reads through
  `_detection.diagnostics()` (owner's choice); `resetDiagnosticsCounters`, `resetSourceRejectSummaries` and
  `setDiagnosticsEnabled` stay on the runtime (they touch runtime-owned state). Gone with it: the
  `DetectionPipelineResult` snapshot, `latestPipelineResult` / `hasLatestPipelineResult` / `scalarReportGeneration`
  / `resetDiagnostics` (no callers) and `setProfileName` (only the snapshot read it; four callers drop the line).
  Host sizes: DetectionRuntime Analyzer variant 65,512 -> 64,008, Node variant 54,880. Host 68/68, T2h
  byte-identical, the Analyzer variant syntax-checked on the host (CI builds it).
- 2026-10-26, item 8: `serial_line::LineReader<N>` (app/SerialLine.h) replaces the seven hand-rolled readers (Node
  USB, Analyzer USB and Serial2, the ack wait, the RAW pump, Emitter Serial2 and USB): one byte filter, one
  policy, a too-long line refused whole with `ERR line_too_long max=N`. `text_parse::parseUnsigned`,
  `tokenValue` and `rest` (app/TextParse.h) replace the Analyzer's local helpers, 44 hand-counted `token + N`
  / `line + N` offsets and the bare `strtoul` calls: RAW `f=abc` and the Emitter's `freq=abc` are refused with
  `ERR <cmd> bad value token=` instead of being read as 0; SEQ TRIES / VERBOSE / FREQUPDATEEVERYSAMPLES and
  `verbose=` the same. Left as is: the `lo-hi` band parse (range-checked) and the hop list (end-checked).
  New host suite `test_text_parse` (7 tests) in native-test-behavior. notes_manual: a "Serial lines" paragraph.
  Host 75/75, T2h byte-identical; the mode layer builds on CI.
- 2026-10-26, item 9 prep (8758f09): `ScalarTransientConfig` and `BehaviorGateConfig` carry their ms and count
  fields as `uint32_t`, the registry's width; the Node bound them through `reinterpret_cast<uint32_t*>` behind a
  static_assert that only holds on the 32-bit target, and the shared binding runs on the host too (Linux CI).
  One overload fix (`selftest::field` took the uint32 as ambiguous). Host 75/75, T2h byte-identical.
- 2026-10-26, item 9 (PAR-003): `src/app/ParamBindings.h` (`bindScalarTransientParams`, `bindBehaviorParams`:
  the 13 + 13 paths, ranges and ids in one place; the Node's registrations call them, same paths as before) and
  `src/app/ParamCommands.h/.cpp` (one PARAM handler for every mode, the Node's lines; it returns what it wrote
  and the caller applies: the Node's `applyParamModule`, nothing on the Analyzer). The Analyzer holds a
  `ScalarTransientConfig _scalarTuning` on its own `ParamRegistry` (13 params, no store: SAVE / LOAD answer
  `status=no_store`, RESET returns to the profile's values); a value applies at the next SEQ start as before;
  the observed stream is the profile's (owner: not a param). Gone: `AnalyzerTuning`, the key=value parser
  (`parseScalarTransientToken`, `parseFeatureStreamToken`), the four `printParamField` overloads and the 14
  diff blocks, `printParamStatus`, the five `PARAM TUNING TEMPORARY` markers; an old-form token answers
  `ERR PARAM unknown; use PARAM SET detection.scalar_* <value>`. The host replay's `--set` takes the same
  paths through the same binding (`detection.scalar_*`, `detection.contrast_medium`, `detection.amp_medium`),
  range-checked like the board (`tools/bench/notchsweep.py` updated; the corpus test's 40000 limit is 32767,
  the range's top). Node side effect: PARAM LOAD / RESET now also apply `output.silent` (applySilent is
  idempotent; RESET used to leave a silenced node silent until the next SET). New host suite
  `test_param_commands` (5 tests). `text_parse::equalsIgnoreCase`; the Node's copy forwards to it.
  notes_manual Analyzer section and knob table, roadmap-param-config PAR-003 DONE. Host 80/80, T2h
  byte-identical, include direction strict; the Node's registered path list unchanged (39), the Analyzer's
  13 are the first 13 of it; the three mode builds are CI's.

## Handoff 2026-10-26 (to the session on main with the bench)

State: PR #64 (`ccr-b69abead-y64emm`, head f454d88) carries items 1-9 and is green on CI (run 110: eight ESP envs,
host suites 80/80, include direction). Not merged; merging is the owner's call. What is still open in step 11e is
the bench gate, which needs boards, and the diagnostics tests the owner asked for after reading item 7.

### What item 7 moved, and what it saved

Moved out of `DetectionRuntime` into `DetectionDiagnostics` (Analyzer build) / `NullDetectionDiagnostics` (every
other build): the eight per-frame counters (`observeFrame`, `freshDetectorInput`, `noFreshFrequencySkip`,
`detectorDrain`, `detectorReportRefresh`, `occurrencePopped`, `validOccurrencePopped`, `verdictProduced` plus the
pushed / dropped pair), the 4-slot `DetectionPipelineEvent` queue and its sequence ids, the detector report
capture (`ReportSlot` per pending inspection, `activeDetectorReport()`, the mismatch counter), the
history-incomplete snapshot (issue #26), and the records `SourceDiagnosticRecord`, `PipelineIntegrity`,
`DetectionPipelineEvent`, `HistoryIncompleteSnapshot`. The runtime kept what it owns and the Analyzer resets
through it: `resetDiagnosticsCounters`, `resetSourceRejectSummaries`, `setDiagnosticsEnabled`. Dropped as dead
on the way: the `DetectionPipelineResult` snapshot (nobody read it) and `setProfileName`.

Savings, measured (CI runs 95 before, 100 after; host sizeof with g++ 64-bit):

| | before | after |
|---|---|---|
| Node binary RAM / flash (esp32dev) | 100,108 / 848,445 | 100,108 / 848,461 |
| Analyzer binary RAM / flash (esp32dev-analyzer) | 93,548 / 415,717 | 92,476 / 414,493 |
| `sizeof(DetectionRuntime)` Analyzer variant, host | 65,512 | 63,936 |
| `sizeof(DetectionRuntime)` Node variant, host | 54,864 | 54,808 |
| `#if` lines in DetectionRuntime .cpp / .h | 24 / 8 | 0 / 0 |

So: the Node saved nothing (the ifdefs already kept the recording out of it), the Analyzer saved about 1 KB of
RAM (the dead snapshot), and the point of the item was the shape, not the bytes: one build decision (the
`ActiveDetectionDiagnostics` alias) instead of 32 conditional lines, and a runtime body that reads the same in
both builds.

### The owner's concern: a separate diagnostics layer as a source of error

The risk is real and specific. The Analyzer's SEQ_SOURCE / SEQ_INSPECT / SEQ_EXPLAIN lines and the trial
classifier read the side channel (`diagnostics().popPipelineEvent`, `activeDetectorReport`, the counters), not
the verdict queue, so a hook called at the wrong point, a report captured too late (the detector's report
refreshes while an occurrence waits in the pending queue), or a reset that clears one side and not the other
would make the Analyzer describe a different trial than the detector decided. Two things are known today:

- The replay (T2h) and `test_replay_corpus` build the Null variant, so until now nothing on the host ran the
  full layer. Checked 2026-10-26 on this branch: the replay built with `-D ANALYZER_MODE` (the full layer
  compiled in and called on every frame) is byte-identical to the Null build over `test/fixtures` (sha
  c4bf73ec), and `test_replay_corpus` passes 21/21 under it. The layer does not change verdicts; it is not yet
  shown to describe them correctly.
- Nothing asserts that what the layer records matches what the runtime decided. That is the gap.

### Tests to write (host first, then the bench)

Host, `test/test_detection_diagnostics` in native-test-behavior, built with `-D ANALYZER_MODE` (the only flag
that selects the full variant inside `src/detection`; `main.cpp` and `NodeNet.cpp` are not in the test build):

1. Equivalence as a standing check: the fixtures replayed through both variants in one binary is awkward (one
   alias per build), so add a second host env `native-replay-diag` extending `native-replay` with
   `-D ANALYZER_MODE`, and have CI diff its `--verbose --dir test/fixtures` output against `native-replay`'s.
2. One event per verdict: over every fixture, `verdictProducedCount` equals the verdicts popped plus
   `verdictEventDroppedCount`; every popped `AcceptedPipelineResult` event carries the occurrence id and the
   start ms of the verdict it belongs to, in order; `detectorReportMismatchCount` stays 0 and
   `detectorReportMatched` is true on every event.
3. The report captured at pop time: a synthetic stream that produces two occurrences close enough that the
   first waits in the pending queue while the second refreshes the detector's report (the shipped values make
   this rare: min duration 92 ms, one occurrence per 85 ms or more, a wait of 15 ms or less). Assert the first
   event's report is the first occurrence's, not the second's. This is the case the `ReportSlot` exists for.
4. Overflow: five verdicts with no consumer draining the 4-slot event queue; `pipelineEventOverflowCount` is 1,
   the next event's `PipelineIntegrity` says `queueOverflowAffected` with reason `PipelineEventQueueOverflow`,
   and the verdict queue (the Node's path) is unaffected.
5. Resets: `resetDiagnosticsCounters` zeroes the counters and leaves the event queue; `resetDetectionState`
   (a SEQ trial boundary) leaves no event from the previous trial behind (`eventTrialAttribution`), and the
   history-incomplete snapshot is cleared with the history.
6. Counters against the frames fed: `observeFrameCount` equals frames fed, `freshDetectorInputCount +
   noFreshFrequencySkipCount` equals the valid frames, `detectorDrainCount` is at most the frames.
7. The Null variant: a compile-time check that `NullDetectionDiagnostics` is empty (`sizeof == 1`) and that
   `DetectionRuntime`'s Node-variant size does not grow (a `static_assert` with the current 54,808 on the host
   is too brittle; print it in the sizes probe and record it here instead).

Bench (the step's gate, owner's hardware; `tools/bench/seqrun.py`, compare with `seqcmp.py` against the last
pre-pass session in `bench/sessions/`): Analyzer SEQ 10/10 on the D-AMP pair with `SEQ MODE detail`, so every
trial prints SEQ_SOURCE / SEQ_INSPECT / SEQ_EXPLAIN; the fields that come from the diagnostics (report matched,
event ids, `SEQ_HISTDBG`) must read as before the pass. One run with `PARAM SET detection.scalar_onset_threshold`
raised until trials miss, to see the new PARAM grammar reach the detector at the next SEQ start. Node smoke:
boot (`PARAM load`, `RB PROFILE detail`), RB STATUS, ten hop trains answered, `PARAM RESET` with `output.silent 1`
saved beforehand (the node must sound again: the LOAD / RESET side effect of item 9). A radio PARAM round trip
(`NET <node> PARAM DUMP`). Then `test_analyzer_pass_rules` and `test_timebase` on a board (item 5 touched their
thresholds; syntax-checked only).

Closing the step: gate results as a dated line here, `Archived:` header, move this file to `archive/`, close
#63 with the commit, PAR-003 is already DONE.

## Decisions recorded in this pass

- decisions/2026-10-25-analyzer-params-on-registry-and-line-policy.md (items 8 and 9).
