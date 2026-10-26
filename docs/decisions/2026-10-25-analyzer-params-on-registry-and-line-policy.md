# The Analyzer's tuning knobs go on ParamRegistry with the Node's grammar; long serial lines are refused on every port

Status: decided 2026-10-25 (owner, asked at the start of step 11e); implemented in step 11e items 8 and 9.
Date: 2026-10-25.

## Decision

- The Analyzer registers its 14 `detection.scalar_*` knobs (the `AnalyzerTuning.scalarTransient` fields) on a
  `param::ParamRegistry` through the same binding function the Node uses, and answers `PARAM LIST | GET | SET |
  DUMP` through the same handler. The paths are the Node's (`detection.scalar_onset_threshold`, ...).
- The Analyzer's `PARAM key=value` grammar (`PARAM scalar_onset_threshold=2000`, with camelCase aliases) is
  removed, not aliased. An old-form token answers `ERR PARAM unknown; use PARAM SET detection.scalar_* <value>`.
- One `LineReader` serves every serial port in every mode. A line longer than its buffer is dropped whole and
  answered with an ERR line (the Node's rule since issue #31); the Analyzer's USB console and the Emitter's
  two ports no longer run the truncated line.
- A numeric command value that does not parse is refused with an ERR reply. Bare `strtoul` (which read
  `f=abc` as 0) is gone from the command paths.

## Why

- PAR-003 has said since step 10 that the Analyzer should register on the registry; the hand-written parser
  spelled the 14 keys in six places across four files and two modes, and was marked TEMPORARY in five.
- One grammar for the same knob on both modes: a bench operator types `PARAM SET detection.scalar_*` on a
  Node already; the Analyzer should take the same line. No bench script uses the old form (grep of
  tools/bench on 2026-10-25); the owner chose to drop it rather than carry a key map.
- The truncation bug was fixed on the Node in step 3d (A3) and left on the other two readers by accident of
  there being seven readers. One reader, one policy.

## Rules out

A second PARAM grammar on any mode; a per-port overflow policy; command parsers that read a bad number as 0.

## Revisit when

VEKTOR's command surface replaces the serial grammar (decisions/2026-10-25-vektor-replaces-net.md), or the
Analyzer grows knobs that are not `ScalarTransientConfig` fields and would need an enum param type.

## Source

Step 11e (docs/refactors/archive/review-fixes-2026-10-25.md), issue #63; roadmap-param-config.md PAR-003.
