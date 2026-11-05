# 2026-11-05 echo room, five boards, the owner clapping

All five on esp32dev-echo a7d6715c. E2 (COM3) and E3 (COM4) on the PC's USB and logged (boardview.py, boards*.log);
E1, E4, E5 on a power-only charger, set by radio (E2 briefly as a Node to relay NET PARAM, then back to echo).
echo.gain 64 on all; values PARAM SAVEd on each board.

Round 1 (boards.log): last night's spread values: wait 700, jitter 1500, level_max 20, offset 20, rest 2000,
attack 40, pre 100 / post 200, onset 800, min_peak 800. Owner: loudness fine; the replay sounds cut at begin and
end (the clip is capped at 300 ms: 48 kHz, the heap's largest block); finger snaps register better than claps; a
high-pitched distortion travels toward a chain's end (a copy of a copy: each generation re-records the speaker's
distortion and noise and amplifies it); the far boards answered first (the jitter, the boards close together on
the bench). About half the occurrences failed the attack limit (slow=117 of 221 on E2).

Round 2 (boards_round2.log): pre 30 / post 270, attack 70, level_max 40, offset 40 (p = 1 up to 40 answers: the
owner wanted chains twice as long, the distortion accepted). Owner: claps still do not really register. The log:
of 1217 valid claps heard, 616 already_scheduled (an answer pending swallows them), 174 resting, 53 refractory:
the chain keeps every board busy, so a new clap of the owner's starts nothing new.

Then wait 400 / jitter 600 set (not listened to): the owner parked the echo room until the code changes:
24 kHz (a 600 ms clip in the same memory), a damping per generation as one param, a short fade-out.
