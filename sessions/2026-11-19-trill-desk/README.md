# 2026-11-19 trill-only desk check

R1 (stripboard lr2, USB) and E2 (D-AMP, radio) at 1 m, gateway E0 on COM12. Window 2500-3500 Hz. Presets trill,
fieldtrill (one bin, shape check dropped by params, decisions/2026-11-19-trill-only-drops-the-shape-check.md) against
cricket. Noise: bench_audio.wav from the lab PC (Windows volume 100 %), speaker midway between the nodes, 40 cm off
their line, the same place as the earlier noise tests.

Two listening setups (hearingtest.py / falsealarm.py --listen):

- own (setup B, `hearing/`, `falsealarm/`): band_per_node 1, each node listens on as many bins as its call has steps.
  A one-bin call is then heard only by a node on the same bin.
- band (setup A, `hearing-band/`, `falsealarm-band/`): band_per_node 0, every node listens on the whole window.

## Heard (5-min blocks, 2 rounds, about 235 calls per row)

| Call | own: R1->E2 | own: E2->R1 | band: R1->E2 | band: E2->R1 |
|---|---|---|---|---|
| cricket | 100 % | 99 % | 100 % | 98 % |
| trill | 15 % | 16 % | 100 % | 98 % |
| fieldtrill | 20 % | 9 % | 100 % | 99 % |

The remaining 1-2 % under band are "detected" (the verdict line lost on the radio), none missed.

## False detections, nobody calling (20 min per preset and condition)

Zero valid verdicts in every block, both setups, both presets, quiet and noise. E2 had 1 (own) and 3-4 (band)
detector rejects in noise, all duration_too_short.

The noise is too weak to count: loudest input 1220-2390 against the onset of 2000, over the onset 0-4 s per
20 min. The noise half needs a louder rerun (falsealarm.py --conditions noise, the speaker turned up).

## Louder noise (`falsealarm-loud/`, evening, speaker turned up, every node on the whole window)

Noise blocks only, 2 x 20 min per preset. Now the noise counts: over the onset about half of every block (R1 ~670 s,
E2 ~970 s per 20 min), loudest 8200-16800.

| Preset | R1 valid (per 10 min) | E2 valid (per 10 min) | Rejects R1 / E2 |
|---|---|---|---|
| trill | 18 (4.5) | 21 (5.2) | 3004 / 6841 |
| cricket | 0 | 0 | 2808 / 6956 |

The trill-only setup fails the false-detection gate (about one per 10 min): the shape check is what keeps cricket at
zero. The false calls were short (75-110 ms, single_pulse). By the decision's "Revisit when", the next try is a short
climb at the end of the trill with the shape check kept.
