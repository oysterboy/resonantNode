# 2026-10-23: step 10e, loose ends before 10c block 2

Boards: E1 (COM8) Node, the builds below; E3 (COM4) Emitter (e58c4c2 tree) for heard trains; E4 (COM10) Node 8ec0764
in the drift run; E5 held silent. Same desk layout as DET-017.

## 1. idle_timeout reschedule (host only)

Behavior change in d275992: a live idle_timeout / variation change reschedules the running
idle wait from its start. Host tests test_shorter_idle_timeout_applies_to_the_running_wait and
test_longer_idle_timeout_extends_the_running_wait_and_same_value_keeps_it; both fail without the fix.

## 2. hop_check gap_not_below_debounce: not a bug

The soft warning is deliberate and pinned by a host test since 5h (docs/lab/2026-10-20-step5h-host-checks.md):
hop gap 10 = release debounce 10, so the train is one occurrence only where the room tail bridges the gaps. The
measured lever (debounce 20 reads dry trains whole) is a tuning change; 5h's dry-room NODE-012 cell decides it.

## 3. The 3.4 KB loop-stack low point: found and fixed (9972fe4)

Probe builds (temporary `PROBE <stage> free_min=` lines, `probe.py`, `probe3.py`; logs stack_probe1/3) on E1.
The loop task has ~15.4 KB free at begin(). It falls from 14,172 to 3,688 B inside `DetectionRuntime::resetState()`:
`FeatureHistory::resetStream()` did `buffer = {}` on a 12 KB StreamBuffer, and GCC built the value-initialized
temporary on the stack. Nothing else came near it (PARAM SET, idle calls with log full, DUMP, SAVE all stayed at the
boot value). The same call runs on every detection reset: startup rebase, RB rebase, RB PROFILE, self-test.

Fix: memset (every member defaults to zero; static_assert trivially copyable). After the fix (stack_after_fix.log):
14,172 B free after boot, after RB rebase, after RB PROFILE and after 12 s of idle calls with log full (was 3,448).
T2h byte-identical to t2h_replay.518ffe6.txt; host suites 21 + 34.

## Also: strength fields for the drift item (db5213c)

- heard_strength_e1.log: five Emitter hop trains to E1 (silent, listening): five `RB verdict ... strength=11227..11373
  duration_ms=202` lines; RB STATUS heard.valid 0 -> 5, heard.strength_sum 0 -> 56,574 (= the five lines' sum).
  (A first try ran with E4/E5 accidentally released; each train was then also answered, second verdicts 24-26k.)
- drift90b (nodegroup, E1 + E4 free 90 s, drift bins 30 s): `E1 <- E4: 24258(7) 24066(8) 24089(5)`, E1 STATUS mean
  23986 / 23846 per bin. E4 runs 8ec0764 (no strength=), so E4 <- E1 has no table row. The first STATUS bin read "-";
  fixed afterwards (counters count from 0 at the run's reset).

Left: E1 store output.silent=1 (boots silent); E4/E5 held silent by a port-holding process.
