# 2026-11-05 evening: the second labelled set (C920), more kinds of whisper

C920 as before (device 1, 24 kHz), plan ../2026-11-05-desk-plan/labels2.json, the owner at the desk, all five boards
silent meanwhile (output.silent 1 by radio, restored after). The owner skipped one section: what he did at beep 9 was
the chair (planned 10), at 10 the paper, at 11 the clothes, at 12 the typing (planned 9). The WAVs were renamed to what
was done; the schedule.json still has the planned names.

labelfeat2.py (windows of 0.5 s whose 1.5-4.5 kHz band is >= 3 dB over silence.wav's; excess in dB, p10 / p50 / p90):

| label | windows | mid 1.5-4.5 kHz | low 200-700 Hz | 5-8 kHz minus mid |
|---|---:|---|---|---|
| whisper soft 0.5 m | 58 | 4 / 6 / 12 | 2 / 6 / 8 | -9 / -6 / 1 |
| whisper normal 0.5 m | 73 | 4 / 10 / 13 | 6 / 8 / 12 | -9 / -4 / 3 |
| whisper strained 0.5 m | 92 | 7 / 14 / 19 | 7 / 11 / 15 | -7 / -1 / 4 |
| whisper 1 m | 75 | 5 / 11 / 15 | 6 / 9 / 12 | -11 / -5 / 1 |
| whisper 2 m | 83 | 7 / 11 / 17 | 3 / 6 / 9 | -15 / -9 / -5 |
| whisper words 1 m | 44 | 3 / 10 / 17 | 2 / 6 / 10 | -14 / -7 / -1 |
| speech soft 1 m | 54 | 4 / 7 / 10 | 5 / 13 / 18 | -11 / -8 / -1 |
| breath 0.5 m | 54 | 6 / 19 / 31 | 1 / 12 / 25 | -12 / -7 / -2 |
| chair | 87 | 4 / 8 / 18 | 2 / 7 / 17 | -7 / -5 / -3 |
| paper | 89 | 7 / 24 / 30 | 0 / 9 / 16 | -3 / -2 / -1 |
| clothes | 93 | 6 / 14 / 24 | 4 / 11 / 18 | -8 / -5 / -2 |
| typing | 77 | 6 / 11 / 20 | 6 / 13 / 22 | -2 / 3 / 5 |

Reading: the low-band excess separates the whispers (median 6-9, strained 11) from soft speech and typing (13) and
roughly from breath (12), but not from the chair (7), paper (9) and clothes (11) of this set, which in the first set
(2026-11-05-labels-c920) read 12-14 there. So whisper.timbre_check 1 with low_max_db 9 alone is not enough; a second
feature or the rhythm is needed. The 5-8 kHz share (typing +3, paper -2, whispers -4..-9) helps against typing and
paper only. labelrec.py's own table dropped every soft whisper ("nothing loud": 2x the room's rms) - the room-relative
bands above are the useful view, and a soft whisper at 0.5 m is only ~6 dB over the room in the mid band.
