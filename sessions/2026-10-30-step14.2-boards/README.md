# 2026-10-30: step 14.2, the deep review fixes and BEH-008 (16.1-16.2) on the boards

All five boards by OTA to a5193220 (E1 / E4 / E5 by NET * OTA from E1, E2 Analyzer and E3 Emitter on their ports;
every image confirmed). Remote, desk layout of 2026-10-26 (Emitter to E2 79 cm). Before: b006c00 (the soak's build,
2026-10-30-step14-soak47b).

Stored params: the Nodes had behavior.rest_after_field_patterns 4, own_emit_budget 6 and rest_ms 5000 stored (the
old triggers, PAR-018: a stored value wins over a changed default, "PARAM stale"). Set to the new defaults (0 / 0 /
8000) and PARAM SAVEd on E1 / E4 / E5; output.silent 1 stays stored (bench). After: PARAM load stored=50 applied=50
stale=0 invalid=0 on each.

| part | tool | result |
|---|---|---|
| Nodes | tools/bench/nodecheck.py | nodecheck: 25 / 27 with the old answer check (1 answer in 10 trains: the level automaton, 9 declined_by_level) and the RB STATUS key set (new keys behavior.level / declined / yielded against b006c00's log). nodecheck2, the check split: level on, every valid train decided (2 answers, 8 by the level); level_max 0, 10 / 10 answered: 27 / 27 |
| Analyzer SEQ set | analyzerab.py, seqcmp.py | ab_after_a5193220 vs 2026-10-29-step12.4-readability/ab_after_5f1aa75: the same trial results (hop 10 / 10 expected, chirps 10 / 10 rejected), the same line counts per case, the same key set per line type |
| RAW | 2026-10-29-step12.4-readability/rawset.py | raw_after_a5193220 vs raw_after_12410: the same RAW_ keys and CSV headers; post rows the same, pre rows by timing (both_hop 4684 / 4724, feat 4612 / 4628, pcm 3531 / 3525); both_hop captures |
| Chord cell | chordcheck.py (level_max 0 for the answerers, live) | chordcell: alone, same wait, +50 and +150 ms one train per pair (2 valid verdicts per Emitter train, 15 / 15), +250 ms two (3, 15 / 15); every answer given. As 2026-10-29-step14-review |
