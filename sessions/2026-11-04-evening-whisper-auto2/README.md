# 2026-11-04 evening: Whispernet on E5, deaf vs listening while playing, unattended (synthetic whispers)

deskrun.py plan.json (the lab PC plays ws_short: 1.2 s phrases with 4 s pauses, ws_long: 4 s phrases with 3 s pauses,
from the synthetic -35 dBFS set), E5 on the whisper build with whisper.aec 1, listen_erle_db 8, max_phrase_ms 20000
and the centring held to +-taps/2 of aec_delay 1066. deskeval.py: see eval.txt. (The first try, ...-auto, ran on
the older image: the centring had walked the delay to 3274 overnight and a phrase held for 111-154 s: both fixed.)

- Deaf (as before): in each 60 s section ~8-10 s deaf, and every phrase cut by the own playback (5 / 5, 4 / 4).
- The canceller: ERLE 10-11 dB while only the replay sounds (train, short_deaf); in the listening sections, where
  the PC's whispers overlap the replay, it fell to 0 dB and did not recover (the filter kept its peak at tap 32 but
  learned the overlapping whisper: the double-talk rule only holds once ERLE > 10). So listening while playing did
  not take: the node went deaf again whenever ERLE < 8.
Next for the canceller: a double-talk guard that holds while the near voice overlaps (a shadow filter, or a
reference-relative Geigel test).
