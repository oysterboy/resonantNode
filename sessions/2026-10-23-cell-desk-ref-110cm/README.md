# NODE-012 cell: desk (reference, trial layout), 110 cm

Step 5h. tools/bench/cellrun.py; speaker stock D-AMP 3 W / 8 ohm, tone level 0.5 FS, mic and speaker facing up.
- E2 Analyzer (main f38cd69), E3 Emitter (e58c4c2 tree), 110 cm, both facing up; E1/E4/E5 Nodes silent
- the reference the dry, height and site cells compare against

| run | expected/trials | rejected | missed | buffer overruns | avg strength |
|---|---|---|---|---|---|
| hop50 | 50/50 | 0 | 0 | 0 | 10088.5 |
| chirp50 | 50/50 | 0 | 0 | 0 | 4139.3 |

Contrast P75 (limit 30500), inspectstats.txt:

```text
chirp50.f38cd69.log                windows  50  inspected  50
    amp P75      1659-1981 (med 1817)
    amp P90      1821-2098 (med 1946)
    amp peak     2542-3006 (med 2717)
    contrast P75 32739-32748 (med 32744)
    contrast P75 >= 30500: 50 of 50
hop50.f38cd69.log                  windows  50  inspected  50
    amp P75      1946-2059 (med 2027)
    amp P90      2345-2486 (med 2407)
    amp peak     3492-3936 (med 3681)
    contrast P75 32252-32426 (med 32369)
    contrast P75 >= 30500: 50 of 50
```

Room notes: the trial desk as in DET-017 and 10b, quiet, no playback, afternoon 13:14-13:21. Nothing changed: this is the reference.
