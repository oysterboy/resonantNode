# Distance and range: how far does a node hear another, and where does it stop

```text
Status:  exploring (2026-10-28)
Lab:     E001 (docs/lab/experiments/E001_spatialdistance.md), bench/index.csv as of 2026-10-28
         (775 runs, setup fields backfilled the same day, tools/bench/backfill_setup.py)
Related: acoustic-test-suite.md (NODE-012, the full matrix); this note is the distance slice of it,
         sized to the December tryout
```

## Question (owner, 2026-10-28)

There is nothing distance-related in detection yet: "detects up to x cm, too low beyond y cm", ideally
some kind of correlation. Can the existing bench data give that, and which tests are most urgent?

Tryout target (owner, 2026-10-28): a 5 x 6 m room, nodes 1 m apart. So neighbours sit at 1 m, diagonal
neighbours at about 1.4 m, the far side of the room at up to about 7.8 m (the diagonal).

## What the bench data can and can't say

Every D-AMP run sits on one desk, between 60 and 110 cm, except one 10 cm, one 40 cm and one 200 cm run.
That range is less than one doubling of distance.

Received strength against distance, D-AMP, fitted as dB per doubling over 40-200 cm:

```text
                runs   slope (dB / doubling)   scatter (sd, dB)
0.3 FS chirp     98    -0.6                    2.2
0.5 FS chirp     85    +0.4                    3.9
0.5 FS hop       95    -1.8                    2.7
free field             -6.0
```

- **Distance does not show in the strength at 60-110 cm.** The slope is near zero; the scatter between
  runs at one distance is larger than the distance effect. The room comb sets the level here: neighbouring
  frequencies differ 10-20x, and the pattern moves with spacing and between days (lab 2026-10-15 and
  -16). That is why the plain chirp fails at single rungs (100 cm 0/10, 70 cm 0/10 one morning) and the
  hop train, spread over five bins, does not.
- **So no linear law can be read out of this data.** A slope fitted inside one doubling, inside a comb,
  is noise.
- **The level gives a bracket instead.** At 110 cm, 3200 Hz chirp: 0.1 FS 0/50, 0.3 FS 50/50 (E001 phase
  2b). The node ships 0.5 FS (+4.4 dB over 0.3). So at 110 cm the operating point is somewhere between
  4.4 and 14 dB above the detection threshold. In free field, each 6 dB is one doubling of distance: a
  limit somewhere between about 1.7 and 5 m. Too wide to plan with, and a room changes it both ways
  (see below).
- **Edge points exist, but each has a confound.** 40 cm 259/259 and 70 cm 30-35/36 (pair matrix,
  2026-10-17); 110-180 cm "0 or one-way" in the same matrix, with a 20 cm height step and the unstable E3
  in it. 200 cm 0/50, but a single chirp at 0.1 FS (strength 2,022, against ~4,600 at 70 cm the same day:
  -7 dB over 1.5 doublings, the only number close to free field).
- **The hop train passes the whole bench range.** The 5b gate held >= 9/10 at every rung 60-110 cm on
  two days (lab 2026-10-16 section 8, 2026-10-21). Inside that range reliability does not depend on
  distance. The 1 m tryout spacing is inside it, on the desk.

Accept rates per rung in the index mix builds, hop variants and thresholds from tuning sessions; read
strength from it, not rates.

## How it likely behaves in a room

Two regimes, with the change at the room's critical distance (direct sound = reverberant sound; in a
furnished 5 x 6 m room probably around 0.5-1 m, unmeasured):

- **Near (direct sound dominates):** level falls about 6 dB per doubling; the limit is the level margin.
- **Far (reverberant field dominates):** level stops falling much, so the level margin lasts further than
  free field predicts. But the reverb lengthens and smears the train: more `duration_too_long` and a
  weaker contrast (both already seen at low frequencies and under noise, E004). The limit there is the
  shape rule, not the level.

The desk numbers (flat slope) suggest the desk pair already works partly in the second regime. If so, the
useful description is not a line but three zones: reliable up to x, a transition, nothing beyond y, with
x and y depending on the room more than on the signal.

## Tests, in order of urgency

Before any of these: every new session now has to carry `--space` and `--tone-level` (tools/bench/seqrun.py,
2026-10-28), and the index has `signal`, `freq_hz` and `orientation` columns. Use the hop10 train at the
shipped band throughout, and stable boards (not E3).

1. **Level ladder (desk, unattended, cheap).** hop10 at 0.5 / 0.35 / 0.25 / 0.18 / 0.12 FS (about -3 dB per
   step), at 100 cm (the tryout spacing) plus 70 and 110 cm, two spots per rung about 5 cm apart (one spot
   can sit in a comb null). 20 trials per cell. Output: the threshold level per rung, so the margin in dB
   at the operating point. Converts to a range only in the near regime, so it gives the lower bound.
2. **Distance ladder in a large space (the tryout room if reachable, otherwise the largest room at hand).**
   hop10 at 0.5 FS, fixed orientation and height, rungs 100 / 140 / 200 / 300 / 400 / 600 cm, two spots each,
   20 trials. Output: accept rate per rung, strength slope (the regime check), duration and contrast of the
   accepted trains (the smear). This gives x and y for that room. It is NODE-012 cut down to one space.
3. **Tryout room floor and self-echo.** Ambient and class-D idle floor with every node idle; toneOn to the end
   of the own-mic tail against the suppression windows (`behaviorSuppressSelfChirpMs`,
   `detectionSuppressTailMsOwnEmit`, `refractoryAfterEmitMs`). A reverberant room lengthens the tail; at 1 m
   spacing a neighbour's answer lands inside it (seen on the desk, lab 2026-10-17).
4. **Near end: 10 / 20 / 30 cm.** Whether the strength saturates (Strength16 cap) and the train fails
   `duration_too_long`. Only matters if nodes can end up that close; at 1 m spacing it is a check, not a
   design question.

Tests 1 and 2 answer the owner's question. 3 decides whether 1 m works as a network in that room. The
spacing question behind it is behavioural: at 1 m every node probably hears its 4-8 neighbours and maybe
the whole room, so contact range sets how far a call spreads and how often the rest rule fires.

## Reading the results

Per rung: accept rate with a Wilson interval (20 trials gives about +-15 % at 80 %). Fit accept rate against
log2(distance) with a logistic curve; report the distance at 90 % (x, "reliable up to") and at 10 %
(y, "nothing beyond"). Report the strength slope next to it: near -6 dB per doubling says direct field and
level-limited, near 0 says reverberant field and shape-limited.

## Open

- Whether the tryout room is reachable for test 2 before December, or a stand-in room has to do.
- Whether a node should know its range at runtime (strength as a distance hint for behaviour) or range
  stays a design number. Not before tests 1-2.
- Floor stands vs desk: the tryout nodes stand on the floor (owner, case V1), so floor reflection and height
  are part of the room, not of the desk numbers.
