# Boundary map: the Node and Analyzer mode layers (NODE-002)

Status: written 2026-10-22 at main ff2fb0f (after step 8b), the input to step 10. Each section names which code
does which job today, and where one function does several. Line numbers drift; function names are the anchor.
Master plan: docs/refactors/step10-mode-layer.md.

Categories: **Detection** (configures or feeds DetectionRuntime / FreqBandStream / AudioSignal), **Behavior**
(ResonantBehavior and its config), **Output** (tone, hop train, idle call, LED), **Params** (ParamRegistry,
PARAM commands), **Commands** (serial parsing and dispatch), **Status** (RB STATUS, summaries, boot prints,
checks), **SelfTest**, **Glue** (setup/loop, wiring, the startup baseline state machine).

## 1. Node (`src/modes/resonant/`, class `Node`, ~2,000 lines plus SelfTest and Debug files)

Clean, one category each:
- Params: `configureParameters`, `registerDetectionParams` (13 scalar + 2 band), `registerOutputParams` (7),
  `handleParamCommand`.
- Output: `bandBinCount`, `hopFrequencies`.
- Status: `printDetectionProfileDetails`, `printBehaviorRuntimeDetails`, `printSignalConfig`,
  `printHopGapCheck`, `printInspectionPlanCheck`, the `printRb*Summary` family, `resetRbCounters`.
- Detection: `applyActiveDetectionProfile`, `applyBandParams`, `resetDetectionState`,
  `captureFrequencyMeasurementPacket`.
- Behavior: `applyActiveBehaviorGateConfig`, `activeBehaviorProfile`, `makeTonalPulseBehaviorProfile`.
- Commands: `pollSerialCommands`, `handleDebugCommand`, `handleLogCommand`, `handleProfileCommand`.

Mixed (where step 10's splits go):

| function | categories | note |
|---|---|---|
| `update` (~200 lines) | Glue, Detection, Behavior, Output, Status, SelfTest, LED | the own-emit sample gate, the idle-call state machine and the emit span handed to Behavior live inline |
| `handleSerialLine` (~230 lines) | Commands, Behavior, Glue | RB BEHAV parses 13 values and calls 13 setters one by one; `BehaviorGateConfig` has exactly those 13 fields and is not used |
| `handleDetectCommand` | Status | it *is* RB STATUS; the name is wrong. It reads Detection, Behavior, FreqBandStream internals, I2S stats, ChirpOutput |
| `processDetectionFrame` | Detection, Behavior, Status, LED, SelfTest | |
| `begin` | Glue, Detection, Params, Output, Behavior, Status | `resetDetectionState` runs twice at boot |
| `performRbRebase`, `quiesceBehaviorForOutputsOff`, `updateRbBaselineState` | Glue with Behavior / Detection / Status | |
| `applyParamModule` | Params, Detection, Status | the Behavior and Node cases are empty |
| `setProfileFromName` | Detection, Behavior | **RB PROFILE reloads both default profiles: every RB BEHAV value and every `detection.scalar_*` PARAM SET is silently lost** |
| SelfTest `restoreAfterSelfTestHardware` | Output, Behavior, Detection, Glue | a near copy of `resetDetectionState` |
| Debug `observeBehaviorGate`, free `behaviorGateName` | Status rebuilding a Behavior judgement | Behavior reasons reconstructed in Node |

Detection reached beyond the verdict/FieldState path (legitimate today, listed so step 10 does not widen it):
FreqBandStream internals in RB STATUS; `evaluatorFirstEnabledRequirement` (rebuilds has_requirement /
required_label); the two contract checks; the registry binding straight into `_activeDetectionProfile`; Node
feeding FreqBandStream and building the frame packet; Behavior's own-emit gate deciding which samples reach
detection.

`applyActiveDetectionProfile()` resets the detectors (`setDetectorSelection`) and wipes FeatureHistory
(`setInspectionPlan`). It runs at boot, on RB PROFILE, on every rebase, after SELFTEST hardware, and on **every
successful `PARAM SET detection.*`, including `band_low_hz` / `band_high_hz`, which only need FreqBandStream**
(issue #14: one threshold change drops an in-flight occurrence and the next ~256 ms of inspections).

The two param paths: RB BEHAV (13 Behavior knobs, aliases such as `idle` for `idleTimeout`) and PARAM SET (15
`detection.*`, 7 `output.*`). No key overlaps; no Behavior key is on the registry. RB PARAM is an ERR stub
since 7d.

Dead or broken: `printRbDetectorSummary` declared, never defined; NodeDebug `observeOnset`, `observeTransient`,
`printPlotValues` never called, so `RB debug plot` prints nothing; `_wasSelfChirpSuppressed`,
`_rbLastLoggedOnsetRejectCount`, `_rbLastLoggedTransientRejectCount` written, never read; `startsWithTokenIgnoreCase`
/ `equalsIgnoreCase` duplicated from `AnalyzerText`.

## 2. Analyzer (`src/modes/analyzer/` and `src/detection/analyzer/`)

`src/detection/analyzer/` holds three kinds of file:
- **AnalyzerApp member files (9)**: AnalyzerCommands, AnalyzerSequenceSession, AnalyzerSeqReporter,
  tools/AnalyzerEmitterControl, AnalyzerRawCapture, AnalyzerRuntimeReporter, AnalyzerSampleDump,
  AnalyzerSystemReporter, AnalyzerTrialCapture. Each includes `modes/analyzer/AnalyzerModeApp.h`: these are the
  9 include-direction warnings. Nothing outside them includes them, so they move cleanly (include edits inside
  them only; the ESP Node filter already excludes `modes/analyzer/`).
- **Analyzer-only types and helpers** with no AnalyzerApp dependency: AnalyzerReportTypes.h,
  AnalyzerTrialClassifier, AnalyzerText, tools/AnalyzerRawHealth.h, and four stub headers (two never included).
- **AnalyzerPassRules.h, mixed**: `nextOccurrenceId` is a detector identity rule used by the scalar detector in
  every build; the other four functions are Analyzer-only. Because it includes AnalyzerReportTypes.h, **every
  build's scalar detector compiles the Analyzer report types**. ANA-003 must split it, not just move files.

Mixed functions: `handleUsbLine` (Commands, Params, Detection wiring, emitter link, sequence policy);
`startSequenceTest`, `updateSequenceTest`, `runRawTrigger` (~680 lines), `finalizeSequenceTrial`,
`buildSequenceAnalyzerReport` (~320 lines), `update`, `begin`.

Trial classification is spread over five places: the classifier, `selectSequenceTrialSelection` (Late vs Expected),
`finalizeSequenceTrial` (overrides), `buildSequenceAnalyzerReport` (startup artifact, PatternRejected override),
and `handleSequencePending` (window and duplicate logic). `kLateOnsetMinMs = 200` is defined three times.

Detection reached through the ANALYZER_MODE runtime API (legitimate for the Analyzer): diagnostics toggles, reject
summaries, pipeline events, the active DetectorReport, FeatureHistory counts, the history-incomplete snapshot, 11
counters; plus DetectorReport fields, `evaluatorFirstEnabledRequirement`, inspection observations, FreqBandStream
profiling counters.

Dead: `sequenceTrialClassificationName` declared, never defined; unused kCleanDuration / kSmeared / kTooLong /
kNearMax constants; HELP advertises `DET PROFILE`, which nothing handles; `printDetectionParameters` has an empty
body.

## 3. Three param vocabularies for the same detector

| | Node `PARAM SET path value` | Analyzer `PARAM key=value` | Host replay `--set key=value` |
|---|---|---|---|
| scalar keys | 13, `detection.` prefix, bounded | 14 (+ `scalar_observed_stream`), snake and camelCase aliases, unbounded | 10 (no `require_*`, `min_strength`, `observed_stream`) + `contrast_medium`, `amp_medium` |
| band | `detection.band_low_hz/high_hz` | per run: `band=` on SEQ/RAW | `--band` |
| applies | at once (with the reset above) | at the next SEQ start, TonalPulseScalar only; RAW ignores it | per replay |
| bad token | rejected | ERR, the others still apply | exit 2 |
