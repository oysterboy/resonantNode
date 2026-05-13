# Lab Notes

## 2026-05-13

RB timing change:

- previous: `waitAfterTransient = 100 ms`
- previous: `behaviorSuppressSelfChirp = 150 ms`
- current: `waitAfterTransient = 100 ms`
- current: `behaviorSuppressSelfChirp = 200 ms`

RB log note:

- `RB log minimal` is accepted and behaves like `RB log off` for live output.

Detector ownership note:

- `AudioOnsetDetector` is now owned by the node / analyzer layer instead of `AudioSignal`.
- `AudioSignal` keeps the signal conditioning and candidate packaging path, but it now uses an injected detector.

Detector tuning note:

- previous: `onset = 30`
- previous: `release = 20`
- current: `onset = 23`
- current: `release = 20`

Baseline note:

- current baseline: `5 nodes`
- status: `work in progress`
