# E003 - Several nodes sounding at once: how often, and do listeners still hear it?

## Question

When two or more Nodes play their hop trains at the same moment, does a listening Node still read a valid
verdict? How often does that happen in the field?

## Status

open (2026-10-27). Phase 1 (offline, from the committed field logs) logged below. The test design for the
deliberate checks is in docs/research/polyphony.md.

## Setup

No new run. Logs already on the `bench` branch (commit 39a56eb):

- bench/sessions/2026-10-22-step6-trial: five Nodes, 908dc9a, quiet and speech/music free runs, 30 min each.
- bench/sessions/2026-10-22-step10b-field-rerun: five Nodes, 8ec0764, same layout and protocol.
- bench/sessions/2026-10-26-soak24-shape-b: three Nodes (E1, E4, E5), 5fcd3fc, with the DET-015 shape rule, 23.45 h.

Rotations from the roster: E1=0 ... E5=4, confirmed by the `order=` lines in the logs. Behavior answers a fixed
`behavior.wait_after_heard_ms` = 100 ms after the verdict, the same on every Node (no per-node variation, owner
2026-10-17).

## Protocol

`tools/bench/overlapstats.py <log>` (method in its docstring). A round is a run of emission starts within 165 ms of
each other, which is one hop train. Listener cells are Nodes not emitting around the round. Heard means a valid=1
verdict from that Node within 0.3 s of the round's end.

## Log

### 2026-10-27 - phase 1, offline count over the field logs (Claude analysis)

| Session, run | Rounds | Rounds with 2+ trains | Of those, 4 at once | Listener cells on 2+ rounds, valid |
|---|---|---|---|---|
| step6, quiet | 373 | 242 | 50 | 91 of 103 |
| step6, speech/music | 403 | 256 | 38 | 136 of 143 |
| step10b, quiet | 344 | 167 | 114 | 59 of 62 |
| step10b, speech/music | 351 | 174 | 115 | 50 of 56 |
| soak24-shape-b, 3 Nodes | 11,708 | 4,428 | - | 1,261 of 1,305 |

- Within a round the starts spread by a median of 20 ms, at the host timestamps' 10 ms resolution. The trains
  overlap almost completely.
- The typical step 10b round: E1 calls, and E2-E5 answer together. At every step four of the five band bins sound
  at once, and E1 reads the chord as one valid `single_pulse` and answers it.
- Adjacent rotations, which put two trains in neighbouring bins at four of five steps, were heard as well as or
  better than non-adjacent ones. Soak: 854 of 863 adjacent, 407 of 442 non-adjacent. The non-adjacent pair is
  E1+E4 heard by E5, the soak's weakest listener path, so geometry and pairing are confounded.
- Single-train rounds read lower (38-64 % valid), but the script does not exclude resting or refractory listeners
  there. That is not evidence that chords are heard better.

## Findings

Several trains at once is the normal network state, not an edge case. Between 38 % and 65 % of rounds in every
field log have two or more Nodes sounding together. The fixed answer delay is the cause. In these layouts,
listeners read those rounds as valid about as often as anything else.

Not known (Claude inference, unmeasured): why they pass. Two trains of equal level in adjacent bins would put the
contrast near 21,800, against the 30,500 gate. Four would put it near 16,400. Likely reasons, none checked:

- One voice usually arrives much louder at each listener, because of distance and the 1.7x amp spread between boards.
- The contrast inspector reads a P75, so a quarter of the window with clean contrast is enough.

The per-occurrence contrast and shape facts of chord rounds were not logged in these runs.

## Open threads

- Contrast and shape values on chord rounds (SEQ DIAG or a raw capture at a listener during a free run).
- A level-controlled check: two trains at 0 / 3 / 6 / 10 dB difference, adjacent and non-adjacent rotations
  (host mix first, then one bench cell). Design: docs/research/polyphony.md.
- Captures at rotations other than 0: the corpus has none (E002).
- A listener logs one verdict per chord round, so its FieldState counts four trains as one pattern. Whether that
  is intended is an owner question.
