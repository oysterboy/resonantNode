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
  library.
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

## Decisions recorded in this pass

- decisions/2026-10-25-analyzer-params-on-registry-and-line-policy.md (items 8 and 9).
