# 2026-10-22 - Field trial passed (step 6); refactor phase opened (3d, 7, 7a-7d closed)

### Context

Five D-AMP nodes answered each other in one room, and every chain ended (step 6). The steps file was
revised for the next phase: the refactors, held against that field baseline and closed by a rerun of step 6
(10b) and a 24 h run (10c). Steps 4-7 closed between 2026-10-14 and 2026-10-22 without changelog entries; their
results are in roadmap-0-steps.md.

### Changed

- 7a: console output in whole lines and the per-sample path in IRAM, 0 dropped buffers (98b2fd0)
- 7b: host vs board comparison of the frequency path (23d034a); the shipped profile as an on-device and host
  test (test_shipped_profile, 6899683, d847b37)
- 3d closed, its pass doc archived, leftovers handed to steps (576ff3e)
- 7c decisions: FieldState stays as the rest gate; FrequencyMatch is removed (a9a47c3)
- 7d: FrequencyMatchDetector and TonalPulseFreq removed, Scalar the only family (3a11d80); the detector-family
  contract collected in myspec 5.3

### Verification

- step 6: hearing matrix 18/20, 41/41 chains ended in quiet and with speech/music, 0 noise-started, 0 drops
  (bench 2026-10-22-step6-trial)
- 7b: host == board to 1 FrequencyScore16 on the band, contrast within 0.5 %; test_shipped_profile 6/6 on E2
- 7d: Analyzer T3 vs 7a unchanged (chirp 20/20, hop 10/10, 40 ms 10/10 rejected), Node boots TonalPulseScalar
- finding: a 40 ms tone reads 86 ms in this room, because a late reflection sits above the release threshold
  (bench 2026-10-22-7d-dur40-raw); handed to 8b; it is then rejected only by audit 3.1's early-inspection bug
  (future_window_unavailable), not by the contrast gate
- not run: 10b/10c; the host vs board 100 ms duration mismatch (154 vs 127 ms) is unexamined

---

# 2026-10-12 - Micros wrap fixed (step 3c, #30), soak closed (step 3a, #27)

### Context

Both 24 h soak attempts went deaf 71.6 min after the Analyzer booted: the
32-bit micros() wrap broke every comparison between sample times and
millis() windows. The Node runs the same audio path.

### Changed

- one 64-bit TimeBase for audio, detection, Analyzer and behavior; wrap-safe
  comparisons; wraptest envs put the us wrap 180 s after boot (189b24c)

### Verification

- offset wrap: Analyzer T3 10/10 before, across and after; Node 88/88 and
  95/95 verdicts/answers, no own-echo verdicts (bench 85dd0a9)
- real wrap: soak 2026-10-12, 30 blocks / 7.3 h (stopped short, accepted by
  the owner), 299/300, every block after the wrap 10/10 (bench 151d56c)
- soak finding: accept rate flat, strength hour means 9.1k-11.2k vs ~200
  block to block; not attributed (no room notes)
- not run: Node past 72 min of real uptime
- archive: `docs/refactors/archive/micros-wrap.md`

---

# 2026-10-11 - D-AMP bench check (step 3, #20) closed

### Context

Piezo was discontinued for the D-AMP board (MAX98357A + I2S mic on one
full-duplex port). Step 3 measured the D-AMP signal against the recorded
piezo runs before thresholds were judged on it.

### Changed

- tone 0.3 FS, startup quiet threshold 400 on D-AMP, own-emit detection
  suppression over the whole chirp + 60 ms (b6e99b0); TonalPulseFreq
  thresholds rescaled for D-AMP (52e0c98)
- bench tooling: `tools/bench/soak.py` (24 h runner, flags blocks with no
  detection and runs SELFTEST), `tools/bench/selftest.py`, SELFTEST command
  in all three builds (be61884, step 3b in progress)

### Verification

- E3 -> E2 at 10 / 40 / 70 cm: 50/50 each (piezo 50 / 10 / 48), 0 false
  positives in silence, 0 dropped buffers
- Node on E2: quiet boot OK, 0 own-echo verdicts, external chirps answered
- stock TonalPulseScalar answers speech (2/50) and music (6/50) silent
  windows: input for the production profile (step 5)
- archive: `docs/refactors/archive/damp-bench-check.md`; sessions on `bench`

---

# 2026-09-21 - Detection cleanup pass: isolation, memory, naming

### Context

A review-driven cleanup of the detection layer, sequenced in
`docs/refactors/cleanup-0-plan.md`. Every step is build-verified on all
three environments against the real toolchain; none of it has run on
hardware yet. The hardware battery (T2/T3 SEQ regression, T4 Unity, T6/T7)
is the next thing to do before building further.

### Changed

- renamed the evidence-shape vocabulary that collided with detector
  provenance: `Occurrence.scalar`/`.frequency` -> `.magnitude`/`.band`,
  `ScalarInspection*` -> `MagnitudeInspection*`, `ScalarWindow` ->
  `MagnitudeWindow`; printed labels follow (`magnitude_observed` etc.)
- fixed `FrequencyMatchDetector::update()` letting the diagnostics-only
  best-evidence snapshot overwrite the live gate state that `buildReport()`
  reads; this changes `SEQ_SOURCE_SPEC`'s `gate_reason`/`ready_ok`/
  `gate_open` under default settings, which is the bug being fixed
- privatized `FrequencyMatchDetector`'s ~70 public fields; unified
  `DetectionRuntime::drainDetectors()`'s duplicated per-detector loop
- kept Analyzer diagnostics out of the Node and Emitter binaries:
  `ANALYZER_MODE` gating inside `DetectionRuntime`, plus a
  `build_src_filter` so the analyzer tooling is not compiled into them at
  all (it had been); Node RAM 87,940 -> 74,436
- `FeatureHistory`: dropped write-only per-bin `rms`/`peak` (bin 32 -> 24
  bytes), and record only the streams the active inspection plan reads (3
  slots, tied to `kMaxInspectionModules`); Node RAM -> 59,996, a 31.8%
  reduction for the day, Analyzer 99,564 -> 85,124
- renamed `PatternMatcher`/`PatternResult` -> `OccurrenceEvaluator`/
  `OccurrenceVerdict` (`src/detection/evaluation/`): the stage judges one
  occurrence against the plan's support requirements and has no temporal
  logic; "Pattern" is reserved for a future downstream stage. Labels
  follow (`verdict.valid=`, `analyzer.stage=evaluator`, ...). Trial
  classifier and Behavior vocabulary (`ValidPatternInExpectedWindow`,
  `HeardPattern`) deliberately not renamed yet; emission patterns
  (`ChirpPattern`) are correct as they are
- deferred the detector-object union (1,440 bytes measured, a UB hazard on
  profile switch); superseded by `cleanup-detector-family-build.md`:
  detector family as a build flag, profile as reboot-applied config,
  thresholds as live params, one `DetectionFamily.h` carrying the switch,
  and `SimpleThresholdDetector` as the third family and the MVP detector
  (`mvp-app-structure.md` amended accordingly)
- recorded, not changed: `FrequencyMatchDetector` enforces no upper
  duration bound, so a frequency occurrence over ~246 ms already outruns
  the 256-bin history

### Verification

- `platformio run -e esp32dev -e esp32dev-emitter -e esp32dev-analyzer`
  passes at every commit
- the Analyzer translation unit was preprocessed before/after the
  isolation split and diffed: identical apart from ordering and one dead
  local
- the evaluator rename left Node RAM and flash byte-identical
- no hardware runs; T2/T3/T4/T6/T7 all outstanding

# 2026-06-22 - TonalPulseScalar quality path landed

### Context

The TonalPulseScalar scalar-quality refactor reached the landed code state and
the remaining work shifted to live board validation.

### Changed

- landed carrier-quality gates on the scalar transient detector
- added explicit `uncertain` pattern flow for inspector-gate failures
- expanded the TonalPulseScalar inspection plan to target, contrast, and AMP
- updated canonical analyzer output to report the new quality and pattern
  evidence fields
- refreshed `current-pass.md` to describe the live validation step instead of
  the old implementation checklist
- updated the implementation status note for TonalPulseScalar

### Verification

- `platformio run -e esp32dev-analyzer` passes
- `platformio run -e esp32dev` passes
- the analyzer now flashes successfully on `COM6`
- live runtime behavior capture is still pending

# 2026-06-13 - LOG-001 tuning-run logger

### Context

The scalar tuning workflow needs a reproducible, log-first batch scaffold around the `PARAM` command surface.

### Changed

- added the LOG-001 tuning helper under `tools/`
- documented the `SEQ start profile=TonalPulseScalar tries=50 mode=source when=all verbose=1` workflow
- updated the seq-tests batch layout to include `README.md`, `session.log`, `run_01.log` through `run_10.log`, and `block_01_summary.md` through `block_10_summary.md`
- recorded the `Codex-run` and `User-run` workflow split
- added resume support for interrupted batches via `-BatchRoot` and `-StartRun`
- added single-instance batch locking plus resume-state snapshots for LOG-001 tuning runs

### Verification

- the analyzer command surface already supports `PARAM` and `PARAM STATUS`
- no detector semantics change is required for this documentation/helper pass

# 2026-06-12 - Analyzer tuning surface PAR-001

### Context

Analyzer sequence runs needed a small, module-owned tuning surface for the
`TonalPulseScalar` profile, without jumping to the full ParamRegistry design.

### Changed

- added `AnalyzerTuning` as the analyzer-owned runtime tuning surface
- wired `PARAM` parsing to update scalar transient tuning values
- merged the active sequence profile from the tuning surface before runtime
- printed scalar profile parameters in the sequence header and `SEQ` report
- kept `AmpEnvelope` as the internal stream name while showing `Scalar` in analyzer-facing text

### Verification

- `platformio run -e esp32dev-analyzer` passes

---

# 2026-05-26 - 5-node slow circle stabilization

### Context

The current RB pass is now using the committed profile defaults directly:

```text
DetectionProfile.h = TonalPulse / frequency-gated detection defaults
BehaviorProfile.h = behavior timing defaults
node.cpp = no local TonalPulse overrides
```

### Decision

Lock the current 5-node slow circle baseline to the committed profile headers:

```text
TonalPulse detection stays frequency-gated
TonalPulse behavior stays self-suppressed and idle-gated by the default behavior config
runtime startup should defer to the profile headers rather than hardcoded node-side overrides
```

### Current Baseline

```text
5-node slow circle
freq/amp gated
stable as committed
```

# Changelog

This changelog records documentation and architecture decisions.

It is historical. It should not be used as the current implementation contract.

The active architecture contract lives in:

```text
myspec.md
```

The immediate task lives in:

```text
current-pass.md
```

---

## 2026-05-25 — Docs Structure / Roadmap Consolidation v5

### Context

The project now prioritizes a concrete physical test use case:

```text
testing current TonalPulse detection and behavior variations on 5 nodes
```

This changes implementation order.

Near-term work should support repeatable testing before building future detection families, full behavior runtime, or broad param infrastructure.

### Decision

Use one active spec plus specialized implementation-order roadmaps:

```text
myspec.md = current architecture contract

roadmap-general-node.md = cross-roadmap sequencing
roadmap-param-config.md = param/config workflow, hardcoded first, runtime later
roadmap-detection.md = future detection work only
roadmap-behavior.md = active future behavior work
roadmap-output.md = future output/profile/action boundary
roadmap-vektor-later.md = thin future VEKTOR exposure roadmap

current-pass.md = immediate task only
changelog.md = history
archive/ = historical docs
```

### Spec / Roadmap Policy

This docs set does not update `myspec.md`.

Later, update `myspec.md` from:

```text
current source code
this docs package
current test findings
```

Rules:

```text
stable ownership/boundary rule → myspec candidate
planned mechanism / implementation order → roadmap
immediate task → current-pass
history → changelog/archive
```

### Added Guardrail

Minimum viable does not mean throwaway.

MVP means:

```text
smallest useful slice
within the intended architecture direction
```

Avoid both:

```text
empty frameworks
wrong-owner hacks
```

### Current Practical Workflow

For first 5-node tests:

```text
hardcoded params + firmware upload are acceptable
```

Runtime params become worthwhile only with:

```text
PARAM SET + SAVE/LOAD + VERIFY
```

or:

```text
fleet/OTA param apply + VERIFY
```

Unsaved runtime `PARAM SET` is not enough as the main 5-node workflow.

### Naming Direction

Preferred detection terminology:

```text
SignalCandidate        → Occurrence
SignalEmitter          → OccurrenceSource
SignalInspector        → OccurrenceInspector
InspectedSignal        → InspectedOccurrence
```

`PatternCandidate`, `PatternRules`, and `PatternResult` stay.

Profile rename:

```text
FreqAmp → TonalPulse
```

### Next Practical Direction

```text
1. Run rename pass: Occurrence + TonalPulse.
2. Add/clean 5-node status baseline.
3. Compile.
4. Test hardcoded TonalPulse params on 5 nodes.
5. Only then decide whether runtime params, behavior modes, or pulsed chirp work should advance.
```
