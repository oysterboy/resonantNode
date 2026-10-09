# D-AMP bench check (step 3, issue #20)

Status: active, planning the bench session. Started 2026-10-09.
Roadmap: `docs/roadmaps/roadmap-0-steps.md` step 3 -> NODE-010
(`roadmap-node.md`). Decisions: `docs/decisions/2026-10-06-damp-output-hardware.md`,
`docs/decisions/2026-10-09-discontinue-piezo.md`.
Follows: `docs/refactors/archive/damp-board-support.md` (step 2).
Next: step 4, switch all five nodes to D-AMP (issue #21).

Measure, don't tune: thresholds, suppression windows and the tone level are
recorded here; changing `DetectionProfile.h` / `BehaviorProfile.h` is its own
pass (LOG-001 workflow), and the production profile is step 5 (#8).

---

## 1. Goal and gate

Is D-AMP at least as detectable as the piezo was, and does a node's own
chirp stay inside its suppression window?

Gate (issue #20): D-AMP detected at every ladder distance at least as well
as the recorded piezo runs; self-echo inside the suppression window, or the
needed window measured. If D-AMP is a dud, go back to piezo (`BOARD_PIEZO`)
and revisit the decision.

## 2. Inputs folded in from issue #20

Body (rescoped 2026-10-09): ladder with D-AMP emitter -> D-AMP listener,
accept rate and score/contrast from the Analyzer, compared with the piezo
runs in `bench/`; self-echo vs `suppressSelfChirp` /
`detectionSuppressTail`; class-D noise at the mic with the amp idle; pick
the tone level for installation distances; results as a dated line in
NODE-010.

Comment (2026-10-09, from step 2): toneOn -> own mic 29-36 ms (median 32),
toneOff -> quiet 33-54 ms (median 37); a 100 ms chirp is heard on the
node's own mic ~30 to ~135-155 ms after toneOn, ~44 dB over the floor.
Current windows (`behaviorSuppressSelfChirpMs=100`,
`detectionSuppressTailMsOwnEmit=0`) end before that.

## 3. Piezo reference (recorded, bench:sessions/2026-10-08-issue7-*)

TonalPulseScalar (T3, 50 trials), trials judged `expected`:

| Distance | firmware | expected / 50 |
|---|---|---|
| 10 cm | 847b1ef / fbbc2db | 38 / 50 |
| 40 cm | 847b1ef / 62a949e | 14 / 10 |
| 70 cm | 847b1ef | 48 |

TonalPulseFreq (T2) at profile defaults: 0/50 at every distance ("not
evaluable", cleanup-0-plan.md). The piezo numbers are not monotonic in
distance (setup and orientation varied between sessions), so they are a
floor to beat, not a curve. Strength values are not comparable: piezo
firmware read the mic one bit late (levels x2) and its square wave put
aliased harmonics in band.

## 4. Plan (draft)

```text
1. [ ] Distances: 10 / 40 / 70 cm (the piezo records) plus 100 / 150 cm
       (beyond the piezo range). Top rung = the installation spacing
       (owner to give).
2. [ ] Per distance, D-AMP Emitter (COM6) -> D-AMP Analyzer (COM10):
       T3 = SEQ start profile=TonalPulseScalar tries=50 mode=detail
       when=all verbose=1; T2 the same with TonalPulseFreq. seqrun.py,
       one session folder per distance.
3. [ ] Self-echo: Node on one board, own chirps vs the windows (section 2
       numbers give the expected answer).
4. [ ] Class-D idle noise: from the step 2 captures (amp idle, TX zeros:
       100 Hz-8 kHz at -76..-92 dBFS); a new capture only if the ladder
       floor looks worse.
5. [ ] Tone level: 0.1 FS by default; if the far rungs fail, one run at a
       higher level to see whether level or detector is the limit.
6. [ ] Results line in NODE-010, close #20.
```

## 5. Results

(dated lines here)
