# Step 8b: detection follow-up bugs on the folded inspector

Archived: 2026-10-22 - step 8b closed. Landed: verdict-queue overflow and loop-stack minimum in RB STATUS
(733a68b), the inspection-plan contract RB plan_check (881694f), a continuous tone closed at max duration (7b62c84),
the reject-count rule under test (8136d64), getWindow's scratch off the stack (ef86297), Strong-unused comment
(63b1e1f). Gate: bench/sessions/2026-10-22-step8b (on-device 25/25, T3 50/50 and 10/10 vs the step 8 baseline,
T2h byte-identical). Moved, not done: item 1 (inspect when the window is complete) is on branch
step8b-defer-inspection and waits for DET-017's min-duration tuning pass, because with it a 40 ms tone plus room
reflection is accepted. Found and not fixed: the Node's lowest free loop stack (3,416 B of 16 KB) is set at boot or
in the RB STATUS print, not by detection; not traced further.

Status: closed 2026-10-22 (see the Archived line). Step 8b of `docs/roadmaps/roadmap-0-steps.md`, issue #53.
Baseline: bench/sessions/2026-10-22-step8-baseline (T3, T2h), firmware 4efba86 after step 8.
Rule: no threshold in `DetectionProfile.h` / `BehaviorProfile.h` changes here; an item that would move one is
named and handed to a tuning pass or a decision file.

## Items

| # | item | evidence | changes behavior? | plan |
|---|---|---|---|---|
| 1 | **Short occurrences are inspected before their window exists** (audit 3.1, DET-010 Scalar half, #32 P4). TonalPulseScalar inspects contrast and amp over start..+100 ms, accepts from 85 ms, and inspects at release plus the 10 ms debounce. An 85-89 ms occurrence asks for 1-5 ms of future, gets `future_window_unavailable`, and fails as MissingSupport whatever the signal. | bench 2026-10-22-7d-dur40-raw: every 40 ms tone + room reflection read 85-87 ms and was rejected this way (inspected at 7918 for a window ending 7921). | **Yes**: such occurrences would actually be judged. A 40 ms tone plus reflection may then pass the contrast gate. | **Owner decision (A).** |
| 2 | **No check ties the plan to the detector** (#32 P4 history guard, audit 3.1 last sentence). Nothing asserts `minDuration + releaseDebounce >= windowPost`, or `maxDuration + windowPost` within the FeatureHistory depth (512 bins). Both are comment-enforced. | code read | No | A boot/PARAM-time check like `RB hop_check`, printing `RB plan_check status=ok|warn`. The shipped values print `warn` for the first until item 1 is decided. Host test on the shipped profile. |
| 3 | **A continuous in-band tone holds the scalar detector open** (audit 3.7). Max duration is only checked at release, and `PeakStillActive` is never produced. | code read (`ScalarTransientDetector.cpp` close path) | Barely. A tone over 300 ms is rejected either way, and nothing else is heard while it lasts either way, because a new candidate needs a release first. Only the reject moves earlier, and the report says so while the tone lasts. | Close the candidate as `DurationTooLong` once it passes max duration, then require a release before re-arming. Host fixture: a 600 ms synthetic tone, then a chirp right after it ends, still accepted. T2h unchanged on the corpus. |
| 4 | **Strong is unreachable for contrast** (audit 3.7): `strongPeakThreshold = 300000` on a 0..32,767 domain. | code read | Only the printed class: the requirement is Medium. | A profile value: **owner decision (B)**, or leave it with a comment. |
| 5 | **`getWindow()` puts 2 KB on the loop stack** per module per occurrence (audit 3.7). The Node's stack headroom is unmeasured; step 7b's test overflowed the same 16 KB stack. | code read; 7b test overflow | No | Measure first: the loop task's stack high-water mark in `RB STATUS` (and the Analyzer's system line). Move the buffer to a member only if headroom is under ~4 KB. |
| 6 | **Verdict-queue overflow is invisible on the Node** (audit 3.7): the counter exists only in the Analyzer build. | code read | No | Count it in every build, print it in `RB STATUS`. |
| 7 | **#32 13 / B11, scalar aggregate counts.** In a trial with several rejects, `aggregate.rejected_count` should equal the number of rejects, while `source.rejected_count` counts only new best rejects (decision 2026-10-13). Not checked on hardware since 2026-10-14. | open checklist item | No (a check) | Check existing logs with several rejects per trial (step 6 speech/music, OBS runs). If none fit, one SEQ OBS run with speech. A host test asserting the two counts on the radio-speech fixture. |

Closed as moot: DET-001 (FrequencyMatch report freeze, Freq removed in 7d), DET-010's Freq half, audit 3.2,
#32 P2a, the FrequencyMatch half of B11.

## Decided 2026-10-22 (owner)

- A: defer the inspection until the window is complete (decisions/2026-10-22-inspect-when-window-complete.md).
  Replay the dur40 captures on the host before shipping.
- B: leave `strongPeakThreshold` for contrast as it is and add a comment that Strong is unused for contrast.

## Progress

- 2026-10-22, items 6 and 5 (733a68b): verdict-queue overflows and the loop task's lowest free stack are in RB
  STATUS (and the stack in SYSTEM_HEALTH). The stack reading on a board is still to do.
- 2026-10-22, item 1 on branch `step8b-defer-inspection` (5d2c0fa), not merged. T2h on the corpus is
  byte-identical (no 85-89 ms occurrences there). On bench/sessions/2026-10-22-7d-dur40-raw, all three 40 ms
  tones (read 86 ms with the room reflection) are now **accepted**, in single-target and in band mode. They pass the
  contrast gate: a pure in-band tone is tonal. That triggers the decision's revisit condition: the min duration
  (85 ms) is then the only thing between a 40 ms beep plus reflection and a network signal, and it needs a tuning
  pass before the fix ships. Readings for that pass: hop train 195-214 ms, 100 ms chirp 116-154 ms (both at
  110 cm, 2026-10-22), 40 ms tone 86 ms.

- 2026-10-22, owner: item 1 stays parked on its branch until a min-duration tuning pass (DET-017). Step 8b
  closes with item 1 named as moved to DET-017.

## Options as they were put to the owner

**A. How to fix item 1.** Three options:
- *Defer the inspection* until the window end has arrived (an occurrence waits up to `windowPost - duration`,
  at most 15 ms on the shipped values). The window and the thresholds stay as they are. An 85-89 ms occurrence is
  then judged on its contrast, and the 40 ms + reflection case may be accepted.
- *Clamp the window* to the occurrence (inspect start..min(+100, end)). Same effect on acceptance, but the P75
  is then taken over 85-89 ms instead of 100.
- *Keep rejecting* 85-89 ms occurrences, but say so: an explicit `too_short_for_window` reason instead of
  MissingSupport. Behavior is unchanged, and the gap between min duration and the window becomes visible and
  documented.

Recommendation: defer the inspection (the window keeps one meaning). Measure the 40 ms + reflection case on the
host replay before shipping it (the dur40 captures exist). If it is accepted, raising the min duration becomes a
tuning question, logged as its own pass.

**B. Item 4.** Set `strongPeakThreshold` for contrast to a reachable value (a tuning change, its own pass), or
leave it unreachable with a comment saying Strong is unused for contrast.

## Order

6, 5, 2, 7, 3 (behavior-neutral, each its own commit with T2h unchanged), then 1 and 4 once decided.
Gate (roadmap step 8b): each item fixed with a host fixture or named closed-as-moot; T2h unchanged except where
item 1's decision says otherwise; T3 against the step 8 baseline.
