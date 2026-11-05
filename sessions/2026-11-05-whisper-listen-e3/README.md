# 2026-11-05 Whispernet listen-while-playing desk session, E3

E3 (COM4, on the PC's USB) on esp32dev-whisper a7d6715c, no stored params. Calibration before: WHISPER AEC CAL x3,
D = 1066 samples (44.4 ms) each, = the default whisper.aec_delay. E1/E4/E5 on a power-only charger (no data).
Plan: ../2026-11-05-desk-plan/whisper_listen_e3.json (whisper_listen.json with E3). Owner whispering at the desk.
Evaluate: python tools/bench/deskeval.py bench/sessions/2026-11-05-whisper-listen-e3

Result (deskeval):
- Answers come early (WHISPER heard after 0.3-1.4 s of a phrase), every replay makes the board deaf ~1.1 s and
  cuts the phrase it hears (cuts = deafs, 42 over the session).
- Listening while playing (short_listen, long_listen): the canceller's ERLE falls from ~9-10 dB to ~2 dB and once to
  0 (peak tap lost, centring moved 1066 -> 1098), then back to ~7-10 in the deaf sections: the double-talk problem
  of 2026-11-04 again; listening while playing does not work yet.
- Phrases run long (median 9-11 s, up to 18.5 s, just under max_phrase_ms 20 s) and the end-of-phrase verdict is
  mostly invalid (failed=loud etc.): the release threshold (175) is not reached between sentences.
- The pauses also get answers (2-4 asked each): whether those are the owner's last words, the room or its own
  tail is not separable from this log.
