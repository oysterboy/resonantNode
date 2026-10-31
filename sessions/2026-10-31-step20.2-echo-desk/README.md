# 2026-10-31: step 20.2, the echo room on the desk (E5 by NET OTA, the Emitter as the click)

E5 from the Node image to the echo shell and back by `NET E5 OTA` from E1, per run; E1 / E4 silent; the Emitter E3
in REMOTE plays the sources; the music file from the lab PC. Desk layout of 2026-10-26; remote. echo_desk.log per run
(token hidden). Every run returned E5 to the Node image with `PARAM load stored=50 ... stale=0`.

Runs 1-4 found and fixed (commits on main the same evening):
- run 1 / 2: `EMIT PULSES` with a 0 ms gap is refused by the Emitter (the test's bug); the gate levels were guesses
  far above what reaches E5 (onset 12000 Strength16 against ~2,000 for a click at ~1 m, the quiet room 40-100).
- run 2: `clip_missing`: the clip was cut by the source's sample index, which also counts the 25 DMA buffers dropped
  at boot; now the ring's own index (2026-10-31 "cut the clip by the ring's own index").
- run 3: the attack as a fixed floor-to-threshold time made a loud slow ramp look fast; now 10 % -> 90 % of the
  window's peak (cac39e37), levels onset 1000, release 700, floor 300.
- run 4: the answer (wait 350 ms + up to 300 ms jitter) came after the 500 ms ring overwrote the clip's start; now the
  clip is copied as soon as its tail is recorded (744f0b8a). ~800 reject lines under music cost the loop a buffer;
  now only valid claps are printed.

Run 5 (744f0b8a), the result:

| source | occurrences | claps (valid) | replays | note |
|---|---|---|---|---|
| 15 clicks: `EMIT PULSES 1 10 10 freq=2000 ramp=1` | 15 | 15 (attack 3-6 ms) | 12 | three declined by the level automaton |
| 10 swells: `EMIT PULSES 1 200 10 freq=1000 ramp=50` | 10 | 0 (all `slow`) | 0 | |
| bench_audio.wav once (7 min, music and speech) | 842 | 34 (4 %) | 23 | sharp onsets just above the onset level (strength ~1,000-1,400) |

No DMA buffer dropped after boot (25 at boot, as in every echo boot); the loop's one 37 ms pass ~20 s after boot is the
OTA confirm writing flash. Largest free block with ring and clip: ~39 KB.

Open: the music's 34 claps. They are real transients (drums, consonants) just over the onset; a clap near the node is
far louder. Whether the echo room should answer them is the owner's call (a higher onset or a minimum peak).
