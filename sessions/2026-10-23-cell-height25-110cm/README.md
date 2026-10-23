# NODE-012 cell: desk, height offset (E3 raised 25 cm), 110 cm

Step 5h. tools/bench/cellrun.py; speaker stock D-AMP 3 W / 8 ohm, tone level 0.5 FS, mic and speaker facing up.
- E3 Emitter raised 25 cm above E2 (owner), same 110 cm layout as the reference cell otherwise
- E2 Analyzer main f38cd69, E3 Emitter e58c4c2 tree; E1/E4/E5 Nodes silent (saved)

| run | expected/trials | rejected | missed | buffer overruns | avg strength |
|---|---|---|---|---|---|
| hop50 | 42/50 | 8 | 0 | 0 | 6377.9 |
| chirp50 | 50/50 | 0 | 0 | 0 | 10530.1 |

Contrast P75 (limit 30500), inspectstats.txt:

```text
chirp50.f38cd69.log                windows  50  inspected  50
    amp P75      6114-6497 (med 6290)
    amp P90      6346-6702 (med 6534)
    amp peak     6629-7112 (med 6846)
    contrast P75 32757-32758 (med 32757)
    contrast P75 >= 30500: 50 of 50
hop50.f38cd69.log                  windows  50  inspected  42
    amp P75      2310-2551 (med 2463)
    amp P90      3145-3502 (med 3330)
    amp peak     5028-5672 (med 5292)
    contrast P75 30640-31118 (med 30907)
    contrast P75 >= 30500: 42 of 42
```

Room notes: E3 raised 25 cm by the owner, otherwise the reference layout (bench/sessions/2026-10-23-cell-desk-ref-110cm),
15:17-15:25, quiet, no playback.

## Reading (against the reference cell)

| | reference (flat) | E3 +25 cm |
|---|---|---|
| hop expected | 50/50 | **42/50**, 8 rejected `coverage_too_low` at the detector |
| hop read as (duration_ms) | 191-194 | **107-118** (27 of 50 at 108) |
| hop coverage_ms (min 80) | 159-164 | **78-94** |
| hop contrast P75 (limit 30500) | 32252-32426 | 30640-31118 |
| chirp | 50/50, contrast 32744 | 50/50, contrast 32757, amp P75 x3.5 |

Per-bin received level (tools/bench/hopbins.py, 5 RAW captures each, dB re the strongest bin), hopbins.txt:

| bin | 2950 | 3200 | 3450 | 3700 | 3950 |
|---|---|---|---|---|---|
| reference | -9.9 | -11.2 | **-17.8** | -5.5 | 0.0 |
| +25 cm | -6.4 | 0.0 | -3.3 | -7.3 | **-12.4** |

The 25 cm offset moves the nulls. In the reference the deep null is the middle step and the steps around it bridge
it: the train reads whole. Raised, the last two steps are weak, the train ends after step 3 (3 x 25 + 2 x 10 +
release), reads ~108 ms, 16 ms over the 92 ms minimum, and its coverage sits on the 80 ms limit: 8 of 50 fail.
So the hop train's robustness depends on where the room's nulls fall in it, and a weak tail is the bad case: a
minimum near 100 ms (DET-017's first proposal) would have lost most of these. Rotation 0 only (the Emitter plays
2950 first); a Node with another rotation puts different steps at the end.

Levers (not applied, a tuning pass decides; roadmap-0-steps 5h): the train's coverage and min duration against a
truncated train, more steps, DET-015's shape inspector reading the steps rather than one envelope. The plain chirp
is unaffected here.
