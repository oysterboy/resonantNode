# Polyphony: several nodes at once, and chords inside one chirp

```text
Status:  exploring (2026-10-27)
Lab:     docs/lab/experiments/E003_simultaneous_answers.md (several nodes at once, phase 1);
         E002 (shape on single trains only)
```

Owner question, 2026-10-27: why was polyphony never tested, and should it be? Two readings, both worth an answer:

- A. Several Nodes sounding at the same time: chords made by the network.
- B. Polyphonic chirps as such: one Node emitting several tones at once (the owner's meaning).

Claude analysis; nothing below is measured unless it cites a session.

## A. Several nodes at once

### What is known

E003, phase 1: in every committed field log, 38-65 % of rounds have two or more trains sounding together. In step
10b the typical round is four answers at once. Listeners read those rounds as valid about as often as single
trains. The cause is Behavior's fixed answer delay (`behavior.wait_after_heard_ms`, the same on every Node, no
per-node variation by owner choice 2026-10-17). Every Node that hears a call answers within a few ms of the others.

### Why it was never tested on purpose

- Every test tool assumes one source. The hearing matrix has each Node emit in turn. SELFTEST runs with the others
  silent. The replay corpus comes from one emitter at rotation 0. E002 lists "two trains overlapping" as not
  covered.
- The field gates count chains: every chain ends, no chain is started by noise. They pass whether a listener heard
  one voice or four.
- Nobody tied the fixed answer delay to simultaneity.

### Why it is worth a deliberate test

- The pass is unexplained. Contrast compares the strongest bin with the mean of its two neighbours. Two equal
  trains with adjacent rotations, which sit in neighbouring bins at four of five steps, would read about 21,800,
  far under the 30,500 gate. Most likely each listener hears one voice much louder than the others. If so, the pass
  depends on geometry, and a more symmetric installation layout could break it.
- The shape rule (DET-015) follows the strongest bin per ms. With two equal voices, that track can jump between
  trains instead of rising. It was designed and checked on single trains only (E002).
- FieldState sees one pattern per chord round, so a four-voice round counts as one toward the rest rule.
- Changes ahead alter the chord picture: OUT-006, BEH-006 variation, any answer jitter, more Nodes.

### Proposed checks, cheapest first

1. Host mix: sum two (then three) recorded hop captures in `native-replay` at level differences of 0 / 3 / 6 /
   10 dB and start offsets of 0 / 10 / 30 ms, adjacent and non-adjacent rotations. Log contrast P75, the shape
   facts and the verdict per mix. Needs captures at rotations 1-4 (`tools/bench/rawcorpus.py` with
   `output.hop_rotation` set), since the corpus has rotation 0 only.
2. Field readout: SEQ DIAG or RAW at one listener during a free run, so chord rounds get their contrast and
   shape values on record.
3. One bench cell: two Nodes triggered together at one listener, with the level ratio set by distance.
4. Owner: should a chord round count as one field pattern or several? And is the simultaneous answer wanted, as
   the sound of the piece, or should answers be spread by a per-node offset or jitter?

No threshold changes as part of this; tuning stays its own pass.

## B. Polyphonic chirps: one node, several tones at once

### Why it was never tested

- **The hardware could not.** Until 2026-10-09 the nodes were piezos driven by PWM, one square wave at one
  frequency. The `ToneOutput` interface still has that shape: one `setToneHz()`, one sine in `I2sToneOutput`.
  The D-AMP board can sum voices in its renderer, but it arrived 2.5 weeks ago.
- **Since then the work has been about being heard at all.** Room-comb nulls on a single tone led to the hop;
  the tone level went from 0.3 to 0.5 FS to bring the hearing matrix from 7 to 19 of 20 cells
  (decisions/2026-10-21-damp-tone-level-0p5.md).
- **Detection is built around one tone.** The contrast gate wants the peak bin's neighbours empty. The shape rule
  follows one strongest bin. Any second in-band voice is at best ignored and at worst read as noise. The same
  gate already rejected sweeps (decisions/2026-10-15-signal-variation-inside-a-shared-band.md, limit found
  2026-10-16).
- **The one polyphonic plan is sequenced, not forgotten.** OUT-006, the two-layer signal, is a two-voice chirp:
  a free aesthetic voice, low in frequency, plus the detection voice in the band. It waits in step 8a so it does
  not move the field baseline. Its first item, measuring the low voice's leak into the band, has no bench cell
  yet (unproven-assumptions-audit.md 2.4).

### What a chord would do to detection, by where its voices sit

| Voices | Contrast | Shape rule | Level per voice |
|---|---|---|---|
| One in band, others below 1 kHz (OUT-006) | unchanged if leak is small; unmeasured | unchanged | shared full scale |
| Several in band, two or more bins apart | steady on-grid voices are orthogonal over the 64-sample window, so near unchanged; ramps smear | a steady chord is one bin and fails, like a plain chirp; a chord of hop voices follows the loudest voice | shared |
| Several in band, in adjacent bins | about 21,800 at equal level; fails the gate | track may jump | shared |

Shared level is the hard cost. Two equal voices that stay under the 0.5 FS peak play at 0.25 FS each, below the
0.3 FS that left 13 of 20 hearing cells short of 10/10. Speaker and amp intermodulation can also land sum and difference tones
in the band, which nobody has measured.

### Should we?

- **For sound, yes, through OUT-006.** One voice in the band carries detection and the other voices are free. It
  keeps detection unchanged and is already the agreed shape. Start with its leak cell: a low voice alone and with
  the hop, at the level split OUT-006 would use, read at the Analyzer.
- **For detection, no, not without a new reason.** Several in-band voices buy frequency diversity at one instant,
  but the hop already buys it over time and passes 710 of 710 in the soak's room checks. They cost level on every
  voice and break contrast for adjacent bins.
- **In-band chords are already happening.** Part A's chord rounds are the network playing in-band chords, so A's
  host mix covers this case too. If a deliberate in-band chord comes up later, A's data says what it costs.

Open for the owner: is a chord inside one chirp an aesthetic wish (then OUT-006 is the path), or does it carry
meaning for the network, such as identity or a different message (then it needs a detection design first)?
