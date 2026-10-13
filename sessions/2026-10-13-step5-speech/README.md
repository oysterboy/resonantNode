# 2026-10-13 step 5 (#8): speech, Scalar vs Freq

Radio WDR5 (speech) from the owner's speaker, 50 cm from E2's mic, full
volume, speaker at 90 degrees to the mic (same as the earlier speech/music
tests, owner). E3 silent (EMIT MODE REMOTE), E2 Analyzer 7882627, boards at
the 110 cm setup. 50 OBS windows per profile, stock profiles, mode=detail.

| Profile | valid patterns (false accepts) | windows with a candidate | detector accepted | rejected downstream |
|---|---|---|---|---|
| TonalPulseScalar | 0 | 45 of 50 rejected (5 miss) | 0 | 45 (detector reject 48) |
| TonalPulseFreq | 0 | 20 of 50 (18 rejected, 2 unexpected; 30 miss) | 11 | 11 after the detector accepted |

Neither profile produced a valid pattern from speech in 50 windows. Scalar
reacts to speech in almost every window but its inspector rejects them;
Freq reacts in fewer, and in 11 windows its detector accepted before the
pattern stage rejected. The #20 R4 note said Scalar answered speech 2/50
(Node, possibly a different measure): not reproduced here with the
Analyzer's pattern_valid. Not compared like for like; music is the next run.
