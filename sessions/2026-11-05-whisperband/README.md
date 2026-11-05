# 2026-11-05 night: Whispernet's band checks against recordings (tried through the speaker, read on the files)

Try 1 (E5 on esp32dev-whisper afea847d, the others silent, whisperband.py through E0): the labelled WAVs of
../2026-11-05-labels-c920-2 played by the lab PC through the Studio One box at x4. Aborted after the first setting:
- at x4 the phrases stayed under the onset (level_p90 150-220 against onset 250): 0 of 12 passed at "A default";
- a probe at x12 / x24: every recorded whisper failed the timbre check (flat 0.01-0.07, the speaker and the room
  colour the spectrum; live whispers read 0.15-0.49) or the modulation; paper failed modulation. So the playback path
  cannot judge the timbre checks and does not carry the board's levels.

Try 2 (on the files, no speaker): the band view of each recording against its own room - the table in
docs/research/whispernet.md ("Whisper against the room's noises", 2026-11-05 evening). Result: level_band lifts whispers
5-11 dB, but paper / breath / clothes more; the loud bound takes those out; the low-band check takes out speech and
typing; the chair stays in the whispers' range on both. The pair worth trying live: whisper.level_band 1 with a loud bound
near 17-18 dB over the room's band level, and whisper.timbre_check 1, low_max_db 9.

Also built: native-echoreplay --band-level / --low-max DB / --flat-min / --room FILE.wav (the same checks on WAV files
on the PC); the recordings' room (C920) is noisier than the board's, so its Strength16 levels need --level-db around
-24 to -36 and are not the board's.
