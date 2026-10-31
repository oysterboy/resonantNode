# A band per node that wanders: each node's 5-bin window moves on 950-4950 Hz, glides toward what it hears

Status: decided, implemented as a mode, off by default (owner 2026-10-31, "A"; step 19.3; built in 19.4, cef23f87)
Date: 2026-10-31

## Decision

Each Node listens and calls on its own 5-bin window (its hop train is the window's five bins) at one of 13
positions on 950-4950 Hz (the 250 Hz grid, 17 bins; amended the same day from 2200-4950, 8 positions, after the
1 kHz session: owner "950"). The position is the node's base, owned by Behavior: it glides
one bin per own answer toward the window it heard, wanders to a random position before an idle call when it heard
nothing since the last one, and scatters by one bin (up or down) at the level's rest. The Node applies the window to
the detector live and composes the train from it. Of BEH-009's two ways to more range this is the one taken; the
musical tone below 1 kHz (OUT-006) is not built now.

## Why

- The frequency hearing radius is +-2 bins (bench/sessions/2026-10-31-step19-hearing-radius): a listener hears a
  train with 3 of its 5 bins inside its window, so neighbours within two positions hear each other.
- native-fieldsim (field-activity-response.md "Bands per node in native-fieldsim"): with a one-bin scatter 88-96 %
  of the hearing pairs stay within reach and the field is as lively as on the shared band (chains, ignition), while
  the group's band drifts across the octave; a scatter anywhere leaves 47 % in reach, no wander 27-31 %.
- A pitch range of about two octaves, heard as the arpeggio moving, against a major third with base bins in the
  shared band (step 17).
- Down to 950 Hz (bench/sessions/2026-10-31-step19-1khz): the train heard 10 / 10 on 700-1700 and 950-1950, 85 / 90
  under music, no valid verdict from music on the Nodes; native-fieldsim with 13 positions keeps 83-88 % of the
  pairs in reach.

## Rules out

- A scatter anywhere in band-per-node mode (the field falls apart).
- The guard bins as the "sender below" signal: the bank's peak never lands on a guard
  (FreqBandStream::computeBandScore); the direction comes from the heard train's lowest and highest bins instead.

## Revisit when

- The tryout room's weak zones (E004: 1500-3000 weak in that room) or its music make the low positions deaf or
  noisy: then the range moves up or narrows (behavior.band_range_low_hz).
- The desk shows windows that never meet again after a scatter.

## Source

Owner 2026-10-31 after step 19.1-19.2; roadmap-0-steps.md step 19; docs/research/field-activity-response.md Part 2.
