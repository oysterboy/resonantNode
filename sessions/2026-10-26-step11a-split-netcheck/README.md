# 2026-10-26: step 11a, the role split (5ffab4a) on all five, tools/bench/netcheck.py

All five on Node 5ffab4a code (built from c0b75e9: the split plus docs), output.silent=1 stored, nobody at the bench.
`netcheck.py --gateway E1=COM8 --nodes E2=COM3,E3=COM4,E4=COM10,E5=COM11 --persist-node E3 --selftest E5
--idle-s 120`; report netcheck.txt, everything read in netcheck.log.

Every step ok: radio idle 120 s, PING, GET, SET + SAVE + reboot on E3 (kept), RB STATUS (1,563-1,580 chars per node,
joined), PARAM DUMP (39 lines each), `NET * SELFTEST` refused, `NET E5 SELFTEST` report complete. Dropped buffers 0
on every node in every step except E3 +9 at its SAVE (the flash write).

E5's self-test: board, mic, amp PASS; quiet_boot FAIL (reason=no_quiet, smooth 453 over the 400 threshold at that
boot, FAILED_NO_QUIET after 8 s). E5 is the board on the long repeater cable whose intermittent mic fault the owner
accepted in step 6; it also skipped its startup rebase on 2026-10-23 (smooth 425). Not the build: the other four
boot quiet on it. netcheck.py now marks such a step "warn" (it said ok here).
