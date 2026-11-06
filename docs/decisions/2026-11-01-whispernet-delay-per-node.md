# Whispernet's delay is per node, between 400 ms and 2 s

Status: decided (owner 2026-11-01, "400ms und 2s"; ECHO-002, step 20.3)
Date: 2026-11-01

## Decision

Each Whispernet node replays a whisper after its own delay D, chosen per node between 400 ms and 2 s (the delay line
sized for 2 s at 24 kHz, 96 KB). This replaces the fixed ~2 s of 2026-10-31 and the 250-600 ms range of the research
note.

## Why

- A per-node D spreads the room's replies, the job the Node's jitter does; with a fixed D every node that heard one
  whisper would answer at the same moment.
- The gate opens on a provisional verdict ~0.3-0.4 s into a phrase (300 ms first check, every 100 ms after,
  research/whispernet.md) and passes the phrase from its start: that start must still be unplayed, so D >= ~400 ms.
  The 250 ms bottom of the research note's range would cut every phrase's first syllables.
- 2 s is what the line holds at 24 kHz in the heap the echo build leaves.

## Rules out

- A D below 400 ms without a faster first check (which then reads less of the phrase).
- Jitter on top of D in Whispernet's Behavior.

## Revisit when

- The desk or the room shows the first opening later than ~400 ms on real whispers (the floor of the range moves up),
  or the heap on the 24 kHz build cannot hold 2 s.

## Source

Added in commit efbf9820 (2026-11-01, "Decision: Whispernet's delay per node, 400 ms .. 2 s (owner, ECHO-002)"); the
Status line above gives the owner's words.
