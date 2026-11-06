# Roadmap Changelog

Status: archive log.
Scope: landed roadmap items moved out of the active roadmaps.
Purpose: keep the active roadmaps future-focused while preserving the landed
history in one place.

---

## Detection / Analyzer

```text
[LANDED] DetectionRuntime coordinates the current detection chain.
[LANDED] DetectorReport is the active detector-stage report contract.
[LANDED] RejectedCandidateSummary is the selected reject contract.
[LANDED] PatternMatcher is the public pattern-stage boundary.
[LANDED] PatternMatcherReport is available for compact pattern facts.
[LANDED] OccurrenceInspector is available for inspection evidence.
[LANDED] FieldStateTracker is available for acoustic context reporting.
[LANDED] Clean analyzer reporting exists with SEQ_TRIAL / SEQ_SOURCE / SEQ_INSPECT / SEQ_EXPLAIN / SEQ_SUMMARY.
[LANDED] DetectionDiagnostics and analyzer legacy compatibility were removed from src.
[LANDED] Analyzer app/reporting exists.
```

### DET-001 - detector / report consistency

Status: CLOSED as moot 2026-10-22 (step 8b, docs/refactors/archive/step8b-detection-followups.md): FrequencyMatch removed in 7d;
ScalarTransientDetector freezes its report on accept and on reject. Was: PARTIAL.

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

Status: DONE 2026-10-21 as the hop train (step 5b): 25 ms x 5, gap 10, read as one occurrence and shipped as the network signal (bench-run 2026-10-15/16 and on a second day, 2026-10-21; decisions/2026-10-23-node-hears-the-hop-train-only.md; DET-013, DET-015 carry the band and the shape). Open: the gap contract below, the dry cell in step 15 (audit 2.3). Was (2026-10-14): IDEA, source ready, bench run pending (research note scalar-detector-extensions.md section 1).

10-15 ms pulses with 5-115 ms pauses accepted as one occurrence via maxGapMs / maxTransientDurationMs. Measure the smallest resolvable gap and the false-alarm cost.

Gap contract (step 5h, #47 closed, from audit 2.3): the shipped hop train (25 ms x 5, gap 10) is one occurrence only because the room tail fills the gaps (min duration 85, release debounce 10, max gap 20); `output.hop_gap_ms` and the Scalar gap / debounce / min-duration params are independent with no consistency check. 5h adds the check and a dry hop fixture; whether the room tail is part of the signal contract is decided on that fixture and a dry bench cell.

Source: Emitter `PULSES n pulse_ms gap_ms [freq=] [ramp=] [trial=]`, from the Analyzer as `EMIT PULSES ...` or per SEQ trial as `SEQ start dur=PULSE_MS pulses=N gap=MS [ramp=MS]`. On D-AMP the train is timed per sample in I2sToneOutput (loop edges land on 8 ms blocks); pulse_ms is sound with the ramps inside, gap_ms is silence. Piezo builds time it from the loop.

### DET-013 - band stream: the detector watches the network band

Status: DONE 2026-10-21 (step 5b's gate met: hop >= 9/10 at 60-110 cm on two days, the 3200 Hz chirp unchanged
at 90 cm, speech / music OBS on the Node build, no dropped buffers; the Node runs the 5-bin bank since). Not built,
not sequenced: 2 (single pass) and 5 (Hann, with OUT-006). Was: in progress (step 5b). Done: 1 (14.7 us/sample, 2026-10-15), 3 (bank in `FreqBandStream`, Analyzer
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

### DET-015 - shape inspectors: peak trajectory, band coverage, pulse structure

Status: DONE 2026-10-26 (step 11b): BinTrack + BinShape inspector, the shipped plan requires shape (>= 3 band bins,
2 steps up); decisions/2026-10-26-det015-bin-track-inspector.md, E002. Not built: peak trajectory within a bin
(steady vs sweep, not separable here), band coverage beyond distinct bins.

Facts per occurrence: steady / sweep / hop, span, direction, distinct bins, pulse and gap regularity. Reject random jumping (speech formants) and syllable-like trains; give Behavior shape facts for BEH-007. Data-path decision (detector accumulation vs a stream) gets a `docs/decisions/` file.

Research: docs/research/frequency-drift-detection.md (Inspectors that help)

### DET-017 - min-duration tuning pass, then merge the deferred inspection

Status: DONE 2026-10-23 (bbb5728, e58c4c2; bench/sessions/2026-10-23-det017-min92). Min duration 85 -> 92 ms
(owner), with inspection waiting for a complete window. The 40 ms beep + reflection is rejected at the duration gate
(10/10). Every single-null hop train survives on the host (33/33, shortest 97 ms). Beeps of 60 ms and more and the
plain chirp are heard: hop-only needs DET-015 (decisions/2026-10-23-node-hears-the-hop-train-only.md).

Former status: TODO (owner, 2026-10-22: park the step 8b item 1 fix until this pass)

The fix for audit 3.1 (inspect when the window is complete) is on branch
`step8b-defer-inspection` (5d2c0fa). With it, a 40 ms in-band tone plus a room
reflection (read 86 ms) is accepted, because the 85 ms min duration is then the
only gate against it (bench/sessions/2026-10-22-7d-dur40-raw). A logged tuning
pass (tools/logging LOG-001 workflow) picks the min duration from bench and
replay data, and the fix merges with that value. Readings at 110 cm on
2026-10-22: hop train 195-214 ms, 100 ms chirp 116-154 ms, 40 ms tone 86 ms.
Open: whether the Node must still hear the plain 100 ms chirp (min ~110 ms keeps
it) or only the hop train (min ~130-150 ms). Cells: 40/60/80 ms tones, chirp,
hop train, speech/music OBS, and the replay corpus.

### DET-020 - the band bank's update interval (CPU)

Status: DONE (2026-11-01, 02e6a24; decisions/2026-11-01-band-update-16.md; soak bench/sessions/2026-11-01-step22.4-soak8)

```text
FreqBandStream recomputes the band bank every 4 samples (0.25 ms) over a 64-sample (4 ms) window: each update
overlaps the last by 94 %. One compute on the Node's 8-bin band (10 Goertzel bins with the guards) costs ~108 us
on the board, so at 16 kHz the bank takes ~43 % of the CPU (bench/sessions/2026-10-31-band-update-board, the
Analyzer's profiling build: 108 us at N = 4 / 16 / 32, hop10 10 / 10 at each). The host replay keeps every verdict
at N = 8 / 16 / 32 (bench/sessions/2026-10-31-band-update-replay: strength within 7 %, one duration by 15 ms).
Plan: 1. measure the Node's loop with the bank at 4 and 16 (a profiling Node build); 2. N = 16 as the default
(FreqBandStream, the Analyzer's SeqOutputConfig), T2h re-recorded and its diff explained; 3. the edge cases on
the board: the margin ladder (-6 / -7.5 dB, two hops lost), the chord cell, the music file; 4. a soak.
Why now: before Whispernet (24 kHz detection per block) and ten nodes ask for the loop; the largest CPU item known.
```

### DET-008 - detection-layer cleanup pass

Status: PLAN ARCHIVED 2026-10-25 (docs/refactors/archive/cleanup-0-plan.md, see its Archived line). Open parts:
Phases 5d + 6 are step 9 (deferred until a second family), 7a + 7b are DET-018, step 8's Part B is DET-016.

```text
Executed in docs/refactors/archive/cleanup-0-plan.md (phase order, T1-T8 test
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

Status: DONE 2026-10-23 on Scalar (DET-017: inspection waits for a complete window, min 92 ms). Freq case REMOVED
2026-10-22 with FrequencyMatch (step 7d,
decisions/2026-10-22-remove-frequencymatch.md). Open on Scalar as audit 3.1:
occurrences of 85-89 ms are inspected before the +100 ms Start-anchored
window closes; measured with a 40 ms tone plus a room reflection reading
85-87 ms (bench/sessions/2026-10-22-7d-dur40-raw). Fixed on branch
step8b-defer-inspection; merges with DET-017 (step 8b moved it there).
(found 2026-10-09, #20)

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

### ANA-003 - move AnalyzerApp out of the detection tree

Status: DONE 2026-10-22 (step 10 item 8). The 9 member files are in src/modes/analyzer/ (and tools/); two
stub headers that nothing included are deleted. `nextOccurrenceId` moved to detection/occurrences/OccurrenceIds.h,
so the scalar detector no longer includes AnalyzerPassRules.h or, through it, the Analyzer report types.
include_direction.py is `--strict` in CI (0 hits). Analyzer smoke on E2: 10/10 chirps.

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

Status: DONE 2026-10-22 (step 7a; found 2026-10-09, #24): the capture buffer is sized to pre + post rows, and `RAW_ALLOC` reports the rows, the bytes and the largest free block. The pre ring is held once since 2026-10-29 (12.4.10, b9f19f6: the front rows of the one capture buffer, rotated at the start), and the rows are sized by the largest free block (915f564, `max_rows=`).

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

## Behavior / Output

```text
[LANDED] ResonantBehavior owns the current behavior decision path.
[LANDED] ChirpOutput remains the current output path.
[LANDED] Behavior consumes PatternResult and FieldState.
[LANDED] ResonantBehavior exists.
```

### BEH-005 - per-node signal variation

Status: DONE 2026-10-21 (step 5b's gate: per-node rotation on every board; five distinct rotations from the
roster, step 5g; BEH-009 moves the start to a base bin per Node, step 17). Was: in code (step 5b, 155211e),
Node battery pending. Behavior reports `SignalShape` (NetworkSignal for a
heard-pattern response, Chirp for idle/self-test); the Node plays the hop train over the band bins starting
on `output.hop_rotation`, from the MAC at boot. Shape and order are static per node; variation over time is BEH-006.

```text
Each node plays its own shape and centre inside the network band
(e.g. derived from the node id, or a param): hop set or sweep.
Static per node; enough for the field trial to test nodes hearing
signals that differ from their own.
```

### BEH-008 - answers that follow the field: the level automaton

Status: LANDED (2026-10-31, step 16: host-tested, native-fieldsim, the desk with five Nodes; decided 2026-10-29)
Decision: docs/decisions/2026-10-29-answers-follow-the-field-level-automaton.md
Research: docs/research/field-activity-response.md (the automaton, its values, the set-aside mechanisms,
the desk model tools/sim/fieldramp.py)

```text
One integer level per node (+1 heard valid pattern, +1 own emission, -1 per decay_ms of silence);
p = (level + 1) / L; a heard pattern is answered with probability p after the fixed wait plus a
fixed random jitter; a verdict while an answer is pending drops it with probability 1 - p (yield);
at L the node rests and the level returns to zero. Four behavior.* params: L, decay, jitter, rest.
Behavior counts the level from the verdicts; FieldState is no longer the rest gate.
```

Order inside (host first): `test_behavior_level` (the rules, the yield, the rest, the shipped timing reproduced
with jitter 0 and p forced to 1); `native-fieldsim` (N real Behaviors through a fake channel with the measured
hearing matrix, nodegroup.py's metrics plus the interval and chord profile along each chain); the hearing matrix
in the tryout room at 1 m spacing (tools/bench/hearmatrix.py, strict and lenient cells from the same logs) to set
L and decide the lenient grade; five nodes free on the bench, step 6's protocol, compared against
2026-10-22-step6-trial and 2026-10-26-soak24-shape-b. Gate: every chain ends (3 s silence within 30 s in 80 % of
starts, no minute over M = 100); chords under 25 % of rounds in a chain's first half and above it in its last;
the first answer to a call later than 1 s in most chains; speech and music neither start nor lengthen chains.
Moves the field baseline: its own field session is the gate, as 8a was told to.

### OUT-002 - minimal OutputStatus

Status: DONE 2026-10-23 (step 10f; decisions/2026-10-23-soundoutput-executes-node-composes.md). `OutputStatus` in
src/output/SoundOutput.h: phase, started / done / rejected, last reject, last start / end, span; RB STATUS
`output.*`. The original note:

```text
Add OutputStatus only when behavior/status needs it.
Fields may include busy, lastStartMs, lastDoneMs, currentPattern.
```

### OUT-003 - OutputRequest

Status: DONE 2026-10-23, with one difference to the note below: the Node composes the request from Behavior's
decision and the output.* params (the hop grid comes from the detection band, which Behavior does not know).
`SoundOutput` executes it, sequences the idle call and checks `OutputLimits`. Host test test_sound_output.
The original note:

```text
Behavior emits OutputRequest / BehaviorAction.
Output executes request and validates physical limits.
```

### OUT-005 - chirp shapes: hopping pulses and in-chirp sweep

Status: DONE 2026-10-21 (step 5b's gate: the Node plays its hop train on every board; the sweep plays on the
Emitter and is accepted by shape since DET-015, 11b). Was: in code (step 5b), bench pending. Hop: Emitter `PULSES ... hop=` (2026-10-15, bench-checked),
Node hop train from Behavior's `SignalShape::NetworkSignal` (155211e, compile only). Sweep: `CHIRP sweep=END_HZ`
on the Emitter and `SEQ start sweep=` (9664032, compile only; `I2sToneOutput::setSweep`, phase-continuous).
Design rule from the 2026-10-16 battery: hop steps on the 250 Hz band-bank bins, gaps <= 10 ms, 25 ms x 5.
Sweep on hardware 2026-10-16 (bench/sessions/2026-10-16-sweep-70cm): detected as one occurrence, rejected by the
contrast inspector in 30 of 30 trials (P75 29,000-30,500 while gliding between bins). With the current contrast the
detection-layer shape is the hop; the sweep stays an aesthetic-layer / DET-015 option.

```text
PULSES takes a frequency per pulse (hop train); a tone can ramp its
frequency inside one chirp, phase-continuous (sweep: start/end Hz).
Emitter first, as SEQ trial options, so the Analyzer can drive them;
then ChirpOutput on the Node, with the shape chosen by Behavior (BEH-005).
No OutputRequest needed: parameters on the current ChirpOutput path.
Every shape stays inside the network band
(docs/decisions/2026-10-15-signal-variation-inside-a-shared-band.md).
Source: docs/research/frequency-drift-detection.md (Sequence, step 2).
```

## Params / Config

```text
[LANDED] Hardcoded DetectionProfile defaults exist.
[LANDED] BehaviorGateConfig defaults exist.
[LANDED] DetectionProfile exists.
```

### PAR-001 - Analyzer tuning surface

Status: LANDED

```text
Create AnalyzerTuning as the first module-owned runtime tuning surface.
Move selected hardcoded Analyzer values into AnalyzerTuning while preserving
current behavior.

This is not yet a generic param system. The goal is to create stable fields
that Analyzer runtime code can read normally. Analyzer must not depend on
ParamRegistry lookup during runtime logic.

Good first values are timing windows, trial count / period values, and compact
debug-output switches if they are currently hardcoded and safe to change live.

Scalar profile tuning parameters are printed in the sequence header and can be
updated through the analyzer PARAM surface.
```

### PAR-002 - minimal Param core

Status: LANDED

```text
Add the minimal central Param vocabulary:
ParamId, ModuleId, ParamType, ParamBinding, ParamRegistry.

Use a flat binding list. Dotted paths are public names only, not a runtime
tree. The registry must support typed add helpers so raw void* stays inside
registry internals.

The registry must reject duplicate ParamIds and duplicate paths. It should
also guard against null field pointers, invalid ranges, and capacity overflow.

Do not add persistence metadata yet.
Do not add storageKey yet.
Do not add ParamStore yet.

(2026-10-30: the code now lives in lib/ParamRegistry/ (PAR-016, 89e723e2), the ids in src/app/ParamIds.h, and
duplicate ids are rejected.) Landed in src/param/ParamTypes.h and src/param/ParamRegistry.h/.cpp as a
fixed-capacity (kMaxParams), no-dynamic-allocation table. addUInt16 /
addUInt32 / addFloat / addBool reject a null field pointer and a duplicate
path; capacity overflow is rejected once kMaxParams bindings are registered.
ParamId duplicate rejection is not enforced by the registry itself yet (paths
are checked; ParamId values are still assigned by hand per module).
```

### PAR-003 - Analyzer param registration

Status: DONE 2026-10-26, step 11e item 9 (docs/refactors/archive/review-fixes-2026-10-25.md): the Analyzer's 13
`detection.scalar_*` knobs bind through `src/app/ParamBindings.h` (`bindScalarTransientParams`, since step 11i N7
`param::describeParams` in `detection/detectors/scalar/ScalarTransientParams.h`, shared with
the Node and the host replay) into its own `ParamRegistry`, and `PARAM LIST | GET | SET | DUMP | RESET` run
through the shared handler `src/app/ParamCommands` (no store; SAVE / LOAD answer `status=no_store`). The
key=value parser and `PARAM STATUS` are gone (decisions/2026-10-25-analyzer-params-on-registry-and-line-
policy.md). The observed stream is the profile's, not a param. The `analyzer.*` paths sketched below (trial
window, count, period) were not registered: SEQ start options carry them per run; register them when a
run needs to be reproduced from `PARAM DUMP` alone.

```text
Add registerAnalyzerParams(...).

Analyzer registers its own params by binding fields from AnalyzerTuning into
the central registry. The Analyzer module owns the meaning of the params. The
ParamRegistry owns lookup, parsing, validation, and field writing.

The registration should use clear public paths such as:

analyzer.window_start_ms
analyzer.window_end_ms
analyzer.trial_count
analyzer.trial_period_ms
analyzer.print_inspect

Exact names may change, but they should be stable, grep-able, and module
scoped.
```

### PAR-004 - Serial PARAM command surface

Status: LANDED

```text
Add the first transport frontend: Serial PARAM commands.

Required commands:

PARAM LIST
PARAM GET <path>
PARAM SET <path> <value>
PARAM DUMP

Output should be boring and grep-able. PARAM SET should report status, path,
old value, new value, module, and error reason if any.

Expected errors:

unknown_param
parse_error
type_mismatch
out_of_range
readonly_or_not_writable later if needed

Landed in Node::handleParamCommand (src/modes/resonant/ResonantNodeApp.cpp),
kept separate from the existing RB PARAM / RB BEHAV lines per the "current
focus" note below. type_mismatch and readonly_or_not_writable are not
reachable yet: every bound field is writable and SET always parses against
the binding's own type, so the only rejects seen today are unknown_param,
parse_error, and out_of_range.
```

### PAR-005 - dirty ModuleId apply route

Status: LANDED

```text
After a successful PARAM SET, the registry writes the bound field and returns
the changed ModuleId. Node or a small ParamApplier then routes the change to
the owning module.

For the first pass this only applies Analyzer:

ModuleId::Analyzer
→ analyzer.applyTuning(analyzerTuning)

This apply route is required even if the first Analyzer values are read
directly. It keeps the architecture ready for params that affect derived
configs, prepared thresholds, hardware setup, buffers, profile state, or
multi-module behavior later.

Landed as Node::applyParamModule, dispatching on the ModuleId a successful
PARAM SET returns. The first pass applies Detection instead of Analyzer
(ModuleId::Detection -> Node::applyActiveDetectionProfile), since Detection
already had ad hoc runtime-tunable fields via RB PARAM; ModuleId::Node,
ModuleId::Behavior, and ModuleId::Output are switched on but currently
no-ops until PAR-003/PAR-006/PAR-008 register params for them.
```

### PAR-006 - Behavior params

Status: DONE 2026-10-22 (step 10 item 4): 13 `behavior.*` params, one per BehaviorGateConfig field, bound into
the Node's `_activeBehaviorProfile` and applied through `ResonantBehavior::configure()` (setters only).
kMaxParams 48. The key map from the old RB BEHAV keys is in decisions/2026-10-22-live-params-profile-reset-no-rb-behav.md.

```text
Extend the same pattern to Behavior.

Behavior owns BehaviorTuning. Behavior registers local params under the
behavior namespace. Runtime code reads normal tuning fields. The ParamRegistry
only validates and writes bound fields.

Likely future paths (one per BehaviorGateConfig field, matching the RB
BEHAV keys already in bench session headers, or carried by an alias table
in the replay tooling; never renamed afterwards):

behavior.wait_after_heard_ms
behavior.refractory_after_emit_ms
behavior.rest_after_field_patterns

Not here: the emission's shape and length are output.* since BEH-005.

The apply route is ResonantBehavior::configure() (setters only, no state
reset), fed from a Node-owned _activeBehaviorProfile the params bind into;
kMaxParams grows from 32. Behavior remains responsible for what these
values mean. Sequenced as step 10a; shape and rules in
decisions/2026-10-17-behavior-contract-and-reproducible-sessions.md.
```

### PAR-007 - Detection / TonalPulse params

Status: DONE 2026-10-22 (step 10 item 7). Registered on the Node: 13 `detection.scalar_*`, `detection.band_low_hz`
and `band_high_hz`, and the inspectors' medium limits `detection.contrast_medium` / `detection.amp_medium`
(written to the plan's module with the matching target). All apply live, with no detector reset and no history
wipe; the plan's streams are rebound only when they change. The own-emit suppression values are
`behavior.*` (PAR-006). Not registered: `scalar_observed_stream` (an enum the Node never changes; the Analyzer's
PARAM has it). The FrequencyMatch text below is history (Freq was removed in 7d).

```text
Extend the same pattern to Detection / TonalPulse.

Detection and/or TonalPulse owns typed tuning/config structs. Params should
not become Node glue. The registry only exposes and writes values.

Likely future paths:

detection.tonalpulse.freq_score_min
detection.tonalpulse.freq_contrast_min
detection.tonalpulse.require_amp_support
detection.own_emit_suppress_ms

Some of these values may require a stronger apply step because they can affect
PatternRulesConfig, inspector thresholds, runtime gates, or Analyzer-visible
profile state.

First slice landed: Node::registerDetectionParams binds the four
FrequencyMatchConfig thresholds already exposed by the ad hoc RB PARAM
command onto the registry under detection.freq_attack_score_min,
detection.freq_release_score_min, detection.freq_attack_contrast_min,
detection.freq_release_contrast_min. This intentionally does not yet cover
ScalarTransientConfig, require_amp_support, own_emit_suppress_ms, or a
TonalPulse-specific path prefix; those stay DEFERRED until a real need shows
up, to keep this a small addition rather than a full Detection param sweep.
```

### PAR-010 - persistence backend

Status: DONE 2026-10-23 (decisions/2026-10-23-param-persistence-save-only.md; bench/sessions/2026-10-23-par010-
persistence). As built: no per-param flag (every registered param persists, owner) and no storageKey field: the
NVS key is `p<id>` from the stable ParamId, in namespace `params` (app/NvsParamStore.h). `defaultValue` is
`ParamBinding::defaultBits`, captured at boot by `captureDefaults()`. Each entry stores the default it was saved
against; boot warns `PARAM stale` when the firmware default has moved, and the stored value wins. The registry
takes a `ParamStore` interface, so lib/ParamRegistry stays app-agnostic and is host-tested with a memory store.
The original plan:

```text
Only after runtime params are stable, add persistence.

Extend ParamBinding with:

storageKey
defaultValue
persistent flag
liveApply / apply policy if still needed

Use ESP32 NVS / Preferences as the storage backend. Public paths remain long
and readable. NVS storage keys must be short.

Boot flow:

register params
initialize defaults in bound fields
load valid stored values
write loaded values into bound fields
mark dirty modules
applyAll once

Invalid or missing stored values keep defaults and log clearly.
```

### PAR-011 - PARAM SAVE / LOAD / RESET

Status: DONE 2026-10-23, on the Node (same sources as PAR-010). SAVE skips entries the store already holds as is.
The Analyzer has no registry and no persistence.
The original plan:

```text
Add durable commands only after the persistence backend exists.

PARAM SAVE writes all persistent registered params from bound fields to NVS.
PARAM LOAD reloads valid stored values into bound fields and applies modules.
PARAM RESET clears stored param values and returns to defaults.

Do not rely on runtime params for a multi-node workflow until SAVE / LOAD /
RESET and verification output exist.
```

### PAR-013 - remote param update

Status: DONE 2026-10-26 (step 11a): ESP-NOW transport through the serial command path, on all five boards; 12 h
with the radio on at every pass mark (decisions/2026-10-25-remote-params-espnow.md). Interim: VEKTOR replaces it
(decisions/2026-10-25-vektor-replaces-net.md). Firmware OTA landed separately, as the note asks: step 11d
(2026-10-29, decisions/2026-10-28-ota-firmware-pull-over-http.md; a shared token since 12.0.1). The original note:

```text
Add remote transports only after Serial and persistence are stable.

WiFi, ESP-NOW, and VEKTOR must route into the same command/control path as
Serial. There must be no transport-specific param store and no transport-owned
validation logic.

Remote Param Update changes params.
Firmware OTA / FWOTA changes firmware.
Keep these separate.
```

### PAR-015 - retire RB PARAM / RB BEHAV onto ParamRegistry

Status: DONE 2026-10-22: RB PARAM went with FrequencyMatch (7d); RB BEHAV removed in step 10 (owner D3, not
aliased). Both answer with an ERR naming the PARAM path. The bench tools use PARAM SET / PARAM DUMP behavior.*.

```text
Two command paths currently set the same knobs: the ad hoc RB PARAM /
RB BEHAV token parser in ResonantNodeApp.cpp (no range validation) and the
registry-backed PARAM SET (validated). Register the remaining RB PARAM /
RB BEHAV fields on the registry (the behavior ones are PAR-006), then remove
the ad hoc parser, or reduce RB PARAM / RB BEHAV to aliases that call
ParamRegistry::applyValue.
Sequenced in roadmap-0-steps.md step 10.
```

### PAR-016 - ParamRegistry as an in-repo library

Status: DONE 2026-10-22 (step 10 item 3). `lib/ParamRegistry/` (library.json, 0.2.0) has no app vocabulary: an
opaque uint16 id and uint8 group tag per binding, the group names supplied by the app (`setGroupNamer`), and
templated add*() overloads taking the app's enums. `src/app/ParamIds.h` holds ModuleId and ParamId. It rejects a
duplicate id; unsigned values reject a sign ("-1" wrapped before) and overflow, with integer bounds (no float
round-trip). There is a host test (test/test_param_registry, native-test-behavior). CI fails on any include of
src/ from the library (tools/ci/include_direction.py). PARAM LIST/DUMP output is unchanged.

```text
Decision (2026-09-23): do NOT split ParamRegistry into its own repo yet.
Make it a PlatformIO library inside this repo first, so a later split is a
mechanical move instead of a redesign.

Why not a separate repo now:
- One consumer (Node build), four registered params, one commit (052b03e).
- ParamBinding is not stable: PAR-010 adds storageKey / defaultValue /
  persistent flag / apply policy. Each of those would be a cross-repo
  release + version bump.
- Not generic yet: ParamTypes.h hardcodes ResonantNode's ModuleId
  (Node/Detection/Behavior/Output) and ParamId (Detection_*) enums.
- roadmap-vektor-later.md: VEKTOR must not drive premature local
  architecture; VEK-002 is still DEFERRED.
- Cost with no benefit until a second consumer exists: two repos, pinned
  tags, two PRs per change, both repos attached per agent session.

Steps:
- Move src/param/ to lib/ParamRegistry/ with a library.json, so it cannot
  silently include app headers.
- Decouple app vocabulary: ModuleId becomes an opaque uint8_t group tag
  with the name supplied by the app; ParamId values live in the app.
- Enforce duplicate-ParamId rejection (open note on PAR-002).
- Harden parsing before any reuse: reject trailing garbage ("12abc"),
  reject negative input for unsigned types (strtoul wraps "-1"), and do
  uint32 range checks without a float round-trip (loses precision > 2^24).
- Optional: put list()/dump() output behind a small interface so the core
  can be host-unit-tested without Arduino Print.

Verification: all three envs compile; no behavior change on PARAM
LIST / GET / SET / DUMP beyond the stricter parse rejects.
Sequenced in roadmap-0-steps.md step 10, before PAR-015.
```

## Node / Runtime

```text
[LANDED] FieldStateTracker exists.
[LANDED] ChirpOutput / current output path exists.
```

### NODE-001 - 5-node status baseline

Status: DONE 2026-10-22 (step 6): RB STATUS from all five nodes every 120 s through two 30 min runs, with `# state`
headers (bench/sessions/2026-10-22-step6-trial).

```text
Make STATUS report firmware and build label if available.
Show active detection profile and current detection thresholds.
Show current inspection support source and minimum strength.
Show behavior enabled or mode-like state using current ResonantBehavior fields.
Show output busy or recent state if already cheap.
```

### NODE-002 - boundary map

Status: DONE 2026-10-22: docs/refactors/archive/step10-boundary-map.md (step 10a).

```text
Document which current code paths belong to Detection, Behavior, Output,
Analyzer, and Node glue.
Identify command and status code that can later move to CommandRouter and
registries.
```

### NODE-003 - hardcoded config workflow

Status: DONE 2026-10-21, with step 4: the same firmware (Node ecfc72b) runs on all five nodes, with the defaults
in DetectionProfile, BehaviorGateConfig and output. Per-board data is only app/NodeRoster.h (name, hop rotation).

```text
Keep hardcoded defaults as the primary 5-node workflow.
2026-10-17: for Behavior that is the shipped BehaviorGateConfig, on record
in every session's log header (RB BEHAV + PARAM DUMP, `# state` lines);
decisions/2026-10-17-behavior-contract-and-reproducible-sessions.md.
Ensure hardcoded values live with the correct owner:
DetectionProfile, BehaviorGateConfig, output config.
Upload the same firmware to all test nodes.
```

### NODE-004 - node glue cleanup

Research: docs/research/node-structure.md (proposed Node classes and files; its A7 cut became step 11i, done 2026-10-29; the later seams are still proposals).

Status: DONE 2026-10-29 (step 11i, last paragraph). Was PARTIAL 2026-10-22 (step 10 item 9): RB STATUS and the status prints are in ResonantNodeStatus.cpp;
the emission logic is out of update() (startEmission / updateEmissions); SelfTest uses the one detection reset.
Open: update() and handleSerialLine() are still long; split further when an owner module has a home for it.
2026-10-25 (owner: split by role): ResonantNodeApp.cpp is the core (boot, loop, pipeline, baseline; 1,881 -> 956
lines); configuration in ResonantNodeParams.cpp, command dispatch in ResonantNodeCommands.cpp, the radio in
ResonantNodeNet.cpp, diagnostics in Status + SelfTest; shared helpers in ResonantNodeInternal.h. A file split, not
separate classes: a CommandRouter that serial, NET and VEKTOR feed is NODE-005, with VEKTOR.
Review 2026-10-25 (docs/research/architecture-readability-review-2026-10-25.md A7): the next cut is state per
file (NodeParams owning the registry, NetGateway owning reply / join state, the baseline machine as a struct);
not in step 11e, which only shares helpers (LineReader, ParamCommands) with the Node.
DONE 2026-10-29 (step 11i, issue #71 closed; docs/refactors/archive/node-split.md): N1-N8 gave the Node parts that
own their state (NodeCounters, NodeBaseline, NetGateway, NodeSelfTest, NodeSignalComposer, NodeParams), a
BehaviorStatus snapshot and one command table; bench gate on f2330b1 (bench/sessions/2026-10-29-step11i-nodecheck).
Still open: the replay binds the medium limits itself (A10; "Review leftovers"); CommandRouter stays NODE-005.

```text
Move incidental command parsing and status formatting out of Node only when
the owner module already has a clean home for it.
Do not build a registry before the ownership map is clear.
```

### NODE-007 - CI build matrix and include-direction check

Status: DONE 2026-10-22 (step 7). `.github/workflows/build.yml` runs on every push and PR:
- the eight ESP envs as a matrix;
- `native-replay`, `native-test` and `native-test-behavior` on ubuntu-24.04 (g++ 13; the three `= {}` assignments
  became `InspectionPlan{}`);
- `tools/ci/include_direction.py`: warned on 9 Analyzer files under src/detection/ that included src/modes/;
  `--strict` in CI since ANA-003 (step 10, 2026-10-22), with 0 hits. It also fails on any include of src/ from
  lib/ParamRegistry/.

It becomes `--strict` after ANA-003. The first run was all green: run 39721937551.

```text
GitHub Actions job: `pio run` for every env in platformio.ini, plus
`pio test -e native` once NODE-006 exists.
Known host-compiler difference (2026-10-16, cloud container): g++ 13.3
(Ubuntu 24.04) rejects `profile.inspectionPlan = {};` in
DetectionProfile.h ("no match for operator=" with a brace list on the
aggregate), clang++ and the WinLibs GCC on the Windows machine accept it.
Pin the CI host compiler to clang or make the three assignments
`= detection::InspectionPlan{}` before the native env runs in CI.
Add a check that nothing under src/detection/ includes src/modes/ (fails
today until ANA-003 lands; add it as a warning first).
Must exist before cleanup-0-plan Phase 5d grows the env matrix.
```

### NODE-008 - 5-node field trial

Status: DONE 2026-10-22 (step 6, bench/sessions/2026-10-22-step6-trial): the first trial passed on the bench in the
owner's layout. Next cells (other rooms, outdoors, long runs) are NODE-012.

```text
Depends on NODE-001.
Run five nodes with the same firmware in one room; record STATUS from each
and a session log.
Questions it has to answer: do nodes detect each other at installation
distances, does own-emit suppression hold with several emitters, does the
network settle or run away.
This is the first check of the product behavior rather than single-node
detection.
Prior observation: docs/lab/notes_lab.md 2026-05-26, "stable 5-node slow
circle" on piezo with the profile/gate values listed there (pre-refactor
firmware, no bench session, no STATUS lines).
```

### NODE-009 - D-AMP board support in firmware

Status: LANDED 2026-10-09 (issue #19 closed, 17295c9; docs/refactors/archive/damp-board-support.md)

```text
Decision: docs/decisions/2026-10-06-damp-output-hardware.md.
Pins: move the hardcoded pins in src/app/main.cpp into build macros, one
set per board.
Board variant: D-AMP is the default (plain env names); piezo is
BOARD_PIEZO in esp32dev-piezo* envs, kept compiling as the fallback
(decisions/2026-10-09-discontinue-piezo.md).
D-AMP pinout (from oysterboy/echoSpace): 25 = I2S WS and 26 = BCLK, shared
by mic and amp; 32 = amp DIN; 33 = mic data. UART2 16/17 stays free.
HAL: one class owning I2S_NUM_0 full-duplex (RX+TX), providing both
AudioSource (mic) and ToneOutput (sine, short on/off ramp). The amp can't be
a separate output: it shares BCLK/WS with the mic.
Keep 16 kHz. TX never blocks the detection loop; silence when idle. Pick the
mic channel explicitly (piezo build reads ONLY_RIGHT; echoSpace reads
stereo with the mic on channel 0).
```

### NODE-010 - D-AMP bench check

Status: LANDED 2026-10-11 (issue #20 closed, docs/refactors/archive/damp-bench-check.md)

```text
Measures the D-AMP signal; no live piezo A/B (piezo discontinued,
decisions/2026-10-09-discontinue-piezo.md). One emitter, one listener,
ladder 10/40/70 cm (the distances with piezo records; owner 2026-10-10),
D-AMP emitter at 0.3 FS; accept rate and score/contrast from the
Analyzer, compared against the piezo SEQ runs already in bench/. Plus
self-echo against the suppression window and class-D noise at the mic
with the amp idle (explicit RAW run, pass doc R5).
If D-AMP is a dud, go back to piezo (BOARD_PIEZO) and revisit.
Boards: E2 + E3 from 2026-10-10 (E1 failed the amp check, in storage);
every session starts and ends with the amp check (pass doc AC).
Results: 2026-10-10/11 - E3 -> E2, 0.3 FS, stock TonalPulseScalar: 50/50 at
10 / 40 / 70 cm (piezo 50 / 10 / 48), 0 false positives in 150 silent
windows, 0 dropped buffers; Node on E2: quiet boot OK, 0 own-echo verdicts
in 12 idle chirps; idle class-D amp does not raise the mic floor; stock
Scalar answers speech 2/50 and music 6/50 silent windows (R4). Sessions:
bench sessions/2026-10-10-issue20-damp-*, 2026-10-11-selftest-r5-*.
```

### NODE-011 - switch to D-AMP

Status: DONE 2026-10-21 (issue #21): five D-AMP nodes on one build, each SELFTEST PASS with the others silent
(bench/sessions/2026-10-21-selftest-all5).

```text
All five D-AMP nodes on one build. (D-AMP as the default build and the
BOARD_PIEZO fallback envs land in NODE-009.) Update implementation-status.md, CLAUDE.md build commands, and
myspec.md section 7 if its output list changes.
Open 2026-10-10: E1 (24:dc:c3:4a:b0:50) is faulty (amp/speaker silent,
in storage), so five nodes need E1 repaired or another D-AMP board.
Every node passes the amp check (damp-bench-check.md AC) before the trial.
```

### NODE-016 - status LED on D-AMP

Status: DONE 2026-10-14 (step 4a, issue #29, sub-issue of #21; owner request 2026-10-11). Owner choices 2026-10-12: one LED; 1 flash on every accepted verdict (not only consumed patterns), 2 on every own chirp start. Firmware 261f088 (GPIO 2 on D-AMP, 80 ms on / 120 ms off, PWM levels and solid-while-chirping removed), compile-verified on six envs, flashed on E2 (SELFTEST PASS, bench:sessions/2026-10-12-selftest-e2-node-led). Gate: external LED (GPIO 2, 330 ohm) fitted on E2 and E3, both running the Node build 568dbc0; owner reported the LEDs work (visual check, not recorded in a bench session).

```text
Today: STATUS_LED_PIN is -1 on D-AMP (BoardConfig.h); NodeDebug's detect
flash (observePatternPulse) lost its caller in 2d54d41 (2026-05-19), so
the LED only stays on while a chirp plays, and only on piezo.
Target: 1 flash on detect, 2 flashes on emit, 80 ms on / 120 ms off, a new
call restarts the sequence.
Plan:
  BoardConfig.h: D-AMP STATUS_LED_PIN = GPIO 2 (free; DevKit onboard LED).
    External LED GPIO 2 -> ~330 R -> LED -> GND (strapping pin: pull to
    GND only). Update the pin list in 2026-10-06-damp-output-hardware.md.
  NodeDebug: observePatternPulse -> flashLed(now, count); drop the
    solid-while-chirping mode and the unused brightness constants.
  ResonantNodeApp.cpp: flashLed(now, 1) on verdict.accepted in
    processDetectionFrame; flashLed(now, 2) at chirp start.
Open: detect = verdict.accepted (proposed) or behavior ConsumedPattern;
one LED or two (detect / emit, second on e.g. GPIO 4).
Gate: on a D-AMP node, 1 flash on an external chirp, 2 when it emits.
```

### NODE-014 - pin the espressif32 platform version

Status: DONE 2026-10-16 (pinned; `platform = espressif32@6.13.0` in `[env:esp32dev]`, inherited by every ESP env),
from a cloud session without the ESP toolchain: the first `pio run` on the Windows machine confirms the
pinned version resolves to the one already installed there. The framing re-check stays the rule for upgrades.

```text
platformio.ini pins no espressif32 version, so the I2S legacy driver (and
its RX/TX framing behavior the D-AMP HAL relies on: RX MSB realign, #24)
is whatever the build machine has. Recorded 2026-10-09: espressif32 6.13.0,
arduinoespressif32 3.20017.241212 (Arduino-ESP32 2.0.17, IDF 4.4.7).
Pin it in [env:esp32dev]; rerun the framing check (raw bit 8 toggles) on
any upgrade.
```

### NODE-015 - node self-test (SELFTEST)

Status: DONE 2026-10-21 (step 3b, issue #28): gate run on all five boards, bench/sessions/2026-10-21-selftest-all5.
Earlier: owner request 2026-10-10; firmware work started 2026-10-10 evening; it covers the #20 R1 leftovers.

Pass doc: `docs/refactors/archive/selftest.md` (output format, checks and
criteria, runner, what is compile-verified vs pending hardware). Firmware
(Node, Analyzer, Emitter builds) and `tools/bench/selftest.py` written
2026-10-10, all six envs compile; not yet run on a board.

```text
A SELFTEST serial command in the shipped Node and Analyzer builds, one
PASS/FAIL line per check, plus a tools/bench/ runner that calls it on
every attached port and logs to a bench session (MAC -> label from
bench/README.md).
Hardware I/O:
  board ID: MAC, BOARD_NAME, build version
  mic: non-zero samples, quiet floor in range, I2S framing (raw bit 8
    toggles, #24)
  amp/speaker: own chirp >= ~30 dB over the floor (the #20 amp check, AC)
Node runtime smoke (automates cleanup-0-plan T7):
  quiet baseline reached at boot (no FAILED_NO_QUIET)
  external chirp (Emitter) -> verdict -> the node responds
  own chirp produces no verdict (own-emit suppression)
Out of scope: two-node interaction (NODE-008), UART2 link.
Why: 2026-10-10 E1's amp was silent and E2's mic read 0; the firmware
reported neither, and E1's silence had invalidated two #20 runs.
Relation: replaces the throwaway AC sketch once landed; NODE-001's STATUS
line stays the informational report, SELFTEST is the pass/fail one.
Gate: passes on every board going into NODE-011, logged on bench.
```

### NODE-017 - detection goes deaf 71.6 min after boot (32-bit micros wrap)

Status: DONE 2026-10-12 (step 3c, issue #30 closed). Offset wrap on hardware: Analyzer and Node (bench 85dd0a9); real uptime past 4,295 s: Analyzer in the 2026-10-12 soak (10/10 in every block after the wrap, to ~26,000 s). Node real uptime: done since, see the next lines (was: not run, offset test only).
Real-uptime check done 2026-10-23: five Nodes ran 9.6 h (eight 71.6 min micros wraps) with no reset, no
deaf node and no audio loss (bench/sessions/2026-10-22-step10c-overnight).
Longer since (2026-10-29): 24.00 h (10c block 2, bench/sessions/2026-10-23-installation-run), 23.45 h in the 11c
soak (bench/sessions/2026-10-26-soak24-shape-b), 27 h on 40c92e3 (bench/sessions/2026-10-28-soak96-40c92e3, stopped
for 11d): no resets, no deaf node.
Pass doc: docs/refactors/archive/micros-wrap.md (audit, design, wraptest envs, bench plan).
2026-10-11: fix written, compile-verified only: one 64-bit time base
(src/app/TimeBase.h) for the sample clock and every millis()/micros() read,
absolute ms from 64-bit us, wrap-safe comparisons; envs esp32dev-wraptest and
esp32dev-analyzer-wraptest put the us wrap 180 s after boot. Next: the bench
plan in the pass doc (Analyzer E2 + Emitter E3, then Node on E2).

```text
Symptom: both soaks (bench:sessions/2026-10-10-soak24h-70cm,
2026-10-11-soak24h-70cm) lost every detection between Analyzer uptime
3,638 s and 4,509 s and never recovered; link, Emitter markers, sampling
and the Analyzer's own SELFTEST (mic, amp) stayed fine. micros() wraps at
2^32 us = 4,295 s.
Cause (code reading): the sample clock is a 32-bit microsecond value
(AudioSourceI2S block stamps, AudioSignal sampleTimeUs) and absolute times
in ms are derived from it by /1000 (AudioSignal.cpp:71,103,208; scalar
detector occurrence/report times; RAW capture), then compared with
millis()-based windows (trial windows, own-emit suppression
ResonantBehavior::ownEmitDetectionSuppressed). After the wrap the derived
ms restart near 0 while millis() keeps counting. Some comparisons are not
wrap-safe either (AnalyzerModeApp.cpp:709 `micros() > start`).
The Node build runs the same audio path: a deployed node very likely goes
deaf ~72 min after power-on. No bench run so far lasted 72 min after a reset.
Fix direction: one time base for audio and detection (64-bit or
wrap-consistent with millis()), absolute us -> ms through it, wrap-safe
comparisons (app/TimingUtils.h), and a debug build flag that offsets that
one time base so a wrap test takes minutes, not 72.
Gate: Analyzer and Node keep detecting across the (offset) wrap on the
bench, and a run past 72 min of real uptime.
```

### NODE-018 - code-review findings: debug and fix

Status: DONE 2026-10-22 (step 3d, #31 closed): fixes landed and built, the gate SEQ session passed (bench/sessions/2026-10-22-3d-seq-check). Leftovers are handed to steps 7c, 8, 8b and 10 (roadmap-0-steps.md 3d); #31 and #32 closed.
Pass doc: docs/refactors/archive/code-review-fixes.md (findings, priority fixes P1-P4, tiered list for the rest).

```text
Source: a read-only review of all of src/ on 2026-10-13, traced by hand,
nothing compiled or run.
Priority fixes:
  P1 Analyzer PARAM value offsets (scalar carrier-quality keys parse wrong).
  P2 field node: FrequencyMatch refractory deaf from ~24.8 days uptime;
     the per-block audio overflow flag is never set.
  P3 SEQ evidence: per-trial counters, pipeline / verdict-correlation
     overflow baselines.
  P4 contrast scale vs thresholds, FeatureHistory length, DET-010 on
     TonalPulseScalar: measure, then fix or decide (no drive-by retune).
Rest: tiers A (before the trial), B (before the next SEQ campaign),
C (fold into steps 8 / 10), D (latent).
Gate: as step 3d in roadmap-0-steps.md.
```

### NODE-013 - 24-hour bench soak on D-AMP

Status: DONE 2026-10-12, accepted short (step 3a, issue #27 closed). Third attempt bench:sessions/2026-10-12-soak24h-70cm: 30 blocks / 7.3 h instead of 24 h (owner), 0 missing, 0 errors, T3 299/300, 0 OBS false accepts; accept rate flat, strength hour means 9.1k-11.2k vs ~200 block-to-block (not attributed: no room notes). Earlier attempts 2026-10-10 and 2026-10-11 went deaf at the micros wrap (NODE-017).

```text
Research: docs/research/soak-24h.md (design, gate, open points).
Observations: docs/lab/notes_lab.md 2026-10-09 (day-to-day level change);
results go to a bench session and a docs/lab/ entry.
One Emitter -> Analyzer pair at a fixed distance, boards fixed in place,
short SEQ blocks every 15 min for 24 h via tools/bench/soak.py,
one soak.csv row per block. D-AMP at the default tone level (0.3 FS since
b6e99b0). Blocks in an Analyzer mode without dropped DMA buffers, or the
drops counted per block (ANA-004). Both boards pass the amp check
(damp-bench-check.md AC) at the start and the end of the soak: a loose
wire silenced E1's amp on 2026-10-09, and E1 is in storage since 2026-10-10.
Runs in parallel with NODE-010; doesn't gate NODE-011.
```

## Procedure

```text
When an active roadmap item is finished, move its whole section here, unchanged, under its domain
(heading and ID included), and keep the active roadmap focused on the remaining work. The ID stays
citable from code and decisions; tools/ci/roadmap_ids.py checks that every cited ID resolves here or in an
active roadmap, that this file holds finished items only, and that no ID is used twice.

The one-line [LANDED] lists at the top of each domain are older: they predate item IDs.
```
