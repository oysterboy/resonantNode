# A busy network makes a node rest: field density plus own budget, hard pause, same values on every node

Status: decided 2026-10-17 (owner), implemented the same day (Behavior rest rule, host tests Amended 2026-10-22 by 2026-10-22-network-pacing.md.
`test/test_behavior_rest`); bench check with the ping-pong pair pending.
Date: 2026-10-17.

## Decision

Goal (owner): above a level of network traffic the network goes quiet. Implemented in Behavior
(`BehaviorGateConfig`):

- **Field trigger:** when the field window (`FieldStateConfig::patternWindowMs`) holds
  `restAfterFieldPatterns` = 4 valid network patterns, the node does not answer and rests.
  Only accepted, valid verdicts count: speech, music and traffic do not silence the network.
- **Own budget:** `ownEmitBudget` = 6 own emissions inside `ownEmitBudgetWindowMs` = 10 s also start
  a rest (a node that hears only one loud neighbour sees a thin field).
- **Rest:** a hard pause of `restMs` = 10 s: no answers, no idle voice. Then the node listens normally.
- **No per-node variation** (owner's answer; the earlier wish for slight variation per node was
  overruled by this choice, and can come back as a parameter).
- All four values are `RB BEHAV restField= budget= budgetWindowMs= restMs=` (0 = off) and visible in
  `RB STATUS` (`field.patterns`, `behavior.resting`, `rest_until_ms`, `rests`, `last_rest`,
  `blocked_resting`); a rest start logs `RB rest start reason=...`.

Rules out: the unbounded resonance of option 3 below, and the response-probability curve of option 1
(the owner chose the hard pause only).

## History: the open question as drafted

## The fork

When node A plays its hop train and node B answers, A hears B's answer and can answer again. Nothing in the
spec or the roadmaps says whether that chain should run on (the "resonant" reading of the project) or stop
(`docs/specs/mvp-app-structure.md` 4: "longer refractory after N responses in a row, to avoid feedback loops").
Three timers decide it today, all Behavior's: `refractoryAfterEmitMs` (400), `waitAfterHeardMs` (100) and the
own-emit suppression (whole train plus 60 ms). Tuning any of them without this decision tunes a loop nobody
has defined.

## What was measured (bench 2026-10-16-nodepair-70cm, two Node builds, 70 cm)

- Hop train 25 ms x 5, gap 10: 165 ms. A node's own-emit suppression ends 60 ms after its train, at +225 ms
  from its start.
- Verdict to answer start: 117-127 ms (median 119). The answering train therefore starts about 285-340 ms
  after the first node's train started, and its verdict at the first node lands at about 450-500 ms, i.e.
  50-100 ms after the first node's 400 ms refractory has ended (counted from the end of its train).
- The host test `test/test_behavior_refractory` (PR #39, step 5d) shows the margin is thin: with the shipped
  defaults a reply inside the refractory is dropped; with refractory 0 it is consumed.
- On the bench no decision line ever showed a refractory block (`block=none` throughout); the chains stopped
  at two for another reason (E3 forming no candidate for E2's train, under investigation).
- The idle voice (two 500 ms tones every 20-25 s) is heard by the other node as a valid pulse and answered;
  the answer falls inside the caller's own 1.2 s emission and is not heard. Idle traffic therefore starts
  exchanges but never continues them.

## Options

1. **Resonance, bounded.** Chain runs while both hear; each node lowers its response probability or lengthens
   its refractory with every consecutive answer (the MVP note's rule), so a pair rings down over a few rounds
   and five nodes do not lock into a permanent ping-pong. Needs: a per-node "answers in a row" counter in
   Behavior, a decay rule, refractory set below the round trip (about 300 ms or less).
2. **Call and answer only.** One answer per heard signal, no answer to an answer: refractory above the round
   trip (as today, 400 ms, if the timing holds) or an explicit "do not answer within N ms of my own train".
   Simplest, deterministic, silent network between idle calls.
3. **Resonance, unbounded.** Refractory below the round trip, no decay; the pair rings until one misses. Risk:
   five nodes in a room never stop; the idle voice becomes irrelevant.

## What it rules out once taken

- Option 1 or 3 rule out keeping refractory at 400 ms with a 165 ms train (the reply lands inside it).
- Option 2 rules out BEH-006's "variation over time" having anything to vary on inside an exchange.

## Revisit when

The field trial (step 6) shows a room behaviour the chosen option cannot express, or the train length
changes (every timer above scales with it).

## Source

Owner's question 2026-10-16 ("Check research and roadmap re refractory"); lab note 2026-10-16 section 10;
`docs/research/unproven-assumptions-audit.md` 1.1 (PR #39); `docs/specs/mvp-app-structure.md` 4.
