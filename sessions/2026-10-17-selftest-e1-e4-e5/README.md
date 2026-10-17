# 2026-10-17 step 5c: SELFTEST on the remaining boards (E1, E4, E5), Node build 21a8529 + serial TX buffer fix

Each board flashed over USB with the D-AMP Node build (`esp32dev`, main 21a8529 plus the TX-buffer fix of the
following commit), then `SELFTEST` from its own USB console, output saved as `selftest_<board>.log`. Boards on the
bench next to E2/E3 (idle voice off on all), quiet room.

| Board | port | MAC | mic (floor mean / peak) | quiet boot | amp (own tone at own mic, SNR) | result |
|---|---|---|---|---|---|---|
| E1 (first run) | COM6 | 24:dc:c3:4a:b0:50 (node_id 0x50B04AC3DC24, hop rotation 1) | PASS 945 / 2,776 | PASS | **FAIL** tone 743-1,248, SNR -10 to -3.5 dB | "amp dead" as noted on 2026-10-14 |
| E1 (after the owner reseated a loose 5 V Dupont wire at the ESP32) | COM6 | same | PASS 1,054-1,068 / 3,092 | PASS | PASS tone 270,400-270,900, SNR 32.1-44.3 dB (two runs) | **all checks pass**: the dead amp was the loose 5 V wire, not the MAX98357A |
| E4 | COM7 | 24:dc:c3:4a:ef:ec (node_id 0xECEF4AC3DC24, hop rotation 0) | PASS 1,834 / 6,035 | PASS | PASS tone 248,199, SNR 24.1-38.2 dB (floor during the tones 3,763-5,916) | all checks pass; own-chirp suppression pass; noisier mic floor than E2/E3 (600-1,300), worth a second look at its placement/wiring |

| E4 (short USB cable, E3's) | COM4 | same | PASS 1,265 / 3,461 (run 2: a burst, 20,012 / 362,032) | PASS | PASS tone 304,900-307,700, SNR 37.7-44.5 dB | the 5 m extension cost ~2 dB of tone and raised the floor (SNR min 24 -> 40 dB); E4 is fine |
| E5 (first run) | COM8 | 24:dc:c3:4a:fe:38 (node_id 0x38FE4AC3DC24, hop rotation 0) | **FAIL** all_zero (0 / 0), two runs | SKIP | SKIP (mic failed) | mic data path dead: 3V3, GND, SD/WS/SCK wiring to check (E1's fault was a loose Dupont) |

E1 printed garbage at 115200 before the flash (old firmware or other baud); esptool's auto-reset worked on E1, E4
and E5 (unlike E3). E1's "dead amp" was a loose 5 V Dupont wire at the ESP32: after reseating, two clean passes.

| E5 (short USB cable, the former E3/E4 one) | COM4 | same | PASS 1,145-1,381 / 4,035-5,072 (one burst run: 10,988-15,460 / 203,252-520,237) | PASS | PASS tone 310,400-312,800, SNR 36.5-44.2 dB | **mic works on the short cable**: the all_zero was the 5 m extension |
| E4 (back on the long cable) | COM7 | same | PASS 890-980 / 2,785-3,655 | PASS | PASS tone 262,000-263,000, SNR 35-44 dB | fine on the long cable this time too |

`own_chirp_suppressed` failed in 4 of 7 runs on E4/E5 (`reason=own_verdict`, 2-4 verdicts, `other_verdicts` 5-7)
and passed on a quiet first run. Not a fault of the board under test: the other Nodes on the bench (E1, E2, E4 with
idle off, still answering heard patterns) answer its SELFTEST chirps with their hop trains about 120 ms after the
chirp, which lands inside the self-test's 300 ms echo window and is counted as the board hearing itself. The check
is only meaningful with the other Nodes off or out of earshot; for a five-board bench the self-test needs the
others powered down, or the check should ignore verdicts whose pattern is not its own chirp.

**Step 5c result: all five boards pass board, mic, quiet boot and amp** (E1 after reseating its 5 V wire, E5 on a
short USB cable). Hardware notes for the trial: no 5 m USB extensions for power (E5's mic died on one, E4's SNR
dropped), Dupont wires on the 5 V rail need securing, and the mic bursts below happen on every board.

Setup note (owner): E4 and E5 hang on USB extensions of 5 m or more. The board's 3V3 rail feeds the mic and the
amp sits on 5 V from the same cable, so a voltage drop over the extension is the first suspect for E5's all-zero mic
and for E4's 2-3x mic floor. Recheck both on a short cable (or external power) before touching the wiring.

Mic bursts seen in SELFTEST mic checks across boards (floor mean / peak, normal is ~600-1,800 / 2,000-6,000):
E3 2026-10-16 evening 11,038 / 184,981 (then normal), E1 today 1,615 / 14,702, E4 today run 2 20,012 / 362,032
(run 1 normal). Short, one capture window of 750 ms, never on the amp check right after. Handling at the bench
(cables, boards) is the likely source; a supply or connector issue is the alternative. Worth a dedicated check
with the boards untouched (ten mic checks in a row per board).

Hop rotation from the MAC (`MAC % 5`): E4 and E5 both get 0, E1 1, E3 3, E2 4. Two of five nodes would start their
train on the same bin; `PARAM SET output.hop_rotation` can fix it per boot but nothing persists it (no NVS yet,
PAR-010). For the field trial: assign rotations explicitly or derive them from a per-board table.

## Cable swap repeat (owner: E3 back on the short cable, E5 back on the long one)

All five boards in one pass, idle off on each before its SELFTEST:

| Board | port | mic | amp |
|---|---|---|---|
| E2 | COM3 | PASS, floor 1,095 | PASS, 33.4 dB |
| E3 | COM4 (short cable) | PASS, floor 755 | PASS, 37.4 dB |
| E1 | COM6 | PASS, floor 1,024 | PASS, 41.3 dB |
| E4 | COM7 (long cable) | PASS, floor 1,393 | PASS, 40.5 dB |
| E5 | COM8 (long cable) | **FAIL all_zero** | SKIP |

E5's mic fails on its long cable every time (3 of 3) and works on the short one (3 of 3), while E4 passes on a long
cable. So it is that cable, or E5's supply margin is smaller than E4's: check the extension E5 uses (or give E5 its
own 5 V supply) before the trial.

## E5 on a long cable with an active repeater (SKYMASTER)

COM9, three SELFTEST runs: mic **FAIL all_zero** every time (floor 0 / 0), board PASS. The repeater does not help.

Reading: an exact all-zero stream with the ESP32 running normally does not look like a supply drop (the board's
3V3 regulator holds 3.3 V well below 5 V input, and a starved mic would give noise or garbage, not clean zeros).
The cable change also moves E5 to another place and bends its wiring. Next to separate the two: the short cable at
E5's far position, and the long cable with E5 next to the PC; or measure 3V3 at the mic and reseat its SD/WS/SCK
wires (E1's "dead amp" was a loose Dupont).

## E3 after the rest-rule flash (2ddf2b4): owner hears its output as harsh, "overdriven"

Two SELFTEST runs on COM4 (short cable). Own tone at own mic, five identical 100 ms tones each:

| Run | tone per chirp | floor during the tones | amp result |
|---|---|---|---|
| 1 | 131,637 / 247,356 / 262,423 / 134,923 / 196,269 | 3,678-13,748 | PASS (median) |
| 2 | 165,729-176,172 | 2,262-4,021 | PASS |

Identical tones vary by a factor of two inside one run, and the floor jumps with them; in run 2 the level is
stable but low (175,000 against 260,000-310,000 on E1/E4/E5). Mic and amp disturbed together point at a shared
supply or ground contact on E3 (as with E1's loose 5 V Dupont), or at the speaker connection. The self-test passes
on the median and does not flag it. To check: E3's 5 V and GND wires at the ESP32 and at the MAX98357A, and the
speaker terminals.
