# 2026-10-21: E3 after the EN capacitor, SELFTEST on E3 and E2, Node build ecfc72b

Changes on E3 by the owner (checklist docs/lab/2026-10-17-hardware-fix-steps.md):
- a 10 uF capacitor between EN and GND;
- the wiring checked by eye ("looks okayish"); no wires replaced, no multimeter readings.

What ran:
- Both boards were flashed with the D-AMP Node build `esp32dev` at main ecfc72b, E3 on COM4 and E2 on COM3.
- `esptool read_mac` on E3 without BOOT printed the MAC and no "Wrong boot mode": **the auto-reset works now**.
  The flash also ran without BOOT.
- SELFTEST was run from each board's own port. The other board was held silent: port open, idle off, onset
  200000.

| board | run | mic floor mean / peak | amp tones (5) | floor during tones | spread | result |
|---|---|---|---|---|---|---|
| E3 | run1 | 971 / 3,114 | 173,522-203,358 | 1,292-2,586 | 1.17 | **PASS** (SNR 37.9-43.1 dB) |
| E3 | run2 | 2,143 / 11,701 | 186,952-203,137 | 7,339-62,111 | 1.09 | FAIL low_snr (10.1 dB min) |
| E3 | run3 | 7,373 / 47,688 | 188,040-201,545 | 4,503-57,861 | 1.07 | FAIL low_snr (10.5 dB min) |
| E2 | run1 | 1,668 / 13,077 | 219,110-264,838 | 2,116-10,392 | 1.21 | **PASS** (SNR 26.8-41.9 dB) |

Boot lines on both boards:
- `RB node name=E3|E2 rotation_source=roster`, with rotation 2 for E3 and 1 for E2;
- `RB hop_check status=warn ... issues=gap_not_below_debounce`, as expected (step 5h item 2).

Reading:
- E3's own tone is **steady now**: spread 1.07-1.17, where 2026-10-17 measured a 2x spread. It is still low:
  about 190,000, against 260,000-310,000 on E1/E4/E5 and 219,000-265,000 on E2 today.
- Runs 2 and 3 failed on the floor, not on the tone. The floor was high already in the mic check, before
  any tone, and it varied between tones (4,500 to 62,000). It is either room noise during those runs or
  electrical noise on E3's mic; the two have not been separated yet. E2's mic peak of 13,077 in its run
  (between E3's run 2 and run 3) also points to sound in the room.
