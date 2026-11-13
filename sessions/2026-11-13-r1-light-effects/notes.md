# R1 with the light's effects (2026-11-13, evening)

Firmware: `esp32dev-lr2` at e9212ad9 (the effects b8d388e6, HSB driver, NodeLight; decisions/2026-11-13-light-colour-
model-and-states.md), flashed by USB to R1 (COM3, CP210x, behind the long repeater; USB + buck, 24 V supply limited
to 1 A). Strip: the 28-pixel RGB WS2812 test strip at the bench wiring, no case. E0 gateway on COM12; E1-E3 on the
desk running their chains, unmuted except where noted. Nothing saved on R1 (`PARAM load stored=0`).

Question: does the light following Behavior at 50 frames a second cost the audio loop or the self-test?

## Runs

1. `watch.py COM3 240` (console.log): boot `RB light pin=18 pixels=28 cap_ma=500 status=ok`, `light.max_ma` 500.
   4 min with the field live: 169 valid verdicts, 143 emissions on R1 (a busy desk), and over the 4 min
   `i2s.dropped_dma_buffers` +0, `i2s.overflows` +0, `net.tx_failed` +0, `audio.block_us_max` 3626 us (3217 at the
   start), `heap.free_min` 81212 unchanged. `LIGHT hsb 120 100 100` sent 0,75,0 (green at B = 1: 75 = 255 x the one
   scale for 28 pixels); `LIGHT auto ok`. Two SELFTESTs with the others unmuted: one amp FAIL low_snr (the middle
   chirp's floor 54936 against 1649 in the passing one, the tone the same: a neighbour's train in the floor window),
   one PASS 40.7 dB.
2. `tools/bench/nodecheck_one.py` (nodecheck/): 23/24. The boot check failed on the room: the quiet gate found no
   quiet with E1-E3 in a chain (`RB baseline state=FAILED_NO_QUIET`), active a minute later. Every self-test passed,
   but the check's `LIGHT off` held the strip dark from then on, so these say nothing about the light (the check
   sends `LIGHT auto` since this session).
3. `quiet_selftest.py COM3 COM12` (quiet_selftest.log): E1-E3 muted by radio, R1 restarted (the light on its
   effects, rising to full with each own chirp), three SELFTESTs: amp PASS 40.8 / 40.2 / 42.1 dB (min 27.5 / 34.8
   / 39.8, spread 1.01), own-chirp PASS each; dropped buffers +0, overflows +0, block_us_max 1936 us. E1-E3
   unmuted after.

## Result

The effects cost the audio nothing measurable: no dropped buffers in 4 min of a busy field, the longest block
under 4 ms of the 24 ms queue, the amp check's SNR as with the strip dark earlier the same day (39.5 / 39.2 dB,
2026-11-13-hw005-radio-vs-mic). The one amp FAIL was a neighbour's call in the floor window, as in the first
nodecheck_one run; quiet, 3/3 PASS. Not seen by eye yet: the hues and the timings are first guesses for the owner.

State after: R1 on the effects, defaults, idle call on; E1-E3 unmuted.
