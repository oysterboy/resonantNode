Task: Pass E — Timing / lag logging

Context:
This is Pass E of the current ResonantNode refactor.

Pass A:
Analyzer was checked as the trusted reference path for current AMP/transient detection.

Pass B:
A shared DetectionPipeline scaffold was introduced.

Pass C:
Resonant mode was moved toward the same candidate → DetectionPipeline → PatternResult path as Analyzer, and signal/debug terminology was clarified.

Pass D:
Candidate validity and pattern validity were made explicit and shared.

Current refactor scope:
Introduce a shared detection/classification pipeline scaffold used by both Analyzer and Resonant mode, while keeping the current AMP/transient detector parameters frozen.

Target flow:
DetectorCandidate
→ PatternCandidate
→ PatternResult
→ Analyzer SEQ or ResonantBehavior

Pass E goal:
Expose timing, lag, backlog, and queue/candidate delay clearly enough to diagnose whether bad reactions come from:
- audio input buffering
- detector timing
- candidate queue backlog
- DetectionPipeline timing
- Analyzer SEQ classification timing
- Resonant behavior timing
- output busy/refractory timing

Spec principle:
Timing must be explicit. Detection timing, behavior timing, and analyzer timing should be separated. Detection should report candidates/events; behavior decides output later. Analyzer measures detection quality and may log more detail than runtime behavior. :contentReference[oaicite:0]{index=0}

Important constraints:
- Do not tune detector parameters.
- Do not change AudioOnsetDetector thresholds.
- Do not change AudioSignal candidate generation unless absolutely required to expose existing timing facts.
- Do not implement advanced chirp grouping.
- Do not implement frequency matching.
- Do not implement family matching.
- Do not implement overlap/dominance handling.
- Do not implement VEKTOR transport, OSC, hub integration, or resource registry.
- Do not redesign behavior logic.
- Keep DetectionPipeline simple/pass-through.
- This is a logging/diagnostic pass, not a behavior-fix pass.

Frozen AMP detector baseline:
onsetThreshold = 36.0
releaseThreshold = 26.0
cooldownMs = 300
releaseDebounceMs = 30
minTransientDurationMs = 60
maxTransientDurationMs = 240
minTransientPeakStrength = 40.0

Core timing layers to distinguish:

1. Audio/source timing

Question:
When did the audio block/sample enter processing?

Useful facts:
- source block/sample timestamp if available
- processing loop now
- blocks processed per loop
- samples processed per loop
- audio overflows/drops if available
- audio backlog if available

2. DetectorCandidate timing

Question:
When did the detector think the event started, become accepted, and end?

Useful facts:
- candidate.startMs
- candidate.acceptedMs / heardAtMs
- candidate.endMs if available
- candidate.durationMs
- candidate.peakStrength
- candidate.release/reject reason
- candidate.overflow flag if available
- candidate queue depth / dropped candidate count if available

3. DetectionPipeline / PatternResult timing

Question:
When was the candidate converted into a PatternResult, and what timing did the result preserve?

Useful facts:
- pattern.heardAtMs
- pattern.processedAtMs / pipelineNowMs if useful
- pattern.durationMs
- pattern.strength
- pattern.type
- pattern.reason
- candidate→pattern delay if meaningful

4. Analyzer SEQ timing

Question:
How does PatternResult timing relate to the controlled test trigger?

Useful facts:
- lastEmitCommandMs / triggerMs
- dt = pattern.heardAtMs - triggerMs
- expected / early / late / duplicate / miss classification
- detection window start/end
- trial timeout
- duplicate timing
- miss category

5. Resonant behavior timing

Question:
Given a PatternResult, why did behavior respond or not respond?

Useful facts:
- pattern received time
- behavior update now
- lastHeardMs
- lastEmitMs
- waitAfterHeardUntil
- refractoryUntil
- ignoreAfterEmitUntil / self-suppression window
- outputBusy
- detectOnly / listenOnly state if present
- planned emit time if behavior schedules delayed response

Pass E should not need to solve all behavior block reasons yet.
That is Pass F.
But Pass E should expose the timing values needed for Pass F.

Implementation target:

1. Audit existing timing logs

Find current logs for:
- Analyzer SEQ trial timing
- DetectorCandidate timing
- PatternResult timing
- Resonant behavior update timing
- output timing
- CAP / BASE / debug summaries from Pass C

Identify:
- useful existing timing values
- missing timing values
- ambiguous timing labels
- places where loop now is confused with candidate event time

2. Add minimal timing fields if missing

If PatternResult does not already preserve enough timing, add fields such as:
- heardAtMs
- durationMs
- processedAtMs, optional
- sourceStartMs / candidateStartMs, optional
- sourceAcceptedMs / candidateAcceptedMs, optional

Prefer preserving DetectorCandidate timing into PatternResult rather than recomputing it in Analyzer or Behavior.

Do not overbuild timestamps that cannot be measured reliably.

3. Add shared timing log helper if useful

Optional but preferred:
Add a small debug helper/function that prints candidate → pattern timing consistently in Analyzer and Resonant.

Example conceptual log:

CAND start=12345 accept=12490 dur=145 str=58.2 ovf=0
PATT type=ValidTransient heard=12490 proc=12496 lag=6 reason=FromAcceptedTransient

Do not spam raw sample logs.

4. Analyzer timing output

Ensure Analyzer can show:
- trigger/emit command time
- PatternResult heardAtMs
- dt from trigger
- SEQ category
- duplicate timing
- miss reason/category
- overflow/backlog if available

Analyzer should still classify:
expected / early / late / duplicate / miss

5. Resonant timing output

Ensure Resonant can show:
- PatternResult received
- pattern.heardAtMs
- behavior update now
- delta between heardAtMs and behavior update
- output busy or relevant timing gate if already available
- whether behavior consumed/ignored the result

Do not fully implement behavior blocking reason taxonomy here unless it is already trivial.
Pass F will formalize block reasons.

6. Queue/backlog visibility

If AudioSignal has a candidate queue:
- expose queue depth where cheap
- expose dropped/overflow count where cheap
- log when candidate queue overflows
- avoid one-candidate-per-loop hidden backlog

If audio block/input overflow is available:
- include it in summaries or candidate logs

If not available:
- explicitly report that overflow/backlog visibility is not available yet.

7. Timing semantics

Make sure code comments/log labels distinguish:
- event time: when candidate happened / was accepted
- processing time: when loop handled it
- trigger time: when Analyzer emitted command
- behavior time: when behavior updated or decided

Avoid using one generic “now” in logs without context.

Expected output:
1. Concise Pass E implementation summary.
2. Files changed.
3. Timing fields added or clarified.
4. Existing timing logs audited.
5. New/changed Analyzer timing logs.
6. New/changed Resonant timing logs.
7. Queue/backlog/overflow visibility status.
8. Evidence that detector baseline is unchanged.
9. Notes on missing timing facts that cannot currently be measured.
10. Whether Pass F can begin.

Acceptance checklist:
[ ] DetectorCandidate timing is visible or preserved.
[ ] PatternResult timing is visible or preserved.
[ ] Analyzer logs dt from trigger to PatternResult heardAtMs.
[ ] Analyzer still reports expected / early / late / duplicate / miss.
[ ] Resonant logs delta between PatternResult heardAtMs and behavior handling/update where practical.
[ ] Candidate queue/backlog/overflow visibility is available, or explicitly marked unavailable.
[ ] Event time, processing time, trigger time, and behavior time are not conflated.
[ ] Logs can help distinguish input buffering, detector delay, queue backlog, pipeline delay, SEQ timing, and behavior timing.
[ ] Detector parameters remain unchanged.
[ ] AudioSignal / AudioOnsetDetector candidate generation remains unchanged unless only exposing existing timing facts.
[ ] No advanced chirp/frequency/overlap logic is added.
[ ] No behavior redesign is included.

Definition of done:
Pass E is done when Analyzer and Resonant logs expose enough timing and lag facts to diagnose whether delayed, missed, duplicate, or false reactions are caused by input buffering, detector timing, candidate queue backlog, pipeline handling, Analyzer trial classification, or behavior timing.