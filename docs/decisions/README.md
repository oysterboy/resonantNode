# Decisions

Status: index of recorded architecture and process decisions.
Scope: forks that were taken deliberately and should not be re-litigated by
accident, plus the ones that are still open.
Purpose: give decisions a home that outlives the pass / plan doc they were
made in. Pass docs under `docs/refactors/` get archived when their work
closes; a decision made inside one is still binding afterwards, and the
next session (or person) needs one predictable place to check "is this
already decided?" before proposing it again.

How this relates to the other docs:

```text
docs/specs/myspec.md        what is always true (the rules)
docs/decisions/             which forks were taken, why, and when to revisit
docs/refactors/             how a decided item gets executed (temporary)
docs/roadmaps/              what is next, in what order
```

A decision is not a rule. When a decision hardens into something the spec
states outright, the spec gets the rule and the decision file stays as the
"why". The decision file is the durable record; the pass doc it came from
is the detailed evidence and may be archived.

## Format

One file per decision, named `YYYY-MM-DD-short-slug.md`, with these
sections (keep each short, usually under 40 lines total):

```text
Status       decided | decided, not yet implemented | superseded | open
Date
Decision     one or two sentences, the fork taken
Why          the reasons, with numbers where they were measured
Rules out    what this closes off, so it isn't re-proposed
Revisit when the condition under which this should be reopened
Source       the doc, section and commit where it was made
```

A superseded decision keeps its file with Status changed and a pointer to
what replaced it.

## Decided

| Date | Decision |
|---|---|
| 2026-09-20 | [Occurrence detail fields are evidence namespaces, not per-detector slots](2026-09-20-occurrence-detail-namespaces.md) |
| 2026-09-21 | [Defer the single-active-detector union; superseded by family build flag](2026-09-21-defer-single-detector-union.md) |
| 2026-09-21 | [Rename PatternMatcher / PatternResult to OccurrenceEvaluator / OccurrenceVerdict](2026-09-21-rename-patternmatcher-to-occurrenceevaluator.md) |
| 2026-09-21 | [Fold OccurrenceEvaluator into OccurrenceInspector (Phase 7c, implemented 2026-10-22 as makeVerdict(), step 8)](2026-09-21-fold-evaluator-into-inspector.md) |
| 2026-09-23 | [Detector family is a build flag; no virtual detector or runtime interface](2026-09-23-no-virtual-detector-interface.md) |
| 2026-09-23 | [Keep ParamRegistry in this repo as a PlatformIO library](2026-09-23-keep-paramregistry-in-repo.md) |
| 2026-10-06 | [Nodes move to D-AMP output (MAX98357A over I2S); board is a build variant](2026-10-06-damp-output-hardware.md) |
| 2026-10-08 | [Commit cited hardware runs under bench/, with setup and firmware hash](2026-10-08-bench-data-in-repo.md) |
| 2026-10-09 | [Flash at 80 MHz QIO; I2S sample time comes from the sample index](2026-10-09-flash-qio80-and-i2s-sample-clock.md) |
| 2026-10-09 | [Piezo discontinued: D-AMP is the default board, piezo a fallback build (`BOARD_PIEZO`)](2026-10-09-discontinue-piezo.md) |
| 2026-10-09 | [D-AMP tone level 0.3 of full scale](2026-10-09-damp-tone-level-0p3.md) (superseded 2026-10-21 by 0.5) |
| 2026-10-09 | [The chirp stays at 3200 Hz on D-AMP](2026-10-09-chirp-frequency-stays-3200.md) |
| 2026-10-09 | [Own-emit detection suppression covers the whole chirp plus a tail](2026-10-09-own-emit-suppression-whole-chirp.md) |
| 2026-10-09 | [D-AMP keeps First Difference as the PCM preprocessor until after the field trial](2026-10-09-damp-keep-first-difference.md) (timing superseded 2026-10-14) |
| 2026-10-09 | [D-AMP I2S HAL: extend AudioSourceI2S, TX task, stereo slots, Philips framing with RX realigned](2026-10-09-damp-i2s-hal-shape.md) |
| 2026-10-13 | [The Node does not ignore verdicts on occurrences with dropped audio](2026-10-13-node-reacts-despite-dropped-audio.md) |
| 2026-10-13 | [One reject event per new selected (best) reject](2026-10-13-one-reject-event-per-new-best-reject.md) |
| 2026-10-13 | [SEQ sample dump: default step 4 ms, 256-row buffer kept](2026-10-13-sample-dump-step-4ms.md) |
| 2026-10-14 | [The Node ships TonalPulseScalar; TonalPulseFreq stays a demo detector](2026-10-14-node-ships-tonalpulsescalar.md) (demo half superseded 2026-10-22: Freq removed) |
| 2026-10-14 | [Replace First Difference with a DC blocker inside step 5a, before the new-geometry baseline](2026-10-14-dc-blocker-before-5a-baseline.md) |
| 2026-10-15 | [Signals vary per node and over time inside a shared band; the detector is shape-agnostic](2026-10-15-signal-variation-inside-a-shared-band.md) |
| 2026-10-16 | [Hop acceptance: the contrast inspector guards against noise (30,500), the amp limit drops to 1,500; long gaps leave the acceptance set, max duration 300, history 512](2026-10-16-hop-acceptance-contrast-gate-not-amp.md) (shipped in the profile; confirmed 2026-10-21, step 5b gate: second-day battery and speech / music OBS on the Node build) |
| 2026-10-16 | [The network band is 2950-3950 Hz: bins 2950/3200/3450/3700/3950, the 3,200 Hz chirp on a bin](2026-10-16-network-band-2950-3950.md) |
| 2026-10-17 | [Step 6's third question: after a kick or idle call the group is quiet (15 s silence) within 30 s in 80 % of cases](2026-10-17-trial-third-question-pass-mark.md) (M = 100 set 2026-10-22, 2026-10-22-network-pacing.md) |
| 2026-10-17 | [The idle call wakes the network: 300 Hz announcement, then the node's hop train](2026-10-17-idle-call-wakes-the-network.md) (bench-checked 2026-10-21, step 5g: 27 idle calls, all answered) |
| 2026-10-17 | [A busy network makes a node rest: field density (4 patterns) or own budget (6 in 10 s), hard 10 s pause, same values everywhere](2026-10-17-two-node-exchange-chain-length.md) |
| 2026-10-17 | [Behavior: one program, the shipped struct as its profile, values as params; a bench session records the whole effective state](2026-10-17-behavior-contract-and-reproducible-sessions.md) |
| 2026-10-21 | [D-AMP tone level 0.5 of full scale: at 0.3 the five-board hearing matrix had 7 of 20 cells at 10/10, at 0.5 19 of 20](2026-10-21-damp-tone-level-0p5.md) |
| 2026-10-22 | [Console output in whole lines (serialOut) and the per-sample path in IRAM: Analyzer report 190 -> 3.4 ms, per-sample 73 -> 37 us, 0 dropped buffers](2026-10-22-serial-lines-and-iram-hot-path.md) |
| 2026-10-22 | [FieldState stays as the rest gate; noiseFloor, avgAmbientLevel and chatter go (trim in step 8)](2026-10-22-fieldstate-is-the-rest-gate.md) |
| 2026-10-22 | [Remove FrequencyMatchDetector and TonalPulseFreq; Scalar is the only family (step 7d)](2026-10-22-remove-frequencymatch.md) |
| 2026-10-22 | [Inspect an occurrence when its window is complete, not at release (step 8b)](2026-10-22-inspect-when-window-complete.md) (implemented 2026-10-23, bbb5728) |
| 2026-10-22 | [Live params apply in place; RB PROFILE announces its reset; RB BEHAV removed for behavior.* params (step 10)](2026-10-22-live-params-profile-reset-no-rb-behav.md) |
| 2026-10-22 | [Network pacing: rest 5 s, idle call every 60 s +- 20 s, pass mark "the chain ends within 30 s in 80 %", M = 100](2026-10-22-network-pacing.md) |
| 2026-10-23 | [Params persist on PARAM SAVE only; all registered; stored wins over a changed default, with a PARAM stale warning (PAR-010/011)](2026-10-23-param-persistence-save-only.md) |
| 2026-10-23 | [SoundOutput executes an OutputRequest (hop train, pattern, idle call) with OutputStatus; the Node composes it from Behavior's decision (step 10f)](2026-10-23-soundoutput-executes-node-composes.md) |
| 2026-10-23 | [The Node's network signal is the hop train; a plain chirp need not be heard](2026-10-23-node-hears-the-hop-train-only.md) (enforce by shape, DET-015; min duration cannot: the readings overlap) |
| 2026-10-25 | [VEKTOR will replace NET; NET is the interim remote console; 11a's lessons go to the spec](2026-10-25-vektor-replaces-net.md) (not yet implemented) |
| 2026-10-25 | [Remote params over ESP-NOW through the serial PARAM path; one-line replies only, DUMP/LIST stay on USB (step 11a)](2026-10-25-remote-params-espnow.md) |
| 2026-10-25 | [Analyzer knobs on ParamRegistry with the Node's grammar, old key=value form dropped; long serial lines refused on every port (step 11e)](2026-10-25-analyzer-params-on-registry-and-line-policy.md) (implemented in 11e items 8-9) |
| 2026-10-26 | [DET-015: a bin track read by a shape inspector (>= 3 bins, 2 steps up, band mode); an in-band sweep counts as a network signal](2026-10-26-det015-bin-track-inspector.md) |
| 2026-10-26 | [The Node's boot baseline runs: mic warmup, then the quiet gate (review D2.4)](2026-10-26-node-boot-warmup-runs.md) |
| 2026-10-26 | [Drop `RB debug off\|events` and the compile-time Node tracing (review D2.5)](2026-10-26-drop-rb-debug.md) (implemented 2026-10-27) |
| 2026-10-26 | [Milestone "installation-ready node": gate = 11c soak, 11g batches, 11d OTA firmware; then the step sequence restarts at 12 in a fresh file](2026-10-26-milestone-installation-ready-node.md) (cut-over waits for the gate) |
| 2026-10-28 | [The network band stays at 2950-3950 Hz after the 500-5000 Hz sweep (E004, step 11h)](2026-10-28-band-stays-after-the-sweep.md) |
| 2026-10-29 | [Answers follow the field: the level automaton (one integer level, p = (level + 1) / L for answering and for not yielding, rest at L); FieldState no longer the rest gate](2026-10-29-answers-follow-the-field-level-automaton.md) (BEH-008, not implemented) |
| 2026-10-29 | [The network band widens to 8 bins (2950-4700 Hz) for a base bin per node; band shifting and a musical tone are tested after](2026-10-29-wider-band-base-bin-per-node.md) (BEH-009, not implemented; amends the 2026-10-28 band decision) |
| 2026-10-29 | [Max duration 450 ms (history 640, bin track 1024), so answers 120-220 ms apart are not lost as one too-long sound](2026-10-29-max-duration-450-for-chords.md) (step 14) |
| 2026-10-28 | [OTA firmware: every board pulls the image over HTTP from WiFi on command (NET * OTA for the Nodes), rollback until confirmed (step 11d)](2026-10-28-ota-firmware-pull-over-http.md) |

## Open

These are known forks that have not been taken. Each points at where the
question is tracked; a file is added here when one resolves.
Resolved and taken off this table 2026-10-26: the Node's profile
(2026-10-14-node-ships-tonalpulsescalar.md), keep or consolidate FrequencyMatch
(2026-10-22-remove-frequencymatch.md), persistent config
(2026-10-23-param-persistence-save-only.md).

| Question | Tracked in |
|---|---|
| The goal of the phase after the installation-ready milestone (8a, signal variation part 2, was the candidate; since 2026-10-29 BEH-008 and BEH-009 are decided) | `2026-10-26-milestone-installation-ready-node.md`; `2026-10-29-answers-follow-the-field-level-automaton.md`; `2026-10-29-wider-band-base-bin-per-node.md`; sequenced at the cut-over, in the new roadmap-0-steps.md |
| Which metrics belong in the generic `Occurrence` core vs detector-specific detail | `docs/specs/myspec.md` §5 ("temporary typed accepted-event detail"); DET-003..006 |
| How much `FeatureHistory` to retain on-device | `docs/refactors/archive/cleanup-0-plan.md` Phase 5c (33 KB, 80% of `DetectionRuntime` at last measurement) |
| Exact boundary between analyzer-only and runtime diagnostics | `docs/refactors/archive/cleanup-analyzer-node-isolation.md`; Phase 5a landed the first cut, `DetectionDiagnostics` (step 11e item 7) the runtime side; Analyzer printers still in `src/detection/` (review D2.8, step 11g batch C) |
