# ResonantNode Docs Structure

This package defines the active documentation set around `myspec.md`.

`myspec.md` is intentionally not included here. It remains the architecture contract and should be updated later from this docs package plus the current source code.

---

## Active Docs

```text
docs/
├─ myspec.md
├─ README-docs-structure.md
├─ current-pass.md
├─ changelog.md
└─ roadmaps/
   ├─ README.md
   ├─ roadmap-general-node.md
   ├─ roadmap-param-config.md
   ├─ roadmap-detection.md
   ├─ roadmap-behavior.md
   ├─ roadmap-output.md
   └─ roadmap-vektor-later.md
```

---

## Spec / Roadmap Policy

This package does not update `myspec.md`.

Later, update `myspec.md` using this docs package plus the current source code to keep the spec consistent with landed reality.

Use this rule:

```text
myspec.md:
    stable ownership / boundary rules
    landed architecture contracts
    current implementation state

roadmaps:
    planned mechanisms
    implementation order
    MVP slices
    later work

current-pass.md:
    exactly the next change

changelog.md:
    history and superseded context
```

Future-facing architecture rules may be mentioned in roadmaps as `Spec candidate` notes when they are stable enough to later constrain code.


---

## Document Roles

### `myspec.md`

Architecture contract.

Contains:

```text
landed architecture
stable module boundaries
data contracts
ownership rules
current implementation boundary
stable extension points
```

Does not contain:

```text
old refactor passes
temporary debugging plans
long future feature lists
historical debates
obsolete names
implementation task lists
```

### Roadmaps

Roadmaps are implementation-order documents.

They contain:

```text
short architecture goal
current status
ordered implementation passes
minimum viable first pass
later work
non-goals
spec-candidate notes where relevant
```

They should not become mini-specs.

### `current-pass.md`

Immediate Codex task only.

Contains:

```text
goal
scope
allowed files
forbidden files
exact changes
success criteria
verification
```

Does not contain architecture debate.

### `changelog.md`

History.

Contains:

```text
what changed
why it changed
what was superseded
where old docs moved
```

Does not give current implementation guidance.

---

## Operating Rule

```text
Spec = what is true now.
Roadmaps = ordered future work.
Current-pass = what to do now.
Changelog / archive = how we got here.
```

Whenever code lands:

```text
1. Update myspec if it became architecture.
2. Remove landed item from roadmap.
3. Keep unfinished work in roadmap.
4. Write a narrow current-pass for the next step.
5. Archive obsolete docs.
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

## Current Practical Use Case

Near-term implementation is driven by:

```text
testing TonalPulse detection and behavior variations on 5 nodes
```

This means:

```text
hardcoded params + firmware upload are acceptable first
runtime PARAM SET is not enough unless paired with SAVE/LOAD or fleet apply/verify
do not build a large ParamRegistry before it is useful
do not build future chirp/detection families before current 5-node testing
```

Hardcoded params are acceptable only if they live with the right owner:

```text
good:
    TonalPulse defaults live with DetectionProfile / profile config
    Behavior defaults live with behavior config
    Output defaults live with output config

bad:
    all test params live as random globals in node.cpp
```

---

## Review Notes

This package was revised for:

```text
1. scope separation between spec, roadmaps, current pass, and changelog
2. consistent use of Occurrence / OccurrenceSource terminology
3. consistent TonalPulse profile naming
4. roadmaps structured by implementation order
5. MVP sections that prevent huge empty scaffolds
6. MVP guardrail: minimum viable, not disposable
7. next use case: 5-node TonalPulse testing before future pulse/chirp/different behavior architecture
8. explicit policy: planned mechanisms stay in roadmaps; stable rules can become myspec candidates later
```
