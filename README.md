# ResonantNode

ESP32 firmware (PlatformIO, Arduino) for acoustic resonant nodes: each node hears a tonal pattern in the room and
answers with its own. One codebase, several builds: the Node, the Analyzer, the Emitter, the echo modes and the
radio gateway.

- `CLAUDE.md`: start here before changing anything. The working rules (boards, secrets, what can be verified
  where, the checks before a commit, the architecture's core, comments). Written for Claude; they hold for people.
- `docs/README.md`: the docs system. Which file answers which question, and how work goes from a roadmap item to a
  closed step.
- `docs/notes_manual.md`: the operator guide. Builds and boards, commands per mode, params, OTA, bench workflow.
- `docs/specs/myspec.md`: the architecture spec. Which module owns what, and what must stay true.
- `docs/roadmaps/roadmap-0-steps.md`: the ordered work, one gate per step, and what is running now.
- `test/README`: the tests, on the host and on the board.
