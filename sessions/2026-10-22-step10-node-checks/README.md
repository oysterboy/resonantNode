# 2026-10-22: step 10 Node checks (item 10 and 3d leftovers)

Setup: trial layout; E2 (COM3) Node, built from the tree committed as 9721ed5; E3 (COM4) Emitter 23d034a; E1/E4/E5 held
silent. One script run, node_checks.9721ed5.log.

- `RB debug plot` -> `RB debug mode=events (use off|events)`: the plot mode is gone.
- A3: a 300-character line -> `RB ERR line_too_long max=255`.
- A4: a bare `RB PROFILE` -> `RB PROFILE name=TonalPulseScalar`.
- A1 (#32): `RB CALL`, then `RB rebase force` 60 ms into the call, then an E3 hop train. The node did not answer
  its own call. The forced rebase went straight to "baseline active", so there was no settle window. The E3 train
  (~1 s later) was heard (valid=1) and answered (source=heard_pattern). Not observable here: whether the call's
  sound stopped at the rebase.

## Item 9, the Node glue split (426a03f)

- item9_glue_split.426a03f.log: RB STATUS prints all 61 fields (loop.stack_free_min_bytes=3448). 5/5 hop trains from E3
  answered. SELFTEST full: PASS 5/5 (board, mic, quiet_boot, amp 5/5 with SNR 39-43 dB, own_chirp_suppressed
  through the SelfTest restore, which now calls resetDetectionState()). The forced 3 s idle timeout produced no
  idle call within 9 s: setting the timeout does not reschedule an idle call that is already scheduled
  (pre-existing behavior, not changed here).
- item9_idle_call.426a03f.log: a natural idle call 66 s after boot, through the extracted path: `RB emit
  shape=idle_call announce_hz=300 announce_ms=500 gap_ms=200 then=hop`, then `RB emit shape=hop`.
