# A board's console lines by radio (node.radio_log): a mirror, not a state protocol

Status: decided (owner 2026-11-05: "Ja. Bau schonmal", after boards on a power-only charger could be set by radio
but not heard)
Date: 2026-11-05

## Decision

A board can send every console line it prints anyway as one ESP-NOW frame (FrameType Log), broadcast once, at most
`node.radio_log` lines per second (0 off, the default); every board on USB prints them as `NET log from=<name>
<line>`. The text is the console's, unchanged; nothing on the board parses it. Lines starting `NET ` stay home (no
loop between gateways), a line inside a remote command's reply goes back as the reply instead. All three modes
register the param under one id and path, so a board keeps it across the switch.

## Why

- On 2026-11-05 three of five boards ran on a charger without data: commands, params and OTA reached them by
  radio, but none of their verdicts, emits or claps; every run was read from two boards.
- decisions/2026-10-25-vektor-replaces-net.md keeps NET a remote console and rules out growing it into STATE /
  EVENT / ACTION text. A console mirror stays inside that: it is the serial line moved, not a message set; VEKTOR,
  when it lands, replaces it with the rest of NET.
- Rate-limited and lossy: ~40 frames back to back dropped audio buffers (NodeNet); a full log runs far above what
  the air and the gateway's UART carry.

## Rules out, costs

- Typed status or event frames on NET (left to VEKTOR).
- Lost lines are not detected (no per-log sequence); a count of what was sent and dropped is in `NET STATUS`.
- A board of an older build counts Log frames as rejected.

## Revisit when

VEKTOR's node binding lands, or a run needs every line (then raise the rate, or USB).
