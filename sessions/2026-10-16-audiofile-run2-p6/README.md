# 2026-10-16 5b: bench_audio_p6.wav (louder), one OBS pass

Same setup as 2026-10-16-audiofile-run1; speaker regler unchanged; file built with `--rms-dbfs -18 --peak-safe` (sections at -19.4 music, -21.5 speech female, -18.7 speech male, -18.0 flute dBFS RMS; +4.6, +2.5, +5.3, +6 dB against run1). Playback start unix ms 1792140763988, section offset about 19 s.

222 windows: 207 miss, 15 rejected by the detector (all duration_too_short), 0 reached inspection, 0 valid answers. Per section (rejected of 46-50): music 8, speech female 5, speech male 2, flute 0 (run1: 2, 1, 3, 5). Single pass.
