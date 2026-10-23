# 2026-10-23: step 10f, SoundOutput (OUT-002/003): emission before and after

Same 90 s run twice: `nodegroup.py --boards E1=COM8,E4=COM10 --silent E5=COM11 --secs 90 --status-every 20
--drift-bin 30`, setup output.silent 0, idle_timeout 6000 +- 1000 (unsaved). E4 on Node 8ec0764 both times, E5 held
silent. Desk layout as DET-017. No playback.
- before: `drift90b` (E1 on db5213c, the step 10e session's run)
- after: `out10f` (E1 on f38cd69, built from the tree before the commit: the boot line reads git=9972fe4)

`shapecmp.py` -> `shapecmp.txt`:

| | before | after |
|---|---|---|
| hop shape lines (E1) | 33, all steps=5 pulse 25 gap 10, order 2950..3950 | 31, same |
| idle_call lines | 10, 300 Hz 500 ms gap 200 then=hop | 12, same |
| idle_call line -> train line (PC time) | 0.45-0.62 s | 0.41-0.55 s |
| E4 heard E1's answer trains | 15/23 | 14/19 |
| E4 heard E1's idle-call trains | 6/10 | 6/12 |
| chains ended within 30 s | 16/16 | 12/12 |

The PC stamps lines when it reads them, so the idle-call interval is shorter than the firmware's 700 ms in both.
RB STATUS after: `output.phase=idle output.started=28 output.done=28 output.rejected=0 output.last_reject=none
output.span_ms=165` (the last STATUS at 80 s; emissions after it are not counted there).

Not covered: the reject and fallback paths on a board (host test test_sound_output), the piezo build (compile only).
