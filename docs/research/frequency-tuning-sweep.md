# Frequency tuning sweep: where in the spectrum does the network signal work, and what does noise do there

```text
Status:  exploring (2026-10-27)
Became:  queued as the first step of the next phase, step 12 (owner 2026-10-27; roadmap-0-steps.md, milestone
         paragraph), after 11c, 11g and 11d
Lab:     bench/sessions/2026-10-26-soak24-shape-b (the band as shipped, 24 h); E002 (shape over the corpus)
```

## Question (owner, 2026-10-27)

"How bad is detection / noise acceptance really." Measured as a sweep of the network band, starting at 1000 Hz,
instead of the one band every number so far comes from (2950-3950 Hz, decisions/2026-10-16-network-band-2950-
3950.md).

Two measures per band position:

- **Detection (hits):** how often the hop train is accepted, at what strength, and why it is rejected when it is.
- **Noise acceptance (false accepts):** how often something that is not a hop train is accepted: speech, music,
  radio, room noise, a plain tone.

## What we know today (2950-3950 Hz only)

- Hits: hop train 640/640 accepted by the Analyzer at 79 cm over 24 h (soak24-shape-b), strength steady within
  4 %; Nodes answer each other in 1,253 chains, all ended.
- Noise: 0 answers started by noise out of 13,288 in the 24 h soak, but in a quiet room. Over the recorded noise
  corpus (radio, speech, music, ambient; 24 verdicts) none is accepted, and one noise verdict already reaches a
  Medium shape (3 bins, 3 up) and is stopped by the other gates only (shapesurvey, 2026-10-26).
- Plain chirps are rejected 10/10 by shape at 3200 and 3450 Hz, and by duration when the room's 3200 null is in.
- So: hits are good and noise acceptance is unmeasured beyond a small corpus and a quiet room.

## What moving the band changes, and what not

One knob on the Node: `detection.band_low_hz` / `band_high_hz`. The detector's bins, the hop train and the shape
rule follow (bins `band_low + k x 250 Hz`, spacing fixed by 16 kHz / 64 samples). Not following: the plain chirp
(`CHIRP_FREQUENCY_HZ`, a build flag), the Analyzer / Emitter commands (`freq=`, `hop=`, `band=` per command),
`soakcycle.py`'s fixed hop line. Thresholds stay as shipped during the sweep: tuning follows the data, as its own
pass.

Expected along the sweep (to be measured, not assumed): small speakers lose output below ~1-1.5 kHz; room noise,
speech and mains harmonics carry more energy low; 250 Hz bins are relatively wider low (25 % at 1 kHz, 4 % at
6 kHz), so a sweep or a voice's glide crosses fewer of them; the room's nulls move bin by bin.

## Protocol (sketch)

Positions: band low from 1000 Hz, 5 bins wide (1000 Hz), up to the top the output allows (below 8 kHz). A first
pass in 500 Hz steps (1000, 1500, ... ~6500: about 12 positions), then 250 Hz steps around the good and the bad
ones.

Per position:

1. Host, no bench: the noise corpus replayed with `--set detection.band_low_hz=... --set detection.band_high_hz=...`
   (native-replay): accepted verdicts per hour of noise, by category (shapesurvey groups). The hop-train fixtures
   need their own band, so hits come from the bench.
2. Bench, Analyzer + Emitter at 79 cm: SEQ hop10 on the shifted band (hop order from the band), chirp at a band bin
   (must be rejected), RAW hop captures for per-bin levels.
3. Bench, noise: the Nodes silent, listening while noise plays from the lab PC (Downloads\bench_audio.wav through
   SoundPlayer to the Studio One box; 435 s of speech and music), N minutes per position; count valid verdicts.
4. Optionally the Nodes free for a short run per position (chains, noise-started), as in 11c.

Output: one table per position (hop accept %, strength, chirp rejection, corpus false accepts per hour, live
false accepts per hour), as a lab experiment (E003), and a decision on whether the band moves.

## Tool

`tools/bench/bandsweep.py` (2026-10-27): `host` replays the noise at every position (the corpus, bench_audio.wav's
four sections resampled to 16 kHz and cached in `logs/bandsweep_cache`, synthetic traffic) at several gains and
writes host.csv; `bench` drives the Analyzer / Emitter and the Nodes per position (hop10, chirp on the second bin,
RAW hop captures, noise played from the lab PC while the silent Nodes listen) and writes positions.csv / noise.csv;
`bench --dry-run` prints the commands without opening a port.

## First host pass (2026-10-27, 52bac7e, a preview: dry file, no room, no mic)

Positions 1000-6500 (low edge, 500 Hz steps, 5 bins each); per position 100 s each of drum and bass, female
speech, male speech, flute, and 150 s of synthetic traffic, at gains 16 / 64 / 256 on the 16-bit file (gains 1 and
4 trigger nothing: the file is far below the mic's level, so the gains bracket it). Accepted / rejected over all
positions: gain 16: 1 / 275; gain 64: 2 / 2,363; gain 256: 6 / 3,256. Of the 9 accepts, 7 are at 1000-3000 Hz
(drum and bass, female speech, flute), 2 at 3000-5500 (flute), none above 5500. The checks after the detector
stop all but a handful of thousands of noise occurrences; the low bands let more through. What level the room
and the mic give a real source is the bench part's question.

## Needs before it runs

- (done 2026-10-27) the sweep driver and the corpus band sweep: bandsweep.py.
- More recorded noise: the room corpus is 24 captures, 6 s after the marker. bench_audio.wav and synthetic
  traffic stand in on the host; a room recording of the file at the installation level would calibrate the gains.

## Open questions

- Which noise categories matter for the installation site (speech, music, traffic, birds)?
- One band for all nodes, or is a split band (DET-013 variation) back on the table if low bands are noisy?
- Does the idle call's 300 Hz announcement need to move with the band?
