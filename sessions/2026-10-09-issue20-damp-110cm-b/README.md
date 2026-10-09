# 2026-10-09 issue #20: D-AMP at 110 cm, roles swapped - level, frequency, thresholds, false positives

Owner away; brief: tune for good detection without false positives,
recommend only (nothing changed on `main`), try lower frequencies.

## Setup

- 110 cm. Emitter **COM10** (MAC 24:dc:c3:49:b7:38) -> Analyzer **COM6**
  (MAC 24:dc:c3:4a:b0:50). Roles swapped from the 70/200 cm sessions
  because COM6's amp/speaker went silent after the boards were moved (see
  bench:sessions/2026-10-09-issue20-damp-110cm). Boards were placed for
  COM6 -> COM10 ("speaker faces mic, same height"); the COM10 speaker ->
  COM6 mic orientation was not checked.
- Firmware: Analyzer `943ec31` (main) for `p0_`-`c_`; Analyzer
  `tune/issue20-damp` `8669838` for `d_`-`g_` (tag `tune-issue20-8669838`;
  the same diff rebased is `07b44d0`): Scalar amp inspector thresholds
  5000/2500/1000 -> 2000/1000/500, Freq AmpEnvelope and FrequencyTarget
  inspectors 18000/12000/8000 -> 4000/2500/1200. Emitter `943ec31` /
  `4724fd6` with `-D I2S_TONE_AMPLITUDE=` 0.1 (default) or 0.3 as named.
- Detector thresholds changed at runtime with `PARAM` (in each run's
  `>>` lines). x05 = scalar onset/release/min-peak 1000/750/1500, x03 =
  600/450/900, x02 = 400/300/600.
- Silent false-positive controls: `SEQ OBS` with the Emitter held in
  REMOTE (nothing emitted); any detector accept is a false positive.

## Results (50 trials unless noted; `campaign` table in the run names)

Signal at 0.1 FS, 3200 Hz: 3200 Hz amplitude ~21k at the mic vs ~100
floor (RAW). Feature stream (`RAW_feat_3200_a010`): FrequencyTarget peaks
~1700-1850 vs ~15-100 floor. 0.3 FS: ~64k (linear, no dropout).

| Run | Expected / 50 | Note |
|---|---|---|
| a_def_T3 (stock, 0.1 FS) | 0 | detector never fires: peak ~1800 < min 3000 |
| a_x05/x03/x02_T3 (0.1 FS) | 0 | detector 50/50; all pattern-rejected: amp evidence ~1060 < medium 2500 (contrast saturated, ~32757) |
| **b_a030_def_T3 (stock, 0.3 FS)** | **50** | strength 5479-5721; amp evidence ~3320 (medium 2500) |
| b_a030_def_T2 (stock Freq, 0.3 FS) | 0 | Freq score threshold 18000 unreachable |
| c_f2400 / 1600 / 1200 / 600 (stock, 0.3 FS) | 0 | detector rejects `duration_too_short`, strength 2175-2915 |
| c_f800 | 0 | detector 50/50, amp evidence 1803 < 2500 |
| c_f300 | 0 | nothing |
| **c_f4000 / 4800 / 5600 / 6400 (stock, 0.3 FS)** | **50 each** | strength median 9823 / 9160 / 12958 / 12636; amp 6247 / 5191 / 7216 / 5609 |
| d_T3v_def_f5600 (variant, stock detector, 0.1 FS) | 50 | amp evidence ~2296: passes variant medium 1000, would fail stock 2500 |
| d_T3v_x05_f3200 (variant, 0.1 FS) | 50 | amp ~1050 vs variant 1000: no margin |
| d_T3v_x05_f5600 (variant, 0.1 FS) | 50 | |
| d_T2v_s1000_f3200, d_T2v_s2500_f5600 (Freq variant, 0.1 FS) | 0 | detector 50/50, AmpEnvelope evidence weak (~965 / ~2064 < 2500) |
| e_T2v_s2500_a030_f3200 / f5600 (Freq variant, 0.3 FS) | 36 / 50 | |

False positives (silent windows, detector accepts): a_def_OBS 50,
a_x05/x03/x02_OBS 150, c_f4000/5600_OBS 100, d_T2v_s1000_OBS 101,
d_T3v_x05_OBS 142 (cut by the runner's timeout, no summary line),
f_soak_T3v_x03 143 (same) - **0 in ~690 windows** here, plus 100 in the
first 110 cm session. Empty, quiet building only; no speech, doors, HVAC
transients tested.

Node (`node_quiet_boot_selfecho`, Node `4724fd6` on COM10, 0.1 FS, nothing
else sounding): startup still `FAILED_NO_QUIET` (smooth 254 vs threshold
20; steady 233 later) - the D-AMP floor itself is above the piezo-era
quiet threshold. Each of 5 idle chirps was heard by its own mic as a valid
pattern (strength ~19.7k, ~0.9 s after emit start = the idle pattern's
second, 3200 Hz pulse) and blocked only by `refractory_after_emit`.

Dropped DMA buffers: 21-22 per *detected* trial in `mode=detail` runs
(0 in runs without detections); `g_drops_*`: 0 with `mode=trial` and
`mode=system`, 198 / 10 trials with `mode=detail`. Analyzer report
output, not the detection path; no trial was marked buffer_overrun.
Correction (same night): not D-AMP-specific. Piezo `V_scalar50` (#26) ran
with `SEQ DIAG off` (772 bytes per trial vs ~6.9 KB here); #26 piezo runs
with diagnostics on dropped too. Tracked as ANA-004 on main. (The 14
rejections in `e_T2v_s2500_a030_f3200` are not drops: they recur with 0
drops in `h_T2v_*`; inspector timing, DET-010.)

## R3: TonalPulseFreq rescale (later the same night)

Emitter COM10 at the new default 0.3 FS (main b6e99b0), Analyzer
tune/issue20-damp 8669838, 3200 Hz, mode=system (0 drops):

| Run | Expected / 50 | Silent control |
|---|---|---|
| h_T2v_s2500 (2500/1800) | 36 | 0 / 50 |
| h_T2v_s2000 (2000/1400) | 40 | 0 / 50 |
| h_T2v_s1500 (1500/1000) | 37 | 0 / 50 |
| h_T2v_s2000_detail_diagoff (20 trials) | 15 / 20 | - |

Detector 50/50 in each; the rejected trials are 146-149 ms occurrences
whose peak-centered inspection was taken before its window arrived
(`future_window_unavailable`, DET-010 on main).
