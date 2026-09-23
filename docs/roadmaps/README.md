# Roadmaps

Status: roadmap guide and index.
Scope: active roadmap files.
Purpose: say which file answers which question, keep the active roadmaps in
one style, and explain how landed items move into the archive changelog.
(This file absorbed the former `roadmap-master.md` index, 2026-09-23.)

---

## Who answers what

```text
What exists right now?          the code, then implementation-status.md
Where does a concept belong?    docs/specs/myspec.md
What next, in what order?       roadmap-0-steps.md
What exactly is item X?         the domain roadmap that owns its ID
How is a big item executed?     its pass / plan doc in docs/refactors/
What happened?                  docs/changelog.md, git log
```

The chain, top to bottom:

```text
roadmap-0-steps.md    orders items across areas. Points at IDs, holds no detail.
domain roadmaps       own the items: ID, detail, status.
docs/refactors/       executes an item too big for one commit: phases, test
                      battery, gates. Temporary; archived with an
                      "Archived: ..." header when the item closes.
```

## Ordering rule

Two kinds of document order work, at different levels:

```text
roadmap-0-steps.md          orders BETWEEN items.
the active pass / plan doc  orders WITHIN its item (e.g. cleanup-0-plan.md's
                            phase order, test battery, and gates).
```

If they conflict, `roadmap-0-steps.md` wins on whether and when an item runs;
the pass doc wins on how it runs. Every step in `roadmap-0-steps.md`
references a roadmap ID, and an item executed through a pass doc says so in
its roadmap entry (e.g. DET-008 -> `docs/refactors/cleanup-0-plan.md`), so
the chain never skips the domain roadmap.

## Roadmap files

```text
roadmap-0-steps.md
    Ordered next steps across all roadmaps, with a gate per step. Named to
    sort first, same convention as docs/refactors/cleanup-0-plan.md.

roadmap_detection.md
    Detection and analyzer items (DET, ANA).

roadmap-node.md
    Node infrastructure, testing / CI, multi-node trials (NODE).

roadmap-behavior.md
    Behavior boundary and future behavior architecture (BEH).

roadmap-output.md
    SoundOutput / OutputStatus / OutputProfile boundary (OUT).

roadmap-param-config.md
    Param/config workflow and future persistence / fleet config (PAR).

roadmap-vektor-later.md
    Later VEKTOR exposure after local boundaries stabilize (VEK).

implementation-status.md
    Live status table. Current state only, never dated history.
```

Archive:

- [roadmap archive/roadmap-changelog.md](<roadmap archive/roadmap-changelog.md>):
  landed roadmap items.
- [../refactors/archive/260512_detection-Refactor/detection_roadmap.md](../refactors/archive/260512_detection-Refactor/detection_roadmap.md):
  the 2026-05 detection refactor roadmap.
- The old `current-pass.md` is archived at
  `docs/refactors/archive/current-pass.md`.

## Shared shape

Keep the domain roadmaps in this shape:

```text
Status
Scope
Purpose
Status legend
Architecture goal
Current code state
Implementation order
Current focus      one or two lines: which roadmap-0-steps.md step touches
                   this area. No ordering or constraints of its own;
                   constraints go in Non-goals.
Spec candidates
Non-goals
```

## Procedure

```text
Use short project-wide IDs in the active roadmaps.
Keep roadmap-0-steps.md lean: order and gates only, detail stays in the
domain roadmap for that area.
Give an item executed through docs/refactors/ a roadmap ID that points at its
pass / plan doc.
Move fully landed items to docs/roadmaps/roadmap archive/roadmap-changelog.md.
Keep myspec.md and implementation-status.md aligned with the current code.
```

## ID prefixes

```text
NODE  node infrastructure, testing / CI, multi-node trials
DET   detection
ANA   analyzer
BEH   behavior
OUT   output
PAR   params / config
VEK   VEKTOR later
```
