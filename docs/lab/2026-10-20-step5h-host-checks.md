# 2026-10-20: step 5h host checks (remote, no board)

Step 5h (issue #47) has three host-only items before bench session 1. All three ran on the host replay
(NODE-006, `native-replay` on main), on the 70 cm hop capture
`test/fixtures/hop5_25x5g10_70cm_2026-10-16.csv`:
- 25 ms x 5, gap 10, rotation 0 (steps 2950, 3200, 3450, 3700, 3950 in that order);
- E3 at 0.5 FS, shipped build 5ba805d;
- shipped profile: contrast medium 30,500, amp medium 1,500, max duration 300, release debounce 10, max gap 20,
  min duration 85, cooldown 50.

No threshold was changed. Every changed value below is a replay override (`--set` or
`applyProfileSet`), reported as a measurement.

## 1. Gaps: does the detector bridge them, or the room? (assumption 1)

`tools/bench/hopvariants.py` cuts the capture into the five step windows by the send plan. The onset is
57.2 ms: the first 1 ms RMS reaching 20 % of the peak. In the broadband envelope the gaps are hidden by
the reverb, so they cannot be cut from it. The tool writes two kinds of variant:
- **dry_gN**: the step windows are kept. The gaps and the tail after the train are replaced by the
  capture's own pre-roll noise, with N = 10/15/20/30.
- **wet_gN**: each recorded 10 ms gap is kept with its tail, then N - 10 ms of noise follow, with
  N = 15/20/30.

A step window still holds the previous step's tail, so "dry" removes the tail only in the gaps.

| variant | shipped (debounce 10) | debounce 15 | debounce 20 |
|---|---|---|---|
| recorded (gap 10, tail) | 1, from 59 ms, 201 ms | 1, 201 | 1, 241 |
| dry g10 | 1, from **109 ms** (step 2), 115 ms | 1, from 59, 165 | 1, from 59, 165 |
| dry g15 | 0 | 1, from 109, 135 | 1, from 59, 185 |
| dry g20 | 0 | 0 | 1, from 109, 155 |
| dry g30 | 0 | 0 | 0 |
| wet g15 | 1, from 59, 221 | 1, 221 | 1, 261 |
| wet g20 | 1, from 109, 191 | 1, from 59, 241 | 1, from 59, 281 |
| wet g30 | 0 | 0 | 1, from 114, 266 |

Onset sensitivity (window onset 54/55/56/57.2/58/59 ms, shipped values):
- dry g10 keeps its 115 ms rest at 57-58 ms. At the other four onsets nothing is accepted. No onset reads
  the train from its first step.
- wet g15 is accepted from 59 ms at all six onsets.
- wet g20 is accepted at only two of the six.
- With debounce 20, dry g10 and dry g15 are read whole, from 59 ms, at all six onsets.

Raising max gap from 20 to 40 changes nothing while the debounce is at or below 20: the debounce closes
the candidate first. At debounce 35 with max gap 40, dry g30 is bridged as well.

**Finding:** assumption 1 holds in this room, not in the detector. The shipped gap equals the release
debounce (10 = 10). Without a tail, the first step closes as a 25 ms reject, and the 50 ms onset cooldown
keeps step 2 from starting a clean candidate. A dry room or outdoors reads the train late and short
(115 ms, 30 ms over the minimum) or not at all. The lever, measured but not shipped, is release
debounce > gap; 20 is enough for gaps up to 15 ms. That change is a tuning pass of its own, with its noise
cost to measure.

Tests (test_replay_corpus):
- `test_dry_hop_train_loses_its_first_step`
- `test_dry_gaps_of_15_ms_and_more_are_not_heard`
- `test_room_tail_bridges_15_ms_but_not_30_ms`
- `test_release_debounce_above_the_gap_bridges_the_dry_train`

Commit 0a73165.

## 2. The gap contract (DET-011)

`src/detection/HopGapContract.h` checks `output.hop_*` against `detection.scalar_*`. Hard checks
fail when the train can never be one occurrence:
- the train lies inside the min..max duration;
- coverage: steps x pulse ≥ min coverage;
- longest island: pulse ≥ min longest island;
- gap ≤ max gap.

Soft checks fail when the train is one occurrence only where the room helps:
- gap < release debounce;
- train + 50 ms tail ≤ max duration.

The shipped pair gives hard ok, soft warn (`gap_not_below_debounce`). A host test pins that result. The
Node prints `RB hop_check status=... issues=...` at boot and after PARAM SET on `output.*` or
`detection.*`. This is compiled only; it has not run on a board. Commit 624eff5.

## 3. One and two nulls (assumption 2)

`tools/bench/notchsweep.py` filters the capture with a peaking-EQ notch (RBJ biquad) at one bin, then at
every pair of bins. Depth is 1/3 or 1/7 in amplitude. Each variant is replayed on the shipped profile.
The replay build does not print the contrast P75, so it is bisected: the largest `contrast_medium` at
which the occurrence is still accepted (resolution 50).

Notch bandwidth 125 Hz (bins are 250 Hz apart):

| notched | depth | accepted | failed | dur_ms | contrast P75 | margin to 30500 |
|---|---|---|---|---|---|---|
| none | - | yes | - | 201 | 32442 | 1942 |
| 2950 | 1/3 | yes | - | 166 | 32371 | 1871 |
| 3200 | 1/3 | yes | - | 201 | 32477 | 1977 |
| 3450 | 1/3 | yes | - | 201 | 32335 | 1835 |
| 3700 | 1/3 | yes | - | 201 | 32442 | 1942 |
| 3950 | 1/3 | yes | - | 200 | 32442 | 1942 |
| 2950+3200 | 1/3 | yes | - | 155 | 32229 | 1729 |
| 2950+3450 | 1/3 | yes | - | 166 | 32442 | 1942 |
| 2950+3700 | 1/3 | yes | - | 166 | 32371 | 1871 |
| 2950+3950 | 1/3 | yes | - | 165 | 32371 | 1871 |
| 3200+3450 | 1/3 | yes | - | 200 | 32300 | 1800 |
| 3200+3700 | 1/3 | yes | - | 201 | 32442 | 1942 |
| 3200+3950 | 1/3 | yes | - | 200 | 32477 | 1977 |
| 3450+3700 | 1/3 | yes | - | 201 | 32300 | 1800 |
| 3450+3950 | 1/3 | yes | - | 200 | 32335 | 1835 |
| 3700+3950 | 1/3 | yes | - | 144 | 32442 | 1942 |
| 2950 | 1/7 | yes | - | 165 | 32442 | 1942 |
| 3200 | 1/7 | yes | - | 125 | 31344 | 844 |
| 3450 | 1/7 | yes | - | 97 | 32229 | 1729 |
| 3700 | 1/7 | yes | - | 201 | 32442 | 1942 |
| 3950 | 1/7 | yes | - | 158 | 32442 | 1942 |
| 2950+3200 | 1/7 | yes | - | 125 | 31380 | 880 |
| 2950+3450 | 1/7 | yes | - | 97 | 32229 | 1729 |
| 2950+3700 | 1/7 | yes | - | 165 | 32442 | 1942 |
| 2950+3950 | 1/7 | yes | - | 122 | 32442 | 1942 |
| 3200+3450 | 1/7 | yes | - | 97 | 32300 | 1800 |
| 3200+3700 | 1/7 | yes | - | 124 | 32264 | 1764 |
| 3200+3950 | 1/7 | no | detector | - | - | - |
| 3450+3700 | 1/7 | no | detector | - | - | - |
| 3450+3950 | 1/7 | no | detector | - | - | - |
| 3700+3950 | 1/7 | yes | - | 129 | 32442 | 1942 |

Over the bandwidth (all 30 variants each):

| notch bandwidth | accepted | lost (all at the detector) | shortest accepted | lowest contrast P75 |
|---|---|---|---|---|
| 60 Hz | 30 / 30 | none | 110 ms | 31,273 |
| 125 Hz | 27 / 30 | 1/7 pairs 3200+3950, 3450+3700, 3450+3950 | 97 ms | 31,344 |
| 250 Hz | 24 / 30 | 1/3: 3200+3700; 1/7: 2950+3700, 3200+3700, 3200+3950, 3450+3700, 3450+3950 | 95 ms | 31,522 |

**Findings:**
- One null, at any bin, depth or tested width, is survived. Assumption 2 holds on this capture and
  rotation.
- Two nulls are survived when shallow (1/3), except at 250 Hz width. Deep (1/7) pairs lose 3 to 5 of 10.
  The losses are pairs that leave no run of steps long enough: a lost step splits the train into pieces
  under the 85 ms minimum, so the train never reaches the inspectors.
- The contrast gate is not what fails. Notches move the P75 by at most about 1,200 (32,442 unnotched), and every accepted
  variant stays ≥ 31,273 against 30,500. The margin to the gate here is about 1,900 unnotched.
  - The "~400 margin" in 5h's text is the distance between the gate and the music P75 below it, a
    different number.
- Single deep nulls in the middle (3450 at 1/7) leave 97 ms, 12 ms over the minimum. The shortest
  accepted variant over all widths is 95 ms.
- This is rotation 0 only. On another node's rotation the same pair of frequencies sits at other step
  positions, so its result can differ.

Test: `test_one_deep_null_is_survived_two_in_the_middle_are_not` (the five single 1/7 nulls are
accepted; 3450+3700 at 1/7 is not, and nothing reaches the inspectors).

Levers, if a NODE-012 cell shows two nulls (step 5h; not now):
- more steps or 125 Hz bins;
- DET-015 (pulse structure);
- a longer release debounce, which also joins the pieces (item 1).

## Not done here

The board-side speech/music OBS on the Node build did not run: no board was attached
(`pio device list` empty, 2026-10-20).

Board state: no board attached to the lab PC on 2026-10-20. The owner says the boards may have been
unplugged and moved. The last known state is in docs/lab/2026-10-17-slot1.md: E1-E4 on Node 2ddf2b4 and E5
on an older build. The newest Node code is on no board; this includes RB CALL, the roster, the idle call
and now RB hop_check.
