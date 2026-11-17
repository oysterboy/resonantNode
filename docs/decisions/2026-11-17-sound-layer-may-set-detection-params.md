# The sound layer may set detection params it needs; glide stays the reference setup

Status: decided (owner 2026-11-17: "keep" the trill's release debounce, "loosen the 24 gate of unchanged detection",
and the glide detection settings must be applicable and copyable, not left in an old commit)
Date: 2026-11-17

## Decision

1. Step 24's rule "detection unchanged" is loosened from the detection layer to the detection code: the sound layer
   (24) changes no detector or inspector code. A detection PARAM may change when a sound setting needs it, on these
   terms: the preset that sets it lists it (and says why in a comment), a decisions file or this one names it, and
   the hearing matrix (`tools/bench/callmatrix.py`, step 24.3) shows hearing no worse than the glide setup in the
   same window. The first case: the `cricket` preset sets `detection.scalar_release_debounce_ms 20` (shipped 10) so
   the call stays one sound across the trill gaps (its 15 ms gap would otherwise close the occurrence).
2. `tools/bench/presets/glide.preset` is the reference setup: the shipped field as R1 ran it on 2026-11-16, with
   every detection.*, behavior.* and output.* value it holds (50 params, 24 of them detection), the trill off. It is
   kept complete in the tree, so applying it (`nodepreset.py glide ...`) restores that setup on any board whatever
   another preset changed. A change to a detection default is made in the code and the glide preset in the same
   commit, or the preset says which value it differs on.

## Why

- The trill needs a longer release debounce; keeping the call one sound is cheaper than a second detector path, and a
  param stays inside the owner's rule that margins and values move by PARAM SET before the freeze.
- The reason for "unchanged" (decisions/2026-11-06-sound-layer-before-the-performance.md) was to keep the
  November measurements valid. The matrix against glide is that condition in a measurable form.
- A setup that exists only in an old capture or commit cannot be put back in the room.

## Rules out

- Detector or inspector code changes in step 24, DET-014, BEH-007 (still after the performance).
- A preset that changes a detection param without saying so in its own text.

## Revisit when

The matrix shows the trill (or another sound change) lowers hearing against glide: then the setting stays out of the
performance, or its detection param is looked at as a tuning item after it.
