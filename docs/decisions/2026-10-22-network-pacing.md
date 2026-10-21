# Network pacing for the trial: rest 5 s, idle call every 60 s +- 20 s, pass mark "the chain ends", M = 100

Status: decided 2026-10-22 (owner), implemented in the same commit. It amends
2026-10-17-two-node-exchange-chain-length.md (restMs), 2026-10-17-idle-call-wakes-the-network.md (the idle
timeout) and 2026-10-17-trial-third-question-pass-mark.md (pass mark, M).
Date: 2026-10-22.

## Decision

- `restMs` = 5,000 (was 10,000). The two triggers are unchanged: 4 valid patterns in the field window, or 6 own
  emissions in 10 s.
- `idleTimeoutMs` = 60,000 with `idleTimeVariationMs` = 20,000, so an idle call comes after 40-80 s of quiet (was
  20 s +- 5 s).
- Step 6's third question passes when, after a start (an RB CALL or an idle call), **the chain ends** (a 3 s
  silence) within 30 s in at least 80 % of starts, and **no minute has more than M = 100 emissions**. The 15 s
  silence is still counted, but it no longer passes or fails a session. A start that falls inside a running chain
  belongs to that chain. `tools/bench/nodegroup.py` prints both verdicts (`--chain-gap 3`, `--quiet-within 30`,
  `--max-per-min 100`).

## Why

The first five-node session at 0.5 FS (bench/sessions/2026-10-21-group5-kick-idle) had these numbers:
- Every chain ended within 2.9-7.7 s.
- The group made an idle call about every 13 s, and each one woke all five nodes.
- Only 13 of 31 starts reached 15 s of silence.
- 51-88 emissions per minute.

The old mark measured the idle rate more than the network. The owner's aims:
- the network calms down after a kick (the chain ends), which is the rule's job;
- idle calls are rare enough that silence is normal again;
- a shorter rest keeps it lively.

M = 100 sits just above the observed maximum and is meant as a runaway guard, not a target.

## Rules out

Per-node pacing values (still the same values on every node), and tuning the detection side to calm the
network.

## Revisit

After the first session on these values: if chains get longer with the 5 s rest, or the network feels dead at
60 s idle.
