# Roadmap - Detection and Analyzer

Status: active roadmap.
Scope: detection pipeline cleanup and analyzer follow-up work.
Purpose: keep detection and analyzer work together where the same runtime
facts are shared.

---

## Status legend

```text
[LANDED]    Verified in current code.
[PARTIAL]   Present, but not yet in the intended final shape.
[TODO]      Next implementation step.
[DEFERRED]  Intentionally later.
[REMOVED]   No longer part of the active plan.
```

## Architecture goal

```text
Detection produces facts.
Analyzer reports and classifies trials.
OccurrenceEvaluator decides pattern meaning.
Behavior consumes OccurrenceVerdict and FieldState.
Clean analyzer output should read canonical runtime contracts only.
```

Landed items from this area now live in `docs/roadmaps/roadmap archive/roadmap-changelog.md`.

## Current code state

```text
[REMOVED] DetectionDiagnostics and analyzer legacy compatibility are removed from src.
[PARTIAL] OccurrenceEvaluator currently stays single-proposal oriented.
          Scheduled for removal: cleanup-0-plan.md Phase 7c folds it into
          OccurrenceInspector (OccurrenceVerdict stays as the Behavior-facing
          type). ANA-002 and the Evaluator wording in this file change with it.
[PARTIAL] Frequency reason handling is still string-backed internally.
```

## Implementation order

### DET-006  Move detector-specific Analyzer output detail into detection-side report printer/name helpers.

Goal: adding a new detector should require changes in:

detector class
detector config/profile wiring
detector report detail definition/printer

but not in Analyzer core report assembly.

### DET-001 - detector / report consistency

Status: PARTIAL

```text
Keep detector and clean-summary truth aligned.
Do not retune thresholds as part of this pass.
Keep the remaining cleanup separate from legacy-printer work.

Landed in code: FrequencyMatchDetector now freezes its DetectorReport on the
accept path (FrequencyMatchOccurrence.cpp, capturePendingOccurrence), matching
ScalarTransientDetector. Hardware confirmation (T2 in cleanup-0-plan.md)
still outstanding; close this item when it passes.
```

### DET-007 - decide the Node's production profile

Status: DONE 2026-10-14 (docs/decisions/2026-10-14-node-ships-tonalpulsescalar.md; the Node registers the scalar params, b4566aa)

```text
Code and docs disagree. The Node boots makeTonalPulseScalarProfile()
(ResonantNodeApp.h), but implementation-status.md and myspec.md call
TonalPulseFreq the main runtime profile, and the only detection params the
Node registers (Node::registerDetectionParams) are frequency-match
thresholds, which have no effect under the scalar profile it boots.

Owner lean 2026-10-14: TonalPulseScalar ships, TonalPulseFreq stays a demo detector (not yet decided; step 5a baseline first).
Decide which profile the Node ships with, using cleanup-0-plan.md Phase 0's
field trials as the evidence. Then make the code default, the registered
params, implementation-status.md, and myspec.md agree.

Blocks cleanup-0-plan Phase 5d: it decides which family env:esp32dev builds
and whether 5d's RAM argument (production = frequency family) holds.
```

### DET-011 - pulse trains read as one signal (Scalar)

Status: IDEA, source ready (step 5b, research note scalar-detector-extensions.md section 1; also carries hop trains and sweeps through room nulls, see DET-013). Emitter `PULSES` landed 2026-10-14, compile not run (PlatformIO registry blocked in the cloud session), bench run pending.

10-15 ms pulses with 5-115 ms pauses accepted as one occurrence via maxGapMs / maxTransientDurationMs. Measure the smallest resolvable gap and the false-alarm cost.

Gap contract (step 5h, #47, from audit 2.3): the shipped hop train (25 ms x 5, gap 10) is one occurrence only because the room tail fills the gaps (min duration 85, release debounce 10, max gap 20); `output.hop_gap_ms` and the Scalar gap / debounce / min-duration params are independent with no consistency check. 5h adds the check and a dry hop fixture; whether the room tail is part of the signal contract is decided on that fixture and a dry bench cell.

Source: Emitter `PULSES n pulse_ms gap_ms [freq=] [ramp=] [trial=]`, from the Analyzer as `EMIT PULSES ...` or per SEQ trial as `SEQ start dur=PULSE_MS pulses=N gap=MS [ramp=MS]`. On D-AMP the train is timed per sample in I2sToneOutput (loop edges land on 8 ms blocks); pulse_ms is sound with the ramps inside, gap_ms is silence. Piezo builds time it from the loop.

### DET-012 - lower edge of the network band

Status: RESCOPED 2026-10-15 (was "lower chirp frequencies"; step 5b)

Below 1 kHz is wanted for aesthetics, not detection: it moves to the aesthetic layer of a two-layer signal (OUT-006, `docs/decisions/2026-10-15-signal-variation-inside-a-shared-band.md`). What stays here: where the network band's lower edge can sit. 2026-10-14 single tones: speech/music valid 0/0 at 3200, 4/1 at 2800, 3/4 at 2600, 3/4 at 2000, 1/30 at 1500 Hz (50 windows each, `bench/sessions/2026-10-14-lowfreq-90cm-noise`, `-midfreq-90cm-noise`). So 2500-3000 needs DET-014 first; otherwise the band sits around 3000-3600. Re-measure with the band stream of DET-013, not a single bin.

Depends on step 5a item 2b (DC blocker, landed bfbe0c8).

### DET-013 - band stream: the detector watches the network band

Status: in progress (step 5b). Done: 1 (14.7 us/sample, 2026-10-15), 3 (bank in `FreqBandStream`, Analyzer
`SEQ band=`, Node `detection.band_low_hz/high_hz` default 2950-3950 since 155211e/next commit, bins 2950/3200/3450/3700/3950 so the 3,200 Hz chirp is on a bin), 4 (contrast = peak against
the mean of its neighbours, guards included; limit 30,500 in `docs/decisions/2026-10-16-hop-acceptance-contrast-
gate-not-amp.md`). Open: 2 (cheap single pass), 5 (Hann, with OUT-006), Node timing with the 5-bin bank on
hardware, the gate's final run.

Signals vary per node and over time, as sweeps or hopping pulse trains (`docs/decisions/2026-10-15-signal-variation-inside-a-shared-band.md`). `FreqBandStream` stops watching one 250 Hz bin at 3200 Hz and watches a configured band: a bank of K bins, `FrequencyTarget` = strongest bin (FrequencyScore16 unchanged), peak-bin index and band energy as facts.

1. Profile the stage on D-AMP (`FREQBAND_ENABLE_PROFILING=1`).
2. Same output, cheaper: one pass for all bins, mirrored ring buffer (no `%`).
3. Bin bank behind params (band edges, spacing); K = 3 with today's spacing reproduces current behaviour. Sliding DFT if 1 shows the bank does not fit.
4. Redefine contrast: the neighbour bins are inside the band now. TonalPulseScalar's `FrequencyContrastQuality` inspector depends on it, so this is a production change; record the definition in `docs/decisions/`.
5. Hann window if OUT-006's low layer leaks into the band (measure first).

Gate: fixed 3200 Hz unchanged at 90 cm (5a baseline); hop and sweep from OUT-005 detected at every up-spacing-map spacing (60-110 cm); speech/music OBS no worse than 5a.

Research: docs/research/frequency-drift-detection.md

### DET-014 - tonality inspector

Status: TODO (step 5b if DET-013's OBS gets worse, else step 8a)

Peak-bin power vs total window energy over the occurrence. Pays for the noise a wider band admits; the first candidate before a lower band edge (DET-012) is tried. Data path: per-occurrence accumulation in the detector first, a `FeatureHistory` slot only if needed (3 slots, ~5 KB each).

Research: docs/research/frequency-drift-detection.md (Inspectors that help)

### DET-015 - shape inspectors: peak trajectory, band coverage, pulse structure

Status: TODO (step 8a, after Phase 7c folds the evaluator into the inspector)

Facts per occurrence: steady / sweep / hop, span, direction, distinct bins, pulse and gap regularity. Reject random jumping (speech formants) and syllable-like trains; give Behavior shape facts for BEH-007. Data-path decision (detector accumulation vs a stream) gets a `docs/decisions/` file.

Research: docs/research/frequency-drift-detection.md (Inspectors that help)

### DET-008 - detection-layer cleanup pass

Status: PARTIAL

```text
Executed in docs/refactors/cleanup-0-plan.md (phase order, T1-T8 test
battery, gates) with its topic docs (cleanup.md,
cleanup-detector-family-build.md, cleanup-analyzer-node-isolation.md, ...).
That plan orders the work within this item; roadmap-0-steps.md decides when
it runs (steps 1, 8, 9).

Landed and hardware-verified on piezo (2026-10-08, issue #7): Phases 1,
2, 3, 5a, 5c. Open: Phase 7c, Phase 5d, Phase 6, Phases 7a/7b, Phase 0
(field data, shared with DET-007).
Close when cleanup-0-plan.md is archived with its "Archived: ..." header.
```

### DET-009 - inspection reads an empty FeatureHistory window under load

Status: DONE 2026-10-09 (step 1a, issue #26; docs/refactors/archive/i2s-sample-clock.md)

```text
Found by the issue #7 hardware runs: an accepted occurrence is inspected
against a history window holding no samples (history_window_incomplete,
available_start/end = 0), so the verdict rejects a strong chirp. Reduced by
795f649, still reproducible with TonalPulseFreq and diagnostics on (14/30),
absent with diagnostics off. Fix the mechanism; widening the window or
passing the incomplete case is not a fix.
```

### DET-010 - peak-centered inspection runs before its window has arrived

Status: TODO (found 2026-10-09, #20)

```text
TonalPulseFreq inspects PeakCentered windows (peak -10 .. +90 ms). The
inspection is taken when the occurrence ends; when the peak comes late in
a long occurrence (D-AMP at 0.3 FS: 146-149 ms occurrences), the window's
end is still in the future, the module reports status=missing /
future_window_unavailable (available_start/end 0), every class is
unknown, and the verdict rejects a strong, clean chirp.
Example: anchor 17623, requested 17613-17713, inspection_now 17663.
Rate at 110 cm, 3200 Hz: 10-14 of 50 trials, independent of the detector
score threshold (1500-2500) and with 0 dropped DMA buffers
(bench:sessions/2026-10-09-issue20-damp-110cm-b h_T2v_*,
e_T2v_s2500_a030_f3200). TonalPulseScalar (Start-anchored, 0..+100 ms)
was not hit. Same family as DET-009. Fix the mechanism (defer the
inspection until the window end is available, or anchor the window
inside the occurrence); passing the incomplete case is not a fix.
(The available_start/end = 0 in those lines is ANA-009's print artifact;
the cause is the reject_reason.)
```

### DET-003 - inspection target / payload split

Status: TODO

```text
Keep one simple selector for what must be inspected for acceptance.
Keep source-specific payload fields inside the module config or payload type.
Do not split the current path unless a second payload shape is actually needed.
```

### DET-004 - field state for detection and reporting

Status: TODO

```text
Track actual use of field state for detection and reporting.
Keep the field-state view separate from detector truth.
```

2026-10-22 review of what FieldState holds:

- Behavior reads only `recentPatternCount` (the rest rule); RB STATUS and
  the analyzer also print `quiet`/`dense` and the counts.
- `noiseFloor` is occurrence count / window ms and `avgAmbientLevel` copies
  it: no audio level is measured (audit 1.2). Cleanup: delete both or
  rename to an occurrence rate. No tuning involved; can land any time.
- Candidate additions, each only if step 6's counts show a need (decided
  with step 11's "FieldState gates or goes"):
  - real ambient level: running mean of mic-frame RMS/envelope from the
    feature stream, in its own domain (myspec 5.8 "where explicitly
    configured"; not rescaled from Strength16). Use: louder reply or rest
    in a loud room. Most likely useful one.
  - recent occurrences per hop bin: only if nodes pick a free bin; hop
    rotation is fixed (MAC % 5) today, so nothing would read it.
  - not here: avg occurrence strength/contrast are detector quality, they
    belong in DetectorReport/analyzer.

### DET-005 - detector reason-model parity

Status: TODO

```text
Make frequency detector internal reason handling as explicit and typed as the
scalar reject handling, or document the asymmetry if string-based reasoning is
still intentional.
```

### ANA-001 - analyzer stage vocabulary cleanup

Status: TODO

```text
Keep clean SEQ_TRIAL / SEQ_SOURCE / SEQ_INSPECT / SEQ_EXPLAIN / SEQ_SUMMARY
on canonical detector-report and inspected-occurrence facts.
Do not rebuild detector truth in AnalyzerRuntime.
Keep the analyzer display layer on canonical report fields.
```

### ANA-003 - move AnalyzerApp out of the detection tree

Status: TODO

```text
Nine files under src/detection/analyzer/ and src/detection/analyzer/tools/
define AnalyzerApp:: member functions and include
modes/analyzer/AnalyzerModeApp.h, so the detection tree depends upward on the
mode layer. Move those files to src/modes/analyzer/. Keep only the pure parts
in detection (AnalyzerTrialClassifier, AnalyzerPassRules,
AnalyzerReportTypes, AnalyzerText).
File moves only, no behavior change; update build_src_filter to match.
Enables NODE-007's include-direction check to become an error.
```

### ANA-004 - diagnostic SEQ reports stall the loop and drop audio

Status: DONE 2026-10-22 (step 7a; found 2026-10-09, #20). Cause: Arduino-ESP32 queues each Serial write as its own TX ring item, so small prints filled the ring and blocked at the UART rate; all output now goes out in whole lines (app/SerialOut.h), and a SEQ report takes 3.4 ms instead of ~190 ms. A second cause showed up while fixing it: the per-sample chain ran over budget from flash. With that path in IRAM, the per-sample cost fell from 73 to 37 us, and 40 detail + diagnostics trials ran with 0 drops (bench/sessions/2026-10-22-7a-gate; decisions/2026-10-22-serial-lines-and-iram-hot-path.md). No report task was needed.

```text
With SEQ diagnostics on (the default) and mode=detail, each trial that
has a detection prints ~6.9 KB; the loop stalls and the I2S driver drops
~21-22 DMA buffers (~170 ms of audio) per detected trial. Trials with no
detection, mode=trial/system, or SEQ DIAG off: 0 drops. Seen on D-AMP
(bench:sessions/2026-10-09-issue20-damp-110cm-b g_drops_*) and on piezo
(issue #26 F3_diag_on / F5 runs); the clean piezo reference V_scalar50 ran
with SEQ DIAG off (772 bytes per trial).
Effect on results: none shown so far (buffer_overrun_trials=0 in every
#20 run), but a hole landing in an inspection window would reject the
trial. (The TonalPulseFreq rejections first blamed on this are DET-010:
they recur with 0 drops.)
Until fixed: comparable runs use SEQ DIAG off (as the #26 baselines) or
mode=system, and record dropped_dma_buffers.
Fix direction (2026-10-10): a report task. The loop pushes one compact
record per trial into a FreeRTOS queue; a low-priority task on core 0
formats and prints it, so the audio loop never formats text or waits on
the UART. First measure where the ~170 ms goes (formatting vs UART), then
record struct, task, unchanged text format for the bench tools. Done when
a detail + diagnostics run shows 0 dropped buffers on detected trials.
Research: docs/research/freertos-tasks.md (candidate 1; steps there).
```

### ANA-005 - Analyzer never sends its boot control claim

Status: DONE 2026-10-22 (step 7a; found 2026-10-09, #19): the claim is queued 500 ms after boot; `EVT analyzer_control_claim sent` / `acked`.

```text
AnalyzerApp prints "EVT analyzer_control_claim scheduled" at boot, but
_controlClaimPending is never set, so MODE REMOTE is not sent until the
first EMIT or SEQ command. The Emitter stays in AUTO (chirping every 2 s)
meanwhile. Either send the claim or drop the message. Pre-existing.
Related, likely already fixed: "the first emitter remote claim after each
Analyzer boot times out" (step 1 / #26 side finding; tools/bench/seqrun.py
spends a warm-up EMIT CHIRP on it) fits the reset-junk-byte bug fixed in
17295c9 (app/SerialLine.h). Not re-tested; if the first claim now
succeeds, the warm-up can go.
```

### ANA-006 - bench runner timeout cuts long runs silently

Status: DONE 2026-10-09 (seqrun sizes the timeout from tries x period and warns on a cut run; index keeps partial counts with complete=0)

```text
tools/bench/seqrun.py stops a run at --timeout (default 600 s) and moves
on; a 150- or 400-window SEQ OBS run (~2.9 s per window) is cut before
SEQ_SUMMARY, and the index then has no row for it (two #20 soak runs).
Derive the timeout from tries x period, or warn loudly and keep the
partial trial count in the index.
```

### ANA-007 - trial dt is anchored on a late-polled marker

Status: DONE 2026-10-22 (step 7a; found 2026-10-09): dt is anchored on the time the command was sent (`dt_anchor=command` in SEQ_TRIAL). It is constant at 51 ms (chirp) and 48 ms (hop) on E2/E3, where the polled marker jittered 22-38 ms.

```text
SEQ trial dt is measured against the Emitter's EMIT_START marker at the
time the Analyzer polls it, after the sample loop. With the sample clock
fixed (#26) onsets read ~38 ms before that anchor on piezo, while onset
minus the planned trigger was +63 ms: the CHIRP command itself left ~60 ms
late in detail mode because the previous trial's report printed first
(ANA-004). On D-AMP dt also contains the ~32 ms TX queue (#19 latency).
Fix: anchor dt on the time the CHIRP command was sent (or timestamp the
marker on receipt); until then compare dt only within one firmware and
one board type.
Source: docs/refactors/archive/i2s-sample-clock.md "Side findings".
```

### ANA-008 - RAW capture memory limits

Status: DONE 2026-10-22 (step 7a; found 2026-10-09, #24): the capture buffer is sized to pre + post rows, and `RAW_ALLOC` reports the rows, the bytes and the largest free block. Still open: the pre-trigger audio is held twice (ring plus capture buffer), so pre=500 fails for memory, now with a clear RAW_ERR/RAW_MEM.

```text
RAW allocates a fixed 72 KB capture buffer plus the pre-trigger ring
against a ~110 KB largest heap block: pre=500 and pre=300 fail, and
pre=150 returned only 256 pre samples. Workaround: pre=0 post=350 and
analyse the tail; decim=N for multi-second captures (a3c717d). Fix: size
the buffer from the request (post + pre) and report what was actually
allocated, or capture pre-trigger audio from the sample clock's ring
instead of a second buffer.
Source: docs/refactors/archive/i2s-first-difference-revisit.md section 7.
```

### ANA-009 - invalid inspection windows print available_start/end = 0

Status: DONE 2026-10-22 (step 7a; found 2026-10-09, #26): `available_start/end_ms=na` when the window held no values.

```text
SEQ_INSPECT prints inspect.available_start_ms=0 available_end_ms=0 for
any window that is not valid (incomplete history edge, future window,
missing stream), because the inspector fills the available range only for
a valid window. It reads as "empty history" and misled two analyses (#26,
and the first DET-010 write-up). Fix: print the range the history did
hold (or "na") and keep inspect.reject_reason as the cause.
Source: docs/refactors/archive/i2s-sample-clock.md "Symptom".
```

### ANA-002 - multi-occurrence pattern proposals

Status: DEFERRED

```text
Allow patterns made from groups of occurrences, not only one occurrence at a
time.
Keep competing hypotheses private to OccurrenceEvaluator.
Keep OccurrenceVerdict compact and behavior-facing.
Expose only compact explanation facts through OccurrenceEvaluatorReport.
```

## Current focus

```text
Order: roadmap-0-steps.md. DET-008 (the cleanup pass) is steps 1, 8, 9;
DET-007 is step 5 (decided on D-AMP); ANA-003 is step 10. The rest is
unsequenced.
```

## Spec candidates

```text
DetectorReport is the detector-stage truth.
OccurrenceEvaluator is the public pattern-stage boundary.
AnalyzerReport stays on canonical trial classification plus scoped details.
Clean analyzer output should not read retired legacy diagnostics.
```

## Non-goals now

```text
Threshold retuning.
Behavior rewrite.
Output rewrite.
New command system.
Pattern helper types as public architecture boundaries.
```
