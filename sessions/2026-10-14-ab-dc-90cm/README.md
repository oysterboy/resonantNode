# 2026-10-14 step 5a item 2b (#35): preprocessor A/B at 90 cm (code bfbe0c8)

E2 Analyzer, E3 Emitter 0.5 FS, 90 cm, facing up, 3,200 Hz, Scalar, window and doors closed,
Roomba off. Sides: 2026-10-14-ab-fd-90cm (First Difference, -D PCM_PREPROCESS_MODE_VALUE=1),
this folder (DC blocker, 100 Hz corner, output x0.588, the new default).
SELFTEST sessions: 2026-10-14-selftest-ab-fd-90cm, -selftest-ab-dc-90cm.

| | First Difference | DC blocker |
|---|---|---|
| T3 (10) | 10/10, amp strong, strength 9,390-9,490, duration ~148 | 10/10, amp strong, strength 9,570-9,860, duration 135-155 |
| SELFTEST E2 amp | snr 47.8 dB, tone 231,600, floor 936 | snr 35.4 dB, tone 242,226, floor 4,101 |
| SELFTEST E2 quiet_boot, mic | PASS | PASS |

The x0.588 scaling keeps the chirp strength within ~3 % (same thresholds apply).
The mic floor in the quiet SELFTEST window is ~4.4x higher with the DC blocker
(First Difference attenuates the 100-1000 Hz band the blocker passes), so the SELFTEST
SNR drops by ~12 dB. Silence, speech and music with the new filter not yet run.
