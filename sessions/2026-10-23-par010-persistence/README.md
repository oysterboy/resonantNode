# 2026-10-23: step 10d, PARAM SAVE / LOAD / RESET on a Node (PAR-010/011)

Setup: E1 (COM8) only, Node `esp32dev` built from the tree committed as main 3f2ed74 (the boot line reads
`git=78dd50e`: it was built before the commit, from 78dd50e plus this change). E4/E5 held silent with ports open;
E2/E3 untouched. No audio. Scripts: `persist_test.py` (run 1), `persist_test2.py` (run 2, log `run2_rotation.log`).
Reboots by RTS pulse with DTR held low.

Run 1 (output in the session transcript, lines summarized):
- fresh boot: `PARAM load stored=0 applied=0 stale=0 invalid=0`
- SET behavior.idle_enabled 0, behavior.rest_ms 6000, output.hop_rotation 3; `PARAM SAVE written=37 unchanged=0
  failed=0 status=ok`; a second SAVE `written=0 unchanged=37`
- reboot: three `PARAM load path=... value=... default=...` lines (hop_rotation 3/0, idle_enabled 0/1, rest_ms
  6000/5000), `stored=37 applied=37 stale=0 invalid=0`; PARAM GET returns the saved values
- SET rest_ms 7000 (unsaved), `PARAM LOAD`: back to 6000
- `PARAM RESET store=cleared values=defaults`: rest_ms 5000, hop_rotation 0; next boot `stored=0`

Run 2 (`run2_rotation.log`): a saved rotation prints `RB node rotation_source=stored rotation=3` after the roster
line; RB STATUS after the boot shows hop_rotation=3, i2s.dropped_dma_buffers=0, read_errors=0,
loop.stack_free_min_bytes=3448 (same as before); RESET, next boot stored=0.

Left on the board: store cleared (defaults), silenced with unsaved PARAM SETs, port held open.

Not covered on the board: the `PARAM stale` path (needs a firmware with a moved default; host test
test_stored_value_wins_over_a_changed_default_and_counts_stale) and invalid entries (host tests). The time a
37-entry SAVE blocks the loop was not measured; SAVE is an operator command, not run during sessions.
