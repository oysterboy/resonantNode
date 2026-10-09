# 2026-10-09 issue #24, I2S drift classification, piezo analyzer

Purpose: classify the slow drift in the raw INMP441 stream (issue #24,
`docs/refactors/i2s-first-difference-revisit.md` sections 6 and 8).
Analyzer `464b8fc` (80 MHz QIO flash, sample-clock fix in place; decode and
preprocess unchanged), `RAW ... mode=i2s` (decoded words straight from the
driver, bypassing First Difference). Quiet room, emitter in remote mode.
Analysis: `tools/logging/raw_capture_slope.py`.

Capture-size limit found: RAW always allocates a 6,000-row capture buffer
(72 KB) plus the pre-trigger ring, and the largest free heap block is
110 KB, so issue #24's `pre=500 post=200` (and `pre=300`) fail with
`raw_buffer_alloc_failed`. `pre=150` runs, but the pre ring delivered only
256 samples, fewer than the script's 512 minimum, so the `pre150` captures
are analysed over the whole window, chirp included. The clean measurement is
the `post350` set: 350 ms after the trigger, analysed only from
`emit_done + 20 ms` to the end (`*_quiettail` copies, 3,680-3,781 samples,
about 0.23 s).

| Capture | Window analysed | dc (PCM) | slope 1k->7k |
|---|---|---|---|
| pre150_1 | all (chirp inside) | 14,776 | -0.4 dB/oct |
| pre150_2 | all (chirp inside) | -69,167 | -1.0 dB/oct |
| pre150_3 | all (chirp inside) | -40,825 | -0.7 dB/oct |
| post350_1 quiet tail | after chirp | -43,680 | -1.9 dB/oct |
| post350_2 quiet tail | after chirp | -6,913 | -1.2 dB/oct |
| post350_3 quiet tail | after chirp | -15,525 | -1.5 dB/oct |

Band levels of the quiet tails (raw, dB): 125-250 Hz ~104, 250-500 ~98,
500-1k ~91, 1-2k ~89, 2-4k ~90.5, 4-8k ~86. Flat above 1 kHz, rising about
5-6 dB/octave below it.

Reading: an integrating stage would fall about 6 dB/octave across the whole
band. The floor is flat from 1 to 7 kHz, so nothing between the mic and
decode integrates; the drift is low-frequency content (mic DC offset and
1/f noise, room LF). Per issue #24 this points to a DC blocker / high-pass
as the eventual replacement for First Difference (a retune, after the field
trial), with the D-AMP HAL carrying First Difference unchanged meanwhile.

Not done here (needs the owner): clap test, capture on a second piezo node.
The drift (`dc`) wanders between captures (-69k to +15k within minutes).

## Follow-up: capture-path variations (same bench, 10:29-10:43)

Firmware `9f584bb` built with the flags below (the file hash does not show
flags). `cfg_*` files are the full captures; slope and band numbers are from
the quiet tail (emit_done + 20 ms onward). `c*`, `undecoded`, `msb` used a
1 ms chirp. Band columns: raw / first-differenced, dB.

| Config | Flags | 125-250 Hz | 1-2 kHz | Note |
|---|---|---|---|---|
| right (control) | none (ONLY_RIGHT, STAND_I2S, 16 kHz) | 106-108 / 76-79 | 89 / 78 | random walk below 1 kHz |
| left | `I2S_CHANNEL_FORMAT_VALUE=I2S_CHANNEL_FMT_ONLY_LEFT` | - | - | all zeros: undriven slot reads 0, not floating |
| stereo | `...=I2S_CHANNEL_FMT_RIGHT_LEFT` | 107-114 / 76-86 | 92-95 | both word positions identical; 4-8 kHz artifact |
| c16k_apll | `I2S_USE_APLL=1` | 112 / 84 (2nd capture) | 104 / 93 | drift unchanged |
| c32k | `AUDIO_I2S_SAMPLE_RATE_HZ=32000` | 111-116 / 76-81 | 96-100 | drift unchanged |
| c48k | `AUDIO_I2S_SAMPLE_RATE_HZ=48000` | 113-117 / 74-78 | 98 | drift unchanged (BCLK 3.07 MHz) |
| undecoded | `RAW_I2S_UNDECODED` | 107 / 78 | 95 / 84 | low byte always 0; **bit 8 always 0** |
| msb | `RAW_I2S_UNDECODED`, `I2S_COMM_FORMAT_VALUE=I2S_COMM_FORMAT_STAND_MSB` | 104-105 / 76 | 90 / 80 | bit 8 toggles: full 24 bits; drift unchanged |

Findings:
- Not slot selection, not sample rate, not the clock source, not framing:
  the random walk below 1 kHz (~28-35 dB above the differenced level) is
  in the mic's own 24-bit output in every configuration.
- Separate bug: with `I2S_COMM_FORMAT_STAND_I2S` the ESP32 reads each word
  one bit late (data in bits 31..9, bit 8 always 0): values are doubled and
  the mic's sign bit is dropped (harmless below half scale). On this IDF
  4.4 build `I2S_COMM_FORMAT_STAND_MSB` frames the INMP441 correctly.
  Switching halves all levels, so it is a threshold-relevant change.
- Level: the low-frequency part is -49 to -35 dBFS in a quiet room, far
  above what an INMP441 should output there.

## Cover test (11:33-11:40)

Mic port packed with cotton and taped by the owner. Firmware `b253dd5`
built at 40 MHz DIO (the committed 80 MHz QIO image boot-looped on this
board after the mic was re-wired, see below). `cover_sealed_1..3`: 1 ms
chirp, quiet tail; `cover_sealed_chirp100`: normal 100 ms chirp.

| | Open (earlier) | Sealed |
|---|---|---|
| 100 ms chirp peak (PCM) | ~292,000 | ~127,000-158,000 (-6 to -7 dB) |
| 125-250 Hz raw / diff (dB) | 108-111 / 78-81 | 108-117 / 78-88 |
| raw RMS of quiet tail | 29k-43k | 28k-75k |

The seal leaks (tone down only 6-7 dB) and cotton blocks low frequencies
less than high ones, so this cannot fully exclude acoustic LF. But the LF
random walk did not drop at all, and its level (about 70-85 dB SPL
equivalent) is implausible for a quiet room: consistent with an electrical
source. Decisive next: a second mic unit, or the D-AMP mic (#19).

Board note: after the owner re-wired the mic (board powered), the 80 MHz
QIO image boot-looped; 40 MHz DIO and 80 MHz DIO images booted, and a fresh
flash of the same QIO image (`7ac8855`) then booted 3/3 with unchanged
wiring. A one-off, not a QIO problem; nothing on the flash pins.

## Second piezo node (12:00)

Node 2 (MAC 24:dc:c3:4a:ef:58), placed by the owner on the bench in place of
node 1 on COM6, emitter on UART2. Same analyzer firmware (`1c743a2`, see file
names; 80 MHz QIO, booted 3/3). `node2_1..3`: 1 ms chirp, quiet tail;
`node2_chirp100`: 100 ms chirp (peak 160,576).

| | Node 1 | Node 2 |
|---|---|---|
| 125-250 Hz raw / diff (dB) | 106-111 / 76-81 | 106-110 / 77-82 |
| raw RMS of quiet tail | 29k-43k | 19k-55k |
| dc between captures | wanders | wanders (-28k to +12k) |

Node 2 shows the same random walk below 1 kHz. Not a single bad mic unit:
systematic to the piezo node design or to this repo's capture firmware,
which both boards run. `node2_3` also caught broadband sound (all bands up).
Next discriminator: the D-AMP mic read through this repo's HAL (#19).

## Clap test, node 2 (12:15-12:35)

`RAW trigger f=3200 dur=100 pre=0 post=3000 decim=8 mode=i2s` (3 s at an
effective 2 kHz; `decim` for mode=i2s added in this session). The 100 ms
beep at the start is the owner's cue; one clap about 1 s later per capture.

- **Round 1** (`clap_r1_*`, standard framing, claps at 30-50 cm): peaks at
  ~half of the true mic full scale, so the one-bit framing bug wrapped sign
  (16M sample jumps). 50 ms means then jumped +-2..6M, decayed over ~250 ms
  and left a +-0.5..1M offset. Not usable: wrap + near-overload.
- **Round 2** (`clap_r2_msb_*`, built with
  `I2S_COMM_FORMAT_VALUE=I2S_COMM_FORMAT_STAND_MSB`, clap from 1.5-2 m):
  peaks 2-17% FS. 50 ms means stay inside the pre-clap drift band; at the
  largest clap (1.47M peak) the 50 ms deviation is 33k. **The level returns
  within one 50 ms window: the chain does not integrate the audio.**

Conclusion for #24: the drift is an additive low-frequency random walk, not
an integration of the audio; with the cover test, its level, and node 2,
most likely electrical and systematic to the piezo node design.
