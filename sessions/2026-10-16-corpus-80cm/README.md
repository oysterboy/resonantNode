# 2026-10-16 capture corpus, 80 cm (tools/bench/rawcorpus.py)

Same capture setup as 2026-10-16-corpus-60cm (mode=both, 50 ms pre, 250 ms post, combined build, E3 0.5 FS). pos0 = the placement of 2026-10-16-hopcand-80cm, captured right after its cells.

| f (Hz) | pos0 |
|---|---|
| 2800 | 31,796 / 27,443 |
| 3200 | 32,593 / 38,264 |
| 3300 | 9,667 / 9,852 |
| 3800 | 29,318 / 29,475 |

3,300 Hz in a null (a third of the others). In the hop cells this is the step that hop5 survives 9/10 (one contrast-weak reject) and that splits hop4's train (6/10, duration_too_short).
