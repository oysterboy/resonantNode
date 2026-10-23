# 2026-10-23: DET-017, min duration 92 ms with deferred inspection

Setup: trial layout. E2 (COM3) Analyzer and E3 (COM4) Emitter on main e58c4c2, 110 cm apart. E1/E4/E5 (Nodes
8ec0764) held silent with ports open. SEQ, 10 trials per cell, TonalPulseScalar shipped values (min 92 ms).

| cell | result | read as |
|---|---|---|
| beep 40 ms (+ room reflection) | 10/10 rejected, `duration_too_short` | 84-86 ms |
| beep 60 ms | 10/10 expected | 107-109 ms |
| beep 80 ms | 10/10 expected | 132-135 ms |
| chirp 100 ms | 10/10 expected | 96-154 ms |
| hop train (25 x 5, gap 10, band) | 10/10 expected | 195 ms |

Host side (before the bench; commit e58c4c2): the notch sweep (5 bins x 2 depths x 3 widths + unnotched, 33
variants of the 70 cm hop capture) keeps 33/33 single-null trains at min 88-95 (shortest 97 ms). At 100 it loses
the deep 3450 Hz null (31/33). The 40 ms beep captures (2026-10-22-7d-dur40-raw) are rejected from min 88 up.
T2h on the corpus is byte-identical.

Reading: the 40 ms beep is now kept out by the duration gate, by design (it used to fail only through audit 3.1's
early inspection). Beeps of 60 ms and more are heard. The min duration cannot keep them out without losing hop
trains with a null (readings overlap: decisions/2026-10-23-node-hears-the-hop-train-only.md). That needs the
shape inspector, DET-015.
