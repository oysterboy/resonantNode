# 2026-10-16 capture corpus, noise: phone radio speech and music at the 110 cm placement (tools/bench/rawcorpus.py)

Phone radio 50 cm from the E2 mic, 90 degrees, full volume, as in 2026-10-16-radio-phone and -radio-phone-combined: WDR5 speech (`radio_speech_r0..11`), then Spotify drum and bass (`radio_music_r0..11`). Each capture is a RAW trigger with a 20 ms marker tone at 3,200 Hz (`dur=20 pre=0 post=350 mode=both`): 350 ms = 5,600 samples, of which the first ~100 ms hold the marker and its room tail; a replay should start after that. Same build and board state as the corpus-*cm folders (combined build, E3 0.5 FS, boards at 110 cm). Twelve captures per source, 4.2 s of each.

| Source | peak |pcm| per capture | 3,200 Hz amplitude after the marker |
|---|---|---|
| speech | 32,500-125,100 | 53-2,414 |
| music | 48,400-173,800 | 462-10,426 |

The music captures carry tonal content inside the band (3,200 Hz amplitude up to 10,400 in `r0`, 8,000 in `r6`), the speech captures mostly not. The tone amplitudes are over the whole capture, marker included, so they are an upper bound for the content. These are the material for the false-answer side of an offline threshold sweep (contrast limit about 30,500 rests on 27 false-answer occurrences from the SEQ runs; the captures let the inspectors be re-run over the same audio).
