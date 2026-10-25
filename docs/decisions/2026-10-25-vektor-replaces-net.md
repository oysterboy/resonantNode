# VEKTOR will replace NET; NET is the interim remote console

Status: decided (owner), not yet implemented (VEKTOR has no hub decision and no appendices B-F yet).
Date: 2026-10-25.

## Decision

- NET (step 11a, ESP-NOW remote console) is interim. When VEKTOR's node binding lands, it replaces NET.
- What carries over: NodeNet's transport (ESP-NOW setup, receive queue, non-blocking outbox, repeat filter), the
  single command path into the registry (PAR-013), the bounds. What does not: NET's text frames and replies.
- VEKTOR is not fixed: this implementation feeds it. Lessons from 11a are proposals for the spec
  (docs/research/vektor-lessons-from-11a.md).

## Why

Owner 2026-10-25, after the comparison of 11a with docs/specs/vektor-spec.md: one radio protocol, not two.

## Rules out

Growing NET into a second semantic protocol (STATE / EVENT / ACTION in text); building the VEKTOR binding before the
hub is decided and appendices B-F exist.

## Revisit when

The hub is decided (PC with the OSC host API, or an ESP hub), or the appendices exist.

## Source

Owner answers 2026-10-25 (1 replace, 2 not decided, 3 not yet); research note above; roadmap-vektor-later.md.
