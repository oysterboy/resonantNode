# A bare ESP32 is the bench's radio gateway (esp32dev-gateway, named E0)

Status: decided (owner 2026-11-05 at the desk: "Ein neuer SoC als Funkrelay? (bare ESP)", "S0 = E0", "Flash relay")
Date: 2026-11-05

## Decision

A new mode `GatewayApp` (src/modes/gateway/, build env esp32dev-gateway, -D GATEWAY_MODE): radio and USB console
only, no audio, no detection, no Behavior. Its console takes `NET <target|*> <command>` and prints the boards'
`NET reply` and `NET log` lines; it runs nothing of the others' and has no params. The roster names it E0
(`0x809071F4E9D4`, d4:e9:f4:71:90:80, a CH340 board). The radio's command set gains `WHISPER` (the echo build's
pollNet already ran it; the Node-side gateway refused it).

## Why

- Until now the radio's USB gateway had to be a Node (RB, a full audio chain), so one of the five boards with a mic
  and an amp sat silent on a data cable and could never run echo or Whispernet as gateway: an echo or whisper board
  receives commands but cannot send them. Changing a mode of the others meant making a board a Node and back
  (2026-11-05, E2 and E3 switched five times).
- Boards with a mic and an amp want a charger or a hub with power; the data connection wants none of that (the PC's
  USB port dropped E2 with a playing amp).
- A bare board is cheap, has no audio load to drop, and prints at the UART's pace.

## Costs and what it rules out

- The radio log's rate limit sits in each sender (node.radio_log lines/s), so a gateway does not complete it
  (tools/bench/logcomplete.py measures that).
- The first flash needed BOOT held: the board has no auto-reset (esptool: "Wrong boot mode detected (0x13)"); tried
  again without the button at the owner's request: it fails the same. Later updates by WiFi (OTA).
- A new board needs a roster line (the eFuse MAC is printed at boot) or it names itself "unknown".
- Not a protocol change: the frames and the allowlist stay (VEKTOR still replaces NET, decisions/2026-10-25-vektor-
  replaces-net.md).
