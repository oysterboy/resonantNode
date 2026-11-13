# R1: the first long recording over USB (step 26's test recording, 2026-11-13, late)

Board: R1 (stripboard board 1, lochraster_v2) on COM3 (CP210x behind the long USB repeater), flashed with
`esp32dev-lr2-whisper` (Whispernet, 24 kHz). Playback off for the run (`PARAM SET whisper.enabled 0`, live, not
saved). The room: the desk with E1-E3 in their chains, no voices. Tool: `tools/bench/longrec.py` (REC at 921600
baud, framed int16, src/audio/LongRecorder.h).

## Runs

1. `REC ERR no_heap`: the recorder's ring wanted one 12 KB block; with Whispernet's 96 KB line and the radio up the
   largest free block was 7.9 KB (8.7 KB in a build without the light, `-DLIGHT_PIN=-1`: the light is not the
   cause). Fixed: the ring takes the largest of 12 / 8 / 6 / 4 KB that fits and `REC start` says which
   (`ring_bytes=`); the port drains twice what the mic fills, the ring only bridges the loop's pauses.
2. `test_60s.wav` (console.txt): ring 6144 B, 1432976 / 1440000 samples (99.5 %), 0 bad frames, board
   `dropped=7024`, all in the first 0.4 s: the ring filled from REC while the first frame waits 400 ms for the
   PC's baud switch. DMA buffers dropped during the run: 0 (12 since boot, before REC).
3. Fixed: the ring fills from the first frame on. `test_60s_b.wav` (console_b.txt): 1440000 / 1440000 (100 %),
   0 bad frames, `dropped=0`, DMA drops during the run 0 (13 since boot, before REC); the console answered at
   115200 after `REC end`. Loop pass max 7.0 ms while streaming (2.6 ms before), the audio block max unchanged.

`longrec.py` gained `--boot-wait`, `--setup` and `--status`: opening R1's port restarts it, so the REC must wait for
the boot, and live settings (playback off) only last while the port stays open.

## Result

Step 26's first gate item: a test recording with under 1 % of the samples missing: 0 % on run 3. The corpus
sessions can use R1 (or any stripboard node) as is. The level: the quiet desk reads RMS ~27, peak ~440 of 32767
(int16 = the mic's 24 bits >> 8); whether a whisper at a metre sits well above that is the first thing to look at
with a voice.

State after: R1 back on the Node image (`esp32dev-lr2`), nothing saved.
