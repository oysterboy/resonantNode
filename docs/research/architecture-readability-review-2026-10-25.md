# Architecture critique and readability review, 2026-10-25

Status:  promoted (2026-10-25); re-read 2026-10-26 at main 4612ce9 (after steps 11b, 11e, 11f), Part D added
Became:  step 11e, docs/refactors/archive/review-fixes-2026-10-25.md (items 1-9 of the suggested order; #63);
         step 11f (2026-10-26, #65): the small parts of A9, A12, A13, B7, B12
Lab:     none; a code reading at main c0b75e9 (after 5ffab4a "Node: split by role"), host `sizeof` probe;
         re-read at 4612ce9 by three parallel readings (detection / audio / host; the mode layer; docs, tests,
         tools), every claim checked against the source before it was written down here

A review of the whole tree for two questions: does the code match the boundaries `docs/specs/myspec.md`
states, and what does a human maintainer trip over when reading or changing it. Line numbers are as of
c0b75e9 and will drift; the file and function names are the durable pointers. Everything below was checked
against the source, dead-code claims by grep. Nothing here is a commitment; items that get picked up should
be promoted to roadmap items or decisions per `docs/research/README.md`.

Re-read 2026-10-26 (main 4612ce9): the `->` line under each heading in Parts A and B says what steps 11e and
11f did to it. Short form: A1-A3, A5 (the payload), A6 (the param side), B1, B3, B6, B9, B10, B11 (the listed
items), B12 are done; A4, A5 (the twins), A13, B2, B7, B8, B16 are part done; A7, A8, A10, A11, B4, B5, B13, B14,
B15 are untouched and stay the big-ticket items. Part D holds what the re-read found that this note did not
have, including two correctness findings in code that did not exist on 2026-10-25 (the shape inspector's bin
track) and one in a test that cannot link since item 7. Line numbers in Parts A-C are still those of c0b75e9;
Part D's are of 4612ce9.

Calibration first: the repo is in better shape than its biggest files suggest. The things that are done well
are listed at the end and should not be undone while fixing the rest.

Known already-open items this overlaps with: DET-018 (vestigial verdict vocabulary), DET-016 (verdict
boundary rule), NODE-004 (Node file split, "a file split, not separate classes" in roadmap-node.md), step 9
(detector family build flag). Where a finding is covered by one of them it says so.

---

## Part A. Architecture: where the code and the spec disagree

### A1. Output composition sits in the Node; spec 6.2 assigns it to Behavior (high)

-> done in step 11e item 1 (myspec 6.2 / 7 rewritten: Behavior decides the kind, the Node composes, "the composer's rule" for the fallback); the code matches. But see D1.1: the spec's "a reject is a composition bug" is not true today, 2026-10-26

`src/modes/resonant/ResonantNodeApp.cpp`: `hopFrequencies` / `composeHopTrain` (around 583-615) own
rotation and bin order; `startEmission` (around 770-848) picks HopTrain vs IdleCall vs Pattern from
`_hopEnabled && _behavior.signalShape()`, fills the announce tone, and on a rejected request decides
"fallback=pattern" and restarts. The knobs `_hopEnabled/_hopRotation/_hopPulseMs/_hopGapMs/_hopSteps/
_idleAnnounce*` are Node members (`ResonantNodeApp.h` 224-241), bound as `output.*` params.

`myspec.md` 6.2 lists "which bins, in what order, how many steps, pulse and gap length" and "node-specific
variation" as Behavior configuration and says these "do not belong to: Node". Decision
2026-10-23-soundoutput-executes-node-composes.md permits the Node to compose (Behavior should not know the
band grid), but the spec was not amended, so the canonical document and the code contradict each other.
`ResonantNodeApp.h:36` "Does not implement behavior decisions" is not true for the fallback rule.

Options: (a) amend 6.2: Behavior decides the kind (`SignalShape`), the Node composes the shape from the
band grid, with a pointer to the decision; (b) a `HopSignalComposer` that Behavior asks for a complete
`OutputRequest`, so the fallback rule moves out of the Node. Either is fine; the spec must say which.

### A2. Behavior includes a concrete output device header (high)

-> done in step 11e item 1 (`src/output/OutputRequest.h`; Behavior includes no device header), 2026-10-26

`src/behavior/ResonantBehavior.h:7` includes `../output/ChirpOutput.h` to get `ChirpOutput::ChirpPattern`
(used at .h 157/266 and throughout the .cpp). `OutputRequest` (`src/output/SoundOutput.h:32`) carries the
same device enum. Spec 7.1: "Behavior should not care which output hardware path is active." The request
vocabulary (Single / Idle / Triple, or better `OutputRequest::Kind`) belongs in a device-free header, e.g.
`output/OutputRequest.h`, and `ChirpOutput` consumes it.

### A3. The Analyzer's diagnostics build is woven through DetectionRuntime with the preprocessor (high)

-> done in step 11e item 7 (`DetectionDiagnostics` / `NullDetectionDiagnostics`, no `#if` left in the runtime; host suite and the Null-vs-diag replay diff in CI). Spec 5.5 still calls the file archive material: D3.1, 2026-10-26

`src/detection/DetectionRuntime.cpp` has 21 `#if/#ifdef/#endif` lines, `DetectionRuntime.h` 6. One
`#ifdef ANALYZER_MODE` sits inside a method signature (`inspectAndRender(..., #ifdef const DetectorReport&)`,
.h 235-238, .cpp 505-509), one inside the `PendingInspection` struct (.h 276). `drainDetectors` and
`observeFrame` interleave core and diagnostics statements line by line. Every edit must be reasoned in two
preprocessor states; CI catches breakage, reading does not.

Direction: a `DetectionDiagnostics` collaborator (counters, pipeline event queue, history-incomplete snapshot,
detector report capture) that is a full class in the Analyzer build and an empty struct with inline no-op
methods in the Node build. `DetectionRuntime` calls it unconditionally; the one remaining `#ifdef` picks the
type. The isolation cleanup (archive/cleanup-analyzer-node-isolation.md) got the binary split right; this is
the readability half of it.

### A4. Residue of the removed OccurrenceEvaluator / correlation queue is still load-bearing (high)

-> partly done in 11e item 3 (`capturePipelineResult` gone, `firstEnabledRequirement`, the dead enumerators). Still: `PipelineIntegrity.correlationComplete` and the three never-assigned overflow reasons (`DetectionDiagnostics.h:48-65`), `inspectionPresent = detectorReportMatched` (`.cpp:169`), the occurrence id three times per event, `TransientEvidence` with no users, `InspectionTarget::TargetBand` produced by no profile; `AnalyzerTrialClassifier.cpp:22, 66` still names the evaluator (DET-018 7a), 2026-10-26

- `DetectionRuntime::capturePipelineResult` (.cpp 573-650) takes two references, aliases them to pointers
  `matchedInspectedOccurrence` / `matchedDetectorReport`, and null-checks them about ten times. They can never
  be null since step 8; the matching logic is the correlation queue's, left in place.
- `PipelineIntegrity` keeps `correlationComplete` and the reasons `InspectionQueueOverflow`,
  `VerdictQueueOverflow`, `PipelineEventQueueOverflow` from the deleted queue. `integrity.inspectionPresent`
  is assigned `event.detectorReportMatched`, which is a different fact.
- `DetectionPipelineEvent` stores the occurrence id three times (`occurrenceId`, `sourceOccurrenceId`,
  `sourceRecord.sourceOccurrenceId`), the string `sourceSelection` twice, `detectorReportPresent` twice.
- `evaluatorFirstEnabledRequirement()` (`DetectionProfile.h:88`) is named for the removed class; used in 4
  places.
- Dead enumerators (grep over src/, counting uses outside the declaring and the name-table file):
  `VerdictType` 4 of 6 never produced (makeVerdict emits only SinglePulse; Behavior branches on Ambiguous,
  which is never produced); `VerdictRejectReason` 10 of 13 never assigned (`Frequency*`, `NoProposal`,
  `Duplicate*`, `Unexpected*`); `VerdictReasonCode::DetectorRejected/AmbiguousEvidence` never assigned;
  `OccurrenceRejectReason` 8 of 11 never assigned (the inspector uses `UnsupportedKind` and `InvalidTiming`);
  `BehaviorDecision::Disabled/WaitingAfterHeard/IgnoreAfterOwnEmit/CooldownAfterDetect/
  ResponseProbabilitySkipped/UnknownBlocked` never assigned; `TransientRejectReason::PeakStillActive` never
  assigned; `DetectorRejectClass::Threshold/Cooldown/Window` never assigned; `MagnitudeInspectionMode` 5 of 7
  unused by any profile; `InspectionTarget::TargetBand` never produced; `TransientEvidence`
  (`InspectorTypes.h`) has no users.
  DET-018 covers the verdict vocabulary (7a/7b); the runtime residue above is on no roadmap item.

### A5. Occurrence has become the diagnostics dump spec 5.4 forbids (high)

-> partly done in 11e item 6 (`Occurrence.magnitude.evidence` gone: Occurrence 248 B, InspectedOccurrence 1,080 B on the 64-bit host). Still: the `AcceptedOccurrenceSummary` / `SelectedRejectSummary` twins with the `strength` alias and `mean == meanStrength` (`DetectorReport.h:14-70`); and a second dead payload found now, `Occurrence.band` (D2.1), 2026-10-26

`Occurrence.magnitude.evidence` (`occurrences/Occurrence.h`) embeds a full `MagnitudeInspectionObservation`
(272 bytes on the host). The Inspector writes into its copy of the occurrence
(`OccurrenceInspector.cpp:286`, `out.occurrence.magnitude.evidence = observation`), once per module, so with
the shipped two-module plan only the second module's observation survives; which one depends on plan order.
`InspectedOccurrence` already holds every module's observation. Spec 5.4: "Occurrence must not become a
detector diagnostics dump"; the same section defers "payload trimming" to after the evaluator cleanup, which
is now done. The one reader is `AnalyzerModeApp.cpp:1144-1145`, which could read
`inspected.magnitudeObservations[]` instead.

Measured on the host build (x86-64; the xtensa numbers differ by alignment, cleanup-0-plan T5):

```text
Occurrence                        504 B
MagnitudeInspectionObservation    272 B   (MagnitudeWindow, its twin: 272 B)
InspectedOccurrence             1,336 B
OccurrenceVerdict                 112 B
DetectorReport                    536 B
DetectionPipelineEvent          2,120 B   x 4 queue slots
DetectionPipelineResult         2,064 B
ScalarTransientDetector         2,040 B
FeatureHistory                 51,472 B
DetectionRuntime (Analyzer)    68,024 B
ResonantBehavior                  576 B
```

`NodeNet.h:83` now allocates the radio outbox on the heap with the comment "static RAM is full". The
pipeline types above are where the static RAM went. `AcceptedOccurrenceSummary` and `SelectedRejectSummary`
(`DetectorReport.h`) are field-for-field identical apart from `rejectClass`/`detectorReason`, and carry
`strength` as a "compatibility alias for peak" plus `mean` and `meanStrength` set to the same value.

### A6. No single source of truth for a knob's default; one param is six to eight touch points (high)

-> partly done in 11e items 5 and 9 (config structs held, `src/app/ParamBindings.h` for the 13 + 13 paths, the Analyzer on the registry, the replay through the same binding). Still: `detection.contrast_medium` / `amp_medium` bound in two places against different fields (`ResonantNodeParams.cpp:28-30` Node copies, `Replay.cpp:266-274` the plan module); an `output.*` knob is still six touch points; no add-a-param recipe in the guide; `ScalarTransientConfig.h` initialisers are a non-shipped set labelled "fallback", 2026-10-26

- `AudioSignal.h:262` initialises `_baselineTrackingQuietThreshold = 40`; all three callers set 20
  (`ResonantNodeApp.cpp:646`, `AnalyzerModeApp.cpp:603`, `host/Replay.cpp:320`); the comment at .h 239 says
  "quiet < 20". The header default never runs. Same pattern for the smoothing factors.
- `ScalarTransientConfig` has four default layers before the registry: detector members
  (`ScalarTransientDetector.h:97-109`, 75 / 67.5 / 500), the config struct (`DetectionProfile.h:52-66`,
  18000 / 12000 / 50), the profile factories, and the build macros `SCALAR_AMP_MEDIUM_THRESHOLD` /
  `SCALAR_CONTRAST_MEDIUM_THRESHOLD`. None of the first two is a shipped value. `BehaviorGateConfig` vs
  `ResonantBehavior` members is the same (wait 100 vs 500, idle 60000 vs 20000).
- One registered param, `output.hop_pulse_ms`: `RuntimeDefaults.h` constant, `ParamIds.h` id,
  `ResonantNodeApp.h` field initialiser, `ResonantNodeParams.cpp` add + path + range, apply site, three print
  sites in `ResonantNodeStatus.cpp`, `notes_manual.md` pointer.
- The Analyzer has a separate hand-written 14-key PARAM parser (`AnalyzerCommands.cpp:123-180`, two spelling
  aliases per key), 14 diff-print blocks (283-328), the same 14 fields printed again in
  `AnalyzerRuntimeReporter.cpp:34-67` and 102-134 and `AnalyzerSequenceSession.cpp:334-365`, and a fourth
  dump on the Node (`ResonantNodeApp.cpp:102-200`). `//PARAM TUNING TEMPORARY` appears five times. Its
  syntax (`PARAM key=value`) differs from the Node's (`PARAM SET path value`). `host/Replay.cpp:251-283` is a
  third key-to-field map with hard-coded `modules[0]`/`modules[1]`.
- No "adding a param" recipe exists in `notes_manual.md`.

Direction: a `describeParams(ParamRegistry&)` next to each owning config struct, used by Node, Analyzer and
host replay alike; field initialisers as the one default (the registry's `captureDefaults()` already treats
them so); delete the detector/behavior member defaults by holding the config struct (see B3).

### A7. The Node split is a file split, not a decomposition (medium; NODE-004 says so)

-> worked out further in docs/research/node-structure.md (classes, files, the later roadmap's seams), 2026-10-27
-> open (NODE-004). As described: 35 members plus `SelfTestState` in the header, the baseline machine run by hand in `ResonantNodeCommands.cpp:92-94`, reply / join state in file-scope globals `ResonantNodeNet.cpp:59-70` never reset by `begin()`, `setTee` paired by hand in four places, `makeTonalPulseBehaviorProfile()` still `return BehaviorGateConfig{}`. New on top: the `Boot` state is unreachable (D2.4), 2026-10-26

- All 35 data members plus the 25-field `SelfTestState` body stay in `ResonantNodeApp.h` (190-293, 116-156);
  every one of the 7 Node TUs includes it and touches members across roles.
- `ResonantNodeCommands.cpp:108-142` runs the baseline state machine directly (`_rbBaselineState = Rebase;
  performRbRebase(); _rbBaselineState = Settle; ...`), the same four lines `updateRbBaselineState`
  (`ResonantNodeApp.cpp:516-522`) executes. `ResonantNodeNet.cpp` reads `_selfTest.phase`;
  `ResonantNodeParams.cpp` drives `_detection`, `_freqBandStream` and plan internals (A10).
- Radio reply state is in file-scope globals (`ResonantNodeNet.cpp:67-78`: `gOpenReply`,
  `gOpenReplyActive`, `gJoin`) that `Node::begin()` never resets; the `serialOut().setTee` is set and cleared
  by hand in three places around a flag (an RAII guard would make the pairing structural).
- The three new TUs carry a byte-identical 20-line include preamble (md5 equal), including `<new>` twice and
  four headers none of them use (`HopGapContract`, `InspectionPlanContract`, `NodeRoster`, `VerdictNames`).
- `ResonantNodeInternal.h:53-56` says "nothing outside includes it" and declares three external-linkage
  functions defined in App.cpp. `makeTonalPulseBehaviorProfile()` (`ResonantNodeApp.cpp:83`) is
  `return BehaviorGateConfig{};`.
- `RBBaselineState::Rebase` is set and overwritten two statements later (516-518 and Commands 115-117); no
  observer can ever see it.

The split lowered the biggest file's line count. The next cut should give each file its own state: a
`NodeParams` object owning the registry and the bindings, a `NetGateway` owning reply/join state, and the
baseline state machine as one struct with its own `update()`.

### A8. AnalyzerApp is one object across sixteen files (medium)

-> partly 2026-10-27 (11g C4): `SequenceTest` is now `RunConfig` (copied once at start; `PendingSequenceStart` holds one, parsed into a local copy) / `TrialState` (reset to defaults as a whole at each trial start, `TrialDiagnostics` inside) / `RunTotals` / `SampleDumpState`, bodies in `AnalyzerSequenceTypes.h`; one `SeqOutputConfig` rule (`changeSeqOutputConfig`); `mutable` 6 -> 1 (`_sequenceProfileScratch`). Still: classification decided three times and `behaviorEligible = valid` (C5). Was: `SequenceTest` 46 fields + 50-field `TrialDiagnostics` (`AnalyzerModeApp.h:141-323`, the 39 write-only ones went in 11e item 3b, eight more found now: D2.6), `PendingSequenceStart` repeats 25, `SeqOutputConfig` twice and every SEQ sub-command writes both (D1.4 shows them disagreeing), six `mutable` members, classification decided three times (`AnalyzerSequenceSession.cpp:693-714`, `AnalyzerModeApp.cpp:943-948`), `behaviorEligible = valid` (`AnalyzerModeApp.cpp:960`), 2026-10-26

- `SequenceTest` (`AnalyzerModeApp.h:160-420`): 46 top-level fields plus `TrialDiagnostics` with 66, plus
  full copies of `OccurrenceVerdict`, `InspectedOccurrence` and `DetectorReport`, plus `sampleHistory[128]`
  and `sampleRows[256]`. `PendingSequenceStart` (421-450) repeats 25 of the fields with different defaults.
  `SeqOutputConfig` lives twice (`_seqOutputConfig`, `_sequenceTest.outputConfig`) and every SEQ sub-command
  writes both.
- Write-only by grep (assigned, never read): `bestPendingDtFromTriggerMs, bestPendingOrigin,
  rawPendingCount, pendingPreWindowCount, pendingInWindowCount, pendingPostWindowCount, pendingOverflowCount,
  emitDurationMs, emitStartTrialId, emitDoneTrialId, emitDriveSeen, duplicatePatternPeakMs,
  duplicateDeltaFromPrimaryMs, duplicatePatternReleaseMs, acceptedPatternOnsetStrength,
  acceptedPatternReleaseStrength, acceptedPatternPeakMs, acceptedPatternReleaseMs, lastOnsetMs,
  lastRejectDurationMs, ampPeakMs, primaryValidPatternDtMs, primaryAcceptedOccurrenceDtMs`, the whole
  `pendingSamples[]` array, `diagMode`, the legacy tallies `hits/expectedHits/lateHits/misses/
  startupArtifacts` (superseded by `cleanSummary`). Never referenced outside the header:
  `trialTransientReject*CountAtStart`, `onsetReject*`, `firstOnsetRejectMs`, `lastOnsetRejectMs`,
  `peakActiveAtEnd`, `kPrintIntervalMs`. `printRuntimeSize` reports this struct's byte size as a memory
  concern.
- Six `mutable` members (`AnalyzerModeApp.h:569-603`) including `mutable detection::DetectionRuntime
  _detection`, so const printers can mutate; `effectiveSequenceProfile() const` rebuilds a profile into a
  mutable scratch on every call.
- Trial classification is decided three times: `finalizeSequenceTrial` (`AnalyzerSequenceSession.cpp:
  702-735`) with its own precedence ladder, then `classifySequenceTrial()` (`AnalyzerModeApp.cpp:975`), then
  overridden at 978-982 when the verdict is invalid. The host-tested classifier therefore does not determine
  what the board prints. `pattern.behaviorEligible = pattern.valid` (994) and
  `entry.behaviorEligible = verdict->valid` (`AnalyzerTrialCapture.cpp:169`) synthesize a Behavior fact the
  Analyzer does not own (spec 5.9).
- Any edit to the header recompiles 9 TUs.

Direction: `RunConfig` (copied once at start), `TrialState` (reset by `trial = TrialState{}`), `RunTotals`;
move the struct bodies out of the header; one classification site fed the verdict validity as input; drop
`behaviorEligible` from Analyzer output or rename it `verdictValid`.

-> done on the host 2026-10-27 (11g C4 / C5): RunConfig / TrialState / RunTotals, one output-config helper, mutables 6 -> 1
(C4); one classification site in AnalyzerTrialClassifier, host-tested, `behaviorEligible` gone (C5). Bench A/B after 11h.

### A9. The Node re-derives Behavior truth for printing (medium)

-> partly done in step 11f (Behavior::state(); the BehaviorStatus snapshot and setOutputsEnabled stay with A7), 2026-10-26

`behaviorGateName` (`ResonantNodeApp.cpp:236-250`) and `NodeDebug::observeBehaviorGate`
(`ResonantNodeDebug.cpp:121-123`, literal `state == 1`, `state == 3` against `stateCode()`) reconstruct the
gate reason from timers although `lastBlockReasonName()` exists and is printed twenty lines later.
`rbOutputsEnabled()` gates `_behavior.update` and verdict delivery in the Node, so "outputs off" is a blocking
reason the Node owns while spec 6.1 lists blocking reasons under Behavior. The Node calls 58 distinct
`ResonantBehavior` getters (grep), which is why `ResonantBehavior.h` is 285 lines, about 30 of them getters
for RB STATUS. Direction: `Behavior::setOutputsEnabled(bool)`; a `BehaviorStatus` snapshot struct Behavior
fills and the Node prints; delete `stateCode()`.

### A10. The Node writes into inspection-plan internals (medium)

-> open (`ResonantNodeParams.cpp:62-105`, the two Node floats `ResonantNodeApp.h:266-267`). Related new finding: `shape.minLevel` is a boot-time copy of the release threshold (D1.5), 2026-10-26

`ResonantNodeParams.cpp:147-184`: `moduleForTarget()` walks `_activeDetectionProfile.inspectionPlan.
modules[]` and `applyLiveDetectionParams` writes `magnitude.supportStrength.mediumPeakThreshold`, then
re-pushes the whole plan. Two Node floats (`_contrastMediumThreshold`, `_ampMediumThreshold`) exist only to
give the registry a stable address. Three detection-internal layers for one knob. Direction:
`DetectionRuntime::setInspectionMediumThreshold(InspectionTarget, float)`.

### A11. Board selection is repeated in every mode app (medium)

-> open: 18 `#if BOARD_PIEZO` blocks across the three mode apps and their self-tests; `PiezoToneOutput.cpp:16-26` and `PiezoToneOutputBTL.cpp:35-45` still byte-identical, 2026-10-26

18 `#if BOARD_PIEZO` blocks across `src/modes` (Emitter .cpp 5, .h 2; Node .h 3, .cpp 2, SelfTest 1;
Analyzer .cpp 1, .h 2, SelfTest 2). Each piezo build instantiates both `PiezoToneOutput` and
`PiezoToneOutputBTL` and picks one at runtime by `chirpBtlPin >= 0`, identically in the Node and the Emitter.
`PiezoToneOutput.cpp:16-26` and `PiezoToneOutputBTL.cpp:35-45` are byte-identical; the only difference is
`begin()` attaching an inverted pin. `EmitterApp.cpp` carries device knowledge ("LEDC switches at once",
Nyquist bound) that belongs to the ToneOutput. `BoardConfig.h` picks pins; the devices that follow from the
board are chosen three times. Direction: one `hal/BoardAudio.h` owning the board's mic port and ToneOutput
with the single conditional inside; one `PiezoToneOutput(pin, invertedPin = -1)`.

### A12. SoundOutput restores literals and carries a device knob; ChirpOutput keeps a superseded shape (medium)

-> done in step 11f (restores what it found, ramp left to the device, duplicate branch merged; Idle / Triple patterns and the consuming finished() kept), 2026-10-26

- `SoundOutput.cpp:164, 225, 241` each restore `setTiming(kDefaultChirpDurationMs, 150)` with the bare 150
  (also the member default in `ChirpOutput.h:61-69`). Any caller that configured the chirp differently (the
  Emitter does) is silently reset by an IdleCall.
- `SoundOutput.cpp:181` passes `kDefaultI2sToneRampMs` into `startPulseTrain`: the sequencing layer carries
  a device envelope knob; a piezo build receives a ramp it cannot use.
- `ChirpPattern::Idle` with `_idleFirstPulseToneHz = 2000` and the `_tripleChirp*` timings coexist with
  `OutputRequest::Kind::IdleCall`; `ResonantNodeApp.cpp` branches on both. `ChirpOutput.cpp:108-126` has two
  textually identical branches.
- `ChirpOutput::finished()` is a consuming getter; `SoundOutput::update()` relies on calling it exactly once.

### A13. Analyzer-side ownership leaks into shared headers (low)

-> partly done in step 11f (ScalarCandidateEvidence, RemoteCommandPolicy.h); DetectorReport.source* moves with A8, 2026-10-26

`DetectorReport.sourceSelection/sourceOccurrenceId/sourceCandidateId/sourceReportMatched` are never set by
the detector; only `AnalyzerModeApp.cpp:893-903` writes them. They are Analyzer bookkeeping riding in the
detector-truth struct. `ScalarInspectEvidence` (`DetectorReport.h`) is detector-owned but named "Inspect",
colliding with the Inspector stage. `remoteCommandAllowed` (`NetFrame.h:145-156`) bakes the Node's command
names into the wire-format header.

---

## Part B. Readability and maintainability

### B1. Five hand-rolled ring buffers

-> done in 11e item 4 (`src/app/RingQueue.h`), 2026-10-26

Three in `DetectionRuntime` (verdict queue, pending inspections, pipeline events: each with
readIndex/count/overflow and its own push and pop), `_emitTimes` in `ResonantBehavior`, the outbox in
`NodeNet`. One `template <class T, size_t N> struct RingQueue` removes about sixty lines and the index
arithmetic. `resetDetectionQueues` zeroes only slot 0 of each array, which is correct given count = 0 but
reads like a bug.

### B2. Reset proliferation with overlapping sets

-> partly: DetectionRuntime has six resets (was eight), `resetDiagnostics` gone; `_verdictQueueOverflowCount` still cleared in two (`DetectionRuntime.cpp:54, 58`), `ScalarTransientDetector::resetState` clears `_reportDetail` three times (`.cpp:54, 59, 64`); the Analyzer's two 50-line reset lists (`AnalyzerSequenceSession.cpp:168-219`, `444-478`) and the Node's four Behavior reset paths are as described, 2026-10-26

`DetectionRuntime` has eight: `resetState`, `resetDetectors`, `resetDetectionState`, `resetDetectionQueues`,
`resetDiagnostics`, `resetDiagnosticsCounters`, `resetDetectionBookkeeping`, `resetSourceRejectSummaries`.
`_verdictQueueOverflowCount` and `_detectorReport` are each cleared in two of them; `resetDiagnostics()` is a
pure alias; `setDetectorSelection` re-implements `resetDetectionBookkeeping` inline. `ScalarTransientDetector`
has six; `resetState` clears `_reportDetail = {}` and then `_reportDetail.accepted = {}` and
`_reportDetail.selectedReject = {}` again. The Analyzer keeps two 50-line reset lists for overlapping subsets
(`AnalyzerSequenceSession.cpp:171-232` with memset, 449-500 with `= {}`). The Node resets Behavior from four
paths, and `performRbRebase` resets it twice in a row. A maintainer cannot tell which reset is the right one
to call. Direction: state as structs reset with `= State{}` (the Node's `SelfTestState` already does this).

### B3. Config structs re-plumbed as 13 individual setters

-> done in 11e item 5 (`setConfig` / `config()`, `configure()` / `config()`), 2026-10-26

`applyScalarTransientConfig` (`DetectionRuntime.cpp:33-47`) mirrors 13 `ScalarTransientConfig` fields into
13 detector setters into 13 members with their own defaults. `ResonantBehavior::configure` does the same for
`BehaviorGateConfig` (14 setters, 15 getters). Holding the struct removes 26 setters, two default layers and
the drift in A6. The only setter with logic (`setIdleTimeMs` rescheduling the running wait) stays as a
method.

### B4. ScalarTransientDetector tracks the same event twice on two clocks

-> open. Two clocks still (`ScalarTransientOccurrence.cpp:72-75` from `_peakStartedUs`, `:194-200, 230` from `timeMs`; the two durations can differ by 1 ms), `refreshReportDetail` ~70 lines setting `rejectReason` twice (`ScalarTransientReport.cpp:26, 71`) and `buildReport` overriding (`140-150`), ~75 members, 2026-10-26

About ninety members. The candidate lifecycle (`_peak*`, `_candidate*`) and the "accepted occurrence pending"
tracker (`updateAcceptedOccurrencePending`, `_acceptedOccurrence*` scalars) both record start, peak and
strength of the same event: the first from `_peakStartedUs` via `timebase::msFromUs64`, the second from
`audioSamplePacket.timeMs`. So `Occurrence.startMs` and `DetectorReport.accepted.startMs` come from different
sources. Three parallel representations of the accepted event exist (`_acceptedOccurrence*` scalars,
`_acceptedOccurrence` summary, `_pendingOccurrence`, plus `_reportDetail.accepted`).
`refreshReportDetail` (`ScalarTransientReport.cpp:8-78`) is seventy lines of nested ternaries over three
sources that set `rejectReason`, `noEmitReason` and `gateReason` to one value (`rejectReason` assigned
twice), and `buildReport` then overrides them again. Direction: a `Candidate` struct with open / update /
close, one clock, and `ScalarInspectEvidence` reduced to the one reason field.

### B5. Giant functions

-> open: `handleUsbLine` 98-801 (SEQ START 443-756), `runRawTrigger` 103-770 with its paired free ladders, `buildSequenceAnalyzerReport` 825-1148, `printRbStatus` 53-220; indentation drift at `AnalyzerCommands.cpp:260-319`. `build="pass-c"` fixed in 11e item 2, 2026-10-26

```text
AnalyzerCommands.cpp      AnalyzerApp::handleUsbLine            735 lines (246-980); SEQ START alone ~300
AnalyzerRawCapture.cpp    runRawTrigger                          683 lines; six manual malloc/free pairs
AnalyzerModeApp.cpp       buildSequenceAnalyzerReport            321 lines; 14 nested ternaries
ResonantNodeStatus.cpp    printRbStatus                          195 lines; one ~1.1 KB output line
I2sToneOutput.cpp         renderBlock                             90 lines; three interleaved state machines
ScalarTransientReport.cpp refreshReportDetail                     70 lines of nested ternaries
```

Indentation drifts from 8 to 4 spaces inside `handleUsbLine` from about line 455; `OccurrenceInspector.cpp`
has case bodies at the `case` column. `report.context.build = "pass-c"` (`AnalyzerModeApp.cpp:876`) is a
frozen string from an earlier pass.

### B6. Command parsing

-> done in 11e item 8 (`serial_line::LineReader<N>`, `text_parse::*`; 11f named the three prefix kinds). Left: bare `strtoul` + clamp in `SelfTestReport.cpp:209-215` (D1.8), the band / hop parses (range- and end-checked) on purpose, 2026-10-26

- 44 hand-counted token offsets (`token + 6`, `+ 7`, `+ 12`, ...) in `AnalyzerCommands.cpp` 630-840 and
  360-378, `ResonantNodeCommands.cpp:171-227`, `ResonantNodeNet.cpp:123`, next to `tokenValue()`
  (`AnalyzerCommands.cpp:117-121`) whose own comment says "counting key lengths by hand got four wrong".
- 35 unvalidated `strtoul(..., nullptr, 10)`: the RAW branch (360-378) uses them while SEQ START 250 lines
  later uses the validating `parseUnsignedValue` for the same keys; `EmitterApp.cpp` has 19. `f=abc` becomes
  0, the bug issue #31 R2-4 fixed in `TextParse.h`.
- Seven serial line readers with three overflow policies and two byte-filter policies:
  `ResonantNodeCommands.cpp:27-51` (rejects long lines, issue #31), `AnalyzerCommands.cpp:222-244` (silently
  truncates and runs the cut line), `AnalyzerEmitterControl.cpp:67-115` and 26-56, `AnalyzerRawCapture.cpp:
  332-432`, `EmitterApp.cpp:120-161` twice. `app/SerialLine.h` offers only `dropByte`.
- `text_parse::parseFiniteFloat` is used only by the Analyzer PARAM path; the Node never includes
  `TextParse.h`.

Direction: `serial_line::LineReader<N>` with one overflow policy; `tokenValue()` and `parseUnsigned` in
`TextParse.h`; a `{key, parser, &field}` table for SEQ START.

### B7. Duplication across the three modes

-> partly done in 11e and 11f (helpers, names, freq fields, statics, emitter-control header); selftest ladders, I2S constructor, sample pump and emitter commands stay with A7 / A8, 2026-10-26

- `sampleOffsetUs` defined four times (`ResonantNodeApp.cpp`, `AnalyzerModeApp.cpp` with external linkage,
  `hal/AudioSourceI2S.cpp`, `audio/AudioSignal.cpp`).
- `equalsIgnoreCase` / `startsWithTokenIgnoreCase` inline in `ResonantNodeInternal.h` and out-of-line in
  `detection/analyzer/AnalyzerText.cpp`; the Emitter has a third, case-sensitive `startsWithToken`.
- The `freq.*` status block byte-identical in `ResonantNodeStatus.cpp:114-142` and
  `AnalyzerRuntimeReporter.cpp:69-99`.
- The I2S constructor block (six `kDefaultAudioI2S*` args) three times; `configureI2SParameters` (0.5 /
  0.005 / 20) identical in Node and Analyzer; the per-loop sample pump skeleton duplicated in `Node::update`
  and `AnalyzerApp::update`.
- SELFTEST: three `handleSelfTestCommand` with the same mic-then-amp / skip-on-mic_failed ladder
  (`ResonantNodeSelfTest.cpp`, `AnalyzerSelfTest.cpp`, `EmitterApp.cpp`); two restore paths reset different
  things with no shared definition of "restored".
- A `FeatureStreamId` name switch written twice in `ResonantNodeApp.cpp` although
  `detection::featureStreamName()` exists; `chirpPatternName` duplicates a switch in `NodeDebug`.
- A local `verdictTypeName` in `ResonantBehavior.cpp:46-60` that is missing two enumerators `VerdictNames.h`
  has (they print "unknown").
- Emitter command composition (`PULSES ... trial= freq= hop=`) duplicated in `AnalyzerSequenceSession.cpp`
  and `AnalyzerRawCapture.cpp`; `band=`/`hop=` validation duplicated between RAW and SEQ START; `EMIT CHIRP`
  and `EMIT PULSES` blocks identical.
- `waitForEmitterAck` forward-declared by hand in three TUs because `AnalyzerRuntimeReporter.h` and
  `AnalyzerSystemReporter.h` are comment-only stubs and there is no `AnalyzerEmitterControl.h`.
- Non-static globals: `kMaxSamplesPerLoop`, `printHeapStatus`, `printRuntimeSize`, `seqOutputModeName`,
  `analyzerProfileDetail*` in `AnalyzerModeApp.cpp`; `printProfileComposition`, `behaviorGateName`,
  `chirpPatternName` in `ResonantNodeApp.cpp`.

### B8. Time has no named type

-> partly: the two in-scope raw subtractions are now `timing::atOrAfter`; no `TimeMs` alias; hand-rolled compares remain in `ResonantNodeSelfTest.cpp:66-68, 405-406` and `AudioSelfTest.cpp:43-45, 85-91, 132, 157` (correct on the 32-bit target). New wrap-unsafe code arrived with BinTrack: D1.1, 2026-10-26

`unsigned long` appears 1,326 times in `src/` for milliseconds; the `timing::` helpers take `uint32_t`. On
the board both are 32 bits. On the host `unsigned long` is 64 bits, so the replay suite truncates at the
helper boundary but runs the raw subtractions at `ScalarTransientOccurrence.cpp:201`,
`ScalarTransientReport.cpp:66` and `ResonantBehavior.cpp:835` at full width; the wraptest envs exist only
for the board. `AudioSelfTest.cpp:43-45, 85-157` hand-rolls `static_cast<int32_t>(a - b) < 0` and defines a
local `inWindow` (half-open) that shadows `timing::inWindow` (closed). A `using TimeMs = uint32_t` in
`TimingUtils.h`, used in signatures, fixes both and makes the time axis greppable. (The time *handling* is
otherwise the best cross-cutting fix in the repo; see Part C.)

### B9. Comment drift in the files a maintainer reads first

-> done in 11e item 2 for every pointer listed. New drift found at 4612ce9: D3, 2026-10-26

- `DetectionProfile.h` 135 / 158: "default 20000" and "default 2500" while the macros say 30,500 and 1,500;
  `minTransientPeakStrength` assigned twice in `makeAmpExperimentalProfile`; the "New profile checklist" still
  lists "pattern rules".
- "First Difference" named as the default at `AudioSourceI2S.h:20-21`, `AudioSourceI2S.cpp:230-231`,
  `AudioSelfTest.h:25,33` (DC blocker since 2026-10-14). Tone level "0.3" at `RuntimeDefaults.h:147-149` and
  `I2sToneOutput.cpp:24` against 0.5 at `RuntimeDefaults.h:66-69`.
- `docs/refactors/micros-wrap.md` pointed at from `TimeBase.h:13`, `platformio.ini:96`,
  `test/test_timebase/test_main.cpp:2`; it lives in `archive/`. `ResonantNodeStatus.cpp:4` cites
  `step10-boundary-map.md`, also archived.
- `build.yml:62` "Warning only until ANA-003" above a line that already passes `--strict`;
  `include_direction.py` says the same.
- `ResonantNodeApp.cpp:642-643` "Transitional ... while the node still runs frequency-based profiles";
  `ResonantNodeApp.h:251-252` buffer sized for "the RB BEHAV usage line"; `.cpp:42` "Does NOT: own probe
  thresholds as diagnostics, not core behavior logic" is not a sentence; `:103` prints `RB DETECT`, a removed
  command.
- After the split: `ResonantNodeApp.cpp:869-871` stacks `// --- serial command handling ---` and
  `// --- command handlers ---` over `processDetectionFrame`; `:956` `// --- remote params over the
  radio ---` is the last line of the file; `:563` `// --- parameter setup ---` heads band/hop helpers.
- `AnalyzerModeApp.h:568` calls `AudioSignal` "Legacy"; `test/README` and `lib/README` are untouched
  PlatformIO boilerplate while the real env-to-test mapping exists only as comments in `platformio.ini`.
- Many code comments are dated changelogs ("92 since 2026-10-23 (was 85; ...)", "1,500 since 2026-10-16
  (was 2,500)"). The current rationale belongs in the comment; the history belongs in `docs/decisions/` and
  `git log`. Provenance citations (issue, decision, bench session) are a strength and should stay.

### B10. Dead code

-> done in 11e item 3 for every item listed except `FieldStateConfig::quietActivityThreshold` (never read, `FieldState.h:21`), `density` from the pattern count against an occurrence threshold (`FieldStateTracker.cpp:84-88`) and the 16 `FREQBAND_ENABLE_PROFILING` lines. New dead code found at 4612ce9: D2, 2026-10-26

- `AudioSignal`: `processBlock` (never called; every path uses per-sample `update()`, which then fakes a
  one-sample block into `_lastBlock*` that nothing reads), `DetectorOccurrenceDetail` (no users),
  `setCurveSampleCallback` / `CurveSnapshot` (never set), `processSample` `(void)`s three of its parameters
  and tests the same condition twice; 32 `IRAM_ATTR` on trivial accessors.
- `RuntimeDefaults.h:141-143` constexpr aliases `kDefaultAudioI2SReadBytes/DmaBufLen/DmaBufCount` have zero
  users; the macros are used directly. 14 `#ifndef/#define` pairs there plus 15 in `BoardConfig.h`, only four
  of which `platformio.ini` ever overrides. Debug knobs (`AUDIO_VERBOSE_DEBUG`, `TEST_LOOP_DELAY_MS`,
  `TEST_LOG_STRESS`, `TEST_SETUP_LABEL`, `FREQBAND_ENABLE_PROFILING`, 17 `#if` lines in
  `FreqBandStream.cpp`) are compile-time knobs in a repo whose rule is "every knob is a registered param".
- `AudioSourceI2S.cpp:39-47` 16-bit and 8-bit decode branches are unreachable (bits fixed at 32) and the
  16-bit branch would be a different scale. `_blockSamples` is a heap `unique_ptr` for a compile-time size
  with null checks in four places.
- `OccurrenceInspector::inspectImpl` has an unreachable final block; `reset()` is empty; `inspect()` (no
  history) has no callers. `FeatureHistory::copyWindowApproximateValues`, `debugFeatureBinSize` have no
  non-debug callers.
- Node: `_rbLastPendingMs/_rbHaveLastPendingMs` write-only; `RBBaselineState::Rebase` (A7). Emitter:
  `_outputPin`, `_outputBtlPin` stored and never read; `ControlSerialKind` unused. Analyzer: `_inputPin`
  written once; `sequenceOutputModeEnabled`; `kSequenceWarmupMs`; the `SequencePendingClass` enum.
- `FieldStateConfig::quietActivityThreshold` is never read; `FieldState.density` is computed from the
  pattern count against `denseOccurrenceCountThreshold` (an occurrence threshold) and read by one site.

### B11. Header and file hygiene

-> partly: BOMs, control bytes, double guards, the inspector's five names, the public constants fixed. Still: the 272-byte twins `MagnitudeWindow` / `MagnitudeInspectionObservation` copied field by field (`OccurrenceInspector.cpp:52-167`), the `scalar_freq_experimental` alias (`DetectionProfile.h:308`), the four host predicates, class `Node` in `ResonantNodeApp.h` and the `AnalyzerModeApp` alias, `printRbStatus(const char*)` discarding its argument, `rebaseResultName` decoding literals, 2026-10-26

- `#pragma once` plus include guards in `DetectionProfile.h`, `FeatureHistory.h`, `RuntimeDefaults.h`.
- UTF-8 BOM on `ResonantBehavior.h` and `MagnitudeWindow.h`; `test/test_net_frame/test_main.cpp` contains
  two raw control bytes so git shows it as binary and its diffs are unreviewable (the `'\x1F'` line-end
  marker should be an escape, not a literal byte).
- `FreqBandStream.h:116-121` reopens `public:` inside the private section for two constants.
- `AnalyzerModeApp.h` (620 lines) is a rebuild hub: about 430 lines are private struct bodies; a global-scope
  `DetectorInputSample`; `debugSequenceTestSize()` as public API to leak a private struct's size;
  `SequenceTrialSelection` declared mid-method-list.
- Class/file naming: class `Node` in `ResonantNodeApp.h`; class `AnalyzerApp` in `AnalyzerModeApp.h` with
  `using AnalyzerModeApp = AnalyzerApp` so `main.cpp` uses the alias; `EmitterApp` matches its file.
  `RBBaselineState` vs `RbRebaseResult` vs `RbLogMode` casing in one class. `RbRebaseResult` is cast to
  `uint8_t` and decoded with literal 1 / 2 in `rebaseResultName`. `printRbStatus(const char* line)` discards
  its argument and, as a "print", mutates `_rbLevelMaxSinceStatus`.
- Four "am I on the host" predicates: `ESP_PLATFORM` (`TimeBase.h`), `ARDUINO_ARCH_ESP32` (`NodeNet.cpp`),
  `HOST_REPLAY` (`SerialOut.h`), `UNIT_TEST` (`ReplayMain.cpp`), plus `build_src_filter`. One
  `app/Platform.h` macro would make the seam greppable.
- `OccurrenceInspector`: five names for one path (`inspect`, `inspectWithHistory`, `inspectImpl`,
  `inspectAcceptedOccurrence`, `inspectAcceptedOccurrenceResult`); `fillMagnitudeObservation` copies 45 fields
  from `MagnitudeWindow` into `MagnitudeInspectionObservation`, two near-identical 272-byte structs (the
  observation could embed the window). `DetectionProfile.h` is config data as 60 imperative assignments with
  trailing comments, plus name tables and a `strcasecmp` parser accepting the legacy alias
  `scalar_freq_experimental`, in one header.

### B12. Magic numbers repeated across files

-> done in step 11f except the line-buffer literals, 2026-10-26

`16000UL` sample-rate fallback seven times in six files although `runtime::kDefaultAudioI2SSampleRateHz`
exists; `7900` Nyquist bound six times although `OutputLimits::maxHz` exists; `1500` ms ack timeout four
times; `150` chirp gap three times (A12); PULSES bounds (50 / 1000 / 2000 / 50) in `EmitterApp.cpp` re-typed
as literals in the Analyzer's SEQ validation and its error string; line buffers 96 / 128 / 256 as bare
literals in about twenty places; `kMaxHop = 8` (`I2sToneOutput.h`) and `kMaxHopSteps = 8` (`SoundOutput.h`)
independent, with `setPulseHop` silently dropping to 0 hops when `n > kMaxHop`.

### B13. Behavior state machine details

-> open: `_outputBusy` assigned in ten places and always `_state == Chirping` (`ResonantBehavior.cpp:121 ... 444`), the decision / reason pair set fourteen times, the second Idle-to-HeardPattern transition (`:231-235`) still cannot fire, refractory on two fields (`:292` vs `:473`), `_lastFieldState` captured by one of the two overloads only, 2026-10-26

`State` has four values but eight orthogonal timers and flags sit beside it (`_restArmed`,
`_behaviorSuppressArmed`, `_ownEmitDetectionSuppressArmed`, `_testChirpPending`, `_networkCallPending`,
`_chirpRequested`, `_wouldEmit`, `_outputBusy`). `_outputBusy` is recomputed in six places and always equals
`_state == State::Chirping`. `_lastDecision` / `_lastBlockReason` are set as a pair in twelve places (one
`block(reason)` helper). The Idle-to-HeardPattern transition exists twice (`handleOccurrenceVerdict` and the
`update()` Idle case; the second cannot fire). Refractory stores both `_refractoryStartedMs` and
`_refractoryUntilMs` and the two readers use different ones. The two `handleOccurrenceVerdict` overloads
share `_lastFieldState` captured from the previous call, a hidden temporal coupling.

### B14. Hidden second scale in the audio path

-> open (`AudioPcm.h:71-79`, no contract line), 2026-10-26

`AudioPcm.h:71-79` `pcmToHistorySample` scales by `kCanonicalPcmMax` (8,388,607) while `Strength16` scales
by `kAmpStrengthGainReferencePcm` (262,143): the `RawSampleHistory` int16 values are about 32x smaller than
Strength16 for the same PCM. Spec 5.2 names Strength16, FrequencyScore16, contrast and the DC-blocker output
scale; it does not name this fourth domain and nothing in the header says "not comparable". One line of
contract in `AudioPcm.h` fixes it.

### B15. Blocking calls inside the Arduino loop

-> open: `waitForEmitterAck` + `delay(100)` in `startSequenceTest` (`AnalyzerSequenceSession.cpp:259-275`), `runRawTrigger` blocking and silently stopping the run (`AnalyzerRawCapture.cpp:110`), `I2sToneOutput::end()` busy-wait (`:40-52`), the SEQ start path still not resetting `_loopLastUs`, 2026-10-26

`AnalyzerSequenceSession.cpp:273, 288` (`waitForEmitterAck(..., 1500)` then `delay(100)`) inside
`startSequenceTest`, called from `update()`; `AnalyzerCommands.cpp:943, 953`; `runRawTrigger` blocks for the
whole capture and silently `stopSequenceTest()`s. Acceptable for a bench tool, but `AnalyzerApp::update`
measures its own loop health and these stalls land in it; `restoreAfterSelfTest` resets `_loopLastUs` for
exactly this reason while the SEQ start path does not. `I2sToneOutput::end()` busy-waits with magic 20 / 40 /
5 and `_task` is a `volatile` handle written by both the loop and the dying task. Three large diagnostic
methods carry `IRAM_ATTR` with no comment on why.

### B16. Test coverage

-> partly: new host suites `test_text_parse`, `test_param_commands`, `test_detection_diagnostics` (9), `test_shape_inspector` (10), the SoundOutput restore test, the Null-vs-diag replay diff in CI. Still untested: `ChirpOutput`'s loop-timed fallback (the fake tone always accepts), `I2sToneOutput::renderBlock`, `AudioSourceI2S::stampBlock`, `NodeNet` pacing and the `SeqFilter` across a reboot, `NvsParamStore`, `selftest::parseCommand`, `PcmPreprocessor` alone, `AnalyzerTrialClassifier` on the host. Two suites run nowhere in CI and one cannot link on its board env: D4, 2026-10-26

Covered on the host: ParamRegistry (9 tests incl. persistence), SoundOutput sequencing (7), NetFrame codec /
filter / joiner (6), TimeBase wrap math (and on device), the replay corpus (19) through the real chain,
behavior refractory and rest. Not covered anywhere: `ChirpOutput`'s loop-timed fallback (the test `FakeTone`
always accepts the device-timed train), `I2sToneOutput::renderBlock` hop / sweep / envelope math,
`AudioSourceI2S::stampBlock`, `NodeNet` outbox pacing (the file is compiled out on the host),
`NvsParamStore`, `selftest::parseCommand`, `text_parse::parseFiniteFloat`, `PcmPreprocessor` on its own. The
first three are pure arithmetic trapped inside FreeRTOS classes; extracting `SampleClock` and a `ToneSynth`
struct makes them testable in `native-test-behavior`. Tests include `.cpp` files directly because
`test_build_src = no`; workable, but each test re-declares its slice of the chain. `platformio.ini` lists the
six host test names twice (`test_ignore`, `test_filter`) with nothing checking they match.

---

## Part C. Done well, and to be kept

- Time handling: zero `millis()` / `micros()` calls in `src/` (three comment lines), one 64-bit clock, ms
  derived once per millisecond (`MsFromUs64`), the two wrap points pinned by `test_timebase` and
  reproducible on a bench in three minutes with the `*-wraptest` envs.
- One serial sink: no direct `Serial.print` outside `SerialOut.h` and `main.cpp`, 2,311 `serialOut()` call
  sites, measured rationale in the header; the radio reply tee needed no printer changes.
- `lib/ParamRegistry` is a real library: CI-enforced independence from `src/`, integer-exact range checks,
  NaN / sign / junk rejection, save / load with stale-default detection, nine host tests. The Node's PARAM
  path is declarative and complete (`applyParamModule` as the one dirty route; `RB PROFILE` reports exactly
  which live values a switch reset).
- `makeVerdict` as a stateless function, `FieldStateTracker`, `HopGapContract.h`, `InspectionPlanContract.h`,
  `NetFrame.h`: small, pure, host-tested, exactly the shape the spec asks for.
- Node to Behavior to SoundOutput data flow in `processDetectionFrame` respects the boundary: the Node pops
  `OccurrenceVerdict`, passes it with `fieldState()`, never reads `Occurrence` or `InspectedOccurrence`;
  `SoundOutput` owns sequencing and `OutputStatus`; `SoundOutput` validates before sounding and the test
  pins the request timings.
- `Node::SelfTestState` reset with `= SelfTestState{}`, the `BandRestore` RAII guard, the "parse into copies,
  activate only when every token is valid" idiom, the ESP-NOW receive callback that only copies, the
  `static_assert`s locking numeric contracts: the right patterns, waiting to be applied next door.
- `NodeDebug` is a genuine observer with no decisions. `ResonantNodeCommands.cpp` dispatch is flat,
  early-return, token-boundary matched, and refuses rebase during SELFTEST. `AnalyzerSeqReporter.cpp` and
  `AnalyzerSampleDump.cpp` are coherent single-purpose files; the clean SEQ_* output reads only
  `AnalyzerReport` as 5.9 requires.
- The host shim is deliberately minimal and says so; a new Arduino dependency in a detection file fails the
  host build.
- Comments cite issues, decisions and bench sessions as the norm. The 2026-10 splits moved code without
  behavior change and documented it honestly ("a file split, not separate classes").

---

## Part D. Findings at 4612ce9 (re-read 2026-10-26)

What steps 11b, 11e and 11f added or left, read with the same two questions. Severity in brackets. D1 is
correctness (a concrete input or state makes the code do the wrong thing); D2 architecture and readability; D3
the written word against the code; D4 tests and tools. Each was traced from a real caller, input or print
string before it was written down.

### D1. Correctness

1. **BinTrack compares absolute milliseconds and freezes at the 32-bit wrap; the shape gate then passes
   everything (high).** `src/detection/features/BinTrack.h:54` (`ms > _newestMs || _newestMs - ms >= kMs`) and
   `:78` (`if (nowMs <= _newestMs) return;`) are the plain comparisons `app/TimingUtils.h:5-8` forbids (issue
   #30). The runtime feeds it `audioSamplePacket.timeMs`, a `uint32_t`: 49.7 days after boot `advanceTo` never
   advances again, `at()` answers false for every ms, `annotateShape` sets `applicable = false`
   (`OccurrenceInspector.cpp:319-325`), `makeVerdict` skips the Shape requirement when it is not applicable
   (`VerdictProjection.cpp:65`), and plain chirps and steady tones pass the hop-train-only rule (decision
   2026-10-23) until a reset. The shape loop `for (ms = start; ms <= end; ++ms)` (`:319`) and the clamps
   `startMs > preMs ? startMs - preMs : 0` (`:208, 317`) are the same class. Reproducible in three minutes with
   the `-wraptest` envs (`TIMEBASE_OFFSET_US`). Fix: `timing::after` / `timing::delta` in BinTrack, the shape loop
   over a `uint32_t` count, and a `test_shape_inspector` case that notes across `0xFFFFFFFF`. Nothing in the
   current test set would have caught it: the two timebase suites do not touch the track (D4.2).
   -> done 2026-10-26 (before the 11c restart). Reproduced on the board first: E1 on a Node build with
   `TIMEBASE_OFFSET_US=4294787296000` (ms wrap 180 s after boot) answered hop trains as valid on both sides of the
   wrap and then accepted 5/5 plain 3450 Hz chirps, which it rejects 10/10 before the wrap. BinTrack on `uint32_t`
   with `timing::after`, the shape loop over a count from `startMs - preMs` (wrap-safe, no clamp to 0);
   `test_shape_holds_across_the_ms_wrap` (a train across 0xFFFFFFFF, a split occurrence after the wrap that looks
   back before it, a steady tone after it) fails on the old code ("Expected 5 Was 0") and passes. Left as it is:
   the magnitude window's clamp at `:208` (every plan's `windowPreMs` is 0, and the clamp only bites in the first
   `windowPreMs` ms after boot or at the wrap).

2. **`output.*` ranges exceed `OutputLimits`; a legal `PARAM SET` turns every answer into a chirp no node hears
   (medium).** `ResonantNodeParams.cpp:51-53` allows `hop_pulse_ms` 1-1000, `hop_gap_ms` 0-2000, `hop_steps`
   1-8; `OutputLimits.maxSpanMs = 6000` (`OutputRequest.h:69`), checked at `SoundOutput.cpp:119`. `PARAM SET
   output.hop_gap_ms 2000` with the shipped 5 steps of 25 ms is an 8,125 ms train: `SpanTooLong` on every
   emission, `startEmission` falls back to `Kind::Pattern` (`ResonantNodeApp.cpp:770-777`), a single chirp that
   since DET-015 no node accepts. `RB hop_check` validates the train against the detector, not against
   `OutputLimits`, so boot and the SET print `status=ok`. `OutputRequest.h:61` and myspec 7.2 say "a reject
   means a composition bug". Fix: bind the ranges so the worst train fits, or validate the composed request in
   `composeHopTrain` / `printHopGapCheck` and refuse the SET with a reason; drop the sentence from the header
   and the spec. Related: `startEmission` books a rejected request as an emission before it checks `started`
   (`:791-801`: Behavior counts it toward the own-emit budget, `_rbChirpStartedCount++`, the self-test opens a
   chirp window that never closes) and prints "RB emit rejected" twice.
   -> done 2026-10-26 (11g B2): `hop_pulse_ms` / `hop_gap_ms` 1-250 / 0-250, so 8 steps (3,750 ms) and the longest idle
   call (5,950 ms) fit 6,000; the sentence stays, now true. A start that fails books nothing (no budget, count or
   self-test window); the second line reads `fallback=none`.

3. **The radio sequence filter drops one real command per gateway reboot (medium).** `NodeNet::_nextSeq`
   starts at 1 on every boot (`NodeNet.h:80`, `.cpp:134-137`); receivers remember the last seq per sender MAC
   and drop an equal one (`NetFrame.h:173-181`). A gateway that sent N commands, is reset and sends again has
   its N-th command of the new life rejected by every node (`rxRejected++`), while it prints `NET sent` and
   gets no answer. Fix: seed `_nextSeq` from `esp_random()` (nonzero) at `begin()`, or a boot nonce in the frame.
   No test covers the filter across a reboot.
   -> done 2026-10-26: `begin()` seeds `_nextSeq` from `esp_random()` (nonzero). Correction from the test
   (`test_net_frame`, 11g A): the filter keeps only the last seq per sender, so the old start at 1 lost a command
   only when the previous life sent exactly one (its last seq 1 = the new first), not "the N-th" as written above.

4. **`SEQ start` during a run discards the run; `SEQ STOP` leaves a pending start armed; `SEQ STATUS` reads two
   copies of the config that disagree (medium-low).** `AnalyzerCommands.cpp:443-756` has no `_sequenceTest.active`
   check; the pending start runs `initializeSequenceTest`, which zeroes `cleanSummary`
   (`AnalyzerSequenceSession.cpp:218`) without printing the summary and never finalizes the trial in flight.
   `SEQ STOP` (`:436-441`) clears `active` but not `_pendingSequenceStart.active`, so a `start` + `stop` pair in
   one buffer still starts. `printSequenceStatus` prints mode / when / verbosity from `_sequenceTest.outputConfig`
   and tries / diag / freqband from `_seqOutputConfig` (`AnalyzerSeqReporter.cpp:576-592`); `SEQ MODE quiet`
   while active disables the runtime diagnostics but leaves `outputConfig.diagnosticsEnabled` true, from which a
   later `SEQ WHEN` re-enables them (`AnalyzerCommands.cpp:336-359`). Fix: refuse `SEQ start` while active (as
   SELFTEST does), clear the pending start in STOP, one `SeqOutputConfig` copied once at start (A8).
   -> partly 2026-10-27 (11g C1): start refused while active, STOP clears the pending start, STATUS reads one
   config, quiet clears the running diagnostics flag; the one `SeqOutputConfig` is C4.

5. **`shape.minLevel` is a boot-time copy of the release threshold (medium).** `DetectionProfile.h:176-178`
   copies `onsetReleaseThreshold` into `modules[2].shape.minLevel` and says so; `applyLiveDetectionParams`
   rewrites only the two medium limits (`ResonantNodeParams.cpp:91-105`). After `PARAM SET
   detection.scalar_release_threshold 3000` the detector holds a candidate above 3000 while the shape inspector
   counts bins at 1500 (and the reverse for a lower release): the two stages disagree on which milliseconds are
   the occurrence. Fix: derive the level from `_scalarTransientConfig.onsetReleaseThreshold` in
   `setInspectionPlan` / `annotateShape` instead of storing a copy.
   -> done 2026-10-26 (11g B2): `minLevel` left the plan; DetectionRuntime hands the inspector the release threshold
   on every config or plan change (`setShapeMinLevel`); `test_shape_level_follows_the_release_threshold`.

6. **The Amp inspector overwrites detector facts with window statistics from another domain (medium).**
   `OccurrenceInspector.cpp:251-258` writes `magnitude.value / strength = classificationValue` (the window P75),
   `magnitude.baseline = observation.mean` and `lift` into the occurrence copy. The detector had set
   `magnitude.baseline = audioSamplePacket.baseline`, the quiet floor in PCM units
   (`ScalarTransientOccurrence.cpp:205`). `makeVerdict` exports it as `primaryAmbientBaseline`
   (`VerdictProjection.cpp:52`), the Analyzer stores it as `acceptedAmbientBaseline` into a field it initialised
   from `_audioSignal.baseline()` and prints it as `ampBase`: one field, PCM or Strength16 depending on whether
   the plan has an Amp module. Spec 5.2 ("no second private scaling"), 5.6 (modules produce support facts). Fix:
   the inspector writes `strengthClass` only; the window mean stays on `InspectedOccurrence`. Changes one
   Analyzer field, nothing on the Node.
   -> done 2026-10-26 (11g B1, 1f7114d): `ampBase` is now always the detector's PCM floor. T2h byte-identical.

7. **On-device `test_shipped_profile` cannot link on `esp32dev-analyzer` since 11e item 7 (medium).**
   `test/test_shipped_profile/test_main.cpp:36-48` includes `DetectionRuntime.cpp` but not
   `DetectionDiagnostics.cpp`; the env its header names carries `-D ANALYZER_MODE`, which selects
   `DetectionDiagnostics`, whose constructor and hooks live in the omitted file. The 11e gate ran
   `test_analyzer_pass_rules` and `test_timebase` on a board, not this one. By inclusion analysis (no xtensa
   toolchain here). Fix: include `DetectionDiagnostics.cpp` (it is self-guarded).
   -> done 2026-10-26: included; run on the board on esp32dev-analyzer (see the 11c restart in roadmap-0-steps.md).

8. **SELFTEST values are parsed with bare `strtoul` and clamped (low).** `SelfTestReport.cpp:208-216`:
   `SELFTEST external wait_ms=abc` runs with 100 ms, `chirp n=abc` with one chirp, no error line; the one parser
   the step 11e policy (decision 2026-10-25) missed. Fix: `text_parse::parseUnsigned`, `Kind::Unknown` on failure.
   -> done 2026-10-27 (11g D): values through `text_parse::parseUnsigned`; a non-number makes the command Unknown
   (help printed); the range clamps stay.

9. **Small ones (low).** `_pendingInspectionOverflowCount` never reset and read only by a test
   (`DetectionRuntime.cpp:52-55, 175, 208`); `inspectWithHistory` iterates `_inspectionPlan.count` unclamped
   (`OccurrenceInspector.cpp:406`) while its three siblings clamp to `kMaxInspectionModules`;
   `checkInspectionPlan`'s shape bound omits `releaseDebounceMs + 1` (`InspectionPlanContract.h:55` vs `:65-66`,
   81 ms of slack today); `setDiagnosticsEnabled` and `SEQ DIAG on` promise an `EVT transient` line that
   `AUDIO_VERBOSE_DEBUG = 0` compiles out (`ScalarTransientReport.cpp:83`); `--dump-freq` prints a `uint32_t`
   with `%lu` (`Replay.cpp:538`); `HEAP_STATUS` is printed as two lines (`AnalyzerModeApp.cpp:101-106`); an
   unknown `RB` command gets no reply on the Node (`ResonantNodeCommands.cpp:30-144`, the Analyzer prints
   `EVT analyzer_unknown_cmd`); a radio reply frame refused by the outbox is dropped silently and can lose the
   `last` flag (`ResonantNodeNet.cpp:37-40`; not reachable with today's reply sizes).

### D2. Architecture and readability

1. **`Occurrence.band` is a dead 96-byte payload (medium).** `FrequencyBandOccurrenceDetail.present / score /
   contrast / measurement` (`Occurrence.h:51-59`) are assigned nowhere in src/ or test/; only its three
   `StrengthClass` fields are written (`OccurrenceInspector.cpp:261-267`). The embedded 72-byte measurement
   packet rides in every Occurrence (248 B), every InspectedOccurrence, both pending-inspection slots and all
   four pipeline events. The rule A5 was fixed under (spec 5.4) applies again. Fix: the three classes only;
   re-measure with the T5 probe.
   -> done 2026-10-26 (11g B1, 1f7114d): the three classes only. Host T5: Occurrence 200 -> 136, InspectedOccurrence
   864 -> 800, pipeline event 1,440 -> 1,376.

2. **`RawSampleHistory` costs 8 KB in every build and nothing reads it (medium).** `AudioSignal.h:46-123, 213`,
   `copyRawSampleHistory` / `rawSampleHistoryAvailable` / `AudioSamplePacket.rawHistoryReady` have no callers
   in src/ (two tests set the flag); the push runs on the per-sample IRAM path (`AudioSignal.cpp:72`).
   `AudioSignalStats.blocksProcessed` counts samples since the block path went and is still printed as blocks
   (`AnalyzerRuntimeReporter.cpp:213`). Fix: delete, or move behind the Analyzer's RAW tool.
   -> done 2026-10-26 (11g B1, 1f7114d): deleted with `rawHistoryReady` and the SIZE line; `blocksProcessed` gone
   (`frames_emitted` prints the sample count, OCCURRENCE summary drops `blocks=`). AudioSignal 8,168 -> 136 B (host).

3. **The host replay re-implements the shape rule and has drifted from it (medium).** `Replay.cpp:345-451`
   (`printShapeWindow`) counts runs and steps with `d == spacing || d < -3*spacing` (`:390`) while the inspector
   accepts up to `maxRisingStepBins` bins with `d % spacing == 0` (`OccurrenceInspector.cpp:356-359`); on the
   same capture `--shape` can report fewer rising steps than the verdict used, and `tools/bench/shapesurvey.py`
   builds E002's tables from it. A second `struct BinTrack` (`Replay.cpp:298`) shares the runtime class's name.
   Fix: one run / step function over a bin source, used by the inspector and printed by the replay from the
   verdict path.
   -> done 2026-10-26 (11g B2): `detection/inspection/BinShape.h` (`measureBinShape` over any source with BinTrack's
   `at()`); the replay's log is `ReplayBinLog`, `--shape` prints the inspector's window (with `preMs` / `tailMs`) and
   the class. shapesurvey after it: the split variants read 5 bins as the verdicts did; one noise verdict reads
   3 bins / 3 up (Medium) and is rejected by the other gates.

4. **The Node's `Boot` baseline state and the mic warmup never run (medium; ask before touching).**
   `Node::begin()` calls `_audioSignal.begin(false)` (`ResonantNodeApp.cpp:265`; `doRebase = false` skips the 2 s
   warmup) and sets `ListenForQuiet` directly (`:321`), so `RBBaselineState::Boot`, its "RB warmup waiting" case
   (`:453-463`), `kRbStartupMicWarmupMs` and the `== Boot` test in `ResonantNodeSelfTest.cpp:129` are dead, and
   the comment "boot warmup 2 s + quiet timeout 8 s + margin" (`:38`) describes a sequence that does not happen.
   Either the quiet gate replaced the warmup on purpose (delete the state, fix the comment) or the warmup was
   lost (run Boot first). The baseline timing is the owner's.
   -> decided 2026-10-26: Boot runs (decisions/2026-10-26-node-boot-warmup-runs.md); step 11g batch D.
   -> done on the host 2026-10-27 (11g D): the Node boots in Boot, waits the warmup with the loop running, rebases
   (`RB warmup done ... rebase= baseline=`), then ListenForQuiet; begin(false) stays (no blocking delay). Bench: boot timing.

5. **`RB debug off|events` is a no-op in every shipped build (medium).** `RB_VERBOSE_DEBUG` defaults to 0
   (`RuntimeDefaults.h:20-22`) and no env sets it, so every `NodeDebug::observe*` returns at its first line
   (`ResonantNodeDebug.cpp:104, 144, 194, 206`); `notes_manual.md:65` documents the command as event tracing.
   `_coreLoopUs*` / `_fullLoopUs*` and `_debugChirpVisibleUntilMs` are written and never read. Same shape as
   D1.9's `AUDIO_VERBOSE_DEBUG`: a compile-time knob behind a runtime switch, in a repo whose rule is "every knob
   is a registered param". Fix: one gate, the runtime one; delete the unread stats, or delete the command and
   the manual line.
   -> decided 2026-10-26: the command goes (decisions/2026-10-26-drop-rb-debug.md); step 11g batch D.
   -> done 2026-10-27 (11g D): NodeDebug is now NodeLed (the LED only); command, help, manual line,
   RB_VERBOSE_DEBUG and the unread loop stats removed.

6. **Write-only Analyzer fields that survived 11e item 3b (medium, with A8).** `TrialDiagnostics.duplicate*`
   (`AnalyzerModeApp.h:188-192`, written `AnalyzerTrialCapture.cpp:200-204`), `emitSeen / emitTrialId /
   emitDoneDtMs` (`:154-159`, written `AnalyzerEmitterControl.cpp:74-82`), `handleSequencePending`'s
   `liveFrequencyMeasurementPacket` and `dtFromTriggerMs` computed and `(void)`ed (`AnalyzerTrialCapture.cpp:
   80-94`), `windowStartOffsetMs` a constant 0 kept as a field. Also `_detectorSelection` written and never read
   (`DetectionRuntime.cpp:79`); `FrequencyBandMeasurementPacket.matched / confidence / targetHz / ageSamples`
   set by the builder and read nowhere; `FeatureStream` aliases and `FeatureHistory::record(const
   FeatureStream&)`, `hasSamples`, `latestValue` without callers; the commented-out `history.record(...)` lines
   naming removed enumerators (`FeatureExtractor.h:40-44`); `ScalarTransientDetector::begin()` called only by
   tests (so `_statsStartUs` is 0 on the board); the Emitter constructor with hard-coded pins
   (`EmitterApp.h:29-32`, no caller); `ResonantNodeCommands.cpp:102-105` skipping tabs `dropByte` never
   delivers.
   -> re-checked 2026-10-27 at HEAD: `bestPending*` and `duplicateFirst/LastDtMs` already gone; the current list,
   with what the bench tools read and which printed keys were constant over every committed run, is the "Output
   audit" section of `docs/refactors/archive/analyzer-run-trial-split.md` (11g batch C, items C2 / C3 / C5).
   -> done 2026-10-27 (11g C3): `duplicatePattern* / duplicateOriginWindow / duplicateReason`, `emitSeen /
   emitTrialId / emitDoneDtMs`, `handleSequencePending`'s packet parameter and both `dtFromTriggerMs`,
   `windowStartOffsetMs` (the header prints a literal 0), `_detectorSelection`, the packet's `matched / confidence /
   targetHz / ageSamples`, `FeatureStream` with its aliases, `record(const FeatureStream&)`, `hasSamples`,
   `latestValue` (with StreamBuffer's write-only `latestTimeMs / latestValue` and
   `representativeValueForAccumulator`), the commented `history.record` lines, the pin-default Emitter
   constructor, the tab skip; kept: `duplicateCount` (report `debug.duplicates`, `duplicateTrials`), the
   `latestTimeMs()` method (DetectionDiagnostics), `ScalarTransientDetector::begin()` (tests).

7. **Nine shape knobs are not registered params (medium; repo rule).** `ShapeInspectionConfig`
   (`InspectorTypes.h:146-156`: `preMs, tailMs, maxRisingStepBins, minLevel, minRunMs, mediumMin*, strongMin*`)
   has no `bind*` in `ParamBindings.h`; `DetectionProfile.h:169` calls `tailMs` "a tuning question for the
   board", which today means a reflash per value. Fix: `bindShapeParams` (`detection.shape_*`), applied through
   `setInspectionPlan` (threshold-only, history kept); D1.5 falls out of it.
   -> done 2026-10-26 (11g B2): `param::bindShapeParams`, eight `detection.shape_*` (Node and replay; the Analyzer's
   SEQ tuning does not bind them yet, with A8); counts widened to uint16; `kMaxParams` 48 -> 64 (47 used).

8. **Analyzer presentation lives in detection/ and the detector prints (medium-low).**
   `DetectorReportPrinter.cpp:12-149` and `ScalarTransientPrinter.cpp:9-94` render `SEQ_SOURCE_CORE` /
   `detail.scalar.*` through `serialOut()`, are compiled into every ESP env (`build_src_filter` excludes only
   `detection/analyzer/`) and have one caller (`AnalyzerSeqReporter.cpp:406`); `printTransientStatsIfDue`
   prints `EVT transient ...` from inside the detector. Spec 5.3 and 5.9. Fix: move both printers to
   `modes/analyzer/` (they read only `DetectorReport`), the EVT line to the Analyzer's reporter.
   -> done 2026-10-27 (11g C2): printers in `modes/analyzer/report/`; the EVT line deleted (compiled out by
   AUDIO_VERBOSE_DEBUG, D1.9) with the detector's diagnostics flag.

9. **Behavior mirrors SoundOutput's busy state (low).** `_outputBusy` (B13) duplicates `output.phase`; RB STATUS
   prints both (`ResonantNodeStatus.cpp:77-78, 84-85`). Spec 7.2 says Behavior reads `OutputStatus`; the code has
   the Node forwarding `spanMs` and `finishedAtMs` (`ResonantNodeApp.cpp:791, 815-817`), which is fine, but the
   sentence and `outputBusy()` should agree. Fix: `outputBusy()` returns `_state == Chirping`; one sentence in 7.2.
   -> done 2026-10-27 (11g D): `outputBusy()` is `_state == Chirping`, the member and its 11 writes gone; spec 7.2
   says how Behavior learns OutputStatus (span, finish time through the Node).

10. **Three statements of the line bound (low).** `ParamCommands.cpp:12` `kMaxLineLength = 255`, the Node's
    `kSerialLineCapacity - 1`, the Analyzer's `LineReader<255>` + `_commandScratch[256]`, `net::kMaxText` 233
    checked separately, five `char buffer[96]` in `EmitterApp.cpp` against `LineReader<95>`. Fix: one
    `serial_line::kCommandLineMax` the others derive from.
   -> done 2026-10-27 (11g D): `serial_line::kCommandLineMax` (255) and `kEmitterLineMax` (95) in SerialLine.h;
   ParamCommands, the Node, the Analyzer, the Emitter and the Analyzer's Emitter readers derive from them.

11. **Smaller (low).** `PendingSequenceStart.setupLabel` points into the struct's own storage
    (`AnalyzerModeApp.h:343, 354`; works only because it is passed by reference); 42 `IRAM_ATTR` in
    `FreqBandStream.cpp`, about 25 on trivial getters and setters (B10 fixed this in AudioSignal only);
    `annotateShape` carries ~1.2 KB of run arrays on the detection loop's stack (`OccurrenceInspector.cpp:
    300-384`), the resource step 8b freed; `FrequencyBandMeasurementPacket` defined in `inspection/InspectorTypes.h`
    though the inspector never reads it; `setInspectionPlan` wires `setBinTrack` as a side effect
    (`DetectionRuntime.cpp:95`); `inspectionPlanName` returns "custom" for the shipped plan, which RB PROFILE and
    SEQ print; `ScalarTransientDetector` and `FreqBandStream` are the two detection classes outside `namespace
    detection`; the 11e / 11f fixes left "was defined three times (review B7, step 11f)" history in nine headers
    (B9's own rule); unused includes in `ResonantNodeApp.cpp:12-13` and `ResonantNodeStatus.cpp:12, 15, 17`; the
    Emitter's post-selftest restore differs from the Node's and the Analyzer's (three definitions of "restored").

### D3. Spec, guide, roadmap against the code

1. **The spec declares the live diagnostics file dead (high).** `myspec.md:743`: "`DetectionDiagnostics` is
   historical archive material, not active source code" while `src/detection/DetectionDiagnostics.h` is the
   Analyzer's side channel since 11e item 7, selected by `-D ANALYZER_MODE`, host-tested, with the Null-vs-diag
   replay diff in CI. Fix: rewrite the 5.5 line as the collaborator contract: records, never changes a verdict,
   hook order, the CI diff that enforces it.
   -> done 2026-10-26 (step 11g A): myspec 5.5 states the side channel's contract (Null vs Analyzer variant,
   hook order, records only, the native-replay vs native-replay-diag diff).

2. **Names in the spec that the code does not have (medium).** `RejectedCandidateSummary` eight times
   (`:505, 537, 709`, present tense) for `SelectedRejectSummary`; the evidence targets `SupportStrength /
   FrequencyScoreStrength / FrequencyContrastQuality / TargetBandStrength` (`:773-777`) for the enum `Amp /
   TargetScore / Contrast / TargetBand / Shape`, which the SEQ lines print; `detail.frequency.*` (`:728-730`),
   `PatternCandidate` (`:1099`), "pattern rule config" (`:309`); 5.3's "Adding a detector family" describes
   `DetectionFamily.h`, `ActiveDetector`, `DETECTOR_FAMILY_*` in the present tense though none exist (step 9's
   plan inside the spec); `docs/myspec.md` (`:1282`) is the wrong path. Fix: the code's names, or a mapping stated
   once; the family block marked as agreed future terms.
   -> done 2026-10-26 (step 11g A): SelectedRejectSummary throughout (old name mapped once in 5.5), the
   InspectionTarget names with their SEQ spellings, detail.frequency, PatternCandidate and "pattern rule config"
   gone, the family block marked as agreed terms not yet built (step 9), the path fixed.

3. **Guide and help against the parsers (medium).** `notes_manual.md:49-51` says 95 characters "on the Emitter
   link"; the Emitter's USB reader is also `LineReader<95>` (`EmitterApp.h:98-99`). Not in the guide: SELFTEST
   `dur_ms=` and the Analyzer's `SELFTEST external`, RAW `decim=` / `sweep=` / `band=`, SEQ start `band=` (also
   missing from `SEQ help`), the Emitter's `EMIT_START` / `EMIT_DRIVE_ON` markers. `RB help` omits PARAM SAVE |
   LOAD | RESET. Spec 5.9 lists SEQ_EXPLAIN and "SEQ STATUS"; `SEQ help` lists `SEQ_DETAIL`, `SEQ_SOURCE_CORE`,
   `SEQ_SOURCE_SPEC`, not SEQ_EXPLAIN, and the status line is `SEQ_STATUS`; `benchlib.py` still carries
   `SEQ_DETAIL`. RAW: the spec names two CSV layouts, the firmware prints four (`AnalyzerRawCapture.cpp:674, 686,
   694, 708`), and the pcm mode's `RAW_BEGIN fields=ms,pcm,baseline` (`:663`) disagrees with its four-column
   header (`:674`); `rawcorpus.py` reads the header, so harmless today. The 11e guide text says the Analyzer's
   PARAM has no store: true, but `PARAM LIST|GET|SET|DUMP|RESET` in the summary block omits SAVE | LOAD, which
   the handler answers with `status=no_store`.
   -> partly 2026-10-26 (step 11g A): guide has the line bounds per port, SELFTEST dur_ms= / external, RAW
   decim= / sweep= / band= and its four layouts, SEQ start band=, the four Emitter markers, the Analyzer's SAVE|LOAD;
   help strings fixed (SEQ help: band=, SEQ_EXPLAIN for SEQ_DETAIL, SEQ_STATUS; HELP: RAW keys, SELFTEST external,
   no_store; RB help: SAVE|LOAD|RESET); spec: SEQ_STATUS, all four RAW layouts acknowledged. Left: the pcm
   `RAW_BEGIN fields=` value (an output line, code batch) and `benchlib.py`'s SEQ_DETAIL (tools/).

4. **`implementation-status.md` is history, not a live table (high for the rows named).** Params row: "a
   registry on the Analyzer: not implemented", "38 Node params" (PAR-003 is DONE; the Node registers 39, 17
   `detection.` + 13 `behavior.` + 8 `output.` + 1 `node.`). Tests / CI row: two on-device suites (misses
   `test_shipped_profile`), two host suites "15 tests" (there are eleven suites in three native envs plus the
   replay diff), "include_direction ... warning only until ANA-003" (strict since step 10). Detection rows:
   "two-inspector path" (three modules incl. Shape since a33f293), "Future TargetBandStrength", "Node build,
   sweep and the final gate run pending" (11b closed it), a `FrequencyMatchDetector` fix (removed code), a dated
   history entry at `:21` in a table whose README says "never dated history".
   -> done 2026-10-26 (step 11g A): table rewritten as current state, no dated history; params (39 on the Node,
   the Analyzer's registry), tests / CI (three on-device suites, three host envs, diag diff, strict include
   check), inspection (two module kinds, shape required), signal variation, removed Freq rows regenerated.

5. **Roadmaps and decisions (medium).** `roadmap-0-steps.md:1094` `11c. [NEXT]` while `:1098` and HEAD say
   running (the legend has no running tag); `:1172-1173` lists PAR-003 under "still open, not sequenced";
   `:1154` names PAR-013 and OUT-002/003 (done). Every domain roadmap's "Current code state" block predates
   steps 7d-11e: `roadmap-param-config.md:138-148` (`[PARTIAL] RB PARAM / RB BEHAV`, "in src/param/",
   `[DEFERRED] PARAM SAVE/LOAD/RESET`, four FrequencyMatch thresholds), `roadmap-node.md:35-43`,
   `roadmap-detection.md:25, 32, 459` ("OccurrenceEvaluator decides", "[REMOVED] DetectionDiagnostics"),
   `roadmap-behavior.md:36, 43`, `roadmap-output.md:34-36` ("OutputStatus not landed"); their focus lines end at
   step 10. `decisions/README.md` open table: the Node profile (`:97`), FrequencyMatch keep / consolidate (`:98`)
   and persistent config (`:102`) are decided; rows `:80-88` out of date order; two "pending" annotations
   (`:69, :71`) unverified against 5g / 6 / 11b. `research/README.md` omits `scalar-detector-extensions.md` and
   the `Became:` marks of `frequency-drift-detection.md` and `vektor-lessons-from-11a.md`; `soak-24h.md` has no
   link to 11c / `soakcycle.py`. Stale pointers to pre-archive paths: `platformio.ini:41`
   (`cleanup-analyzer-node-isolation.md`), `tools/bench/selftest.py:5` (`selftest.md`); `test/README:4-6` lists
   six host suites of eleven; `DetectionProfile.h:20-28` lists enums without `BinShape`, `AmpMagnitude`, `Shape`;
   `DetectionCoreTypes.h:48-50` and `Occurrence.h:19-29` describe earlier passes.
   -> partly 2026-10-26 (step 11g A): steps file (11c RUNNING with the legend extended, PAR-003 out of "still
   open", OUT-002/003 and PAR-013 marked done in step 11), the five domain roadmaps' code-state blocks and focus
   lines, decisions/README (dates sorted, three resolved open rows removed, three "pending" notes checked against
   5b / 5g / 5f), research/README and soak-24h.md. Left: the pointers in platformio.ini, tools/bench/selftest.py,
   test/README and the src/ header comments (files outside this batch's docs scope).

### D4. Tests and tools

1. **`shapesurvey.py` silently drops every shape line (high for E002's tables).** `tools/bench/shapesurvey.py:36`
   expects `other_steps=(\d+) seq=` while `Replay.cpp:434-444` prints `other_steps=N dips_pct=... edips_pct=...
   seq=...` since 466495a, the commit that added both; `re.match` returns None, the survey reports zero verdicts
   per category with no error. Both files were created in that one commit and neither changed since, so the
   survey never worked as committed; E002's text discusses the dips, so its tables came from an uncommitted
   intermediate state of the tool. The committed tool cannot reproduce them. Fix: parse `k=v` pairs; one
   fixture-driven test of the parser.
   -> done 2026-10-26 (parser): `shape_fields()` reads `k=v` pairs and stops on a shape line missing a field
   instead of dropping it. No parser test yet.

2. **Two suites run nowhere in CI (medium).** `test_timebase` (the wrap math of issue #30) and
   `test_analyzer_pass_rules` are in no native filter (`platformio.ini:150-158, 166, 183`) and `build.yml` has no
   device job; both build the slice `test_shipped_profile` already builds on the host. Fix: add both to
   `native-test-behavior`. With D1.7, three of the four on-device suites are either unrunnable or unrun.
   -> done 2026-10-26: both in `native-test-behavior` (8 / 9 tests pass on the host); each got the
   `runTests()` / `main()` split `test_shipped_profile` has, `setup()` on the board as before.

3. **No host test for (medium, list).** `src/hal/` (all), `NodeNet.cpp` (pacing, outbox, the filter across a
   reboot), the three command dispatchers and every status / report printer the bench tools parse,
   `src/selftest/`, `NvsParamStore.h`, `PcmPreprocessor.h` alone, `AnalyzerTrialClassifier.cpp` on the host,
   `*Printer.cpp`, `FieldStateTracker.cpp` directly, BinTrack at the wrap (D1.1). The `test_ignore` /
   `test_filter` twin lists in `platformio.ini` agree today and nothing checks that they do.
   -> partly 2026-10-26 (11g A, 64535d3): the radio filter across a reboot (`test_net_frame`), the outbox queue
   (`test_ring_queue`), the SEQ_SOURCE_CORE / SPEC lines seqcmp parses (`test_report_printers`), BinTrack at the wrap
   (5fcd3fc), shapesurvey's parser (`tools/bench/test_shapesurvey.py`, in CI). Open: NodeNet pacing (the file is
   ESP-only; the reply spacing would need a pure helper), the mode apps' printers (RB STATUS, NET reply, SEQ_TRIAL /
   SUMMARY, RAW_*), src/hal, selftest, NVS store, the dispatchers, the twin-list check.

4. **Tool parsers that do match today (verified, for the record).** `netcheck.py` (`NET reply`, `PARAM path=`,
   the `RB STATUS ... board=` tail), `nodegroup.py` (`BEH emit start`, `RB emit shape=hop`, `RB verdict=`, `RB
   rest start`; all behind `RB log full`), `soakcycle.py` (`SEQ_SUMMARY`, `SEQ_TRIAL reject_reason=`;
   `SEQ_EXPLAIN failed_label=` matches inside `verdict.first_failed_label=`, brittle), `rawcorpus.py`,
   `notchsweep.py` (now `--set detection.contrast_medium=`). `build.yml:1-3` says the include direction "is
   reported" where it is enforced.

### Suggested order for Part D

1. D1.1 with its wrap test and D1.7 before the next long run or bench day; D1.3 (one line) with them.
2. D1.2 (ranges or validation, and the spec sentence), D1.5 and D2.7 together as "shape params"; D3.1 as a
   one-line spec fix in the same commit as whichever lands first.
3. D2.1 and D2.2 as one byte-identical-replay pass with the T5 probe; D1.6 in the same pass (it changes one
   Analyzer field; record it).
4. D4.1 and D4.2 (tools and CI), D3.4 and D3.5 (one documentation pass regenerating the status table and the
   "Current code state" blocks from the code).
5. D2.4 and D2.5 after the owner says what Boot and RB debug are for; everything else with A7 / A8.

## Suggested order, if any of this is taken up

Cheap and safe first, each a compile-only change with `T2h` byte-identical replay as the gate:

1. B9 comment drift and the three stale doc pointers; B11 BOM / control bytes / double guards.
2. B10 dead code and A4's dead enumerators (fold into DET-018), A8's write-only Analyzer fields.
3. B1 `RingQueue`, B3 hold the config structs (drops two default layers of A6).
4. A5 drop `Occurrence.magnitude.evidence` (one Analyzer reader to move); re-measure with T5.
5. A3 `DetectionDiagnostics` collaborator.
6. B6 `LineReader` + `tokenValue` everywhere; A6 `describeParams` shared by Node, Analyzer, replay.
7. The spec question in A1 / A2, decided in `myspec.md` before anyone refactors toward either answer.
8. A7's next cut of the Node and A8's `TrialState` split, each with its own pass doc.

Status 2026-10-26: 1-6 done (step 11e, one commit per item), 7 settled in the spec (11e item 1), 8 open (A7,
A8). Part D has its own order above; its first item landed in 5fcd3fc, the rest is step 11g (four batches).

Process notes: `docs/refactors/` has no active top-level doc while NODE-004 is open (CLAUDE.md says to flag
this); since 2026-10-22 the step text in `roadmap-0-steps.md` carries the plan, so this is the convention, not
a gap. A1's spec / decision conflict was settled in writing in 11e item 1 before the code moved.
