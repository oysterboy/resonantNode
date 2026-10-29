# OTA firmware: every board pulls the image over HTTP from WiFi, on command, with rollback until confirmed

Status: decided (owner 2026-10-28); implemented 2026-10-28 (40c92e3), bench-checked 2026-10-29 (f2330b1: all five
boards over WiFi, NET * OTA for the Nodes, the rollback of a crashing image; bench/sessions/2026-10-29-step11d-ota).
Date: 2026-10-28.

## Decision

- `OTA url=http://<host>:<port>/<file>.bin` on a board joins the WiFi named in `include/wifi_secrets.h` (local,
  git-ignored; a placeholder `wifi_secrets.example.h` is in the repo), downloads the image into the other app slot
  and reboots into it. On the Nodes the command also travels by radio: `NET * OTA url=...` updates every Node at
  once, the USB Node then itself. The Analyzer and the Emitter take it on USB.
- The image is served by `tools/bench/otaserve.py` on the lab PC (node.bin, analyzer.bin, emitter.bin from the
  build).
- The default partition table already has two app slots (app0 / app1, 1,280 KB each) and otadata; no repartition.
- A new image starts unconfirmed and marks itself valid after 20 s of running (`verifyRollbackLater()` returns true;
  `ota::confirmRunningImage`); one that crashes or resets before that is rolled back by the bootloader. A failed or
  interrupted download leaves the running image as it was.

## Why

Owner choice 2026-10-28: pull, all at once (one command over the radio updates the installation; no per-board
addresses), over push one by one (espota, five uploads, every board in a listen mode). The radio leaves its ESP-NOW
channel only while a board updates; a failed attempt puts it back (`NodeNet::restoreChannel`).

## Rules out

WiFi kept on in normal operation; credentials in the repo or in params; firmware over ESP-NOW.

## Revisit when

VEKTOR's transport replaces NET (decisions/2026-10-25-vektor-replaces-net.md), or an installation has no WiFi at
hand (then a phone hotspot with the same SSID, or a board that serves the image).

## Source

Owner answers 2026-10-28 (credentials; "Pull, all at once"); step 11d, issue #61.
