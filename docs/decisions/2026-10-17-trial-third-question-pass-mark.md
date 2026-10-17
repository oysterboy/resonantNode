# Step 6's third question ("does the network settle or run away") as counts and a pass mark

Status: open, pass mark proposed (step 5f, issue #45); the owner sets N before step 6.
Date: 2026-10-17.

## Proposal

The owner's goal (2026-10-17): above a level of network traffic the network goes quiet. Measured with
`tools/bench/nodegroup.py --kick <node>` (an `RB CALL` every 90 s, step 5g) on all trial nodes, idle voice on,
shipped values, `# state` lines in the header:

| Count | From the tool's summary |
|---|---|
| answers and idle calls per minute, per node | per-node lines, group emissions per minute |
| longest chain | (from the log; a turn is an answer within 0.8 s of another node's emission) |
| longest silence | silence list |
| time to quiet after a kick | "time to quiet (gap 15 s)": kick to the last emission before the first 15 s silence |
| who heard whom | listener <- sender matrix |

Pass mark (proposal): **after a kick the group falls quiet (a 15 s silence) within N = 30 s in at least 4 of 5
kicks**, and no minute of the session has more than M emissions (M from the first session). Fail: a kick that
never quiets before the next one, or a sustained chain over the whole session.

## Why

With a goal set and a rest rule that reads FieldState (decision 2026-10-17-two-node-exchange-chain-length),
the question has a mechanism and can pass or fail, instead of being deferred as 5f first said. The bench numbers
today: rest tuning reached 13-19 emissions per minute with silences up to 21 s for four nodes; N = 30 s with a
15 s quiet gap is therefore reachable on tuning, not on luck.

## Open

N and M (owner); whether the idle call (which wakes the network by design, decision
2026-10-17-idle-call-wakes-the-network) counts as a kick or is excluded from the time-to-quiet window.
