# Roadmaps

Status: roadmap guide.
Scope: active roadmap files.
Purpose: keep the active roadmaps in one style and explain how to move landed
items into the archive changelog.

---

## Roadmap files

```text
roadmap-general.md
    Short next-step index only.

roadmap_detection.md
    Detection and analyzer follow-up work.

roadmap-node.md
    Cross-roadmap node infrastructure details.

roadmap-behavior.md
    Behavior boundary and future behavior architecture.

roadmap-output.md
    SoundOutput / OutputStatus / OutputProfile boundary.

roadmap-param-config.md
    Param/config workflow and future persistence / fleet config.

roadmap-vektor-later.md
    Later VEKTOR exposure after local boundaries stabilize.

roadmap-master.md
    Pointer to the active roadmap set.

current-pass.md
    The next implementation pass only.
```

## Shared shape

Keep the active roadmaps in this shape:

```text
Status
Scope
Purpose
Status legend
Architecture goal
Current code state
Implementation order
Current / first cleanup pass
Spec candidates
Non-goals
```

## Procedure

```text
Use short project-wide IDs in the active roadmaps.
Keep roadmap-general.md lean and future-focused.
Put the detailed steps in the domain roadmap for that area.
Move fully landed items to docs/archive/roadmaps/roadmap-changelog.md.
Keep myspec.md and implementation status aligned with the current code.
```

## ID prefixes

```text
NODE  cross-roadmap node infrastructure
DET   detection and analyzer
BEH   behavior
OUT   output
PAR   params / config
VEK   VEKTOR later
```
