# Remote params over ESP-NOW, through the serial PARAM path, one-line replies only

Status: decided, implemented 2026-10-25 (step 11a, PAR-013); long-run radio cost still open.
Date: 2026-10-25.

## Decision

- Transport: ESP-NOW on a fixed channel (NET_CHANNEL 1, network id NET_ID 0x52), no router. Commands are
  broadcast three times (receivers drop repeats by sender and seq), replies go back unicast. src/net/.
- One command path: a radio command is the same text line serial takes and runs through `handleParamCommand`;
  the reply is a copy of what it prints (a tee on serialOut). No transport-specific store or validation.
- Any USB node is the gateway: `NET <target|*> <command>`, replies printed as `NET reply from=...`.
- By radio only: PARAM SET / GET / SAVE / LOAD / RESET and NET PING. PARAM DUMP and LIST, and everything that
  sounds or blocks (RB CALL, SELFTEST), stay on USB.
- The radio is on by default; `node.radio_enabled` (read at boot, PARAM SAVE + reboot) turns it off.
- Sending never blocks the loop: an outbox sends one frame per pass, 4 ms apart; the gateway prints one reply
  frame per pass and only with 4 KB free in the UART ring.

## Why

- Owner 2026-10-25: set and save params on every node without cables ("OTA params"); persistence (10d) existed.
- Nothing on the radio is time-critical: the network itself stays acoustic. So the radio carries only operator
  commands, and the rule is that it must not cost audio.
- Measured on five boards (bench/sessions/2026-10-25-step11a-ota-params): radio idle 120 s, PING, GET, SET by
  radio: 0 dropped DMA buffers on every node. A `NET * PARAM DUMP` (17 frames per node) made the gateway drop 52
  buffers while it printed ~160 lines over USB (0 with printing off; 6 with one frame per pass), hence DUMP and
  LIST are USB-only.
- PARAM SAVE drops 8 buffers per node whether it comes by serial or radio: the NVS flash write, a cost of 10d,
  not of the radio. `NET * PARAM SAVE` costs every node those 8 at once; do it outside a session.

## Rules out

A second param store or validation for the radio; sound-making or blocking commands over the air; multi-line
replies over the air; a router dependency.

## Revisit when

A PC gateway (bigger replies at no audio cost), security beyond the network id (anyone with ESP-NOW on channel 1
and the id can set params: fine for the installation, not for a public space), or VEKTOR (PAR-013's third
transport).

## Source

Step 11a, issue #58; roadmap-param-config.md PAR-013.
