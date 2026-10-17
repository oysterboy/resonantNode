# 2026-10-17 slot 1 (unsupervised): is E3 a bad listener? No: deafness wanders, and after the replug no sound crosses

Boards as left on 2026-10-16 evening (70 cm, facing up) until the owner replugged the USB cables mid-session (ports
changed: E2 COM5 -> COM3, E3 COM5 -> COM4). E3 on the Node build a5a9bbb throughout (needs BOOT to flash); E2 on the
Node build 3952521 (merged main), briefly a5a9bbb, then the diagnostic build; Analyzer at the end.
`tools/bench/nodepair.py` runs; `--kick a` = SELFTEST chirps from E2, `--kick none` = idle voice only.

| Run | E2 build | A=E2 emits -> B=E3 verdicts | B=E3 emits -> A=E2 verdicts | note |
|---|---|---|---|---|
| pair_kickA_60s | 3952521 | 3 kicks -> 3 (+1 idle) | 4 -> 2 | E3 hears again; E2 misses E3's answers to its own SELFTEST chirps |
| pair_idle_600s (10 min) | 3952521 | 29 -> 25 | 38 -> 3 | **E2 deaf** for ten minutes |
| pair_idle_oldbuildA_180s | a5a9bbb | 11 -> 5 | 9 -> 8 | E2 hears on the old build |
| pair_idle_newbuildA_180s | 3952521 | 13 -> 5 | 13 -> 12 | E2 hears on the new build too: no regression |
| throughput (60 s x 3, `RB STATUS`) | 3952521 | - | - | stream whole (99.9 %); dropped DMA buffers 24 -> 46 -> 62 (0.35/s) with and without logging |
| pair_idle_status_1200s (aborted at 348 s) | 3952521 | 22 -> 6 | 21 -> 15 | **E3 deaf** from minute 4 |
| (owner stopped the sounds, replugged the USB cables) | | | | |
| pair_idle_suppress_600s | diag build | 30 -> **0** | 30 -> **0** | **both deaf**; E2's new fields: suppressed_now=0 at every poll, suppressed samples = exactly its own idle emissions, sample clock 58 ms behind the system clock (constant), dropped 20/min, chirp never stuck active |
| OBS_e3chirps (E2 Analyzer, 30 windows) | Analyzer | E3 10 SELFTEST chirps -> 2 candidates of 4-7 ms, strength 2,200-2,500 | | almost no sound crosses after the replug |

Reading:
1. E3 is not a bad listener. Deafness hit E2 for ten minutes, then E3 for the end of a run, then both; it is not
   tied to a board or a build (E2 heard on both builds), and when a node is deaf its own suppression, clock and chirp
   state look normal (diagnostic fields, `pair_idle_suppress_600s`).
2. After the replug nothing crosses: E3's chirps reach E2's Analyzer as 4-7 ms fragments at strength ~2,400, where
   the same path gave 5,000-9,000 and full chirps on 2026-10-16. The placement must have changed (orientation,
   distance, something on a speaker). To be verified by the owner before any further listening test.
3. Still open and real: a Node on the bench loses DMA buffers at 0.3-0.4 per second (8 ms each) in every run today,
   logging on or off, while the Analyzer batteries of 2026-10-16 counted zero. And the wandering deafness before the
   replug is not explained; the suppression diagnostics rule out the own-emit path for E2's case only.
4. Side note: a SELFTEST chirp as a kick has the self-test's own 300 ms echo window, so the kicking node cannot hear
   the answer; a real network-signal command on the Node is the better kick.
