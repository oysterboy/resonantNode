# 2026-10-21: bench session 1, part 1 (owner at the bench)

## E3 fix (bench/sessions/2026-10-21-e3-cap-selftest)

The owner put a 10 uF capacitor between EN and GND on E3 and checked its wiring by eye. Results:
- `esptool read_mac` and the flash both ran without BOOT: the auto-reset works.
- SELFTEST: 2 of 4 runs PASS. Runs 2 and 3 failed on the mic floor while the owner was handling things at the
  bench. Run 4, with the bench left alone, passed.
- The own tone is steady now (spread 1.00-1.07; on 2026-10-17 it was 2x).
- The owner no longer hears the harsh output.

## Five boards on one build (bench/sessions/2026-10-21-selftest-all5)

All five boards were flashed to Node ecfc72b without BOOT. Ports: E1 COM8, E2 COM3, E3 COM4, E4 COM10, E5 COM11.
- At boot each board prints `RB node name=E<n> rotation_source=roster` (rotations 0-4) and `RB hop_check
  status=warn` (step 5h item 2).
- Each board passes SELFTEST with the other four held silent. E4 needed a solo reboot: its quiet-boot check
  failed while all five booted at once.
- Amp levels: E1 200k, E3 215k, E2 219k, E5 251k, E4 335k.
- E5 was on the 5 m+ repeater cable, unchanged since 2026-10-17, when its mic read zeros on that cable; today
  it passed. The fault is intermittent.

Steps 3b, 4 and 5c were closed (5fe3cb8).

## 5b on a second day: chirp and hop train, 60-110 cm (bench/sessions/2026-10-21-hop-day2-*)

Setup:
- E2: Analyzer, shipped build (esp32dev-analyzer, main 5fe3cb8).
- E3: Emitter at 0.5 FS (`-D I2S_TONE_AMPLITUDE=0.5f`).
- Mic and speaker facing up on the bench; the owner moved the boards between spacings.
- E1, E4 and E5 were Nodes on the same bench, held silent (ports open, idle off, onset 200000).
- 10 trials per cell. Script: `tools/bench/hopday2.sh`.

| spacing | chirp 3,200 Hz | hop5 25 ms x 5 gap 10 | 2026-10-16 hop5 |
|---|---|---|---|
| 60 cm | 10/10 | **10/10** | (section 6) |
| 70 cm | 10/10 | **10/10** | 10/10 |
| 80 cm | 10/10 | **10/10** | 9/10 |
| 90 cm | 10/10 | **10/10** | 10/10 |
| 100 cm | **0/10** (rejected) | **9/10** | 0/10 at max dur 200, re-run at 300 >= 9/10 |
| 110 cm | 10/10 | **10/10** | 10/10 |

Reading:
- **The hop train is at or above 9/10 at all six spacings on a second day with the shipped builds.** The first
  5b gate item is met.
- The one hop reject at 100 cm was trial 2: a 140 ms occurrence with contrast class weak, below the medium
  limit (a step in a comb null, the case step 5h item 3 measured on the host).
- The plain chirp at 90 cm is unchanged (10/10), as the gate requires.
- At 100 cm the plain chirp is 0/10, against 3/10 on 2026-10-16. Its rejects are mostly duration_too_short and
  window_invalid: the comb state of the single tone at that spacing. This is why the network signal is the hop
  train, and not a gate item.

Left for 5b after the battery: speech/music OBS on the Node build (audit 2.2).

## Speech OBS on the Node build (bench/sessions/2026-10-21-node-obs-radio)

Setup: all five nodes on Node ecfc72b, idle off, shipped values; `tools/bench/nodegroup.py`, 180 s per run;
the owner's phone radio playing speech.

| run | radio | false triggers in 180 s |
|---|---|---|
| OBS_radio_50cm_E2_full | full volume, 50 cm from E2 | 2 (E2 at 0.5 s, E1 at 121.6 s); each started a five-node chain, and the rest rule ended both |
| OBS_speech_room_volume_1m | room volume, about 1 m | 0 (a chain from before logging began ended at 0.73 s) |

Speech at room level gives no false triggers, in line with 5a. This was the last phone-radio run.

Music: music (drum and bass) and a tonal flute from `bench_audio.wav` (the 2026-10-16 file), 200 s, played through
the lab PC and the Studio One box at the 2026-10-16 volume. The speaker was midway between the nodes and 40 cm off
their line, and the owner heard it. Result: **0 verdicts of any kind in 215 s** on five nodes. **Step 5b is done.**

`nodegroup.py` now attributes a verdict to an emission from 0.05 s on, not 0.15 s: answers are stamped 0.13-0.14 s
after the sender's emit line.

## Owner checks

- 4a: the LEDs do what they should, on all five boards.
- E5 is accepted on its long repeater cable as it runs. Its intermittent mic fault is watched through RB STATUS.

## 5d: two-node exchange with the state header (bench/sessions/2026-10-21-pair-state-header)

E1-E4 on the shipped build, the others silent, RB CALL every 60 s. Each kick gives 7 answers in a row; then
`rest_field_dense` ends the chain, and the pair is quiet after 3.6 s. This happened three times. The gate part is
met; the owner still has to confirm the rest values.

## 5e: hearing matrix in the trial layout (bench/sessions/2026-10-21-hearing-matrix)

Layout (owner):
- E2 and E3 stand 110 cm apart.
- E1, E4 and E5 stand left of E3, 40 cm apart, starting 40 cm from E2.
- E1, E4 and E5 are 10 cm lower than E2 and E3.

Method: `tools/bench/hearmatrix.py`. Each node plays 10 RB CALLs. The listeners rest instead of answering
(restField=1), so no chains form.

| listener \ caller (0.3 FS) | E1 | E2 | E3 | E4 | E5 |
|---|---|---|---|---|---|
| E1 | - | 5 | 10 | 10 | 6 |
| E2 | 0 | - | 2 | 0 | 0 |
| E3 | 4 | 0 | - | 0 | 7 |
| E4 | 10 | 1 | 7 | - | 10 |
| E5 | 9 | 10 | 10 | 10 | - |

At 0.5 FS every cell was 10/10 except E2 <- E1 (0/10). The owner chose 0.5 FS as the shipped level
(decisions/2026-10-21-damp-tone-level-0p5.md, faaa814).

## 5g / 5f: five nodes free-running on faaa814 (bench/sessions/2026-10-21-group5-kick-idle)

Idle on, RB CALL every 90 s, 360 s.
- **5g:** RB CALL 4 of 4 and idle calls 27 of 27 were answered, so the 5g gate is met.
- **5f:** 51-88 emissions per minute. Every chain ends within 2.9-7.7 s, and the longest silence was 19.2 s.
  Against the 15 s / 30 s / 80 % pass mark this is a **FAIL** (13 of 31 starts). At 0.5 FS each of the group's idle
  calls, about one every 13 s, wakes all five nodes.
- Open for the owner: M, and the idle timeout or a restated pass mark.

## After midnight (2026-10-22, owner away)

- **3d SEQ check** (bench/sessions/2026-10-22-3d-seq-check): P1 echo, B12a, B9, B12b and P3 pass. A miss
  right after rejects could not be forced. The other items of #32 stay open.
- **Owner decisions** (decisions/2026-10-22-network-pacing.md):
  - rest 5 s;
  - idle call every 60 s +- 20 s;
  - pass mark: the chain ends within 30 s in 80 % of starts;
  - M = 100.
- **Verification** (bench/sessions/2026-10-22-group5-pacing, 908dc9a, 600 s): chains 13 of 13 ended (max 4.6 s),
  max 48 per minute. **5d and 5f are done.**

Board state (end of the session): all five on Node 908dc9a (0.5 FS, new pacing): E1 COM8, E2 COM3, E3 COM4, E4 COM10, E5 COM11,
in the trial layout. They are held silent by a background python process on the lab PC (ports open, idle off,
onset 200000). Ending it, or a reboot of the PC, resets the boards and brings back their idle voice.
