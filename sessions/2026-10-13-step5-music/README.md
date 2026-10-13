# 2026-10-13 step 5 (#8): music, Scalar vs Freq

Music (station/track not named) from the owner's speaker, 50 cm from E2's
mic, full volume, speaker at 90 degrees to the mic (same as the speech
run, sessions/2026-10-13-step5-speech). E3 silent, E2 Analyzer 7882627,
110 cm setup. 50 OBS windows per profile, stock profiles.

| Profile | false accepts (valid pattern) | detector reacted | detector accepted, rejected later | miss |
|---|---|---|---|---|
| TonalPulseScalar | **1** (trial 15: 129 ms, strength 9,145, contrast medium, amp strong, scored `late` because OBS expects nothing) | 43 | 1 (this one) | 7 |
| TonalPulseFreq | **0** | 34 | 14 | 16 |

Scalar once accepted a real-looking chirp from music; Freq's detector
accepted in 14 windows but the pattern stage rejected every one.
Earlier R4 (Node build, #20): Scalar speech 2/50, music 6/50. Here, with
the Analyzer's pattern_valid: Scalar speech 0/50, music 1/50. Not like for
like; one window is not a rate.
