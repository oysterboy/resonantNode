# 2026-10-29: step 11i / 11j bench gate, the Node split on the board (f2330b1, delivered over WiFi)

All five boards on f2330b1 (11i N1-N8, 11j, the OTA flush fix), flashed by OTA (2026-10-29-step11d-ota).

    python tools/bench/nodecheck.py --gw E1=COM8 --a E4=COM10 --b E5=COM11 --emitter COM4 \
        --out bench/sessions/2026-10-29-step11i-nodecheck --ref-log bench/sessions/2026-10-28-soak96-40c92e3/soak.log

results.txt: 25 / 27 PASS on the first run, the two misses explained and re-run (E4_selftest_rerun.log,
E4_selftest_unsilenced.log):

- Boot (E1): BUILD 0.49 s, warmup done 2.72 s (rebase ok), baseline active 5.45 s; PARAM load stored=39 stale=0.
- RB STATUS key set identical to 40c92e3's (the soak96 log), 75 keys.
- Every command in the Node's table answers as before (RB help / summary / log / PARAM x / BEHAV x / PARAM GET /
  DUMP (47 params) / NET STATUS / RB rebase / RB rebase force / RB PROFILE / OTA without url); an unknown line gets
  `RB ERR unknown_command`.
- Silent (stored): 3 hop trains heard, decision=outputs_off, no answer. Silent off: 10 / 10 valid, 10 answers,
  hop order 2950..3950 (rotation 0).
- PARAM SAVE / reboot / load on E4 (behavior.idle_timeout_ms 60000 -> 61234 -> reboot -> 61234), restored and saved.
- Radio: NET * NET PING (E4, E5 reply); NET * PARAM SET round trip (status=ok from E4, E5); NET E5 SELFTEST by radio,
  board / mic / quiet_boot / amp PASS in the replies.
- SELFTEST on E4, first run: quiet_boot FAIL reason=no_quiet (E4 had rebooted for the PARAM check into a loud
  moment: smooth up to 508 against 400); after a fresh reboot PASS. external_chirp: first run SKIP not_requested (the
  script sent SELFTEST external while the plain SELFTEST still ran); re-run in order: PASS, latency 308 ms.
- own_chirp_suppressed: FAIL chirp_not_started on a silent Node (the self-test only requests chirps while outputs
  are enabled, before and after 11i N6); with output.silent 0: PASS, 5 started, 0 own verdicts.
