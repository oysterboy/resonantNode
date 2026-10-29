# 2026-10-29: step 14, hop captures at rotations 0-4 and the chord mix

E3 -> E2 at 79 cm (layout of 2026-10-26), Analyzer 0e8b3bd. tools/bench/rawcorpus.py, hop5 25/10 ms at the shipped
band, rotation r = the train starting at bin r, 3 captures each (hop5_rot<r>_r<n>.csv / .wav).

tools/bench/chordmix.py sums two or three captures (aligned at the trigger, voice 2+ delayed and lowered) and
replays the sum through native-replay (0e8b3bd host build). Caveat: all voices were recorded from the one Emitter
position; real chords come from Nodes at different places (different combs), and each recording adds its own noise.

chordmix.csv (348 mixes, offsets 0-30 ms, 0-10 dB, rotation 0 against 1-4, and trios): valid 228 / 240 pairs,
98 / 108 trios. All pairs at >= 6 dB valid. Fails only at 0-3 dB: 0+1 contrast (5 / 60), 0+3 shape (7 / 60);
trio 0+1+2 contrast (8 / 36). One occurrence per chord in every mix.

chordmix_offsets.csv (offsets 100-260 ms): equal level: 1 train to 100 ms, NOTHING at 120-220 ms (the merged sound
runs past max_duration_ms 300, both trains lost), 2 trains from 230 ms. At 6 dB the blind window narrows to about
140-220 ms (partly); at 10 dB the weaker train is never counted on its own.
