# Detecting a drifting chirp: more Goertzels, sliding DFT, FFT, or something else

Status:  exploring (2026-10-15)
Lab:     bench/sessions/2026-10-14-up-spacing-map (comb nulls a few hundred
         Hz apart, 10-20x deep), freertos-tasks.md (~39 of 62.5 us/sample)

Expands section 3 of `scalar-detector-extensions.md` (DET-013, step 5b,
issue #36). Desk analysis from the code; no new measurements. All cost
figures below are operation counts, not ESP32 timings: the first step of
any option is a profiled build (`FREQBAND_ENABLE_PROFILING=1`).

## What the detector does today

`FreqBandStream` (`src/detection/features/FreqBandStream.cpp`):

- 16 kHz, rolling window N = 64 samples (4 ms), bin spacing 250 Hz.
- Every 4 samples (`frequencyUpdateEverySamples`, Analyzer can set
  1/4/8/16) it recomputes from scratch: total energy plus three Goertzels
  (target 3200, 2950, 3450 Hz), each a full pass over the 64-sample ring
  buffer with a `%` per sample.
- Output: `FrequencyTarget` (FrequencyScore16, target amplitude) and
  `FrequencyContrast` (target / (target + mean of the two neighbours)).
  Scalar observes `FrequencyTarget`.

Cost: ~4 passes x 64 iterations per 4 samples = 64 inner iterations per
sample, each a load, int->float, multiply-add and a modulo. Recomputing
75 % overlapping windows from scratch is the main waste in this stage.

## What drift does to that

Goertzel with a rectangular 64-sample window, tone offset d bins from the
target (1 bin = 250 Hz):

| offset | Hz | amplitude | loss |
|---|---|---|---|
| 0.25 | 62 | 0.90 | -0.9 dB |
| 0.5 | 125 | 0.64 | -3.9 dB |
| 1.0 | 250 | 0 | null (and all of it is now in the "neighbour" bin) |

So the current stream tolerates about +-100 Hz. Beyond ~125 Hz the score
falls and the contrast inverts: the neighbour bins, which are the noise
reference, now hold the signal.

Two things follow that matter more than the choice of algorithm:

1. **Window length is not the problem.** A sweep of 600 Hz over a 100 ms
   chirp moves 24 Hz inside one 4 ms window: no smear. The short window
   that gives Scalar its ~10 ms timing stays. What is missing is
   frequency *coverage*, i.e. more bins.
2. **Drift turns a frequency null into a time gap.** At any instant the
   tone sits at one frequency; if the room has a null there, every
   algorithm sees little energy. A sweep moves through nulls, so the
   envelope shows dropouts of roughly null width / sweep rate (60 Hz
   null at 6 Hz/ms = 10 ms). That is what makes a swept chirp robust
   against the comb, and it lands on Scalar's existing gap logic
   (`maxGapMs`, island and coverage params), not on the spectral stage.
   Whatever option is chosen, test it together with the gap params.

How much coverage is needed depends on which drift is meant; the notes
use the word for three things:

| drift | span | known shape? | best match |
|---|---|---|---|
| sweep inside the chirp against the comb (design direction, 2026-10-14) | >= comb spacing, ~300-600 Hz | yes, if we define it | bin bank or band; matched filter possible |
| frequency hopping across pulses of a train (section 1) | a few discrete frequencies | yes | one Goertzel per hop frequency |
| behaviour modulation, node-specific variation (myspec 6.2) | config-defined range | no (per node, random) | bin bank over the range, peak pick |

Speed-of-sound / temperature drift does not shift the tone (the emitter
is crystal-timed); it moves the comb, which is the same problem as row 1.

## Options

Costs per sample at 16 kHz, K = number of bins. RAM in bytes.

### A. Goertzel bank (more Goertzel runs)

K Goertzels at 125-250 Hz spacing across the band, report the max bin
(score), its index (frequency estimate) and the band sum.

- Timing: as today, linear in K. Done naively (K separate passes):
  16 K iterations/sample; K = 7 (3000-3750 at 125 Hz) is ~1.75x today's
  inner-loop count, K = 13 ~3x. Cheap fixes before anything else:
  one pass over the buffer updating all K states (sample loaded and
  converted once), and a mirrored ring buffer (write each sample at i
  and i + N, read contiguous) to drop the `%` (+256 B). Decimation 8
  halves it again at 0.5 ms update granularity.
- RAM: K coefficients + K results, < 100 B. Mirror buffer +256 B.
- Architecture: none at the stream boundary. Stays inside
  `FreqBandStream`; `FrequencyTarget` keeps its FrequencyScore16 domain
  (myspec: no rescaling downstream), it is just the max over the bank.
  Contrast needs a new definition: the old neighbours are inside the
  band now. Candidates: peak vs guard bins outside the band, or peak
  power vs total window energy (already computed, i.e. a tonality
  ratio).
- Noise: max over K bins raises the noise floor ~ by the chance that one
  of K bins spikes (a few dB for K ~ 8). Band *sum* costs 10 log K dB of
  SNR against a single tone; use max for the score, sum only as a
  diagnostic.

### B. Sliding DFT bank (same answer, O(K) per sample)

Per bin: S(n) = S(n-1) + x(n) e^{-jwn} - x(n-N) e^{-jw(n-N)}. Same
magnitude as the Goertzel on the same 64-sample window, but updated
incrementally every sample instead of recomputed.

- Timing: ~8-10 float ops per bin per sample, no buffer rescan. K = 8 is
  ~70-80 ops/sample, at or below today's 3-bin Goertzel path, and gives a
  value every sample (no decimation, no packet-age logic).
- RAM: per bin complex state + phasor, ~24 B; K = 16 < 400 B. Reuses
  the existing 64-sample ring for x(n-N).
- Risk: float rounding accumulates in the running sum and the phasor.
  Standard cures: renormalise the phasor every N samples and recompute
  S from the buffer every few hundred samples (one Goertzel-cost pass),
  or a damping factor r slightly below 1. With N chosen so the target
  sits on an integer bin (N = 80 gives 200 Hz bins, 3200 = bin 16) the
  phasor is a fixed twiddle and the recursion is the textbook SDFT.
- Architecture: same as A (inside `FreqBandStream`).

### C. FFT

ESP-DSP's radix-2 float FFT (optimised for the ESP32's FPU) is the usual
choice; whether it links cleanly in this Arduino/PlatformIO setup is
unchecked.

- Timing: block-based, not per sample. A 64-point FFT is on the order of
  a few thousand cycles (to be measured); at a hop of 16 samples (1 ms)
  that is a few us/sample spread out, but in bursts, and it yields all
  32 bins of which ~4-8 are in the band.
- RAM: N = 64: complex buffer 512 + twiddles ~512 + window 256 ~ 1.3 KB;
  N = 256 ~ 5 KB. Not the constraint on an ESP32.
- Architecture: the biggest change. Streams become hop-rate (per block)
  instead of per-4-samples; a window function is needed (Hann: lower
  sidelobes, wider main lobe, -1.4 dB worst-case scalloping instead of
  -3.9). A longer N for resolution costs timing (N = 256 is 16 ms).
- Worth it only if we need the *whole* spectrum: several nodes on
  different, unknown frequencies at once, or spectral-shape features
  (flatness, harmonics) for speech/music rejection. Neither is on the
  roadmap.

### D. Other algorithms

- **Band-pass IIR + envelope** (e.g. two biquads, 2.8-4.0 kHz): ~10
  ops/sample, any drift inside the band passes with no scalloping. Gives
  up selectivity: every tone or noise inside the band counts. This is the
  "wider band" option of DET-013. Good as a cheap pre-gate or as a
  second stream next to a bank, weak alone given the 2026-10-14 lesson
  that noise rejection is the hard part.
- **Zero-crossing / instantaneous frequency** after that band-pass:
  almost free, gives a frequency estimate per cycle and tracks drift by
  construction. Fragile under noise and multipath; useful as a
  frequency-trajectory fact, not as the detector.
- **Matched filter / dechirp** for a known sweep shape: multiply by the
  conjugate reference chirp and the sweep becomes a fixed tone, then one
  Goertzel/FFT. Best SNR (processing gain ~ time x bandwidth) and
  integrates over the comb instead of falling into it. But it needs the
  chirp start (sync) or a sliding correlation (L = 1600 samples for
  100 ms: far too expensive per sample; FFT overlap-save at block rate
  is possible but a different architecture). Only fits a deterministic
  sweep, not node-random drift. Keep as the long-term option if the
  network settles on one defined chirp shape.
- **PLL / adaptive notch tracker**: tracks one tone well, needs lock
  time comparable to a short pulse, poor at onset. Not a fit for 10-15 ms
  pulses.

## Comparison

| | timing / sample | RAM | architecture change | drift handled |
|---|---|---|---|---|
| today (3 Goertzels, /4) | ~64 iterations + `%` | 512 B ring | - | +-100 Hz |
| A. Goertzel bank, K = 8, single pass, mirror | ~1.5-2x today | +~300 B | none at stream level; new contrast def | band of K bins |
| B. Sliding DFT, K = 8 | <= today | +~200 B | none at stream level; stability care | band of K bins, per sample |
| C. FFT 64, hop 16 | bursty, ~today (measure) | +1.3 KB | stream rate, windowing, library | whole spectrum |
| D. band-pass + envelope | ~10 ops | ~50 B | new stream or replaces target | anything in band, no selectivity |
| D. dechirp | depends on sync | small-large | sync / block processing | one known sweep only |

The tight resource is time, not RAM: the Node uses ~39 of 62.5 us per
sample, and the DMA ring (3 x 128 samples, 24 ms) is the only slack
before buffers drop. RAM matters where a *new stream* enters
`FeatureHistory`: 256 bins x ~20 B ~ 5 KB per stream, and
`kMaxActiveStreams = 3` is static-asserted against the inspection plan.
A frequency-estimate stream recorded for inspection would hit that limit
before any spectral buffer does.

## Proposed path

1. **Profile the current stage** on D-AMP (`FREQBAND_ENABLE_PROFILING=1`,
   Analyzer, quiet run): Goertzel us vs total per-sample cost. Decides
   how much budget there is to spend.
2. **Cheap wins in place:** single-pass multi-bin Goertzel + mirrored
   buffer. Same output, should cost less than today. Bench check: T3 and
   silent OBS identical to before.
3. **Bin bank (A) behind a param:** K and spacing configurable, K = 3 /
   neighbour layout reproduces today's behaviour. `FrequencyTarget` =
   max bin; peak bin index and band sum as diagnostics in the report
   first, not as a FeatureHistory stream. New contrast definition chosen
   and logged as a decision.
4. Only if step 1 shows the bank does not fit: **sliding DFT (B)** as a
   drop-in implementation of the same bank.
5. Bench with a swept / hopped source together with Scalar's gap params,
   at the comb spacings of the up-spacing map (60-110 cm), plus the
   speech/music OBS battery: widening the band admits more noise, and
   2026-10-14 showed false answers are the real cost.

Needs from the output side (not this note): the chirp renderer plays one
tone per chirp; the Emitter `SWEEP` steps the frequency *between*
chirps. An in-chirp sweep needs a SoundOutput change before step 5 can
test drift inside one signal.

Boundary (myspec): the bank, the peak-bin index and the band energy are
detector facts in the feature stream. Judging whether the peak moved
like a plausible sweep (trajectory consistency) belongs to inspection,
not to the detector and not to Behavior.

## Open questions

- Which drift is the target: defined sweep, hop set, or random per node?
  Decides between A/B (any of them) and a matched filter.
- Sweep span: the comb spacing (a few hundred Hz) suggests >= 400 Hz;
  with 250 Hz bins that is 3-5 bins, with 125 Hz bins 5-9.
- Does max-over-bins raise false accepts on speech/music measurably at
  K ~ 8? Only a bench battery answers it.
- Should the frequency estimate become a FeatureHistory stream (costs a
  slot and ~5 KB) or stay a per-occurrence fact in the report?
