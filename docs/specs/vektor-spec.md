# VEKTOR Core Spec v1.1

Status: authoritative external spec, imported for reference.
Scope: the VEKTOR field protocol and resource model that ResonantNode should
stay compatible with. This is not a ResonantNode implementation plan; see
`docs/roadmaps/roadmap-vektor-later.md` for how/when ResonantNode adopts it.
Purpose: give the local roadmaps a concrete definition to check against
instead of "VEKTOR later" placeholders.

---

## 0. Quick control

```text
Model:    Observed State + Control Write + Action + Event
Topology: Host -> Hub -> Nodes
Runtime:  Snapshot-gated (v1)
Minimal v1 resources:   AXIS, LAMP
Extended resources:     SCALAR, SENSOR, SYSTEM
Protocol: CMD, STATE, EVENT, ACK, ERR
Principle: Semantic, deterministic, bounded
```

## 1. Mental model

VEKTOR is a semantic control system, not a signal transport.

```text
You control resources (axis, lamp), not channels.
The Hub maintains a live system model.
Nodes execute and report.
The Host interacts with the system through the Hub's semantic model and API.
State is truth, Actions change it, Events signal change.
```

## 2. Semantic model

### 2.1 Observed State

```text
Current condition of a resource.
Descriptive.
Overwriteable.
No lifecycle.
```

### 2.2 Control Write

```text
Overwriteable desired value applied to a resource.
No lifecycle.
Last write wins.
May be reflected later in observed state.

Examples: lamp.setLevel, scalar.set
```

### 2.3 Action

```text
Discrete operation with execution over time.
Requires tracking.
Must resolve: complete or fail.

Examples: axis.moveTo, axis.home
```

### 2.4 Event

```text
Discrete occurrence.
Momentary.
Best-effort delivery.
Does not replace state.

Examples: moveComplete, sensor trigger
```

### 2.5 Semantic -> protocol mapping

| Semantic       | Protocol |
|----------------|----------|
| Observed State | STATE    |
| Control Write  | CMD      |
| Action         | CMD      |
| Event          | EVENT    |

## 3. Resource schema

Normative reference: detailed resource definitions are specified in
Appendix A (below) as "Resource Schemas".

### 3.1 Core resource types

```text
AXIS
LAMP
SCALAR
SENSOR
SYSTEM
```

### 3.2 Resource rules

```text
Resource defines semantic behavior.
Implementation is hidden.
Resource indices are stable.
One controller per resource.
No implicit or "special" values.
```

### 3.3 Resource definitions (overview)

```text
AXIS   - positionable actuator   -> see Appendix A
LAMP   - light output            -> see Appendix A
SCALAR - numeric control         -> see Appendix A
SENSOR - input                   -> see Appendix A
SYSTEM - node-level               -> see Appendix A
```

## 4. Field protocol

Normative reference: logical structure and encoding are defined in
Appendix B (Packet Architecture) and Appendix C (Encoding & Framing).
Appendices B-F are not reproduced in this import; see the upstream VEKTOR
spec when those layers are implemented.

### 4.1 Message kinds

```text
CMD
STATE
EVENT
ACK
ERR
```

### 4.2 Addressing model

Each message includes:

```text
nodeId
resourceType
resourceIndex
```

### 4.3 Operation model

CMD carries downstream operations in two classes:

```text
WRITE  - overwrite-style control write (no lifecycle)
ACTION - tracked operation with lifecycle
```

Rules:

```text
WRITE:
  no cmd_id required
  last write wins
  no ACK required

ACTION:
  requires cmd_id
  must be ACKed
  must resolve
```

### 4.4 Core flows

```text
Action: CMD(ACTION) -> ACK -> EVENT(moveComplete | fail)
State:  STATE (batched observed state)
Event:  EVENT (optional upstream signal)
```

### 4.5 Delivery semantics

```text
STATE:       lossy, overwrite
CMD(WRITE):  last wins
CMD(ACTION): must resolve
EVENT:       best-effort
```

### 4.6 Protocol constraints

```text
bounded packet size
bounded queues
deterministic execution
```

## 5. System architecture

### 5.1 Roles

```text
Host - generates intent
Hub  - owns system model, schedules communication
Node - executes resources, reports state/events
```

### 5.2 Architectural rules

```text
Hub mediates all communication.
Host is transport-agnostic.
Node semantics are defined by resource composition.
Transport is per-node.
```

## 6. Runtime profile - Snapshot Hub v1

Implementation guidance: see Appendix F (Implementation Notes) upstream.

### 6.1 Runtime

```text
one round in flight
hub polls nodes
builds snapshot
returns aggregated state

Async runtime is explicitly out of scope for v1.
```

### 6.2 Configuration

```text
static node config
no dynamic discovery
```

### 6.3 Transport use

```text
RS485 / Serial
ESP-NOW / RF (later)
```

### 6.4 Platform notes

```text
Hub:  ESP
Node: ESP
```

## 7. Constraints & bounds

System must remain bounded. Implementation must define:

```text
max nodes (TBD)
max resources per node (TBD)
max actions in flight (TBD)
max event queue size (TBD)
max packet size (transport-dependent)
```

## 8. Versioning & compatibility

### 8.1 Versioned artifacts

```text
Field protocol
Resource schemas (AXIS.v1 etc.)
Firmware

Host API versioning is optional.
```

### 8.2 Not versioned

```text
node types
command lists (defined within schemas)
```

### 8.3 Compatibility rules

```text
protocol major must match
resource schema must be supported
```

## 9. Minimal v1 slice

```text
1 hub
1 node
AXIS[0], LAMP[0]
```

Operational flow:

```text
Host sends moveTo(position)
Host sends setLevel(value)
Hub forwards CMDs
Node executes
Node sends ACK for action
Node reports state via snapshot
Node signals completion via EVENT
```

## 10. Deferred features

### 10.1 v1.2 - safe extensions

```text
CMD_MULTI    - grouped commands
Query system - on-demand state
```

### 10.2 v1.3+ - capability extensions

```text
DESCRIBE           - introspection
Dynamic discovery  - auto registration
Confirmed events   - reliable delivery
```

### 10.3 v2.0 - architectural changes

```text
Async runtime  - continuous updates
Gateway nodes  - protocol bridging
```

### 10.4 Design rule

```text
New features must not break deterministic snapshot behavior.
```

## 11. Design rationale

```text
resource-based model avoids packet explosion
hub centralization simplifies distributed control
snapshot runtime ensures deterministic behavior
SCALAR enables flexible modulation
protocol kept minimal for embedded constraints
```

## Appendix A - Resource schemas (authoritative)

### AXIS.v1

Purpose: a positionable degree of freedom. Represents any actuator with a
meaningful position domain, independent of implementation (stepper, servo,
DC + encoder, etc.).

Identity:

```text
resourceType: AXIS
indexed per node (AXIS[0], AXIS[1], ...)
index must remain stable
```

Observed state (required):

```text
position - current position, unit implementation-defined but consistent per resource
busy     - boolean, true while executing an action
```

Observed state (optional):

```text
target - current target position (if applicable)
homed  - boolean, indicates valid reference position
error  - implementation-defined error flag or code
```

Control writes: none in v1. AXIS is controlled via Actions only.

Actions:

```text
moveTo(position)
  requires cmd_id, tracked, sets busy=true until resolved

stop()
  may interrupt current action
  hard stop vs controlled deceleration is implementation-defined

home()
  may take a long time
  sets homed=true on success
```

Action semantics:

```text
only one action active at a time per AXIS
default behavior: reject new action while busy
optional future: replace/queue modes
```

Events:

```text
moveComplete - emitted when moveTo finishes successfully
homed        - emitted when homing completes successfully
fail (optional) - emitted when an action fails
```

State / action relationship:

```text
position is authoritative
target is advisory (optional)
completion must be signaled via EVENT, not inferred only from state
```

Notes / constraints:

```text
position domain must be consistent per resource
resolution and scaling are implementation-defined
AXIS does not define speed/acceleration in v1
multi-axis coordination is outside resource scope
```

### LAMP.v1

Purpose: a light output resource. Represents intensity control of a
light-emitting element (LED, lamp, etc.).

Identity:

```text
resourceType: LAMP
indexed per node
```

Observed state (required):

```text
level - current output level, normalized range 0.0-1.0
```

Observed state (optional):

```text
on            - boolean (derived from level > 0, optional explicit flag)
effectRunning - boolean, indicates internal effect active
```

Control writes:

```text
setLevel(value)
  overwrite semantics, no lifecycle, last write wins
```

Control semantics:

```text
level updates may be applied immediately or with internal smoothing
node may internally clamp or scale values
repeated writes overwrite previous values without acknowledgment
```

Actions (optional, not required for v1):

```text
fadeTo(value, duration) - tracked action, emits completion event
startEffect(effectId)   - start internal light effect
stop()                  - stop active effect
```

Events (optional):

```text
fadeComplete - emitted when fadeTo completes
```

State / control relationship:

```text
level is authoritative observed output
setLevel writes desired value
observed level may lag behind control input
```

Notes / constraints:

```text
level must be normalized (0-1)
hardware-specific scaling is internal
LAMP is not a generic numeric channel (use SCALAR for that)
color control (RGB etc.) is out of scope for v1
```

### General resource rules (all resources)

```text
Identity:
  resourceType + index uniquely identifies a resource within a node

State:
  STATE reflects observed truth, not intent
  may be delayed or quantized

Control writes:
  overwrite semantics
  no lifecycle
  not guaranteed to be acknowledged

Actions:
  require cmd_id
  must resolve (complete or fail)
  must not execute twice (deduplication required)

Events:
  best-effort delivery
  must not be relied on as sole source of truth
  must correspond to real state changes

Ownership:
  one active controller per resource
  no concurrent conflicting control

Extensibility:
  optional fields must not break required fields
  new actions/events must be additive
  versioning handled via resource version (e.g. AXIS.v2)
```

Summary: AXIS and LAMP define the minimal actuator model.

```text
AXIS -> position with lifecycle (ACTION-driven)
LAMP -> level with overwrite control (WRITE-driven)
```

Together they establish the two core control patterns in VEKTOR: tracked
actions, and continuous control writes.

## Appendices B-F (not imported)

The upstream spec defines these; ResonantNode does not need their detail
until a real transport lands (see `docs/roadmaps/roadmap-vektor-later.md`):

```text
Appendix B - Packet Architecture     (logical message structure, payload composition)
Appendix C - Encoding & Framing      (byte layout, framing e.g. COBS, limits)
Appendix D - Transport Bindings      (transport-specific behavior)
Appendix E - Application API         (host-facing API, OSC paths, arguments, patterns)
Appendix F - Implementation Notes    (non-normative: scheduler, buffering, debugging, staging)
```

## Summary

VEKTOR is a hub-centric semantic control architecture with a minimal
protocol, deterministic snapshot runtime, and a clear separation between
semantics (main spec) and mechanics (appendices), enabling robust and
scalable embedded systems.
