# 2026-10-08 issue #7, 10 cm

Purpose: T2/T3 baseline vs current and Item 2 diag on/off
(`docs/refactors/cleanup-0-plan.md`) at a distance where the emitter is
loud. Baseline firmware `847b1ef`, current `fbbc2db` (same firmware source
as `62a949e`; the commits between them touch only docs and tools).
Emitter `62a949e`, remote mode via UART2. Vertical emit, vertical mic, 10 cm.
First session recorded with `tools/bench/seqrun.py`.

| Run | FW | Expected | Miss | Rejected | Det. accepted | Strength median |
|---|---|---|---|---|---|---|
| T2_freq | 847b1ef | 0 | 50 | 0 | 0 | - |
| T2_freq | fbbc2db | 0 | 50 | 0 | 0 | - |
| T3_scalar | 847b1ef | 38 | 0 | 12 | 50 | 16481 |
| T3_scalar | fbbc2db | 50 | 0 | 0 | 50 | 14247 |
| I2_diag_off_freq | fbbc2db | 0 | 30 | 0 | 0 | - |
| I2_diag_on_freq | fbbc2db | 0 | 30 | 0 | 0 | - |

RAW feature capture (`RAW_feat_3200`, 3 triggered chirps, current fw):
peak `freq_score` 15642-16020, peak `amp_strength` 16894-17386.

Findings:

- **TonalPulseFreq cannot open on this hardware.** Its attack threshold is
  `frequencyMatch.attackScoreMin = 18000` (`DetectionProfile.h`); the
  measured peak score is about 16000 at 10 cm and about 7000 at 70 cm. The
  detector never produces a report on either firmware, so T2 and the Item 2
  diag on/off comparison cannot be evaluated until that profile is retuned
  (a tuning pass, not part of issue #7) or the emitter gets louder. Bears
  on issue #8 (which profile the Node ships).
- **TonalPulseScalar: firmware difference found.** Baseline rejects 12/50
  at strong signal; 11 of them because both inspections returned
  `reject_reason=history_window_incomplete` with an empty available window
  (`available_start_ms=0 available_end_ms=0`). The same failure appears in
  the baseline at 40 cm (3 trials) and 70 cm (2 trials). Current firmware
  shows it in 0 of 130 scalar trials across all sessions. Behavior changed
  between `847b1ef` and `62a949e`, in the good direction, but the cleanup
  phases were meant to be pure refactors; the responsible commit is not yet
  identified. FeatureHistory changed in `48e4f90` and `795f649` (Phase 5c).
- Field names: only the planned pattern -> verdict rename labels differ,
  same as at 40 cm.
