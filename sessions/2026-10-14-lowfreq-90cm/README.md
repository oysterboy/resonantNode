# 2026-10-14 DET-012 / step 5b: lower chirp frequencies, DC blocker, 90 cm (as set), up, Emitter 0.5 FS, Scalar

E2 Analyzer default build (DC blocker, code bfbe0c8), E3 Emitter 0.5 FS (REMOTE), boards facing up.
Owner away for the first runs (quiet studio assumed, no room notes). The boards were re-plugged
since the earlier 90 cm runs: 3,200 Hz raw amplitude is ~35k now vs ~60k then (placement not
identical). SEQ `freq=` sets both the Emitter tone and the band target at runtime.

Raw received amplitude vs frequency, 500-3,600 Hz in 100 Hz steps: 2026-10-14-up-90cm-rawscan-wide
(speaker/mic response is far stronger at 600-1,300 Hz: 134k at 600, 240k at 700, 274k at 800,
128k at 1,000, 113k at 1,300; 26-87k from 1,500 to 3,600; dips at 2,300-2,400 Hz: 14-17k).

| f (Hz) | T3 10 trials | strength | notes |
|---|---|---|---|
| 2,600 | 10/10 | ~12,200 | |
| 2,000 | 10/10; 50/50 in T3_f2000_50 | 10,953 avg | silent OBS 50: 0 accepts, 3 source rejects |
| 1,500 | 10/10; 50/50 in T3_f1500_50 | 6,332 avg | silent OBS 50: 0 accepts, 0 rejects |
| 1,000 | 0/10 | ~20,700 | source reject duration_too_long (max 200 ms) |
| 800 | 0/10 | 32,767 (saturated) | duration_too_long |

Detection works down to 1,500 Hz with no threshold retune. At 1,000 and 800 Hz the received
tone is so strong that the event outlasts the 200 ms maximum duration (ringing, saturation); a
lower tone level or a longer max duration would be needed. Speech / music at the lower
frequencies not run (needs the owner at the bench). One placement, one room.
