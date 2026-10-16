# 2026-10-16 Node first boot, logged from the first line (why run 1 of node-band-70cm heard nothing)

E2 flashed to the Node build e191851 (17:17), RTS-reset over USB, `RB log full` sent at the banner, 120 s logged
together with E3's USB lines. Boards at 70 cm.

Node: `RB startup baseline=LISTEN_FOR_QUIET` at 1.3 s, `RB rebase waiting smooth=358 quietMs=0` at 1.4 s,
**`RB baseline active` at 2.8 s**, then only idle emits (6 in 120 s). Verdicts 0.

E3: **0 `OK CHIRP` lines, no `emitter_ready` line**: it never reset into AUTO and stayed in REMOTE (silent) from the
sweep SEQ runs before. Its 29 lines are `EVT emitter_cmd line=<junk>`, the control UART reading noise while E2 (a
Node) does not drive those pins.

So the "deaf first run" of `2026-10-16-node-band-70cm` was a silent sender, not a deaf Node. E3's auto-reset circuit
does not act on the USB RTS/DTR pulse reliably (and does not enter download mode on its own); a port open sometimes
resets it, sometimes not. Verify from its lines before any Node count.
