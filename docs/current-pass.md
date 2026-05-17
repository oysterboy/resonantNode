# Current Pass

## End Target

The analyzer end target is the roadmap in `docs/analyzer-roadmap-v0.1.md`.

That roadmap says the analyzer should become a stable report layer over `DetectionProfile`, with:

- `SEQ_TRIAL` as compact truth
- `SEQ_EXPLAIN` as why/how detail
- `SEQ_SUMMARY` as run comparison
- raw sample capture kept separate from normal SEQ output

## Working List

The itemized pass sequence lives in `docs/analyzer-refactor-pass-overview-v0.1.md`.

Use that file as the ordered implementation path:

- Pass 0: freeze current outputs
- Pass A: quarantine legacy output
- Pass B: add the AnalyzerReporting skeleton
- Pass C: build `AnalyzerReport` from the current trial
- Pass D: make the new default `SEQ_TRIAL`
- Pass E: add `SEQ_EXPLAIN`
- Pass F: clean up `SEQ_SUMMARY`
- Pass G: separate legacy report storage
- Pass H: harden profile switching
- Pass I: optional shared audio reporting extraction
- Pass J: remove legacy outputs after the new ones are stable

## Completed Baseline

Pass 0 is done.

The frozen baseline lives in `docs/log_refactorpasses/analyser_oldbaseline.md`.

Goal:

- capture representative current Analyzer output before more refactor work lands
- keep the existing output vocabulary intact while the baseline is frozen
- make later changes easy to compare against known-good logs

What to capture:

- default `SEQ`
- `trialbrief`
- `summary+trial`
- `full`
- `raw`
- `debug=2`
- `RAW trigger`
- `tries=10 debug=2`

What this pass should not do:

- do not rename the runtime report lines yet
- do not remove legacy aliases yet
- do not change detector behavior while the baseline is being recorded

## Current Pass

Pass A is now the live working step.

Goal:

- quarantine old SEQ raw/report/freq-class/trialbrief/long-trial outputs behind explicit legacy or explain wrappers
- keep backwards aliases for now
- clarify help text without touching `RAW trigger`

What this pass should not do:

- do not change the underlying detection behavior
- do not remove the legacy outputs yet
- do not jump ahead to `AnalyzerReporting` or `SEQ_TRIAL` restructuring

## Context

The current Analyzer code already has the old and new vocabulary mixed together in places.

Keep the work framed around the stable layer split:

- detection produces evidence
- pattern logic produces meaning
- analyzer reports trial-level truth

## Files Likely Involved

- `docs/analyzer-roadmap-v0.1.md`
- `docs/analyzer-refactor-pass-overview-v0.1.md`
- `docs/log_refactorpasses/analyser_oldbaseline.md`
- `docs/changelog.md`
- `src/modes/analyzer/AnalyzerApp.cpp`

## Implementation Steps

1. Read the roadmap and the overview before changing anything else.
2. Keep the baseline log file as the comparison point.
3. Quarantine legacy output paths behind explicit wrappers or labels.
4. Preserve the existing aliases while the new wording lands.
5. Keep the pass small and factual.

## Verification

- Manual serial smoke run of the listed SEQ and RAW commands
- `platformio run -e esp32dev-analyzer`

## Changelog Instruction

If code or help text changes in this pass, add a concise factual entry to `docs/changelog.md` under `Changes Since Last Commit`.
