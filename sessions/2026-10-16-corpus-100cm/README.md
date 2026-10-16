# 2026-10-16 capture corpus, 100 cm (tools/bench/rawcorpus.py)

Same capture setup as 2026-10-16-corpus-60cm (mode=both, 50 ms pre, 250 ms post, combined build, E3 0.5 FS). pos0 = the placement of 2026-10-16-hopcand-100cm, captured right after its cells.

| f (Hz) | pos0 |
|---|---|
| 2800 | 31,945 / 36,270 |
| 3200 | 12,288 / 13,263 |
| 3300 | 29,490 / 29,911 |
| 3800 | 27,652 / 27,879 |

3,200 Hz weak (a third of the others): the plain chirp was 3/10 here (duration_too_short). The hop bins are all strong; the hop5 0/10 at this placement was `scalar_max_duration_ms` 200 against a 214 ms train, not the comb (10/10 with the limit at 300).
