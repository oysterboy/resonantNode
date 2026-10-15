# 2026-10-15 5b: 5-step hop at 110 cm with the amp medium limit at 1,500 (experimental build)

Analyzer built with `-D SCALAR_AMP_MEDIUM_THRESHOLD=1500.0f` (default 2,500), 512-bin history, band 2800-3800, boards up at 110 cm, quiet room.
Hop 2800,3050,3300,3550,3800, 20 ms pulses.

| cell | result (limit 1,500) | reference (limit 2,500, 2026-10-15-gate5-110cm) |
|---|---|---|
| gap 5 ms | 10/10, amp medium | 0/10 (amp weak) |
| gap 15 ms, scalar_max_duration_ms 300 | 10/10, amp medium | 6/10 |
| plain 3,200 Hz chirp | 10/10 | 10/10 |

False answers with this limit: 2026-10-15-ampthr1500-noise (music 18/50).
