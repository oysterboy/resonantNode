# 2026-10-09 issue #19: D-AMP bring-up (link, chirp, mic, node)

Purpose: first run of the D-AMP firmware (step 2, issue #19,
`docs/refactors/damp-board-support.md` on `main`): Analyzer <-> Emitter UART
link, I2S chirp out, mic in, Node on D-AMP. Also the drift check from #19
comment 2 (does the D-AMP mic through this repo's HAL show the low-frequency
walk seen on the piezo nodes in #24?).

## Setup

- Two D-AMP boards (echoSpace pinout: BCLK 26, WS 25, amp DIN 32, mic SD 33,
  mic L/R to GND; MAX98357A, 3 W / 8 ohm speaker; no LED).
  - COM6, MAC 24:dc:c3:4a:b0:50: Emitter.
  - COM10, MAC 24:dc:c3:49:b7:38: Analyzer, later Node.
- UART2 cable between them (16/17 crossed). Both on USB from one PC.
- Distance and orientation between the boards: not recorded (desk).
- Firmware: `main` 17295c9 (board=damp), 16 kHz, 32-bit stereo slots, mic
  slot 0, STAND_I2S with RX MSB realigned, First Difference in the runtime
  path (RAW mode=i2s bypasses it), tone 0.1 FS with 5 ms ramp.

## Runs

| File | What |
|---|---|
| `link_test.17295c9.log` | Both reset; Analyzer `EMIT CHIRP freq=3200 dur=1`; Emitter echoes each received line (`EVT emitter_cmd`). |
| `RAW_i2s_chirp100.17295c9.log` | Analyzer `RAW trigger f=3200 dur=100 pre=0 post=350 mode=i2s`. |
| `RAW_i2s_quiet_1/2.17295c9.log` | Same with `dur=1` (no audible chirp): quiet mic. |
| `node_vs_emitter_auto_30s.17295c9.log` | Node on COM10 (`RB log full`, `RB summary` at 30 s), Emitter in AUTO (3200 Hz, 100 ms, every 2 s). |

## Results

- **Link works.** `MODE REMOTE` ack and `CHIRP` arrive. Junk bytes from a
  board reset (and, with the Node on the far end leaving its TX floating,
  crosstalk of the Emitter's own TX) arrive as separate, ignored lines.
  Before 17295c9 that junk sat in front of the next command and hid it.
- **Chirp out, mic in.** The Analyzer's mic sees the Emitter's chirp
  starting ~15 ms after the trigger: 3200 Hz amplitude ~38-40k (24-bit
  units) for ~100 ms, vs ~100-1000 before and after (about 30 dB).
- **Framing.** Bit 0 of the decoded word (raw bit 8) is 1 in ~45-55% of
  samples, so the RX MSB realign works (with plain STAND_I2S it was always
  0, #24).
- **Drift: present on D-AMP too, and variable.** Quiet windows: raw rms
  10-12k (about -57 dBFS), first difference 0.7-1.0k, drift 17-70k PCM/s,
  floor slope -8.0 and -2.8 dB/oct. Earlier quiet captures on a dirty build
  of the same code the same afternoon (not committed) gave raw rms 30-60k
  (-49 to -43 dBFS) and slopes -2.8 / -1.4. Same order as the piezo nodes in
  #24 once the framing halving is allowed for. So with this repo's firmware
  the walk is not piezo-board specific. The owner reports the same mic
  without drift under echoSpace's firmware (#19 comment, 2026-10-09
  15:35Z): that points at firmware/config, untested here.
- **Node on D-AMP.** Hears the Emitter (15 verdicts for 15-16 chirps; 2
  accepted as valid patterns, the rest `ignored_invalid_pattern`), emits
  twice (one `heard_pattern`, one `idle`). Startup baseline never found
  quiet (`smooth` 240-520 vs threshold 20): FAILED_NO_QUIET, outputs on.
  Detection numbers are for step 3 (#20), not judged here.

Not done: speaker level vs distance, self-echo, class-D idle noise (step 3).
