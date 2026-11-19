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
