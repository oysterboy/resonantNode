# 2026-10-16 Node build on hardware: band 2950-3950, per-node hop train, 70 cm

E2 flashed to the Node build aa2f5c9 (17:02; shipped defaults, `detection.band_low_hz/high_hz` 2950-3950, hop train
25 ms x 5 gap 10 on the band bins, start bin from the MAC). Boot: `RB node_id=0xE84C4BC3DC24`,
`RB signal band=2950-3950 bins=5 hop=on rotation=4 steps=5 pulse_ms=25 gap_ms=10 order=3950,2950,3200,3450,3700`.
E3 Emitter f2dccc2 in AUTO (plain 3,200 Hz chirp, 100 ms, every 2 s, 0.5 FS), boards facing up at 70 cm.
`tools/bench/nodecount.py --mode chirps --secs 100`; E3's chirps counted from its `OK CHIRP` lines because its
`EMIT_*` marker lines arrive garbled on its USB log while E2 runs the Node build (E3 reads junk on the control UART:
`EVT emitter_cmd line=...`; the chirps themselves are unaffected).

| Run (100 s) | E3 chirps | Node verdicts | valid | Node answers (hop train) | idle chirps |
|---|---|---|---|---|---|
| N_chirps_100s (right after the flash) | 50 (tool: 0, garbled markers) | 0 | 0 | 0 | 5 |
| N_chirps_100s_r2 (after `RB log full` again, 15 min later) | 50 (61 OK lines incl. the warm-up) | 51 | 51 | 51 (`RB emit shape=hop ... order=3950,2950,3200,3450,3700`) | 0 |

Between the two runs a 30 s watch with the log on showed 15 chirps -> 15 valid verdicts -> 15 hop answers
(about 120 ms from heard to emit start). `RB STATUS` after the runs: `i2s.dropped_dma_buffers=0` over 850,000
samples, band and hop fields as configured. **The first run's 0 is not understood**: the Node printed only its idle
emits for 100 s although outputs were enabled (idle chirps fire only then). Candidate: the startup baseline with E3
chirping every 2 s (`RB rebase waiting ...`, `baseline_ok=False` in both runs: the tool never saw `RB baseline active`).
Repeat on a fresh boot with the log on from the first second before trusting a Node right after power-up.

Findings: the Node hears a plain on-bin chirp in band mode and answers with its hop train, every time, at 70 cm;
the 5-bin bank fits the Node's per-sample budget (no dropped buffers). Not measured here: a Node hearing another
Node's hop train (needs two Node builds), the Node's own hop through its suppression (no self-triggering seen:
50 chirps, 50 answers, nothing extra), noise on the Node build.
