# Changelog

This changelog records documentation and architecture decisions.

It is historical. It should not be used as the current implementation contract.

The active architecture contract lives in:

```text
myspec.md
```

The immediate task lives in:

```text
current-pass.md
```

---

## 2026-05-25 — Docs Structure / Roadmap Consolidation v5

### Context

The project now prioritizes a concrete physical test use case:

```text
testing current TonalPulse detection and behavior variations on 5 nodes
```

This changes implementation order.

Near-term work should support repeatable testing before building future detection families, full behavior runtime, or broad param infrastructure.

### Decision

Use one active spec plus specialized implementation-order roadmaps:

```text
myspec.md = current architecture contract

roadmap-general-node.md = cross-roadmap sequencing
roadmap-param-config.md = param/config workflow, hardcoded first, runtime later
roadmap-detection.md = future detection work only
roadmap-behavior.md = active future behavior work
roadmap-output.md = future output/profile/action boundary
roadmap-vektor-later.md = thin future VEKTOR exposure roadmap

current-pass.md = immediate task only
changelog.md = history
archive/ = historical docs
```

### Spec / Roadmap Policy

This docs set does not update `myspec.md`.

Later, update `myspec.md` from:

```text
current source code
this docs package
current test findings
```

Rules:

```text
stable ownership/boundary rule → myspec candidate
planned mechanism / implementation order → roadmap
immediate task → current-pass
history → changelog/archive
```

### Added Guardrail

Minimum viable does not mean throwaway.

MVP means:

```text
smallest useful slice
within the intended architecture direction
```

Avoid both:

```text
empty frameworks
wrong-owner hacks
```

### Current Practical Workflow

For first 5-node tests:

```text
hardcoded params + firmware upload are acceptable
```

Runtime params become worthwhile only with:

```text
PARAM SET + SAVE/LOAD + VERIFY
```

or:

```text
fleet/OTA param apply + VERIFY
```

Unsaved runtime `PARAM SET` is not enough as the main 5-node workflow.

### Naming Direction

Preferred detection terminology:

```text
SignalCandidate        → Occurrence
SignalEmitter          → OccurrenceSource
SignalInspector        → OccurrenceInspector
InspectedSignal        → InspectedOccurrence
```

`PatternCandidate`, `PatternRules`, and `PatternResult` stay.

Profile rename:

```text
FreqAmp → TonalPulse
```

### Next Practical Direction

```text
1. Run rename pass: Occurrence + TonalPulse.
2. Add/clean 5-node status baseline.
3. Compile.
4. Test hardcoded TonalPulse params on 5 nodes.
5. Only then decide whether runtime params, behavior modes, or pulsed chirp work should advance.
```
