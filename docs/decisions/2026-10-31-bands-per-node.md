# A band per node that wanders: each node's 5-bin window moves on 2200-4950 Hz, glides toward what it hears

Status: decided, not yet implemented (owner 2026-10-31, "A"; step 19.3)
Date: 2026-10-31

## Decision

Each Node listens and calls on its own 5-bin window (its hop train is the window's five bins) at one of 8
positions on 2200-4950 Hz (the 250 Hz grid, 12 bins). The position is the node's base, owned by Behavior: it glides
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
- A pitch range of about an octave, heard as the arpeggio moving, against a major third with base bins in the
  shared band (step 17).

## Rules out

- A scatter anywhere in band-per-node mode (the field falls apart).
- The guard bins as the "sender below" signal: the bank's peak never lands on a guard
  (FreqBandStream::computeBandScore); the direction comes from the heard train's lowest and highest bins instead.

## Revisit when

- The tryout room's weak zones (E004: 1500-3000 weak in that room) make the lowest positions (2200-2700) deaf: then
  the range moves up or narrows.
- The desk shows windows that never meet again after a scatter.

## Source

Owner 2026-10-31 after step 19.1-19.2; roadmap-0-steps.md step 19; docs/research/field-activity-response.md Part 2.
