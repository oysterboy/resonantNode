# 2026-11-05 labelled recordings, Logitech C920 webcam mic

PC mic = C920 (sounddevice device 1, MME, 24 kHz resampled; the camera itself stops at ~8 kHz and applies its own
level processing: some whisper seconds are quieter than the silence). Webcam next to E3, E3 silent
(whisper.enabled 0 live). Plan ../2026-11-05-desk-plan/labels.json, owner at the desk. A phone notification
~10-15 s into whisper_05m (cut 9-17 s in the analysis).

labelrec.py's own table (windows > 2x room rms) drops nearly every whisper window: on this mic a whisper is only
~4 dB above the room in total, because the room's power is low-frequency. Per band against silence.wav
(labelfeat.py, and the 0.5 s windows with > 6 dB excess in 1.5-4.5 kHz):

| label | windows | 1.5-4.5 kHz excess dB p10/50/90 | 200-700 Hz excess | 5-8 kHz minus mid |
|---|---:|---|---|---|
| whisper 0.5 m | 71 | 7 / 10 / 14 | 2 / 6 / 9 | -9 / -7 / -1 |
| whisper 1 m | 51 | 6 / 9 / 12 | 2 / 5 / 6 | -3 / 2 / 8 |
| whisper 2 m | 24 | 6 / 7 / 13 | 2 / 4 / 8 | -9 / 0 / 3 |
| speech 1 m | 68 | 7 / 12 / 18 | 12 / 21 / 27 | -6 / 2 / 11 |
| typing | 68 | 6 / 9 / 17 | 11 / 15 / 22 | 3 / 10 / 12 |
| chair | 66 | 8 / 12 / 24 | 5 / 13 / 23 | -1 / 2 / 6 |
| paper | 107 | 10 / 20 / 27 | 5 / 12 / 19 | 0 / 5 / 7 |
| clothes | 70 | 9 / 17 / 25 | 8 / 14 / 21 | 0 / 3 / 6 |

Reading: a whisper is a moderate bump in 1.5-4.5 kHz with almost nothing added below 700 Hz; every other label
adds 12-21 dB (median) there. Paper is the closest (low p10 5) but louder in the middle band. Not tested on the
board's mic yet; one speaker, one room, one camera with its own processing.
