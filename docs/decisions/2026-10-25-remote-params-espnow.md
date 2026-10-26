# Remote commands over ESP-NOW, through the serial command path; large replies paced, SELFTEST one node at a time

Status: decided, implemented 2026-10-25 (step 11a, PAR-013); long run with the radio on done 2026-10-26 (12 h, every pass mark, no leak).
Date: 2026-10-25.

## Decision

- Transport: ESP-NOW on a fixed channel (NET_CHANNEL 1, network id NET_ID 0x52), no router. Commands are
  broadcast three times (receivers drop repeats by sender and seq), replies go back unicast. src/net/.
- One command path: a radio command is the same text line serial takes and runs through `handleSerialLine`;
  the reply is a copy of what it prints (a tee on serialOut). No transport-specific store or validation.
- Any USB node is the gateway: `NET <target|*> <command>`, replies printed as `NET reply from=...`.
- By radio: every Node command (PARAM, RB, SELFTEST) and NET PING; not the other NET commands. First built
  for one-line PARAM replies only; widened the same day (owner: "add all, with a warning in the manual";
  SELFTEST by radio is useful during installation setup). SELFTEST needs one named target (`*` refused at the
  gateway and the node); "the others silent" is the operator's rule (notes_manual.md, Remote Commands).
- Replies are a byte stream cut into frames, paced 150 ms per frame per node; the gateway joins long lines
  (RB STATUS, ~1.6 KB) per sender. A self-test keeps its reply open until it ends.
- The radio is on by default; `node.radio_enabled` (read at boot, PARAM SAVE + reboot) turns it off.
- Sending never blocks the loop: an outbox sends one frame per pass, 4 ms apart; the gateway prints one reply
  frame per pass and only with 4 KB free in the UART ring.

## Why

- Owner 2026-10-25: set and save params on every node without cables ("OTA params"); persistence (10d) existed.
- Nothing on the radio is time-critical: the network itself stays acoustic. So the radio carries only operator
  commands, and the rule is that it must not cost audio.
- Measured on five boards (bench/sessions/2026-10-25-step11a-ota-params): radio idle 120 s, PING, GET, SET by
  radio: 0 dropped DMA buffers on every node. A `NET * PARAM DUMP` (17 frames per node) made the gateway drop 52
  buffers while it printed ~160 lines over USB (0 with printing off; 6 with one frame per pass). Paced at 150 ms
  per frame: `NET * PARAM DUMP` (13 frames per node) and `NET * RB STATUS` (6 frames, one ~1.6 KB line) 0 drops
  on every node; `NET E5 SELFTEST` all checks PASS, report complete, 0 drops.
- PARAM SAVE drops 8 buffers per node whether it comes by serial or radio: the NVS flash write, a cost of 10d,
  not of the radio. `NET * PARAM SAVE` costs every node those 8 at once; do it outside a session.

## Rules out

A second param store or validation for the radio; SELFTEST on all nodes at once; unpaced large replies; a
router dependency.

## Revisit when

A PC gateway (bigger replies at no audio cost), security beyond the network id (anyone with ESP-NOW on channel 1
and the id can set params: fine for the installation, not for a public space), or VEKTOR (PAR-013's third
transport).

## Source

Step 11a, issue #58; roadmap-param-config.md PAR-013.
