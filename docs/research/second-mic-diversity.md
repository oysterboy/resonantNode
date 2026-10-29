# A second mic per Node: diversity against the room comb

```text
Status:  exploring (2026-10-29)
Lab:     none yet; motivated by E005 (docs/lab/experiments/E005_margin.md) and the step 14 chord mix
         (bench/sessions/2026-10-29-step14-rotations)
Related: room-comb-and-margin.md (the comb, one spot vs the next), distance-range.md, polyphony.md (chords)
```

## Question (owner, 2026-10-29: "Stereo mic?")

Would a second microphone on each Node help? Which problems would it address, and what does it cost?

## What the hardware already allows

The D-AMP board reads stereo I2S frames (`src/app/BoardConfig.h`: `AUDIO_I2S_FRAME_SLOTS 2`,
`I2S_CHANNEL_FMT_RIGHT_LEFT`) and takes the mic from slot 0 (`AUDIO_I2S_MIC_SLOT`). A second mic of the same type,
its L/R select pin tied to the other level, would arrive in slot 1 on the same BCLK / WS / data lines: no new port,
no new pins beyond the mic. The firmware would read slot 1 as well instead of discarding it.

## What it would address

- **Room nulls (the margin).** E005 measured 3-6 dB of level margin at one spot, and a train survives one lost hop
  but not two in the middle. The losses come from the room's comb: a bin that sits in a null at one mic position.
  Two mics 5 cm or more apart (about half a wavelength at 3-4 kHz is 4-6 cm) sit at different points of the comb, so
  a hop that is dipped at one is rarely dipped at both. Taking the better of the two per bin (selection diversity)
  should make a dipped hop much rarer. This is the main reason to try it.
- **Chords: little.** The chord mix (step 14) found the danger is temporal: two trains 120-220 ms apart merge past
  the 300 ms maximum and both are lost. A second mic does not change that. Separating sources by direction
  (beamforming, time difference of arrival) is too heavy for the per-sample budget.

## Cost and risk

- **CPU.** The Analyzer uses about 51 of its 62.5 us per sample (issue #26); the Node has more headroom but runs the
  same per-sample chain. A second full detection path is unlikely to fit. Cheaper options, in order:
  1. One band bank fed by the louder slot per block (switching, not combining).
  2. Two Goertzel band banks (one per slot), the detector fed the larger of the two strongest-bin values per frame.
     Costs one more band bank, not a second detector.
  3. Sum of the two slots before the band bank (cheapest, but a sum can also cancel: not true diversity).
- **Spec.** Diversity sits inside the frequency feature (FreqBandStream), before the detector: the Detector still
  sees one stream and the numeric domains stay as they are (myspec.md). Must not leak a second stream past the
  feature layer.
- **Self-suppression and SELFTEST** need the second mic checked too (mic check per slot).
- **Hardware:** one more mic per Node, mounted 5-10 cm from the first; the enclosure has to allow it.

## How to test it, cheapest first

1. **On the desk, with no firmware change:** RAW `mode=i2s` reads one slot. Add a RAW mode that dumps both slots
   (or temporarily set `AUDIO_I2S_MIC_SLOT 1` on a second build), wire a second mic on one Analyzer, and record
   hop10 at several spots. Per bin, compare the two mics' levels (sweepbins.py per slot): how often is a hop
   10 dB down at one mic and not at the other?
2. **On the host:** replay both slots and the max-of-two stream through the detector (a replay option), and compare
   the accept rate against either mic alone, on the E005 level ladder's captures repeated with two mics.
3. Only if 1-2 show a clear gain: the firmware option (2) above on one Node, then the desk A/B and a soak.

## Open questions

- How far apart can the mics sit in the enclosure, and is 5 cm enough at the band's low end (2950 Hz, 11.6 cm
  wavelength)?
- Does the second mic change the own-chirp ring-down (self-suppression windows)?
- Is this worth more than spreading the band (BEH-009) or the per-node base bin, which attack the same comb from
  the frequency side?
