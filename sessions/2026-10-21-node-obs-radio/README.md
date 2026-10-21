# 2026-10-21: speech/music OBS on the Node build (5b gate item, audit 2.2), phone radio at full volume, 50 cm from E2

Setup:
- All five boards on Node ecfc72b, free-running with idle off (`--setup "RB BEHAV idleEnabled=0"`) and the
  shipped detection and behavior values. `# state` lines are in the log header.
- `tools/bench/nodegroup.py`, 180 s.
- The owner's phone radio ran at full volume, 50 cm from E2. The program content (speech, music, or both) was
  not recorded. The radio was already playing when logging started.

Counted: a valid verdict with no other node's emission in the 0.05-0.8 s before it is a false trigger from the
radio. The run itself printed "unattributed verdicts: 4" with the old 0.15 s lower bound. Two of those four were
answers stamped 0.13-0.14 s after the sender's emit line; the bound is 0.05 s since this run.

| t | node | what |
|---|---|---|
| 0.54 s | E2 | valid; E1, E3 and E4 had invalid verdicts at 0.51-0.53 s, so a loud radio event was heard by all |
| 121.64 s | E1 | valid; no emission in the 20 s before it |

Result:
- **2 false triggers from the radio in 180 s** across five listening nodes (15 node-minutes). Each one started a
  chain over all five nodes. Emissions per minute were 12, 0 and 22. The rest rule ended each chain (4 rests,
  `rest_field_dense`).
- Compared with 5a (the Analyzer at 110 cm, speech 0/50 and music 0/50 windows, 2026-10-16), this is **worse**.
  The conditions also differ: full volume at 50 cm. The gate item ("no worse than 5a") is not met under these
  conditions, and a run at the 5a level and distance is needed to compare like with like.
