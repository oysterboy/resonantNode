# A call people can place: broadband energy next to the detection climb

Status:  exploring (2026-11-17)
Became:  step 24.1.x of docs/roadmaps/roadmap-0-steps.md (two variants to build and try; nothing built yet)
Lab:     none yet; the code reading below is not a measurement

## The wish

Viewers in the room should be able to tell where a node is. Owner 2026-11-17: more broadband, mixed into the call,
detection signal kept. This note says what the detector does and does not see, so the mix-in can be sized.

## Why a call is hard to place (general acoustics, not measured here)

- A steady tone in the few-kHz range gives little to place: the level difference between the ears carries the
  direction there, the phase difference no longer does, and room reflections of a long tone blur the rest.
- What helps: sharp onsets (the first arrival decides the direction) and a wide spectrum (more cues, and the ear's
  own spectral cues, mostly 4-16 kHz). A cricket-like trill of sine pulses on one pitch has only its onsets.
- Ceiling here: 16 kHz sampling (AUDIO_I2S_SAMPLE_RATE_HZ), so nothing above 8 kHz; the speaker is a small 3 W
  driver and no response curve is in the repo (docs/research/acoustic-test-suite.md plans it). Its top end is a
  bench question.

## What the detector sees (read from the code, 2026-11-17)

The Node's shipped profile (`makeTonalPulseScalarProfile`, DetectionProfile.h) works on three things:

```text
onset, duration, release   FrequencyTarget: the strongest Goertzel bin of the band bank, not raw PCM
                           (AmpEnvelope is the AmpExperimental profile only)
contrast (inspector)       power of the strongest bin T over the mean of its two neighbour bins N:
                           contrast = T / (T + N + 1) * 32767            (FreqBandStream::computeBandScore)
                           limit detection.contrast_medium 30500 = 0.931, so N <= ~7.4 % of T
                           (about 11 dB under the peak, the mean of the two neighbours), P75 over 100 ms
amp (inspector)            FeatureStream AmpMagnitude, P75, limit detection.amp_medium: full band, sees
                           out-of-band energy, which can only raise it
shape (inspector)          the strongest bin's track over the occurrence: 3 distinct bins, 2 steps up
```

The bank is the band's bins (250 Hz apart, window 64 samples = 4 ms) plus two guard bins, at most 10 bins
(`kMaxBankBins`); `band_per_node` sets the window (the glide field: 2500-3500 Hz). Total energy is measured but is
not in the contrast.

What follows for a broadband mix:

```text
outside the bank     no bin changes, so onset, contrast and shape do not move; only amp rises (harmless).
                     Open: spectral leakage of a 64-sample window. Whether the window function is rectangular
                     and what far-away broadband leaks into the bins is not read yet: measure it.
inside the bank      the strongest-bin track can jump to a broadband bin (shape, target) and the two
                     neighbours of the tone's bin rise (contrast): per-bin broadband must stay about 11 dB
                     under the tone, less the tone's own leakage.
shared peak          tone and broadband share the DAC's full scale; a share x of the peak costs the tone
                     20 log10(1 - x) dB, so 30 % is about 3 dB of detection level and of the hop train's
                     margin (step 13).
clipping             a mix that clips puts harmonics into the bank.
```

Other nodes: a listener's window is its own; broadband above the highest window in use (the range's top, 6500 Hz
in the matrix; the output ceiling 8 kHz) is outside every one. Inside the range it is in some node's bins.

## What it costs, by where the broadband goes

```text
A  beside the climb, above the windows   cheapest for detection; hardest for the speaker and the room
B  in the trill pulses, before the climb the climb stays exactly as is; the trill bins already are not the
                                         matched call, but the shape look-back (shape_pre_ms 120) can see them
C  as the detection signal itself        a new inspector (no moving ridge for the shape rule), contrast and
                                         noise rejection change: detector code, DET-014 territory, after the
                                         performance (decisions/2026-11-06-sound-layer-before-the-performance.md)
```

## Output side

`I2sToneOutput` renders a sine with a ramp. A broadband part needs a second source in the same per-sample render
(a click, a high-passed noise burst or a fast sweep inside the pulse), a level param with 0 = today's output
(byte-identical, so the 24 gate's T2h check holds with it off), and its own high-pass so it stays out of the bank.

## Measuring

- Level sweep of the broadband against the tone (off, -20, -15, -10 dB) on one board and one listener: `contrast`,
  `shape.*` and `peak_bin` in SEQ_EXPLAIN. The knee is where contrast or shape starts to fail; the shipped level
  keeps at least 6 dB under it.
- `tools/bench/callmatrix.py` against `glide` and `cricket` in the same windows for the detection rate.
- A listening test with people (several positions, eyes closed, point at the node) against the plain call: the
  whole point; the detector numbers only say it does no harm.
- Before any board: a host replay of a synthetic call with the mix, if src/host/Replay takes synthetic PCM.

## Open

- The speaker's top end and the mic's response above 5 kHz.
- The window function and far leakage into the bank.
- Whether people place the trill's onsets better with a click, a noise burst or a sweep: a listening question.
