# 2026-10-29: step 11d, OTA firmware over WiFi, the gate

Lab PC 192.168.178.33 on WiFi "Kosmos"; firewall rule "resonantNode OTA (8070)" (inbound TCP 8070, Private
profile), added 10-29 through an elevated netsh after the owner approved the UAC prompt by remote desktop. Images
served by tools/bench/otaserve.py. All boards had 40c92e3 (USB, 10-28; the one bootstrap flash).

1. E1 `OTA url=http://192.168.178.33:8070/node.bin` (4208e72): WiFi -52 dBm, download, reboot, BUILD 4208e72,
   warmup, `OTA confirmed partition=app1`, 29 s.
2. E1 `NET * OTA url=.../node.bin`: E1 updated itself, E4 / E5 got nothing: the broadcast was still queued when the
   gateway's own WiFi join moved the radio off its channel. Fix f2330b1 (NodeNet::flush before the join).
3. E1 took f2330b1 by OTA (32 s), then `NET * OTA url=.../node.bin`: E1, E4, E5 each `OTA start`, joined WiFi
   (-54 / -48 / -52 dBm), downloaded, rebooted into f2330b1 and confirmed (app0 / app1 / app1), all within 32 s.
4. Analyzer (E2, USB) `.../analyzer.bin`: f2330b1, confirmed app1, 32 s. Emitter (E3, USB) `.../emitter.bin`:
   f2330b1, confirmed app1, 29 s.
5. Rollback: E5 `OTA url=.../crash.bin`, a Node build with -D OTA_TEST_CRASH (aborts 5 s after boot, before the
   20 s confirm): download ok, reboot into the crash image (built 13:19:44), `OTA test_crash aborting before
   confirm`, abort, reboot, and the bootloader booted the previous image (f2330b1, built 13:14:51) by itself. E5
   then ran normally (RB STATUS, answered NET PING with E4).
6. Earlier failure path (10-28, E1, firewall still closed): WiFi joined, download refused, the old image ran on,
   5/5 hop trains answered, radio back on its channel.

All five boards end on f2330b1, flashed over WiFi.
