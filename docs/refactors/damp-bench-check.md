# D-AMP bench check (step 3, issue #20)

Status: active. Bench 2026-10-09: 70 / 200 / 110 cm and an overnight tuning
run at 110 cm (section 5, 6). Next: owner reads section 6; 10 / 40 cm
rungs; COM6 amp wiring. Started 2026-10-09.
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

## 3. Piezo reference (recorded, bench:sessions/2026-10-08-issue7-*, bench 08e602f)

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
The three T3 runs are named in `bench/baselines.csv` (preliminary, see
section 4a).

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

## 4a. Preliminary review findings (Claude, 2026-10-09)

From a Claude Code review of the `bench` sessions (bench `08e602f`), checked
against the logs and the code; preliminary until the owner confirms. Notes
also in `bench:sessions/2026-10-09-issue19-damp-bringup/README.md`.

- Self-echo (plan item 3): detection suppression ends at toneOff (tail 0),
  before the own chirp fades (~135-155 ms after toneOn), but
  `refractoryAfterEmitMs=400` from toneOff (`ResonantBehavior.cpp`
  `notifyChirpFinished`) keeps the behavior from reacting to it; no
  own-echo verdict in the step 2 node log. The open risk is the echo
  counting as field activity and, with several emitters, step 6's
  suppression question, not a node re-triggering itself.
- `FAILED_NO_QUIET` (step 2 node run): not in this doc's inputs until now.
  One committed reading (`smooth=520` vs threshold 20); the Emitter was in
  AUTO during the node's whole 8 s quiet search, so the run cannot tell a
  high D-AMP floor from the setup. Add a node boot with the Emitter silent
  before item 3; until then outputs run without a quiet baseline.
- CPU load: no D-AMP run reports dropped DMA buffers. The Analyzer path
  costs 51-54 us of the 62.5 us per-sample budget (issue #26), and D-AMP
  adds TX on the same port. Record the dropped-buffer count in every
  ladder run (item 2); a run with drops is not comparable.
- Compare counts, not strength, against section 3 (piezo levels x2; the
  piezo fallback build still reads one bit late, `I2S_RX_MSB_ALIGN=0`).


## 5. Results

2026-10-09 70 cm (bench:sessions/2026-10-09-issue20-damp-70cm, Emitter COM6
-> Analyzer COM10, speaker faces mic, firmware 0566cc1, 0.1 FS): T3 46/50
expected (detector 50/50; 4 weakest dropped by the contrast/amp verdict),
piezo record 48/50; T2 0/50 (as piezo).

2026-10-09 200 cm (bench:sessions/2026-10-09-issue20-damp-200cm): T3 0/50 at
0.1 FS (onsets ~2000 < min peak 3000). The 0.4 FS run is invalid: the COM6
amp had gone silent.

2026-10-09 110 cm, COM6 emitting (bench:sessions/2026-10-09-issue20-damp-
110cm): invalid as detection data. COM6's amp/speaker produced no sound at
any level after the boards were moved (its own mic no longer heard its
beep); likely a loose breadboard wire. Its silent OBS controls (100
windows, 0 detections) stand.

2026-10-09 110 cm, roles swapped, overnight with the owner away (bench:
sessions/2026-10-09-issue20-damp-110cm-b, bench 39f8a9b; full table in its
README). Emitter COM10 -> Analyzer COM6; COM10 speaker -> COM6 mic
orientation not checked. Tuning variants on branch tune/issue20-damp (not
main).
- Level is the lever at 3200 Hz. 0.1 FS: tone ~40 dB over the floor, but
  peak ~1800 < min peak 3000 and amp evidence ~1060 < medium 2500 -> 0/50
  at stock settings; lowering only the detector thresholds gives detector
  50/50 and pattern 0/50 (amp). 0.3 FS: 50/50 at stock settings (strength
  5479-5721, amp evidence ~3320, i.e. +2.4 dB over medium).
- Frequency (stock Scalar, 0.3 FS): 2400 / 1600 / 1200 / 600 Hz 0/50
  (`duration_too_short`, strength 2175-2915), 800 Hz 0/50 (amp evidence
  1803), 300 Hz nothing; 4000 / 4800 / 5600 / 6400 Hz 50/50 each, strength
  9823 / 9160 / 12958 / 12636 vs 5588 at 3200 (+4 to +7 dB). About 3.6 dB of
  the 5600 Hz gain is First Difference's tilt (gain 0.89 vs 0.59).
- In both profiles the limiting check is amp evidence (broadband level);
  the tone-specific contrast evidence saturates (~32757). Scalar with the
  amp inspector at medium 1000 (variant): 50/50 at 0.1 FS / 5600 Hz with
  stock detector thresholds; at 3200 Hz it needs detector x0.5 and amp sits
  at ~1050 vs 1000 (no margin).
- TonalPulseFreq: stock score 18000 and inspector medium 12000 are
  unreachable on D-AMP. With score 2500 / release 1800 and inspector medium
  2500 (variant): 36/50 at 3200 Hz, 50/50 at 5600 Hz (0.3 FS); 0/50 at 0.1
  FS (AmpEnvelope weak).
- False positives: 0 detector accepts in ~790 silent windows (~38 min)
  across stock and lowered thresholds (down to x0.2 detector, variant
  inspectors) at 3200 / 4000 / 5600 Hz. Empty quiet building only.
- Node (4724fd6, nothing else sounding): `FAILED_NO_QUIET` again, smooth
  233-254 vs `kRbStartupQuietThreshold` 20: the D-AMP floor is above the
  piezo-era threshold, so the review's open question (4a) is answered: it
  is the floor, not the setup. Self-echo: every own idle chirp (5/5) is
  detected by the node's own mic as a valid pattern (~19.7k) and blocked
  only by `refractory_after_emit`; it counts as field activity.
- Dropped DMA buffers (4a): 21-22 per detected trial in `mode=detail`
  runs with SEQ diagnostics on, 0 in `mode=trial` / `mode=system`
  (g_drops_*). Corrected the same night: not D-AMP-specific. The clean
  piezo reference `V_scalar50` (#26) ran with `SEQ DIAG off` (772 bytes
  per trial vs ~6.9 KB here); #26 piezo runs with diagnostics on dropped
  too. No trial was hit (buffer_overrun 0). Tracked as ANA-004
  (roadmap-detection.md). The TonalPulseFreq rejections are not this:
  see R3 below (DET-010). Ladder runs use the #26 baseline
  settings: `SEQ DIAG off`, `mode=detail`.

## 6. Recommendations (owner decides; nothing changed on main)

1. Tone level 0.1 -> 0.3 FS (`I2S_TONE_AMPLITUDE`). Stock Scalar goes
   0/50 -> 50/50 at 110 cm; COM10's amp is linear at 0.3. Check every
   node's amp wiring first (COM6 went silent after a move).
2. Tone frequency: consider 4000-5600 Hz instead of 3200 (+4 to +7 dB
   margin, same false-positive result). Part of it is First Difference's
   tilt, which goes if the preprocessor changes after the field trial;
   higher tones beam more narrowly, so check off-axis before choosing.
   Lower frequencies are worse on this hardware. A decision file if taken
   (CHIRP_FREQUENCY_HZ, all nodes).
3. TonalPulseScalar thresholds: no change needed at 110 cm with 0.3 FS.
   For more range, lower the amp inspector (medium 2500 -> 1000) rather
   than the detector; contrast stays the discriminator. Only with a
   noise false-positive run (speech, doors) first.
4. TonalPulseFreq: if it stays, its thresholds need rescaling for D-AMP
   (score ~2500, inspectors medium ~2500); it showed no advantage over
   Scalar. Part of step 5 (DET-007).
5. `kRbStartupQuietThreshold` 20 is unreachable on D-AMP (floor ~240):
   set it from the D-AMP floor (e.g. ~400) or make it relative.
6. Own-emit: detection suppression until ~60 ms after toneOff would keep
   own chirps out of field activity (measured echo ends 33-54 ms after
   toneOff). Step 6's multi-emitter question.

Owner decisions on section 6 (2026-10-09): 1 yes, 2 no (stay at 3200 Hz),
3 yes but only after more research (noise false-positive runs first),
4 yes, 5 yes, 6 yes. Decision files: 1 `docs/decisions/2026-10-09-damp-
tone-level-0p3.md`, 2 `...-chirp-frequency-stays-3200.md`, 6 `...-own-emit-
suppression-whole-chirp.md`; 3 is an open row in docs/decisions/README.md;
4 and 5 are tuning values (code comments, this doc). 1, 5 and 6 landed in b6e99b0 (tone 0.3 FS; startup
quiet threshold 400 on D-AMP; own-emit detection suppression for the whole
chirp + 60 ms tail); compiles on all six envs, bench verification pending
(section 7, R1). 4 needs runs (R3), 3 needs noise runs (R4).

## 7. Run plan (from 2026-10-09)

```text
R0 [ ] Owner: fix the COM6 amp/speaker wiring; verify with the beep sketch
       (both boards hear their own beep).
R1 [ ] Verify b6e99b0 at 110 cm: Emitter -> Analyzer T3 stock, both
       directions, mode=system (expect 50/50 at 0.3 FS); Node quiet boot
       on each board with nothing sounding (expect "rebase done", not
       FAILED_NO_QUIET); Node idle chirps no longer produce own verdicts.
R2 [ ] Ladder at 0.3 FS, stock Scalar, SEQ DIAG off + mode=detail (the #26
       baseline settings; ANA-004): 10 / 40 / 70 cm (the
       piezo records) T3 + 50 silent OBS windows per distance; dropped
       buffers recorded. This is the #20 gate.
R3 [ ] TonalPulseFreq rescale (decision 4): at 110 cm, 0.3 FS, 3200 Hz,
       freqScore 2500 / 2000 / 1500 with the inspector variant
       (tune/issue20-damp), each with a silent OBS control; pick, then
       put the values in DetectionProfile.h.
R4 [ ] Noise false positives (decision 3, owner present): phone playing
       speech, then music, ~50 cm from the Analyzer; claps; a door. Silent
       OBS runs at stock vs the lowered amp inspector (medium 1000), and
       signal runs under the same noise (masking). Decide 3 from that.
```

2026-10-09 R3, TonalPulseFreq rescale (decision 4), 110 cm, 3200 Hz, 0.3 FS,
Analyzer tune/issue20-damp (inspector medium 2500), mode=system, 0 dropped
buffers: freqScore/release 2500/1800 -> 36/50, 2000/1400 -> 40/50,
1500/1000 -> 37/50; detector 50/50 each; 0 false positives in 3 x 50
silent windows. The score threshold does not matter in that range. The
10-14 rejections per run are DET-010 (peak-centered inspection taken
before its window has arrived: future_window_unavailable), not drops and
not the thresholds. Rescale applied on main: score 2500 / release 1800,
AmpEnvelope and FrequencyTarget inspectors 4000 / 2500 / 1200. Margin on
amp evidence is thin: AmpEnvelope median ~2818 vs medium 2500 at 110 cm.
