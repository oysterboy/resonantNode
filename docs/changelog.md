# 2026-10-29 - Installation-ready node milestone closed (11c, 11h, 11g, 11d; 11i, 11j beside it); new phase from step 12

### Context

The phase that opened with step 10b closed as the "installation-ready node" milestone
(decisions/2026-10-26-milestone-installation-ready-node.md). Its gate: the unattended soak (11c), the band decision
(11h), the review's Part D fixes (11g) and OTA firmware (11d). The Node split (11i) and a code standards pass (11j)
ran beside it while the owner was away from the bench. The steps file was archived as it stood and a fresh one opened
the next phase from step 12.

### Changed

- 11c: 24 h soak with room checks every 20 min (accepted at 23.45 h): hop trains 710 / 710, chirps 0 / 1,400, every
  chain ended, no answer started by noise, no reset (bench/sessions/2026-10-26-soak24-shape-b). A later 27 h run on
  40c92e3 repeated it (2026-10-28-soak96-40c92e3).
- 11h: the network band swept over 500-5000 Hz for 19 h plus hops under noise (E004): no band accepted noise; below
  1250 Hz music and speech cost 40-60 % of the hop trains; the band stays at 2950-3950
  (decisions/2026-10-28-band-stays-after-the-sweep.md).
- 11g: the review's Part D: BinTrack wrap fix, shape params as live params, the Analyzer's run / trial split and one
  classification site, output read by nobody pruned, the Node's boot warmup back, RB debug dropped.
- 11d: OTA firmware over WiFi: `OTA url=`, `NET * OTA` updates every Node at once, rollback until a new image has
  run 20 s (decisions/2026-10-28-ota-firmware-pull-over-http.md); all five boards updated this way, a crashing image
  rolled back (bench/sessions/2026-10-29-step11d-ota).
- 11i: the Node split into owners (baseline, net gateway, self-test, signal composer, params, command table) with a
  Behavior status snapshot; bench gate met on the OTA-delivered build (2026-10-29-step11i-nodecheck).
- 11j: clang-format at 120 columns with a CI check, duplicated helpers merged, the Analyzer's command handler split.

### Next

Step 12a: a sanity check bench test (content to be defined); 12b BEH-008, answers that follow the field level; 12c
the network in the tryout room; 12d the hop margin.

# 2026-10-25 - Refactor phase closed (8, 8b, 10, 10a-10c); installation build run 24 h; radio remote commands (11a)

### Context

The refactor phase opened on 2026-10-22 closed as planned: the evaluator fold (8), the detection fixes (8b), the
mode-layer cleanup (10, 10a), the field rerun (10b) and a long run (10c). The owner then pulled the installation
items ahead of 10c's second block (change order 2026-10-23: persistence, loose ends, output status), so block 2
ran on that build for 24 h. With the owner away from the bench for a week (from 2026-10-25), step 11 opened with
remote commands over the radio (11a), the DET-015 host phase, unattended runs and firmware OTA.

### Changed

- 8: OccurrenceEvaluator folded into inspection as makeVerdict() (2593327, 5c5f9ed); FieldState trimmed to the rest
  gate (4efba86); step 8's Part B deferred as DET-016
- 8b: inspection-plan contract and `RB plan_check` (881694f); a candidate past max duration closed as too long
  (7b62c84); verdict-queue overflows counted (733a68b); FeatureHistory scratch off the loop stack (ef86297)
- 10a / 10: boundary map (c8ec486); live detection params apply in place (7c2c28e); ParamRegistry as an in-repo
  library (89e723e); behavior.* params, RB BEHAV removed (c4b27fa); RB PROFILE names what it reset (17c8801);
  Analyzer out of src/detection/ (4ee7a42); dead code out (9721ed5); Node glue split (426a03f)
- DET-017: inspection waits until its window is complete (bbb5728); min duration 85 -> 92 ms (e58c4c2); the owner
  decided the Node's network signal is the hop train (a5a6331)
- 10d: PARAM SAVE / LOAD / RESET to NVS, stored values win at boot with a `PARAM stale` warning (3f2ed74);
  `output.silent` (da771a5)
- 10e: a live idle_timeout reschedules the running wait (d275992); a 12 KB stack temporary removed from every
  detection reset, loop stack free minimum 3.4 -> 14.2 KB (9972fe4)
- 10f: SoundOutput executes an OutputRequest with OutputStatus (OUT-002/003, f38cd69)
- drift and room visibility: heard strength on verdict lines and in RB STATUS (db5213c); audio.level fields
  (aa97051); nodegroup drift attributed by the train's own start (1dff962)
- 5h: cellrun.py and hopbins.py (3f3e69d, dbb6305)
- 11a: remote commands over ESP-NOW through the serial command path, paced replies, SELFTEST to one node
  (267b860, 9ef51a1); decided that VEKTOR will replace NET (5cd5959)
- docs: spec / guide / roadmap split as a rule, notes_manual rewritten as the operator guide (2fab9ac)

### Verification

- 8 / 8b / 10: T2h host replay byte-identical through each step; T3 unchanged; host suites green
- 10b: hearing 19/20, every chain ended, 0 noise starts, 0 drops (bench 2026-10-22-step10b-field-rerun)
- 10c: block 1 9.6 h (bench 2026-10-22-step10c-overnight) and block 2 24.0 h on dbb6305 (bench
  2026-10-23-installation-run): 1752/1752 chains ended, 0 noise-started of 30,546 answers, 0 dropped buffers, 0
  resets, no deaf node; per-pair strength drift within about +-15 % over 18 h, one path stepped alone, a
  room-wide change at 10:50 on 10-24 without a noted cause
- DET-017: 40 ms beep rejected, hop and chirp 10/10 (bench 2026-10-23-det017-min92)
- 10f: emission shape and hearing unchanged before / after on E1 + E4 (bench 2026-10-23-step10f-output)
- 5h height cell: E3 +25 cm drops the hop train to 42/50, the train cut to three steps (bench
  2026-10-23-cell-height25-110cm); reference 50/50
- 11a: all five boards, 0 dropped buffers with the radio idle and for SET / GET / PING / STATUS / DUMP / SELFTEST by
  radio; a PARAM SAVE costs about 8 buffers per node (the flash write, serial or radio)
- not run: the dry cell (5h); the long run with the radio on (running); the host vs board 100 ms duration
  mismatch is still unexamined

---

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
`docs/refactors/archive/cleanup-0-plan.md`. Every step is build-verified on all
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
