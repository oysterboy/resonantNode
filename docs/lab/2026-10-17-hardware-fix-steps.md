# Hardware fix steps for E3 and E5 (bench checklist, 2026-10-17)

Status: procedure for the owner at the bench. Findings it answers: `bench/sessions/2026-10-17-selftest-e1-e4-e5`
(README) and `docs/lab/2026-10-17-slot1.md`.

| Board | Symptom | Most likely |
|---|---|---|
| E3 | output sounds harsh; own tone varies 2x between identical chirps and is low (175,000 vs 260,000-310,000); mic floor jumps with it; no auto-reset into download mode (BOOT must be held), RTS reset unreliable | supply or ground contact (as E1's loose 5 V Dupont); separately a weak auto-reset circuit on the dev board |
| E5 | mic reads all zeros on its 3 m and 5 m (repeater) cables, works on a 1 m cable | E5's supply margin or its mic wiring; not separated yet |

D-AMP wiring (`src/app/BoardConfig.h`, D-AMP block):

| Signal | ESP32 pin | Goes to |
|---|---|---|
| I2S BCLK | GPIO 26 | mic SCK and MAX98357A BCLK |
| I2S WS / LRCLK | GPIO 25 | mic WS and MAX98357A LRC |
| mic data | GPIO 33 | mic SD |
| amp data | GPIO 32 | MAX98357A DIN |
| status LED | GPIO 2 | LED |
| mic supply | 3V3 | mic VDD (L/R select as wired on E1/E2/E4, compare) |
| amp supply | 5V (VIN) | MAX98357A VIN |
| ground | GND | mic GND, amp GND, common |

## 1. Measure (multimeter, board powered over its own USB cable)

1. Idle: 5 V at the ESP32's VIN/5V pin, at the MAX98357A VIN; 3V3 at the ESP32's 3V3 pin and at the mic VDD.
   Expected: 4.75-5.25 V and 3.2-3.4 V. Note each reading.
2. Under load: start a tone burst and measure again (from the PC: `SELFTEST chirp n=10 gap_ms=1500` on the board's
   port, or let it answer). A dip of more than ~0.3 V on 5 V during a tone is the fault.
3. Ground: continuity ESP32 GND to amp GND and to mic GND, with the board unpowered. Should be < 1 ohm.
4. Wiggle test: while measuring 5 V and 3V3, move each Dupont wire at both ends (ESP32 side and module side). A
   reading that jumps marks the wire.
5. E5 only: repeat 1 and 2 once on the 1 m cable and once on the long cable, and note 3V3 at the mic for both. If
   3V3 at the mic stays at 3.3 V on the long cable while the mic still reads zeros, the supply is not the cause:
   go to step 2 (reseat) with the mic's SD/WS/SCK wires.

## 2. Reseat and secure the wiring

1. Replace every Dupont on 5 V, 3V3 and GND that moved in the wiggle test with a fresh one, or solder it.
2. Reseat mic SD, WS, SCK and amp DIN, BCLK, LRC.
3. Secure the power wires against pulling (hot glue at the header, or a cable tie to the board).
4. Speaker: check the screw terminals on the MAX98357A and that the cone does not touch anything (a buzz or rattle
   sounds "overdriven" too).

## 3. Reset circuit (E3 only: download mode without BOOT)

1. Solder or plug a 10 uF electrolytic capacitor between EN and GND on the dev board (+ to EN). This is the usual fix
   for "Wrong boot mode detected (0x13)": the USB reset pulse is otherwise too short for the bootloader.
2. Test without holding BOOT (Claude runs it, ~2 s):
   `python ~/.platformio/packages/tool-esptoolpy/esptool.py --chip esp32 --port COM4 read_mac`
   It must print the MAC without "Wrong boot mode". If it still fails, go to step 4.

## 4. ESP change (if step 3 does not help, or the board stays unstable after step 2)

Spare: an ESP32 dev board from the old piezo nodes, same model and pin count as E3's.

1. Plug the spare **alone** into USB (not on the D-AMP carrier). Claude checks the auto-reset (`read_mac` without
   BOOT) and flashes the D-AMP Node build onto it **before** it goes on the carrier: the piezo firmware it carries
   drives GPIO 25/26, which on the D-AMP board are the I2S clocks. Never power the carrier with piezo firmware.
2. Move the wiring from the old ESP32 to the spare pin by pin, per the table above.
3. The board gets a new MAC, so a new node id and hop rotation. Claude records "E3 = new MAC from 2026-10-17" in
   the bench notes so old and new E3 logs stay apart.

## 5. Flash and verify

1. Flash the Node build (Claude): `pio run -e esp32dev -t upload --upload-port COMx`. E3 before step 3: hold BOOT
   during the upload; after a working step 3 or 4: no button.
2. Check the banner (`BUILD role=node git=...`, `RB signal band=2950-3950 ...`).
3. `SELFTEST` twice, with the other Nodes silenced or unplugged (their answers fail the own-chirp check). Pass:
   mic floor mean below ~2,000, and the five amp tones within about +-10 % of each other and around
   260,000-310,000 (E1/E4/E5 today).
4. E5: SELFTEST on the long cable it will use in the trial. Pass means the mic is fine there.
5. One pair run against E1 (`tools/bench/pairmatrix.py --boards E1=COM6,E3=COM4 ...`, 180 s) to see hearing in both
   directions.
