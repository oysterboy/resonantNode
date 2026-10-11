# 2026-10-11 #20 R5: class-D idle noise at the mic (E3, Node be61884)

Measured with SELFTEST's mic check (0.75 s quiet floor on the raw mic
stream) instead of a RAW spectrum: RAW needs a linked Emitter and both boards
were Nodes. On a Node the I2S TX writer runs continuously and sends zeros
when idle, which is R5's "amp idle" condition. E2 held in reset; room quiet.

| Session | Condition | floor_mean | floor_peak |
|---|---|---|---|
| r5-amp-idle-1 | amp idle (TX zeros) | 1,168 | 4,528 |
| r5-amp-idle-2 | amp idle | 963 | 2,250 |
| r5-amp-vin-off-1 | amp VIN unplugged | 1,078 | 2,643 |
| r5-amp-vin-off-2 | amp VIN unplugged | 1,018 | 2,645 |

The idle class-D amp does not raise the mic floor (difference inside
run-to-run spread). VIN-off confirmed by the amp check (-6.5 / -5.7 dB).
Caveat: broadband floor, not the planned 100 Hz-8 kHz / 3,200 Hz band
spectrum. Two earlier amp-idle runs taken while music was still playing
(floor 45-51k) were discarded and not committed.
