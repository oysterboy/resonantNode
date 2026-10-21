# 2026-10-21: all five boards on one build, SELFTEST with the others silent (step 3b/4 gate run, step 5c)

All five D-AMP boards were flashed over USB with the Node build `esp32dev`, main ecfc72b. **None needed BOOT**,
E3 included: it has had a 10 uF capacitor on EN since today (bench/sessions/2026-10-21-e3-cap-selftest).

Ports today: E1 COM8, E2 COM3, E3 COM4, E4 COM10, E5 COM11. The MACs were checked with esptool `read_mac` and
match app/NodeRoster.h. At boot every board prints `RB node name=E<n> rotation_source=roster` and `RB hop_check
status=warn ... issues=gap_not_below_debounce`; the warn is expected (step 5h item 2).

Method: all five ports open. Every board was silenced (idle off, onset 200000); for its turn the board under
test was put back to onset 2000 and sent `SELFTEST`. USB cable lengths were not recorded today.

| board | MAC | mic floor mean / peak | quiet boot | amp tone (spread) | SNR min-max | own-chirp | result |
|---|---|---|---|---|---|---|---|
| E1 | 24:dc:c3:4a:b0:50 | 1,595 / 5,047 | PASS | 199,517 (1.01) | 33.2-39.0 dB | PASS | **PASS** |
| E2 | 24:dc:c3:4b:4c:e8 | 909 / 3,008 | PASS | 219,138 (1.01) | 35.2-39.3 dB | PASS | **PASS** |
| E3 | 24:dc:c3:49:b7:38 | 1,062 / 3,794 | PASS | 214,515 (1.00) | 36.2-41.2 dB | PASS | **PASS** |
| E4 (first) | 24:dc:c3:4a:ef:ec | 7,682 / 29,186 | **FAIL** no_quiet | 334,332 (1.05) | 23.1-37.1 dB | PASS | FAIL (quiet boot only) |
| E4 (rebooted alone, others silent) | same | 943 / 3,213 | PASS | 335,023 (1.01) | 41.5-45.5 dB | PASS | **PASS** |
| E5 | 24:dc:c3:4a:fe:38 | 1,570 / 5,271 | PASS | 251,395 (1.01) | 37.8-42.2 dB | PASS | **PASS** |

Reading:
- **All five pass SELFTEST on one build, with the others silent.**
- E4's first quiet-boot fail came with all five boards booting at once (opening the five ports resets them
  together). Its mic floor was high in that run too. Rebooted alone, it passed every check with the lowest floor
  of the day. That is boot-time noise from the neighbours, not E4.
- own_chirp_suppressed passed on all five. The 2026-10-17 failures were the other Nodes answering; holding them
  silent removes that.
- Amp levels vary by a factor of 1.7 between boards: E1 200k, E3 215k, E2 219k, E5 251k, E4 335k. On 2026-10-17
  E1 measured 270k, so the level depends on the day or the placement as well as on the board. This feeds 5e's
  hearing matrix: name the reference board, and decide whether a per-node level or threshold offset is needed.
