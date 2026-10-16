# How long does a two-node exchange run: open

Status: open (drafted 2026-10-17 from the 2026-10-16 bench, for the owner to decide before step 5d / step 6).
Date: 2026-10-17.

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
