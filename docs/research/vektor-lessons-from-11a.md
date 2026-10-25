# What the first radio implementation (step 11a) teaches VEKTOR

Status:  exploring (2026-10-25)
Lab:     bench/sessions/2026-10-25-step11a-ota-params; decisions/2026-10-25-remote-params-espnow.md

Owner 2026-10-25: VEKTOR will replace NET (the 11a remote console), the hub is not decided, VEKTOR's appendices
B-F do not exist yet, and VEKTOR "is not set in stone, but could also learn from us: the first implementation".
This note collects what 11a measured or ran into that the VEKTOR spec (docs/specs/vektor-spec.md, v1.1) does not
yet say, as proposals for the spec, each with the evidence behind it.

## 1. The node's real-time job bounds the protocol, not only the packet size

VEKTOR 4.6 / 7 ask for bounded packets and queues. 11a showed the binding limit is the node's loop time: the
Node's audio has ~24 ms of DMA queue, and anything that holds the loop longer drops audio.
- 38 frames sent back to back: 9-20 dropped buffers per node; an outbox sending one frame per loop pass: 0.
- A gateway printing ~160 reply lines over USB serial: 52 dropped buffers; 0 with printing off.
- A flash write (PARAM SAVE, NVS): 8 dropped buffers per node, transport-independent.

Proposal: VEKTOR states that a node handles at most one message per loop pass, never blocks on send, and that a
node profile declares its real-time budget (here: no single step over ~10 ms). Persisting is an ACTION whose cost
the node may declare ("this action interrupts the resource's real-time work for N ms").

## 2. Many nodes answering at once is the normal case, and it needs flow control

11a's `*` broadcast made four nodes answer at the same moment; the gateway's 8-deep receive queue overflowed and
replies were lost until each node paced its replies (one frame per 150 ms). VEKTOR's snapshot runtime ("one round
in flight, hub polls nodes") avoids this by design.

Proposal: keep the one-round rule, and say what a hub does for "set this on every node": N polled WRITEs, or
CMD_MULTI (v1.2) with a defined reply schedule. A broadcast WRITE without replies (VEKTOR WRITE needs no ACK) is
cheap; a broadcast that every node answers is not.

## 3. Some state is larger than one frame

RB STATUS is one ~1.6 KB line, PARAM DUMP 38 lines, a self-test report ~11 lines over seconds. ESP-NOW carries 250
bytes. 11a fragments a reply into a paced stream and joins it at the gateway.

Proposal: Appendix B/C define fragmentation for STATE (or require STATE to be split into resource-sized pieces
that each fit a frame, which a resource model does naturally: SCALAR[i] per frame, not one status blob).

## 4. Actions that block or make sound need rules the protocol can enforce

SELFTEST blocks its node for seconds and plays tones; RB CALL makes sound; RB PROFILE resets live values; RB
rebase blocks ~1 s. 11a allows them by radio with operator rules, and enforces one: SELFTEST to one named node
only (`*` refused at the gateway and at the node). "The others silent" stays an operator rule.

Proposal: an ACTION can declare `exclusive` (one node at a time in the system, the hub enforces it) and
`audible` / `blocking` flags, so a hub can refuse or sequence them instead of a manual telling the operator.

## 5. Persistence is a separate step from writing

PARAM SET is live and lost on reboot; PARAM SAVE writes flash (owner's 10d choice: save only on request). VEKTOR's
Control Write is "overwrite, last write wins" with no notion of durable vs live.

Proposal: SYSTEM gets a `persist` ACTION (and `restore defaults`), and STATE reports per SCALAR whether its value
is live-changed, stored, or the firmware default (the PAR-012 visibility item; boot already prints `PARAM stale`
when a stored value was saved against another default).

## 6. Stable identity already exists: use it as the resource index

ParamId (app/ParamIds.h) is stable and never reused; the NVS keys are `p<id>`. That is exactly VEKTOR's "resource
indices are stable" (3.2). Node names come from a static roster (VEKTOR 6.2: static config, no discovery).

Proposal: SCALAR index = ParamId; the dotted path stays the human name (a DESCRIBE answer, v1.3).

## 7. A text console is worth keeping for setup, even under VEKTOR

During installation setup the operator wants SELFTEST and a readable status by radio (owner 2026-10-25). VEKTOR's
host API (Appendix E, OSC) is for programs. If VEKTOR replaces NET, a maintenance path still has to exist: either
VEKTOR carries a "console" action (text in, text out, bounded, paced) or the hub offers the same readouts.

Open: which of the two; it decides whether NET's console code survives the replacement.

## 8. Things VEKTOR does not mention that a radio deployment needs

- Coexistence: the node's own work shares the CPU with the radio stack (WiFi on core 0; 94 KB heap left, flash
  65 % after ESP-NOW, ~465 KB of it the WiFi stack).
- Who may command: 11a has only a network id; anyone on the channel with the id can set params.
- Several hubs: 11a has no ownership; two USB gateways could contradict each other (VEKTOR 3.2 "one controller per
  resource" needs a mechanism, not only a rule).
- Channel: a fixed channel without a router; a site with WiFi may need the channel chosen per site.

## Next

Nothing to implement now (VEK-002 waits for the hub decision and the appendices). When the spec is next edited,
take the proposals above to it; mark each here with `-> promoted to ...`.
