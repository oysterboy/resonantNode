# Boards

Status: live table, edited in place. Rules: `docs/decisions/2026-11-12-board-revisions.md`.
The drawings live in the artifacts; the pins live in `src/app/BoardConfig.h`.

| Board | Gen | RC | Status | Date | Layout artifact | Firmware board (envs) | Fix list |
|---|---|---|---|---|---|---|---|
| E1-E5 (breadboard, D-AMP) | - | - | in use, references | 2026-10-09 | - | D-AMP default (`esp32dev*`) | - |
| HW-001 stripboard | V1 | RC1 | superseded, never built | 2026-10-29 | https://claude.ai/artifact/K28gRvr5mVLPBH6bjfE1x7 | - | - |
| HW-001 stripboard (R-node) | V1 | RC2 | in work; board 1 built 2026-11-12; freezes when batch 1 is cut | 2026-11-12 | https://claude.ai/artifact/AFSXXhcWmjkM6ZtH4Dfeqo | `lochraster_v2` (`esp32dev-lr2*`, issue #98); board 1 reworked to v2 on 2026-11-13, `lochraster_v1` before | Trello checklist "RC2 Fixes", https://trello.com/c/1MuBUqNm |

Parts and stock for the boards: the parts list artifact ("Teileliste", https://claude.ai/artifact/DYDr3MDN9WXruq843a4WHq).

Never flash a D-AMP image onto a stripboard node (`CLAUDE.md`, "Boards").

## Fix codes

The board label (R1, R2, ...) has six boxes for fix codes written by hand. A code marks a deviation of that board
from its RC's layout; the batch boards should need none.

| Code | Board | Fix |
|---|---|---|
| A | R1 | Antenna copper removed through column 25: the right-row GND pin (strip 2) patched by a wire to column 26 |
| B | R1 | Mic SD moved from GPIO 35 to 32 (now `lochraster_v2`); header and cable keep the old order L/R, SD, GND, VDD, WS, SCK |
