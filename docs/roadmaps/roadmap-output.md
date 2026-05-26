# Roadmap — Output

Status: future-supporting roadmap. Scope: future SoundOutput / OutputProfile boundary.

This roadmap keeps emitted sound shape separate from Detection and Behavior.

Output work is probably after the immediate Detection naming pass and early 5-node TonalPulse tests.

---

## Architecture Goal

Eventually:

```text
Behavior requests output.
SoundOutput executes output.
OutputStatus reports output availability.
OutputProfile / ChirpProfile owns available sound shape.
```

Behavior decides modulation / intended drift.

SoundOutput executes requested output values and may validate physical limits, but it does not decide artistic drift.

Detection should not trigger output directly.

---

## Spec Candidates

These are stable rules that should later be considered for `myspec.md`:

```text
OutputProfile defines what can be emitted.
Behavior may request concrete values or profile variants.
SoundOutput executes requested values; it does not decide behavior modulation.
Behavior should not own hardware details.
```

---

## MVP Guardrail

MVP does not mean throwaway.

Each minimum viable pass should be the smallest useful slice that still follows the intended architecture direction.

Avoid two extremes:

```text
too large:
    empty frameworks, generic registries, factories, unused abstractions

too small / wrong:
    hacks in Node, duplicated logic, temporary APIs, shortcuts that must be removed immediately
```

A good MVP:

```text
uses real current modules
solves a real near-term problem
keeps ownership in the right subsystem
can be extended without rewriting the same boundary
does not create compatibility sediment
```

If the quickest implementation would put logic in the wrong owner, prefer a slightly larger but correctly placed slice.

Rule:

```text
Build the smallest slice you can keep.
```


---

## Current Status

Current SoundOutput path exists.

Not landed:

```text
OutputStatus as stable behavior input
OutputRequest
OutputProfile / ChirpProfile
action lifecycle
OutputDispatcher
```

---

## Implementation Order

```text
1. For 5-node tests, keep output behavior hardcoded/current.
2. Expose output status only if needed for behavior/status logs.
3. Later add OutputStatus.
4. Later add OutputRequest.
5. Later add OutputProfile / ChirpProfile.
6. Later connect to ResonantProgram.
```

---

## Minimum Viable First Pass

Goal:

```text
Make output state visible only if needed for the 5-node tests.
```

Do:

```text
- expose busy / lastStartMs / lastDoneMs if already cheap
- keep output generation unchanged
```

Do not:

```text
- add OutputDispatcher
- add profile system
- change output timing
- change behavior decisions
- implement action lifecycle yet
```

Ownership guardrail:

```text
output status should come from SoundOutput
Behavior may consume status later, but should not read output internals
```

Success:

```text
Status/logs can show whether output is busy or recently emitted, if needed for tests.
```

---

## Later Phases

```text
OutputStatus
OutputRequest / action object
OutputProfile / ChirpProfile
action lifecycle
hardware path separation
ResonantProgram compatibility
```

---

## Non-Goals for First Implementation

```text
advanced synthesizer architecture
multi-channel audio engine
generic scheduler
behavior-owned waveform generation
VEKTOR Art-Net / bulk output integration
```

---

## One-Line Strategy

```text
Keep output simple for TonalPulse tests; add OutputStatus/Profile only when behavior variations need a cleaner output boundary.
```
