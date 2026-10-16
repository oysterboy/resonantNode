# 2026-10-16 band 2950-3950 check, 110 cm (tools/bench/bandcheck.sh)

Owner's decision the same evening (`docs/decisions/2026-10-16-network-band-2950-3950.md`): the network band moves
from 2800-3800 to 2950-3950 so the 3,200 Hz chirp sits on a bin (bins 2950/3200/3450/3700/3950). E2 Analyzer COM5,
combined build 162c847 + overrides (contrast medium 30,500, amp medium 1,500, history 512; identical to the defaults
shipped in c26b35d), `PARAM scalar_max_duration_ms=300`, E3 Emitter hop 0.5 FS, mic and speaker facing up, boards
unmoved since 2026-10-16-hopcand-110cm. SEQ DIAG on, 10 trials per cell. Companions: `-bandcheck-90cm`, `-70cm`.

| Cell | expected | amp P75 | contrast P75 |
|---|---|---|---|
| chirp 3,200 Hz in band 2950-3950 | 10/10 | 2,346-2,671 | 32,739-32,751 |
| hop5 25x5 gap 10 on 2950/3200/3450/3700/3950 | 10/10 | 2,733-2,937 | 31,209-32,510 |
| chirp 3,200 Hz single target (reference) | 10/10 | 2,291-2,495 | 32,747-32,751 |

The chirp's contrast in band mode equals the single-target value: 3,200 Hz is on a bin now. (Host replay on the
2800-3800 band had it at contrast weak in every capture, lab note 2026-10-16 section 9.)
