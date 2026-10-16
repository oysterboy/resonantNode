# Where code and roadmap rest on unproven assumptions (audit, 2026-10-16)

```text
Status:  exploring (2026-10-16)
Lab:     docs/lab/2026-10-15-bench-day.md, docs/lab/2026-10-16-bench-day.md,
         docs/lab/notes_lab.md (2026-05-26 slow circle, 2026-10-12 soak),
         docs/refactors/code-review-fixes.md (P4, R2-1, tier C)
```

A read of `main` at 698be73: the roadmaps, decisions, lab notes, refactor
docs and all of `src/` and `test/`, asking one question: where does the
design assume something that no measurement, test or assertion backs?
Nothing here is a decision; each item names what is assumed, why it is
unverified, and the cheapest check. Items marked **verified** were traced
in the code in this session; items marked **audit** come from a line-level
read-only pass and are consistent with the docs but were not re-traced.
This note is the reasoning; checks that get run go to `docs/lab/`, forks
that get taken go to `docs/decisions/`.

Short version: the largest unproven assumption is the one the whole
roadmap points at, that five nodes on the shipped Behavior defaults will
exchange signals at all. Two things in the code argue against it (section
1, items 1 and 2). Below that, the signal-variation design, the shipped
thresholds and the host test harness each rest on single-condition
measurements that the docs flag but the code does not enforce.

## 1. The field trial (step 6, NODE-008)

### 1.1 The two-node exchange is blocked by the refractory default (verified)

`refractoryAfterEmitMs = 400` and `waitAfterHeardMs = 100`
(`src/behavior/BehaviorProfile.h`). A verdict that arrives while Behavior
is in Refractory is dropped, not queued
(`src/behavior/ResonantBehavior.cpp`, `handleOccurrenceVerdict`, the
`State::Refractory` branch); Refractory ends only after the full 400 ms
(`update`, `State::Refractory`).

Timeline for a reply, from the end of node A's own hop train:

```text
B closes A's train                 ~10-50 ms  (release debounce + room tail)
B waits                             100 ms    (waitAfterHeardMs)
B's train plays                     165 ms    (25 ms x 5, gap 10)
A closes B's train                 ~10-50 ms
A's verdict on B's reply           ~300-370 ms after A's train ended
A's refractory                      400 ms    -> verdict dropped
```

So the ping-pong is cut by arithmetic, independent of the detector
problem the 2026-10-16 lab note names (E3 forms no candidate for E2's
trains). Fixing that will expose this gate next. The only multi-node
observation on record, the May 2026 "stable 5-node slow circle"
(`docs/lab/notes_lab.md` 2026-05-26), ran with `refractoryAfterEmitMs = 0`
and `waitAfterHeardMs = 200`. Neither current constant is derived from the
signal length, and `ResonantBehavior` has no test.

Check: nodepair run with `RB BEHAV refractory=0` on both boards, once
the E2 -> E3 direction is measured. Fork to decide: refractory as a
function of the emission span, or a queued verdict.

### 1.2 "Settle or run away" has no mechanism behind it (verified)

`FieldState` is computed by `FieldStateTracker` and handed to Behavior,
which stores it in `_lastFieldState` and never reads it
(`ResonantBehavior.cpp`). The field-state values in every profile
(window 4000, busy 3, dense 6, 0.45) have no observable effect; `noiseFloor`
is defined as an occurrence count divided by the window length and
`avgAmbientLevel` copies it. The spec's rule "FieldState may influence
behavior" (myspec 5.8) is doc-only. There is no response probability (the
`ResponseProbabilitySkipped` decision exists, nothing produces it), no
definition of "settle", and no damping beyond refractory. The trial's third
question is therefore unanswerable by design today: whatever the five nodes
do, no knob in the firmware shaped it.

Check: before step 6, decide what "settle" means in counts (answers per
minute per node, chain length) and whether any field-state input should
gate a response. Until then, write the trial gate as a measurement, not a
pass/fail.

### 1.3 Per-node variation is thinner than step 5b states

Every node plays the same five bins in the same grid order;
`output.hop_rotation` only shifts the start bin
(`ResonantNodeApp.cpp`, `hopFrequencies`). The rotation comes from the
MAC modulo the bin count; five boards are not guaranteed five distinct
rotations. The detector is shape-agnostic by decision
(`2026-10-15-signal-variation-inside-a-shared-band.md`), so "nodes hearing
signals unlike their own" is not something this design can test: the
listener sees a tonal signal in the band either way.

### 1.4 The shipped Node level was never measured with the network signal

`I2S_TONE_AMPLITUDE` defaults to 0.3 FS (`src/app/RuntimeDefaults.h`),
with no override for `env:esp32dev`. Every 5b battery ran the Emitter at
0.5 FS (`docs/lab/2026-10-15-bench-day.md`, setup). The 0.3 decision
(`2026-10-09-damp-tone-level-0p3.md`) was taken horizontal at 110 cm;
facing up, 0.3 FS gave 27/50 at 110 cm where 0.5 FS gave 50/50
(2026-10-14). Hop amp P75 at 0.5 FS and 110 cm was 1,900-2,600 against the
1,500 floor. A hop train at 0.3 FS has been heard in exactly one direction
at one spacing (70 cm, E3 -> E2, nodepair). Either the Node ships 0.5 FS
or the battery is rerun at 0.3.

### 1.5 Boards are assumed interchangeable

Steps 5c and 6 assume five boards behave like E2. E2 and E3 already differ
(E3 forms no candidate for E2's trains; E3's mic was never the listener in
any battery), the mic part is not recorded in `BoardConfig.h`, and the
SELFTEST SNR pass level (30 dB) once measured 17 dB on an amp chirp. The
first five-board session should be a hearing matrix (each node emits in
turn, the other four log), not the trial.

## 2. Signal variation (step 5b, 8a)

### 2.1 Sweep is already half falsified; the spec's modulation model contradicts the grid

The 2026-10-15 decision says sweep and hop are both supported. The sweep
was rejected 30/30 on hardware on 10-16 (contrast P75 29,000-30,500 while
gliding between bins). myspec 6.2's example, emit deviation +-120 Hz over a
3000-3400 Hz accepted band, contradicts the new rule that hop steps must
sit on the 250 Hz bins (`2026-10-16-network-band-2950-3950.md`). BEH-006
(vary centre and span over time) breaks the contrast gate unless it stays
on-grid. The decision file still reads "decided, not yet implemented";
the spec example should change or the decision should record the limit.

### 2.2 The contrast and amp limits come from one room, one phone, two days

The contrast limit 30,500 rests on 27 false-answer occurrences; the margin
to music is about 400 on a 32,767 scale (lab note 10-16, caveats). The
trial room, five mics and speakers, and the Node build's noise behaviour
are unmeasured; the Node build has never run a speech/music OBS. The
decision file names the confirmation battery as its own condition.

### 2.3 Hop detectability depends on the room tail bridging the gaps (audit)

The train is 25 ms pulses with 10 ms gaps; `releaseDebounceMs = 10`,
`maxGapMs = 20`, `minTransientDurationMs = 85` (longer than any pulse).
The train is one occurrence because the tail fills the gaps (the 10-15 lab
note measured ~13 ms of a 20 ms gap filled). In a dry room or outdoors it
may split into five too-short rejects. `output.hop_gap_ms` and the scalar
gap/debounce params are independent live PARAMs with no consistency
check, and there is no hop fixture under `test/fixtures`. The "noise cost
of max duration 300 alone" is still marked unmeasured in
`DetectionProfile.h`.

### 2.4 The two-layer signal rests on unmeasured leak

OUT-006 lists sidelobe leak from an 800 Hz layer, speaker harmonics at
4 x 800 Hz, and a Hann window as "fix if measured". `notes_lab.md`
(2026-10-15) already saw Strength16 saturate at 800 and 1,000 Hz, where
the speaker is 4-8x louder. None of this has a bench cell.

### 2.5 Phase 0 of the cleanup plan was never formally closed

`cleanup-0-plan.md` Phase 0 (keep or consolidate `FrequencyMatchDetector`)
is a "working assumption: keep"; the decisions index lists it as open while
DET-007 decided Scalar ships. The demo detector stays compiled into every
Node binary with its own RAM and the dispatch sites that switch on a
report's `detectorId`.

## 3. Code

### 3.1 Short occurrences are rejected by construction (verified)

TonalPulseScalar anchors inspection at start with `windowPostMs = 100`,
accepts from 85 ms, and inspects at close plus the 10 ms debounce
(`OccurrenceInspector.cpp`, window end = anchor + post;
`FeatureHistory::getWindow`, `requestedFutureAtInspection`). Any 85-89 ms
occurrence asks for a future window and ends as MissingSupport whatever the
signal. Code-review P4 names this; still open. Nothing asserts
`minDuration + debounce >= windowPost`.

### 3.2 The frequency detector's contrast gate is vacuous (verified)

`FrequencyMatchCriteria.h`: `attackOk = evidenceOk && attackScoreOk`;
`attackContrastOk` is computed and discarded, same for release. The
inspector contrast thresholds 25/50/80 in the Freq and AmpExperimental
profiles sit on a 0-32,767 domain where noise scores ~16,000. Demo profile
only, but selectable at runtime, and the "contrast 50" in the May 2026
slow-circle config was therefore never a gate either.

### 3.3 The Node quiet gate was derived from an artefact (verified)

`kRbStartupQuietThreshold = 400` (`ResonantNodeApp.cpp`) because "the
D-AMP floor sits at ~230-250"; R2-1 showed that reading was the old 2000
baseline offset, and after the fix the floor reads 12-62. The gate failed
once with a radio on (411). A node that fails it proceeds after 8 s with
the baseline unchanged; the behavioural difference is unmeasured. The
quiet rebase averages 200 preprocessed, zero-mean samples, so on D-AMP the
baseline it computes is ~0 by construction (audit).

### 3.4 The DC blocker scale holds only at 3200 Hz (verified)

`kDcBlockerOutputScale = 0.588 = sin(pi * 3200 / 16000)` keeps the
First-Difference-era thresholds meaningful at the chirp frequency. The band
is now 2950-3950 Hz, where the old filter's gain ran 0.55-0.70. The comment
says "revisit in the 5a retune"; 5a did not.

### 3.5 Own-emit tail (audit)

60 ms against a 33-54 ms echo measured once, plain 100 ms tone, one room,
before hop trains existed. For device-timed trains `finished()` fires when
the render envelope reaches zero, up to the TX queue (3 x 8 ms) before the
air does. While suppressed, the band stream and history are not fed, so
the first Goertzel window after suppression mixes old and new samples.

### 3.6 Grid and rate constants are duplicated without a link (audit)

- The band grid is computed as float in `FreqBandStream.cpp` and as integer
  in `ResonantNodeApp.cpp::bandBinCount`, with literal 16000/64 fallbacks.
  Nothing asserts `CHIRP_FREQUENCY_HZ` lies on a bin. A wide band is
  truncated to `kMaxBankBins = 10` silently while the Node still reports
  more bins, so hop steps could land outside the detector's bank.
- 16 kHz and 3200 Hz are baked into at least five sites
  (`PcmPreprocessor` default-constructed at 16000, `kDcBlockerOutputScale`,
  `FreqBandStream` defaults, `RawSampleHistory::kCapacity`, `I2sToneOutput`
  fallback). Overriding `AUDIO_I2S_SAMPLE_RATE_HZ` compiles and mis-tunes
  all of them.

### 3.7 Smaller items (audit)

- A continuous tone holds the scalar detector open forever;
  `PeakStillActive` is never produced.
- `strongPeakThreshold = 300000` on a domain capped at 32,767: Strong is
  unreachable in TonalPulseScalar.
- `FeatureHistory` 512 bins is comment-enforced against
  `maxTransientDurationMs + windowPostMs`; `getWindow` puts 2 KB on the
  loop stack per module per occurrence; Node stack headroom unmeasured.
- Result and evaluator queues (capacity 4) have overflow counters only in
  the Analyzer build.
- `magnitude.lift` subtracts a raw-PCM baseline from a FrequencyScore16
  peak (numeric-domain mix, myspec 5.x); diagnostic only today.
- `git log -S` for every threshold lands on one flattened commit, so no
  constant has commit-level provenance beyond comments and decision files.

## 4. Evidence base and tests

### 4.1 The host replay proves host == board for the amp path only (audit)

`test_replay_corpus` compares `audioMagnitudeValue` (integer arithmetic)
against the board column; `FrequencyTarget` and `FrequencyContrast`, which
decide, go through float Goertzel and are never compared. The contrast gate
is a hard threshold on a P75, so a platform drift of a few units can flip a
verdict. `AudioSourceI2S` is not in the native build: the sample clock,
dropped-buffer accounting and `HistoryWindowIncomplete` (issue #26) have no
regression test on any surface.

### 4.2 The false-positive guard on the host is one 350 ms clip

The board produced no candidates for that clip either; the 27 false
answers that set the contrast limit live only in `bench/`. No fixture is a
hop train, a sweep or an idle chirp. The CSV header does not record
`PCM_PREPROCESS_MODE_VALUE`.

### 4.3 The on-device tests disable every shipped gate

`test_analyzer_pass_rules` and `test_timebase` drive the scalar detector
with carrier quality off, min strength off, duration 0-1000 ms and
debounce 1. Coverage, island, gap and the 85-300 ms window are never
exercised. `ResonantBehavior` has zero tests and is excluded from the
native env. `FrequencyMatchDetector` has no wrap test.

### 4.4 Hardware verification is thinner than the status lines read

- NODE-017: the Node real-uptime run past the 72 min wrap was not done
  (offset test only).
- P2a: "offset not confirmed in the logs".
- Soak: 7.3 h, one day, no room notes, ~25 % strength drift unattributed.
- NODE-014: the espressif32 platform is unpinned; the I2S RX realign
  depends on the IDF 4.4.7 legacy driver. The Windows machine and the cloud
  container may not build the same framework.

## 5. Suggested order of checks

Cheapest and most consequential first; each is one bench cell or one
assertion.

1. Nodepair with `refractory=0` on both boards (1.1), after the E2 -> E3
   hearing direction is measured.
2. The 5b hop cells at 0.3 FS, or ship 0.5 FS (1.4).
3. A static_assert tying `minTransientDurationMs + releaseDebounceMs` to
   `windowPostMs`, or move the anchor (3.1); the same for the band grid vs
   `CHIRP_FREQUENCY_HZ` and `kMaxBankBins` (3.6).
4. Add FrequencyTarget/Contrast to the host-vs-board comparison and one hop
   capture to `test/fixtures` (4.1, 4.2).
5. Decide what "settle" means and whether FieldState gates anything, or
   delete the dead field-state plumbing (1.2).
6. Update myspec 6.2 and the 2026-10-15 decision for the bin-alignment
   rule and the sweep result (2.1).

## Open questions

- Should refractory be derived from the emission span plus the expected
  reply latency, or should verdicts queue through Refractory?
- Is the hop train meant to survive a dry room, or is a room tail part of
  the signal contract? If the latter, the gap should be a detector param
  with a documented floor.
- Which of the five boards is the reference mic, and does per-board
  variance need a per-node threshold offset (a param) before step 6?
