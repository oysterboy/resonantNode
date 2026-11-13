# R1: light, power path, amp check (2026-11-13, evening)

Board: R1 (stripboard board 1, `lochraster_v2`, MAC c8:2e:18:c0:bd:0c), image `esp32dev-lr2` built 2026-11-13 15:27
from 36366672 plus the uncommitted light code (older than 0b54c9c0: it has no `LIGHT alt` / `alt4`). 330 ohm in the
mic's SCK wire. R1 on a CP210x behind a long USB repeater (COM3); E0 gateway on a CH340 (COM12). E1-E3 muted by
radio (`output.silent 1`, not saved). Fan at 1 m. 24 V lab supply, limit 1 A, into the MP1584 buck.

## 1. The "bad noise" (owner, by ear)

Heard once on R1's amp before the repeater was connected. Not reproduced with R1 calling every 3 s
(`behavior.call_after_silence_ms 3000`, spread 0, PARAM SAVE by radio): USB + buck, buck only, buck only after a
reboot, USB only: all clean by ear. RB STATUS by radio in each: `i2s.dropped_dma_buffers=9` (all at boot, no rise),
`i2s.read_errors=0`, `net.tx_failed=0`. Cause unknown; one-off. R1 back to defaults afterwards (`PARAM RESET`,
store cleared).

## 2. Light at the cap and the amp check (USB + buck, call off)

Speaker moved this afternoon: now standing on a box (a carton before). Six SELFTESTs over USB, alternating strip
dark / `LIGHT 100` (sent 70,70,70 = the 0.5 A cap, scale 27 %), 17:24:51-17:26:07:

| strip | amp | snr_min-max dB | spread | floors | tones |
|---|---|---|---|---|---|
| dark | PASS | 29.3-38.7 | 1.13 | 2133 1986 2045 5510 4588 | 160k-181k |
| white | PASS | 34.8-38.2 | 1.03 | 2624 2100 2404 2990 2060 | 162k-168k |
| dark | PASS | 33.1-36.7 | 1.03 | 3674 3081 2415 2397 3297 | 161k-166k |
| white | PASS | 33.7-37.2 | 1.11 | 2467 3208 3361 2957 3342 | 161k-179k |
| dark | PASS | 37.0-42.5 | 1.11 | 1220 2388 1672 1758 2464 | 163k-180k |
| white | PASS | 30.1-40.4 | 1.02 | 2109 2230 5308 2585 1641 | 169k-172k |

Mic PASS 6/6, bit8 49.6-50.8 %. No difference between dark and white; no spikes like the radio set of #99
(193k). Before the speaker was settled (17:1x, one SELFTEST each): white tone 45k-62k, dark 47k-73k, both
FAIL low_snr: the geometry, not the light.

Note (not a retune): the first dark run's snr_min 29.3 dB is just under the 30 dB line while still PASS (the check
judges the median chirp).

## 3. Current at 24 V (owner, lab supply display)

| strip | 24 V current | strip's share | ~5 V equivalent (85 % buck, assumed) | firmware est_ma |
|---|---|---|---|---|
| off | 0.034 A | - | - | 0 |
| LIGHT 100 (cap) | 0.109 A | 0.075 A, 1.8 W | ~0.31 A | 500 |
| LIGHT 10 | 0.062 A | 0.028 A, 0.67 W | ~0.11 A | 176 |

With the amp chirping at the cap: 0.113 A. The firmware's estimate runs about 1.6x above the drawn current (30 px,
this RGB reel). Not a decision on the cap.

## 4. What went wrong in the run

- `lightmatrix.py` state a was started twice and stopped twice; the second run's data were lost (written only at
  the end, the kill skipped it). Fixed: the script saves after every cell.
- Opening R1's port restarts it; the run's output was piped through grep and held back.
- `LIGHT alt` is refused by this image (`LIGHT ERR usage`); reflash R1 with the committed image before the matrix.

## Open

- #99's radio half: SELFTEST by radio after the speaker move; `LIGHT_PIN=-1`; `node.radio_enabled 0`.
- A radio stress with the light on (NET traffic while LIGHT 100 is held) was not done.
- #94 check 3 (white reel type) not done.
