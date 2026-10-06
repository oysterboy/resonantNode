# Keep ParamRegistry in this repo as a PlatformIO library

Status: decided; the library move (PAR-016) is step 6 of
`roadmap-0-steps.md`, issue #14; the separate-repo split (PAR-017) is
deferred to step 8.
Date: 2026-09-23.

## Decision

`ParamRegistry` stays in this repository. Move it from `src/param/` to
`lib/ParamRegistry/` with a `library.json` and decouple it from
ResonantNode's enums first, so a later split into its own repo is a
mechanical move. Do not split until a second consumer exists and PAR-010
has settled `ParamBinding`.

## Why

- One consumer (the Node build), four registered params, one commit
  (`052b03e`). Nothing to share yet.
- `ParamBinding` is not stable: PAR-010 adds `storageKey`, `defaultValue`,
  a persistent flag and an apply policy. In a separate repo each of those
  is a cross-repo release and version bump.
- Not generic yet: `ParamTypes.h` hardcodes ResonantNode's `ModuleId` and
  `ParamId` enums.
- `roadmap-vektor-later.md`: VEKTOR must not drive premature local
  architecture; VEK-002 is still deferred.
- Two repos cost two PRs per change and both repos attached per agent
  session, for no benefit until there is a second consumer.

## Rules out

- Starting a `ParamRegistry` repository now.
- Letting `lib/ParamRegistry/` include anything from `src/` (the gate for
  step 6).

## Revisit when

A second consumer exists (another firmware, a host tool) and PAR-010 has
landed.

## Source

- `docs/roadmaps/roadmap-param-config.md`, PAR-016 / PAR-017, "Decision
  (2026-09-23)".
- Commit `7a398b9` ("record decision to keep ParamRegistry in-repo").
