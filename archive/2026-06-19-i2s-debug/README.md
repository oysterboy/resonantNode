# 2026-06-19 I2S debug branch, archived (branches `i2s-debug` and `main-new`, same head 4a9d741)

Six commits from 2026-06-19 that never reached `main` (piezo era, First Difference preprocessor), kept here as a
patch series plus the pass report they produced, before the branches were deleted on 2026-10-17. Base: the
`main` commit they forked from (see the `From`/`In-Reply-To` headers; `git merge-base` at deletion time).

What they contain:
- `AudioSourceI2S`: selectable PHILIPS / LEFT_ALIGNED I2S mode, a warm-up before mode changes, block and
  aggregate Philips-boundary diagnostics (`src/hal/AudioSourceI2S.*`, `src/audio/AudioSource.h`).
- Analyzer: RAW capture extensions and commands for the raw I2S matrix (`AnalyzerRawCapture.cpp`,
  `AnalyzerCommands.cpp`).
- Logging campaign scripts: `tools/logging/raw_i2s_matrix.ps1`, `raw_philips_series.ps1`,
  `+ run_log001_campaign.ps1`.
- `docs/current-pass-report-2026-06-19.md` (copied next to this file) and edits to the then `docs/current-pass.md`.

The I2S framing work that did land took another route (D-AMP HAL, 2026-10-09, `docs/decisions/2026-10-09-damp-i2s-hal-shape.md`);
the raw CSV the branch deleted (`docs/raw_fixed_goertzel_sample_resolution.csv`) is still in `main`'s history.
To look at the code: `git am` the patches onto the base, or read them as diffs.
