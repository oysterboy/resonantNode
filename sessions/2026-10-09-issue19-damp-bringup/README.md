# 2026-10-09 issue #19: D-AMP bring-up (link, chirp, mic, node)

Purpose: first run of the D-AMP firmware (step 2, issue #19,
`docs/refactors/archive/damp-board-support.md` on `main`): Analyzer <-> Emitter UART
link, I2S chirp out, mic in, Node on D-AMP. Also the drift check from #19
comment 2 (does the D-AMP mic through this repo's HAL show the low-frequency
walk seen on the piezo nodes in #24?).

## Setup

- Two D-AMP boards (echoSpace pinout: BCLK 26, WS 25, amp DIN 32, mic SD 33,
  mic L/R to GND; MAX98357A, 3 W / 8 ohm speaker; no LED).
  - COM6, MAC 24:dc:c3:4a:b0:50: Emitter.
  - COM10, MAC 24:dc:c3:49:b7:38: Analyzer, later Node.
- UART2 cable between them (16/17 crossed). Both on USB from one PC.
- Distance between the boards: 30 cm throughout (owner, 2026-10-09).
  Orientation not recorded.
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

## Item 5a: drift, firmware read config vs board (same sitting, COM10)

Question (#19 owner comment 15:35Z): the same mic was drift-free under
echoSpace's firmware on a D-AMP node, so is it our read setup?

Method: throwaway sketch `drifttest-sketch/` (not repo firmware, so the run
files carry `git=throwaway`). D-AMP pinout, full duplex with TX silent,
stereo 32-bit, mic slot 0. Per config: reinstall the port, wait 10 s, two
quiet captures of 16384 samples 3 s apart. Configs: A = ours (16 kHz, RX
MSB realign, DMA 3x128), B = echoSpace (48 kHz, plain STAND_I2S, DMA 4x128),
C = 48 kHz + realign, D = 16 kHz without realign, A2 = ours again at the end.
Analysis: `drifttest-sketch/drift_analyze.py`, output `drift5a_summary.txt`
(dBFS of the 24-bit word, framing-corrected).

Results:
- Framing: without the realign raw bit 8 is 0 in every sample at 16 and
  48 kHz; with it ~50%. Confirms #24 on this board.
- The wander is **below 30 Hz**: 1-30 Hz band -39 to -59 dBFS; 30-100 Hz
  -73 to -78; every band from 100 Hz up -76 to -92 dBFS (mic floor). First
  difference sits at -85 to -90 dBFS.
- **No read config removes it.** B (echoSpace-equivalent) shows it like A:
  1-30 Hz -56 dBFS in both B captures, -57 to -59 in A, -39 / -49 in A2.
  The spread between captures of the same config (A vs A2: 20 dB) is larger
  than any difference between configs.
- So it is not our rate, framing, or DMA setup. Candidates left: the
  mic's own sub-30 Hz output, its supply (the echoSpace wiring doc puts mic
  VCC on 5 V; not measured here), or mechanics. Sub-30 Hz is inaudible and
  hard to see in short windows, which may be why echoSpace looked clean.

## toneOn -> own mic latency (same sitting, COM10 alone)

Method: throwaway sketch `latency-sketch/` built on the repo's
`AudioSourceI2S` + `I2sToneOutput` (copied unchanged from `main` 4b06814;
default D-AMP config: 16 kHz, DMA 3x128, tone 0.1 FS, 5 ms ramp). 20 trials:
100 ms quiet, `toneOn()` at T0, `toneOff()` at T0+100 ms, 200 ms tail. Mic
sample times from the sample clock (the detector's time base). Envelope =
moving max of |x| over 6 samples of the First-Differenced stream; plateau =
mean envelope 50-90 ms after T0. Log: `tone_latency_20.throwaway.log`.

| Measure | min | median | max |
|---|---|---|---|
| toneOn -> envelope at 10% of plateau | 28.9 ms | 31.9 ms | 35.9 ms |
| toneOn -> 50% (ramp included) | 30.9 ms | 34.2 ms | 38.2 ms |
| toneOff -> under 10% for 2 ms | 33.1 ms | 37.2 ms | 53.7 ms |
| own-chirp level over the quiet floor | 35.1 dB | 43.6 dB | 44.9 dB |

Reading: the delay is the TX queue (3 DMA buffers x 8 ms = 24 ms) plus the
block being rendered (0-8 ms; the 7 ms spread is that quantum), plus the
5 ms ramp for the 50% point. Acoustic path on the board is negligible. A
100 ms chirp is therefore heard on the node's own mic from ~30 ms to ~135-
155 ms after `toneOn()`, about 45 dB over the floor. The Node's current
own-emit windows (`behaviorSuppressSelfChirpMs=100`,
`detectionSuppressTailMsOwnEmit=0`, from the node log above) end before
that sound does: for step 3 (#20), not changed here.
