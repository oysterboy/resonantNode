# 2026-10-16 "sweepcheck" at 70 cm: NOT a sweep test, E3 still ran the old Emitter build

Intended as the first hardware run of the in-chirp sweep (`tools/bench/sweepcheck.sh`), but the E3 flash right before it
failed (`esptool: Wrong boot mode detected (0x13)`, the board did not enter download mode on its own), so E3 kept the
Emitter build without `sweep=` and played the `freq=` tone steadily for `dur=`. The cells are therefore single tones on
the band-edge bins, 100 ms (or 150 ms) each, with E2 on the shipped build 5ba805d, band 2950-3950, 70 cm, 10 trials:

| Cell (as run) | expected | amp P75 | contrast P75 |
|---|---|---|---|
| 2,950 Hz, 100 ms (meant: sweep up 2950->3950) | 10/10 | 2,775-2,858 | 32,754-32,755 |
| 3,950 Hz, 100 ms (meant: sweep down) | 10/10 | 2,061-2,318 | 32,737-32,745 |
| 2,950 Hz, 150 ms (meant: sweep up, 150 ms) | 10/10 | 2,665-2,769 | 32,755-32,756 |
| 3,200 Hz, 100 ms (reference) | 10/10 | 3,920-4,431 | 32,754-32,757 |

Useful as such: the two edge bins of the new band are heard 10/10 at this placement. The real sweep run needs E3
flashed with BOOT held during the upload (session to follow).
