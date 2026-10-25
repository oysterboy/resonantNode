# Architecture critique and readability review, 2026-10-25

Status:  exploring (2026-10-25)
Lab:     none; a code reading at main c0b75e9 (after 5ffab4a "Node: split by role"), host `sizeof` probe

A review of the whole tree for two questions: does the code match the boundaries `docs/specs/myspec.md`
states, and what does a human maintainer trip over when reading or changing it. Line numbers are as of
c0b75e9 and will drift; the file and function names are the durable pointers. Everything below was checked
against the source, dead-code claims by grep. Nothing here is a commitment; items that get picked up should
be promoted to roadmap items or decisions per `docs/research/README.md`.

Calibration first: the repo is in better shape than its biggest files suggest. The things that are done well
are listed at the end and should not be undone while fixing the rest.

Known already-open items this overlaps with: DET-018 (vestigial verdict vocabulary), DET-016 (verdict
boundary rule), NODE-004 (Node file split, "a file split, not separate classes" in roadmap-node.md), step 9
(detector family build flag). Where a finding is covered by one of them it says so.

---

## Part A. Architecture: where the code and the spec disagree

### A1. Output composition sits in the Node; spec 6.2 assigns it to Behavior (high)

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

`src/behavior/ResonantBehavior.h:7` includes `../output/ChirpOutput.h` to get `ChirpOutput::ChirpPattern`
(used at .h 157/266 and throughout the .cpp). `OutputRequest` (`src/output/SoundOutput.h:32`) carries the
same device enum. Spec 7.1: "Behavior should not care which output hardware path is active." The request
vocabulary (Single / Idle / Triple, or better `OutputRequest::Kind`) belongs in a device-free header, e.g.
`output/OutputRequest.h`, and `ChirpOutput` consumes it.

### A3. The Analyzer's diagnostics build is woven through DetectionRuntime with the preprocessor (high)

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

### A9. The Node re-derives Behavior truth for printing (medium)

`behaviorGateName` (`ResonantNodeApp.cpp:236-250`) and `NodeDebug::observeBehaviorGate`
(`ResonantNodeDebug.cpp:121-123`, literal `state == 1`, `state == 3` against `stateCode()`) reconstruct the
gate reason from timers although `lastBlockReasonName()` exists and is printed twenty lines later.
`rbOutputsEnabled()` gates `_behavior.update` and verdict delivery in the Node, so "outputs off" is a blocking
reason the Node owns while spec 6.1 lists blocking reasons under Behavior. The Node calls 58 distinct
`ResonantBehavior` getters (grep), which is why `ResonantBehavior.h` is 285 lines, about 30 of them getters
for RB STATUS. Direction: `Behavior::setOutputsEnabled(bool)`; a `BehaviorStatus` snapshot struct Behavior
fills and the Node prints; delete `stateCode()`.

### A10. The Node writes into inspection-plan internals (medium)

`ResonantNodeParams.cpp:147-184`: `moduleForTarget()` walks `_activeDetectionProfile.inspectionPlan.
modules[]` and `applyLiveDetectionParams` writes `magnitude.supportStrength.mediumPeakThreshold`, then
re-pushes the whole plan. Two Node floats (`_contrastMediumThreshold`, `_ampMediumThreshold`) exist only to
give the registry a stable address. Three detection-internal layers for one knob. Direction:
`DetectionRuntime::setInspectionMediumThreshold(InspectionTarget, float)`.

### A11. Board selection is repeated in every mode app (medium)

18 `#if BOARD_PIEZO` blocks across `src/modes` (Emitter .cpp 5, .h 2; Node .h 3, .cpp 2, SelfTest 1;
Analyzer .cpp 1, .h 2, SelfTest 2). Each piezo build instantiates both `PiezoToneOutput` and
`PiezoToneOutputBTL` and picks one at runtime by `chirpBtlPin >= 0`, identically in the Node and the Emitter.
`PiezoToneOutput.cpp:16-26` and `PiezoToneOutputBTL.cpp:35-45` are byte-identical; the only difference is
`begin()` attaching an inverted pin. `EmitterApp.cpp` carries device knowledge ("LEDC switches at once",
Nyquist bound) that belongs to the ToneOutput. `BoardConfig.h` picks pins; the devices that follow from the
board are chosen three times. Direction: one `hal/BoardAudio.h` owning the board's mic port and ToneOutput
with the single conditional inside; one `PiezoToneOutput(pin, invertedPin = -1)`.

### A12. SoundOutput restores literals and carries a device knob; ChirpOutput keeps a superseded shape (medium)

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

`DetectorReport.sourceSelection/sourceOccurrenceId/sourceCandidateId/sourceReportMatched` are never set by
the detector; only `AnalyzerModeApp.cpp:893-903` writes them. They are Analyzer bookkeeping riding in the
detector-truth struct. `ScalarInspectEvidence` (`DetectorReport.h`) is detector-owned but named "Inspect",
colliding with the Inspector stage. `remoteCommandAllowed` (`NetFrame.h:145-156`) bakes the Node's command
names into the wire-format header.

---

## Part B. Readability and maintainability

### B1. Five hand-rolled ring buffers

Three in `DetectionRuntime` (verdict queue, pending inspections, pipeline events: each with
readIndex/count/overflow and its own push and pop), `_emitTimes` in `ResonantBehavior`, the outbox in
`NodeNet`. One `template <class T, size_t N> struct RingQueue` removes about sixty lines and the index
arithmetic. `resetDetectionQueues` zeroes only slot 0 of each array, which is correct given count = 0 but
reads like a bug.

### B2. Reset proliferation with overlapping sets

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

`applyScalarTransientConfig` (`DetectionRuntime.cpp:33-47`) mirrors 13 `ScalarTransientConfig` fields into
13 detector setters into 13 members with their own defaults. `ResonantBehavior::configure` does the same for
`BehaviorGateConfig` (14 setters, 15 getters). Holding the struct removes 26 setters, two default layers and
the drift in A6. The only setter with logic (`setIdleTimeMs` rescheduling the running wait) stays as a
method.

### B4. ScalarTransientDetector tracks the same event twice on two clocks

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

`unsigned long` appears 1,326 times in `src/` for milliseconds; the `timing::` helpers take `uint32_t`. On
the board both are 32 bits. On the host `unsigned long` is 64 bits, so the replay suite truncates at the
helper boundary but runs the raw subtractions at `ScalarTransientOccurrence.cpp:201`,
`ScalarTransientReport.cpp:66` and `ResonantBehavior.cpp:835` at full width; the wraptest envs exist only
for the board. `AudioSelfTest.cpp:43-45, 85-157` hand-rolls `static_cast<int32_t>(a - b) < 0` and defines a
local `inWindow` (half-open) that shadows `timing::inWindow` (closed). A `using TimeMs = uint32_t` in
`TimingUtils.h`, used in signatures, fixes both and makes the time axis greppable. (The time *handling* is
otherwise the best cross-cutting fix in the repo; see Part C.)

### B9. Comment drift in the files a maintainer reads first

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

`16000UL` sample-rate fallback seven times in six files although `runtime::kDefaultAudioI2SSampleRateHz`
exists; `7900` Nyquist bound six times although `OutputLimits::maxHz` exists; `1500` ms ack timeout four
times; `150` chirp gap three times (A12); PULSES bounds (50 / 1000 / 2000 / 50) in `EmitterApp.cpp` re-typed
as literals in the Analyzer's SEQ validation and its error string; line buffers 96 / 128 / 256 as bare
literals in about twenty places; `kMaxHop = 8` (`I2sToneOutput.h`) and `kMaxHopSteps = 8` (`SoundOutput.h`)
independent, with `setPulseHop` silently dropping to 0 hops when `n > kMaxHop`.

### B13. Behavior state machine details

`State` has four values but eight orthogonal timers and flags sit beside it (`_restArmed`,
`_behaviorSuppressArmed`, `_ownEmitDetectionSuppressArmed`, `_testChirpPending`, `_networkCallPending`,
`_chirpRequested`, `_wouldEmit`, `_outputBusy`). `_outputBusy` is recomputed in six places and always equals
`_state == State::Chirping`. `_lastDecision` / `_lastBlockReason` are set as a pair in twelve places (one
`block(reason)` helper). The Idle-to-HeardPattern transition exists twice (`handleOccurrenceVerdict` and the
`update()` Idle case; the second cannot fire). Refractory stores both `_refractoryStartedMs` and
`_refractoryUntilMs` and the two readers use different ones. The two `handleOccurrenceVerdict` overloads
share `_lastFieldState` captured from the previous call, a hidden temporal coupling.

### B14. Hidden second scale in the audio path

`AudioPcm.h:71-79` `pcmToHistorySample` scales by `kCanonicalPcmMax` (8,388,607) while `Strength16` scales
by `kAmpStrengthGainReferencePcm` (262,143): the `RawSampleHistory` int16 values are about 32x smaller than
Strength16 for the same PCM. Spec 5.2 names Strength16, FrequencyScore16, contrast and the DC-blocker output
scale; it does not name this fourth domain and nothing in the header says "not comparable". One line of
contract in `AudioPcm.h` fixes it.

### B15. Blocking calls inside the Arduino loop

`AnalyzerSequenceSession.cpp:273, 288` (`waitForEmitterAck(..., 1500)` then `delay(100)`) inside
`startSequenceTest`, called from `update()`; `AnalyzerCommands.cpp:943, 953`; `runRawTrigger` blocks for the
whole capture and silently `stopSequenceTest()`s. Acceptable for a bench tool, but `AnalyzerApp::update`
measures its own loop health and these stalls land in it; `restoreAfterSelfTest` resets `_loopLastUs` for
exactly this reason while the SEQ start path does not. `I2sToneOutput::end()` busy-waits with magic 20 / 40 /
5 and `_task` is a `volatile` handle written by both the loop and the dying task. Three large diagnostic
methods carry `IRAM_ATTR` with no comment on why.

### B16. Test coverage

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

Process notes: `docs/refactors/` has no active top-level doc while NODE-004 is open (CLAUDE.md says to flag
this). A1's spec / decision conflict is the one item here that should be settled in writing before code
moves.
