# 2026-10-16 5b: bench_audio.wav, one OBS pass (band 2800-3800), level matched to WDR5 by band energy

E2 Analyzer COM5 experimental build (HISTORY 512, amp medium 1500, b22b035), E3 Emitter idle, 110 cm, boards up. Speaker 50 cm from E2, 90 deg. Level: 2026-10-16-audiolevel-calib (file band -83.1/-82.2/-82.8 dBFS vs WDR5 -81.7/-82.6; broadband 5-10 dB quieter than WDR5).
Playback started unix ms 1792139859159; SEQ OBS window 1 at about 18.5 s into the file (offset used for sections: 18.5 s, +-1 s). Sections: tools/bench/bench_audio.sections.txt.

222 windows: 211 miss, 11 rejected by the detector (duration_too_short 8, coverage_too_low 3, duration_too_long 1), 0 occurrences reached inspection. Per section (rejected of 46-50 windows): music 2, speech female 1, speech male 3, flute 5. No valid answer anywhere.
Single pass; at this level the file is a weaker stressor than the radio music of 2026-10-15 (15 of 50 windows reached inspection).
