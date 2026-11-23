# Trill-only is judged at the tryout; trill + rise is the backup; the drift is parked

Status: decided (owner 2026-11-19 evening: "Try trill only at the tryout, see what happens in real life. Trill + Rise
is the backup.")
Date: 2026-11-19
Amended by: decisions/2026-11-23-trill-taper-rise-is-the-fallback.md (the backup is trill-taper-rise)
Amends: decisions/2026-11-19-trill-only-drops-the-shape-check.md (its "Rules out" and "Revisit when")

## Decision

1. Trill-only (presets `trill`, `fieldtrill`, the shape check dropped by params) plays at the tryout although it
   failed the desk's false-detection gate: in the lab PC's noise file played loud (over the onset about half the
   time) it made 4.5-5.2 valid detections per 10 minutes per node, cricket 0 (bench/sessions/2026-11-19-trill-desk).
   The room decides, not the desk: the noise file is a stand-in for an audience.
2. Trill-only plays with every node listening on the whole window (`behavior.band_per_node 0`, detection band =
   the window). With each node on its own bins a one-bin call is heard only by a node on the same bin (15-20 % at
   1 m against 98-100 % on the whole window).
3. The backup is trill + rise (preset `trillrise`): the trill, then a short gapless rise over 4 bins, the shape
   check as shipped. Switched by radio if trill-only answers the room too often. Cricket (trill + full glide) stays
   the proven fallback behind it.
4. The radio log runs during the tryout, so the room's false detections are counted per 10 minutes, against the
   desk's number.
5. The drift (behavior moving the base bin while calls go unanswered) waits for a pitch map, not for the tryout.
   Whole-window listening takes away one of its two jobs (finding the others' pitch); the other, moving off a
   pitch the room cancels between two nodes, is untested: the whole-window desk check played every call at the
   window's lowest pitch. The map (owner 2026-11-19: "step the pitches more fine"; 1 m to 2.5 m is the realistic
   node distance): trill alone, whole window, the player pinned to each pitch in 50 Hz steps (2500-3700 Hz), at 1 m
   and at 2 m. A pitch that fails for one pair brings the drift back; a clean map at both distances drops it.

## Why

- A false detection costs one node answering the room, audible and judged on the spot; a preset switch by radio
  undoes it in seconds.
- The desk's noise is music and speech from one speaker at 40 cm, not a room of people.

## Rules out

- Trill-only at the tryout without trill + rise desk-checked (heard rate and the same loud-noise count) as the backup.
