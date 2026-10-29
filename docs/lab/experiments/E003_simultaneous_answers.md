# E003 - Several nodes sounding at once: how often, and do listeners still hear it?

## Question

When two or more Nodes play their hop trains at the same moment, does a listening Node still read a valid
verdict? How often does that happen in the field?

## Status

answered for one placement (2026-10-29, step 14): phase 1 (field logs), phase 2 (host chord mix and one bench cell)
logged below. The test design is in docs/research/polyphony.md Part A.

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

2026-10-29, phase 2 (step 14), host chord mix (bench/sessions/2026-10-29-step14-rotations; tools/bench/chordmix.py):
hop captures at rotations 0-4 (3 each, 79 cm), summed in pairs and trios and replayed through native-replay.

- Every rotation alone passes (5 bins, 4 up, class strong, 15 / 15): E002's single-train result holds at all five
  rotations.
- Offsets 0-30 ms: valid 228 / 240 pairs, 98 / 108 trios. All pairs with 6 dB or more between the voices pass.
  Fails only at 0-3 dB: rotations 0+1 by contrast (5 / 60), 0+3 by shape (7 / 60); the trio 0+1+2 by contrast
  (8 / 36). One occurrence per chord in every mix.
- Offsets 100-260 ms (equal level): one occurrence up to ~100-110 ms, none at 120-220 ms, two from ~230 ms. In the
  blind window the overlapping trains merge into one sound longer than max_duration_ms (300) and the detector drops
  it (duration_too_long, no verdict): both trains are lost. At 6 dB the window narrows (about 140-220 ms, partly);
  at 10 dB the weaker train is never counted on its own (it is under the level margin, E005).

2026-10-29, phase 2, bench cell (bench/sessions/2026-10-29-step14-chordcell; tools/bench/chordcheck.py): E4
(rotation 3) and E5 (rotation 4) answer the Emitter's hop10 while E1 listens silent; 15 trains per cell. E1's
valid verdicts per train: each answerer alone 2 (train + answer); both at the same wait 2; E5 +50 ms 2; E5 +150 ms
1 (the chord lost, 15 / 15); E5 +250 ms 3 (two answers). Both answerers answered every train in every cell. The
board matches the host mix.

## Findings

Several trains at once is the normal network state, not an edge case. Between 38 % and 65 % of rounds in every
field log have two or more Nodes sounding together. The fixed answer delay is the cause. In these layouts,
listeners read those rounds as valid about as often as anything else.

Why they pass (phase 2, 2026-10-29): real voices are rarely equal at a listener; with 6 dB between them every
mix passes, and even equal adjacent rotations pass 55 / 60 (the P75 contrast and the room's per-bin levels keep
one voice ahead most of the time). The phase 1 inference below is kept for the record.

A listener counts a chord as one train, never several (one occurrence per chord at every offset under ~100 ms).
And there is a blind window: answers 120-220 ms apart are both lost. Today's fixed answer delay keeps answers within
a few ms of each other, so the window is not hit now; any answer jitter or per-node offset (BEH-006, BEH-008) must
keep Nodes under ~100 ms apart or over ~230 ms, or the detector's max duration must change with it.

Phase 1 (unmeasured then): why they pass. Two trains of equal level in adjacent bins would put the
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
