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
