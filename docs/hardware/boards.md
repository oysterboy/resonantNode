# Boards

Status: live table, edited in place. Rules: `docs/decisions/2026-11-12-board-revisions.md`.
The drawings live in the artifacts; the pins live in `src/app/BoardConfig.h`.

| Board | Gen | RC | Status | Date | Layout artifact | Firmware board (envs) | Issue |
|---|---|---|---|---|---|---|---|
| E1-E5 (breadboard, D-AMP) | - | - | in use, references | 2026-10-09 | - | D-AMP default (`esp32dev*`) | - |
| HW-001 stripboard | V1 | RC1 | superseded, never built | 2026-10-29 | https://claude.ai/artifact/K28gRvr5mVLPBH6bjfE1x7 | - | - |
| HW-001 stripboard (R-node) | V1 | RC2 | in work; board 1 built 2026-11-12; freezes when batch 1 is cut | 2026-11-12 | https://claude.ai/artifact/AFSXXhcWmjkM6ZtH4Dfeqo | `lochraster_v1` (`esp32dev-lr1*`) | #96 |

Parts and stock for the boards: the parts list artifact ("Teileliste", https://claude.ai/artifact/DYDr3MDN9WXruq843a4WHq).

Never flash a D-AMP image onto a stripboard node (`CLAUDE.md`, "Boards").
