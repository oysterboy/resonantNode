# ESP-NOW is a thin param/state transport using a VEKTOR v1 subset

Status: decided, not yet implemented (transport work is deferred to step 8
of `roadmap-0-steps.md`; this fixes the shape so it isn't re-designed).
Date: 2026-10-08.

## Decision

When nodes get a radio link, ESP-NOW carries the existing command path,
not a protocol of its own: `PARAM SET <path> <value>` into ParamRegistry,
plus status back. On the wire it uses a subset of VEKTOR v1
(`docs/specs/vektor-spec.md`):

- Param set → `CMD(WRITE)` on a `SCALAR` (VEK-002 mapping). No ACK.
- Status → `STATE` snapshot, hub polls nodes, one round in flight.
- Apply (switch a staged param set live, all nodes together) → the one
  `CMD(ACTION)`: `cmd_id`, ACK, must resolve.
- Confirmation of writes comes from `STATE` (node reports a version /
  checksum of its params), not from per-write ACKs.
- Extension, documented as part of the ESP-NOW transport binding:
  a broadcast / group `nodeId`, sent as one ESP-NOW broadcast, repeated
  2–3× with a sequence number for de-duplication.

Continuous multi-channel streaming (e.g. 5 channels × 100 nodes at
50 Hz, 3 channels back) is outside this decision and outside VEKTOR v1.

## Why

- `roadmap-param-config.md` (PAR-013): ESP-NOW must use the same
  command/control path as Serial, with no transport-owned param store or
  validation. A separate packet format with its own param IDs and pending
  buffer would break that.
- VEKTOR's write/action/state split matches the need: param sets are
  last-write-wins, only "apply" needs tracked completion.
- 1 Hz status from 100 nodes fits a poll round (roughly 2–5 ms per node,
  about 0.5 s per round). Poll keeps VEKTOR's snapshot runtime intact.
- VEKTOR v1 has no broadcast or group addressing (`CMD_MULTI` is v1.2).
  Without it, "set param5 on all nodes" is 100 unicast CMDs, and the
  ESP-NOW hub is limited to about 20 registered unicast peers.
- Continuous streaming needs async runtime (VEKTOR v2.0). At 100 nodes
  the upstream alone is ~5000 packets/s, more than one ESP-NOW channel
  carries.

## Rules out

- An ESP-NOW-specific param store, param ID table or validation.
- Full VEKTOR (AXIS/LAMP, EVENT, DESCRIBE, Host API) as a prerequisite for
  the radio link.
- Node-initiated push status as the default runtime.

## Revisit when

Continuous control is actually needed, upstream VEKTOR adds `CMD_MULTI`
or group addressing, or the hub needs to supervise more than ~100 nodes
at faster than 1 Hz.

## Source

Chat discussion 2026-10-08 (ESP-NOW for 10/20/100 nodes at 1 m spacing),
checked against `docs/specs/vektor-spec.md` §4, §6, §10 and
`docs/roadmaps/roadmap-param-config.md`.
