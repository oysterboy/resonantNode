# 2026-10-22: all five boards on Node fca77e7 (after step 7a), SELFTEST with the others silent

The Node build now has the 7a changes:
- console output in whole lines (app/SerialOut.h);
- the per-sample path in IRAM;
- the 2026-10-22 pacing and the 0.5 FS level.

Ports: E1 COM8, E2 COM3, E3 COM4, E4 COM10, E5 COM11, all in the trial layout. Method as in
2026-10-21-selftest-all5.

**All five pass SELFTEST.**

| board | amp tone, 0.5 FS | SNR min-max |
|---|---|---|
| E1 | 330,363 | 40.6-42 dB |
| E2 | 356,844 | 39.0-42 dB |
| E3 | 351,229 | 40.1-44 dB |
| E4 | 525,161 | 25.3-45 dB |
| E5 | 472,515 | 39.5-44 dB |

At boot every board prints `RB hop_check status=warn` (expected).

After the run, all five boards were held silent again by the background process on the lab PC.
