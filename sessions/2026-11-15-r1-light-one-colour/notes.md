# R1: the Node's light as one colour with a floor (2026-11-15, evening)

Board: R1 (stripboard board 1, lochraster_v2) on COM3, image `esp32dev-lr2` with the rebuilt light
(decisions/2026-11-15-node-light-one-colour-and-floor.md). E1-E3 muted by radio (`output.silent 1`, not saved);
R1 unmuted by its restart. Live for watching, not saved: `behavior.call_after_silence_ms 10000`,
`behavior.call_after_silence_spread_ms 0`, `node.radio_log 1`.

## What the owner saw, and what changed

1. The 2026-11-13 effects (colours A and B, the fall to dark): "does too much, too much colour changing". Rebuilt as
   one colour per node, a floor of 0.35, dither across the pixels, emission the only moment at full.
2. First flash: "kind of steady, no idle call yet": the call waits 30 s of silence plus spread; set to 10 s live.
3. "The emit light should sit on the chirp, not the emit announce": the light now holds through the idle call's
   announcement tone and its gap (SoundOutput's phase), the jump comes with the call. Reflashed.
4. "The fade out after emit can be a lot quicker", then "fade to idle level in 2 s": light.glow_ms 5000 -> 2500
   (reflash) -> 2000 (live, and the default in code).

## E1: a wire fault, not fixed (owner)

E1 (breadboard D-AMP node, MAC 24:dc:c3:4a:b0:50, image c528cd78, up 8.4 days) was silent to the ear while its
firmware played (`silent=0`, 36351 emissions started). SELFTEST by radio, the others muted:

1. Mic `FAIL all_zero` (nonzero 25.3 %, bit8 24.7 %); amp skipped.
2. The owner reattached the mic (E1 restarted). Mic PASS (nonzero 100 %, bit8 49.7 %), quiet boot PASS; amp
   `FAIL low_snr`: tone 51k-59k, floor 4.2k-6.4k, SNR 18.2-22.8 dB (line 30), spread 1.15. By ear: a noisy amp.
3. The owner found a wire issue; not fixable tonight. The picture fits the BCLK crackle of the first stripboard
   (docs/lab/2026-11-12-hw-001-first-board.md: quieter call under crackle) or a loose amp GND / 5 V Dupont (E3,
   docs/lab/2026-10-17-hardware-fix-steps.md). No thresholds touched.

State after: E1 muted by radio (`output.silent 1`, not saved: a restart unmutes it), its idle call back to
30 s / spread 10 s / 500 ms; R1, E2, E3 unmuted. E1 needs the wire fixed and a SELFTEST PASS before it plays again.

## Open

- After the first flash R1 stopped answering by radio (E1-E3 did) while it still answered over USB; COM3 vanished
  once from the port list. A restart over USB brought the radio back. Cause unknown, not seen again in the session;
  watch for it (the USB repeater, or the radio).
- Heard (the lift and build) not yet seen: needs another node calling.
- The 0.8 A test (28.3) and the floor in a dark room.
