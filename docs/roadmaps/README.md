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
How does it work / how do I run
or change it?                   docs/notes_manual.md (the operator guide)
What next, in what order?       roadmap-0-steps.md
What exactly is item X?         the domain roadmap that owns its ID
How is a big item executed?     its pass / plan doc in docs/refactors/
Why was it built this way?      docs/decisions/ (forks taken, and open ones)
How could X be done?            docs/research/ (options, designs; marks what got promoted)
What did we observe?            docs/lab/, bench/ sessions
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

## Spec, guide, roadmap

Three documents, three tenses:

```text
docs/specs/myspec.md      what must be true for the code to be correct: the chain,
                          owns / must not own per module, numeric-domain contract,
                          interface shapes other code or tools depend on.
docs/notes_manual.md      how to run, read and change what is built: commands,
                          param paths, config sites, apply points, gate chain in
                          operating terms. Points at code and PARAM DUMP for values.
docs/roadmaps/            what is not true yet, and in what order.
the code                  what the value is today.
```

The test for a sentence: if it were wrong, would it force a refactor doc? Then
it is spec. Would it only make an operator type the wrong command or read the
wrong header? Then it is guide. A default, a knob list or a command syntax never
goes into the spec; the guide quotes a value only with its date and the code
site it came from, and prefers a pointer (`PARAM DUMP`, the header) to a copy.
The guide drifts fastest because it names things; keep it cheap and
pointer-shaped rather than complete, and fix a stale pointer when you find one
instead of adding a drift banner.

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
its roadmap entry (e.g. DET-008 -> `docs/refactors/archive/cleanup-0-plan.md`), so
the chain never skips the domain roadmap.

## Roadmap files

```text
roadmap-0-steps.md
    Ordered next steps across all roadmaps, with a gate per step. Named to
    sort first, same convention as docs/refactors/archive/cleanup-0-plan.md.

roadmap-detection.md
    Detection and analyzer items (DET, ANA). Renamed from roadmap_detection.md
    on 2026-09-23; archived docs still use the old name.

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

roadmap-echo.md
    The echo room and Whispernet modes, and the OTA switch between them and
    the Node on location (ECHO).

roadmap-hardware.md
    The installation's nodes as built objects: a stripboard prototype, ten
    boards, ten cases, and ten nodes in the firmware (HW).

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
