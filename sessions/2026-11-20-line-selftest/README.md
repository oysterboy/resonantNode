# 2026-11-20 the line of four: self-test, power on play

Setup: R1 (lr2, USB COM3 to the lab PC + lab supply) - 1 m - E2 (D-AMP, USB power bank + long extension) - 1 m -
E4 (D-AMP, USB power bank + long extension) - 1 m - E3 (D-AMP, separate USB supply). Gateway E0 on COM12.
Firmware: E3/E4 by radio to ec38c754 (10:38), E2 (10:42), then all to dd22ab0a (11:09-11:17; R1 by USB).

- SELFTEST by radio, one node at a time, the others muted (selftest_radio.log): all four PASS (board, mic, amp).
  Amp tone: R1 548k, E2 285k, E4 335k, E3 375k. E4's mic floor the highest (mean 1472).
- Play test 10:45-10:56 (playwatch.log; every node calling every 2.5 s, trill-taper-glide-grain-risefall):
  E4 rebooted 4 times (also twice after its OTA while unmuted), the others 0. E4 was on the cable and power bank
  E3 had rebooted on before (owner).
- Cable swapped on E4: 10:58-11:08, 0 reboots on all four (690-850 calls each). But at 11:42 E4's amp was silent
  (SELFTEST amp FAIL, tone 905 against floor 3044; the 11:27 smoke test heard nothing from E4): a loose wire, fixed
  by the owner ~11:47; amp PASS 526k.
- Play test 11:48-11:58 after the fix: 0 reboots on all four (669-822 calls each); E4 amp PASS 520k after it.
