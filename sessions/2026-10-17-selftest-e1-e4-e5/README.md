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
