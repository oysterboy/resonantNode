# 2026-10-15 DET-011: pulse trains with a 512-bin FeatureHistory (-D FEATURE_HISTORY_BINS=512)

Setup as 2026-10-15-pulses-wide (E2 Analyzer COM5, E3 Emitter PULSES 0.5 FS, 3,300 Hz, 90 cm, up), params
max_gap 130, max_duration 600, min_release_coverage 40, release_debounce 130 (Hdef_plain: defaults).
Analyzer RAM 31.7 % (26.1 % with 256 bins). Cause established first: the same 136 ms train (15 ms x3, gap 20) is
accepted with debounce 60 and rejected `pattern_rejected` with unknown classes at debounce 130 or 200, because the
inspector runs after the debounce and the peak is then older than the 256 ms history (2026-10-15-pulses-wide, H15_g20_deb*).

| cell | 256 bins (pulses-wide) | 512 bins |
|---|---|---|
| 15 ms x3, gap 20 | rejected, classes unknown | **10/10 expected**, amp medium, 136 ms event |
| 15 ms x3, gap 60 | rejected, classes unknown | **10/10 expected**, amp medium, 215 ms event, max gap 17 |
| 15 ms x3, gap 115 | rejected, classes unknown | 0/10 pattern_rejected, **amp weak** (326 ms event) |
| 10 ms x3, gap 60 | island_too_short | 0/10 longest_island_too_short (island 12 ms < 20) |
| plain chirp, default params | - | 10/10 expected, strong (unchanged) |

The history length was the blocker for gaps up to 60 ms. At 115 ms the inspector classes now exist but the amp
class is weak (its 100 ms window around the peak holds one pulse of three). False answers on speech / music with the
merged-train params not yet measured.
