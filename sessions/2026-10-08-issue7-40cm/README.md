# 2026-10-08 issue #7, 40 cm

Purpose: T2/T3 baseline vs current, T6 profile switch, Item 2 diag on/off
(`docs/refactors/cleanup-0-plan.md`). Baseline firmware `847b1ef` (parent
of the first cleanup commit), current `62a949e`. Emitter `62a949e`, remote
mode via UART2. Vertical emit, vertical mic, 40 cm.

| Run | FW | Expected | Miss | Rejected | Det. accepted | Avg strength |
|---|---|---|---|---|---|---|
| T2_freq | 847b1ef | 0 | 50 | 0 | 0 | - |
| T2_freq | 62a949e | 0 | 50 | 0 | 0 | - |
| T3_scalar | 847b1ef | 14 | 0 | 36 | 50 | 4075 |
| T3_scalar | 62a949e | 10 | 0 | 40 | 50 | 4182 |
| T6a_freq | 62a949e | 0 | 30 | 0 | 0 | - |
| T6b_scalar | 62a949e | 30 | 0 | 0 | 30 | 4942 |
| T6c_freq | 62a949e | 0 | 30 | 0 | 0 | - |
| I2_diag_off_freq | 62a949e | 0 | 30 | 0 | 0 | - |
| I2_diag_on_freq | 62a949e | 0 | 30 | 0 | 0 | - |

Findings:

- TonalPulseFreq does not hear the emitter at 40 cm on either firmware
  (all miss, no detector activity). T2 and Item 2 are uninformative here;
  rerun at 10 cm (`2026-10-08-issue7-10cm`).
- TonalPulseScalar: detector accepts 50/50 on both firmware. Every
  rejection is `pattern_rejected` with `amp_class=weak`; strength sits on
  the weak/medium boundary (median 4035 vs 4139), so the 14 vs 10 pass
  count is acoustic variance, not a regression. T6b, at higher strength
  (median 4877), passed 30/30 with `amp_class=medium`.
- Field names: the only differences are the planned rename labels
  (`pattern_confidence` -> `verdict_confidence`, `SEQ_EXPLAIN`
  `pattern.*` / `integrity.pattern_*` -> `verdict.*` /
  `integrity.verdict_present`, `integrity.evaluator_report_present`). Line
  counts per type identical.
- T6: switching Freq -> Scalar -> Freq within one boot shows no stale
  state; each profile behaves as it does after a fresh boot.
- The first emitter remote claim after each analyzer boot times out; the
  runner spends it on a warmup chirp.
