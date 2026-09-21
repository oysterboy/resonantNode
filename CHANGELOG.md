# Changelog

Version-level summary of firmware behavior, keyed to `BUILD_VERSION` in
`platformio.ini`. Loosely follows [Keep a Changelog](https://keepachangelog.com/).

This is **not** the engineering record. Architecture reasoning, refactor
detail, and evidence live in `docs/refactors/` (active/archived pass and
plan docs) and `docs/specs/myspec.md` — see `CLAUDE.md` for how those work.
This repo's older, pre-version prose log lives at `docs/changelog.md`; it's
a dated pass-level summary, not a per-commit log (see `CLAUDE.md`'s "Docs
system" section), and it is a separate thing from this file. This file
answers one question only: *what does a given `BUILD_VERSION` actually do
differently, at runtime, from the last one.*

## What goes here

Add a bullet under `[Unreleased]` when a change is user/operator visible at
runtime: a new or changed RB/Analyzer/Emitter command, a detection or
behavior profile change, a fix that changes detected/emitted behavior, a
build/flash-relevant config change. Skip internal refactors, doc-only
changes, and pass/plan bookkeeping — those have no runtime-visible delta and
belong in `docs/refactors/` instead.

## When to cut a version

Bump `BUILD_VERSION` in `platformio.ini` at a meaningful milestone (a field
test baseline, a closed pass that changes runtime behavior) — not on every
commit. When you bump it, move `[Unreleased]`'s contents under a new
`## [x.y.z] - <date>` section and leave `[Unreleased]` empty for what's next.

## [Unreleased]

## [0.4.0] - 2026-09-03

Baseline: `BUILD_VERSION` tracking starts here (previously untracked /
`BUILD_DATE`+`BUILD_REV`). Everything before this point is git history and
`docs/refactors/archive/`, not reconstructed into entries here.
