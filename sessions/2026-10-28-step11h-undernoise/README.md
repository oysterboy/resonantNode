# 2026-10-28: step 11h, hits under noise (42f1eef)

Same boards and layout as 2026-10-27-step11h-sweep24, right after it. Per band: hop10 and chirp150 in quiet, then
bench_audio.wav played in full while the Analyzer runs hop10 back to back (`bandsweep.py bench --under-noise`), each
run labelled by the file section it started in; the silent Nodes listen throughout.

    python tools/bench/bandsweep.py bench --analyzer COM3 --nodes E1=COM8,E4=COM10,E5=COM11 \
        --out bench/sessions/2026-10-28-step11h-undernoise --positions 500,750,2950 --reference 0 \
        --noise-wav C:/Users/admin/Downloads/bench_audio.wav --under-noise

| band | quiet hop10 | under noise (12 runs of 10) | worst section | Nodes' valid verdicts during the noise (E1 / E4 / E5) |
|---|---|---|---|---|
| 500-1500 | 10/10 | 46 / 120 | flute 2/20, male speech 10/30 | 36 / 105 / 116 |
| 750-1750 | 10/10 | 76 / 120 | flute 6/20 | 59 / 116 / 115 |
| 2950-3950 | 10/10 | 120 / 120 (strength 14,230-14,340, as in quiet) | - | 120 / 120 / 120 |

Chirp150 0/10 at all three. Under noise the low bands lose trains to duration_too_long (the noise extends the
occurrence), failed_contrast and occurrence_seen_but_rejected: the music and speech energy sits in the band. The
shipped band does not notice the file at all.
