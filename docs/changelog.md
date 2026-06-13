# 2026-06-13 - LOG-001 tuning-run logger

### Context

The scalar tuning workflow needs a reproducible, log-first batch scaffold around the `PARAM` command surface.

### Changed

- added the LOG-001 tuning helper under `tools/`
- documented the `SEQ start profile=TonalPulseScalar tries=50 mode=source when=all verbose=1` workflow
- updated the seq-tests batch layout to include `README.md`, `session.log`, `run_01.log` through `run_10.log`, and `block_01_summary.md` through `block_10_summary.md`
- recorded the `Codex-run` and `User-run` workflow split
- added resume support for interrupted batches via `-BatchRoot` and `-StartRun`
- added single-instance batch locking plus resume-state snapshots for LOG-001 tuning runs

### Verification

- the analyzer command surface already supports `PARAM` and `PARAM STATUS`
- no detector semantics change is required for this documentation/helper pass

# 2026-06-12 - Analyzer tuning surface PAR-001

### Context

Analyzer sequence runs needed a small, module-owned tuning surface for the
`TonalPulseScalar` profile, without jumping to the full ParamRegistry design.

### Changed

- added `AnalyzerTuning` as the analyzer-owned runtime tuning surface
- wired `PARAM` parsing to update scalar transient tuning values
- merged the active sequence profile from the tuning surface before runtime
- printed scalar profile parameters in the sequence header and `SEQ` report
- kept `AmpEnvelope` as the internal stream name while showing `Scalar` in analyzer-facing text

### Verification

- `platformio run -e esp32dev-analyzer` passes

---

# 2026-05-26 - 5-node slow circle stabilization

### Context

The current RB pass is now using the committed profile defaults directly:

```text
DetectionProfile.h = TonalPulse / frequency-gated detection defaults
BehaviorProfile.h = behavior timing defaults
node.cpp = no local TonalPulse overrides
```

### Decision

Lock the current 5-node slow circle baseline to the committed profile headers:

```text
TonalPulse detection stays frequency-gated
TonalPulse behavior stays self-suppressed and idle-gated by the default behavior config
runtime startup should defer to the profile headers rather than hardcoded node-side overrides
```

### Current Baseline

```text
5-node slow circle
freq/amp gated
stable as committed
```

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
