Task: Pass F — Behavior blocking reasons + detection-only mode

Context:
This is Pass F of the current ResonantNode refactor.

Pass A:
Analyzer was checked as the trusted reference path for current AMP/transient detection.

Pass B:
A shared DetectionPipeline scaffold was introduced.

Pass C:
Resonant mode was moved toward the same candidate → DetectionPipeline → PatternResult path as Analyzer, and signal/debug terminology was clarified.

Pass D:
Candidate validity and pattern validity were made explicit and shared.

Pass E:
Timing / lag logging was added or clarified.

Current refactor scope:
Introduce a shared detection/classification pipeline scaffold used by both Analyzer and Resonant mode, while keeping the current AMP/transient detector parameters frozen.

Target flow:
DetectorCandidate
→ PatternCandidate
→ PatternResult
→ Analyzer SEQ or ResonantBehavior

Pass F goal:
Make ResonantBehavior silence and non-response explainable, and make detection-only mode a clear runtime mode.

After Pass F, when a valid PatternResult reaches ResonantBehavior, logs/state should explain whether behavior:
- consumed it
- ignored it
- intended to respond
- blocked response
- emitted
- did nothing because detection-only/listen-only/output gating was active

Spec principle:
Behavior consumes classified acoustic events and internal state.
Behavior owns reaction logic, timers, cooldowns, refractory windows, and output decisions.
Detection reports candidates/events; behavior decides whether to output.
Analyzer measures; behavior performs.
Timing must be explicit.

Important constraints:
- Do not tune detector parameters.
- Do not change AudioOnsetDetector thresholds.
- Do not change AudioSignal candidate generation.
- Do not implement advanced chirp grouping.
- Do not implement frequency matching.
- Do not implement family matching.
- Do not implement overlap/dominance handling.
- Do not implement VEKTOR transport, OSC, hub integration, or resource registry.
- Do not redesign the full behavior algorithm.
- Do not make autonomous behavior more aggressive yet.
- Keep DetectionPipeline simple/pass-through.
- Keep this pass diagnostic and state/explainability-focused.

Frozen AMP detector baseline:
onsetThreshold = 36.0
releaseThreshold = 26.0
cooldownMs = 300
releaseDebounceMs = 30
minTransientDurationMs = 60
maxTransientDurationMs = 240
minTransientPeakStrength = 40.0

Core problem:
Before cautious behavior re-enable, we need to know why behavior does or does not emit.

A valid PatternResult should no longer disappear silently.

Implementation target:

1. Behavior block reason model

Add a small explicit behavior block/decision reason enum or equivalent.

Suggested enum:

enum class BehaviorDecision {
    None,
    ConsumedPattern,
    IgnoredInvalidPattern,
    IgnoredAmbiguousPattern,
    DetectionOnly,
    ListenOnly,
    Disabled,
    OutputBusy,
    WaitingAfterHeard,
    RefractoryAfterEmit,
    IgnoreAfterOwnEmit,
    CooldownAfterDetect,
    SelfSuppressed,
    AlreadyScheduled,
    ResponseProbabilitySkipped,
    Emitted,
    WouldEmit,
    UnknownBlocked
};

Use smaller names if preferred, but keep the distinction clear.

Important:
Do not conflate detector rejection with behavior blocking.

Detector invalidity belongs to DetectorCandidate / PatternResult.
Behavior blocking belongs to ResonantBehavior.

2. Behavior-facing state

Expose enough behavior state to explain decisions:

Useful fields:
- current behavior mode
- lastHeardMs
- lastEmitMs
- waitUntilMs / waitAfterHeardUntil
- refractoryUntilMs
- ignoreOwnEmitUntilMs
- outputBusy
- detectionOnly enabled
- listenOnly enabled if present
- disabled enabled if present
- lastPatternType
- lastPatternHeardAtMs
- lastDecision
- lastBlockReason
- wouldEmit flag if in dry-run mode

Do not overbuild a full VEKTOR state model yet.
This is local debug/state.

3. Detection-only mode

Make detection-only mode clear and explicit.

Definition:
Detection-only mode means:
- audio input active
- detector active
- DetectionPipeline active
- PatternResult logging active
- ResonantBehavior may observe/log PatternResult
- autonomous output is disabled

It should not mean:
- detector disabled
- candidate drain skipped
- PatternResult skipped
- behavior silently bypassed without logs

If a runtime mode enum already exists, integrate detection-only there.
If not, a clearly named flag is acceptable for now:

bool detectionOnly;

But logs/state should say detectionOnly clearly.

4. Behavior handling of PatternResult

When ResonantBehavior receives a PatternResult:
- record that it was received
- reject/ignore invalid or ambiguous PatternResult with a clear reason
- if valid, evaluate behavior gates/timers
- if detectionOnly is active, do not emit and set reason DetectionOnly
- if output is busy, set OutputBusy
- if refractory active, set RefractoryAfterEmit
- if waiting, set WaitingAfterHeard or AlreadyScheduled
- if self suppression / ignore own emit is active, set IgnoreAfterOwnEmit or SelfSuppressed
- if it would emit but output is disabled by mode, log WouldEmit or DetectionOnly

Do not make the behavior more responsive yet.
Only make decisions explicit.

5. Logging

Add minimal behavior logs or summaries showing:

For each relevant PatternResult:
- pattern type
- heardAtMs
- behaviorNowMs
- accepted/ignored by behavior
- decision/block reason
- key timing gate if blocked
- whether output emitted or would have emitted

Example conceptual log:

BEH pattern=ValidTransient heard=12345 now=12410 decision=DetectionOnly
BEH pattern=ValidTransient heard=22345 now=22400 decision=RefractoryAfterEmit until=23000
BEH pattern=ValidTransient heard=32345 now=32400 decision=WouldEmit delay=120
BEH emit start now=32520

Avoid spamming every loop when nothing changes.
Prefer event-style logs or summary counters.

6. Summary counters

Optional but useful:
Track counts since boot or since reset:

- patternsReceived
- patternsIgnoredInvalid
- patternsIgnoredAmbiguous
- blockedDetectionOnly
- blockedOutputBusy
- blockedRefractory
- blockedWaiting
- blockedSelfSuppressed
- wouldEmitCount
- emittedCount

If this is too much, at least expose lastDecision and lastBlockReason.

7. Serial commands / debug interface

If there are existing serial commands such as:
- detectonly
- summary
- debug

Update them so detection-only mode and behavior reasons are visible.

Possible commands:
- rb detectonly 1/0
- rb summary
- rb debug 1/0
- rb reasons
- rb resetstats

Do not invent a large command system.
Only extend what already exists.

8. Analyzer separation

Do not add behavior blocking to Analyzer.

Analyzer remains:
PatternResult → SEQ expected/early/late/duplicate/miss

Behavior blocking is only for Resonant runtime.

9. Relation to Pass G

Pass F should prepare Pass G.

At the end of Pass F, you should be able to run Resonant in detection-only or dry-run mode and see:

PatternResult arrived.
Behavior would respond or not respond.
If not, why not.
If yes, whether output was suppressed by mode.

Pass G will cautiously re-enable actual behavior/output.

Expected output:
1. Concise Pass F implementation summary.
2. Files changed.
3. Behavior decision/block reason enum or equivalent.
4. Detection-only mode implementation/clarification.
5. New or changed ResonantBehavior API/state.
6. New behavior logs or summaries.
7. Serial/debug commands affected.
8. Evidence Analyzer remains behavior-free.
9. Evidence detector baseline is unchanged.
10. Notes on temporary compatibility shims.
11. Whether Pass G can begin.

Acceptance checklist:
[ ] Behavior has explicit decision/block reasons.
[ ] Detection-only mode is clearly defined and implemented.
[ ] Detection-only mode keeps detection + DetectionPipeline + PatternResult logging active.
[ ] Detection-only mode suppresses autonomous output clearly, not silently.
[ ] Valid PatternResult reaching behavior is logged or summarized.
[ ] Behavior explains ignored/blocked patterns.
[ ] Behavior distinguishes invalid/ambiguous PatternResult from behavior blocking.
[ ] Behavior exposes key timing gates: wait, refractory, ignore-own-emit/self suppression, output busy.
[ ] Last decision / last block reason is visible in logs or summary.
[ ] Optional counters exist, or a clear reason why they were deferred.
[ ] Analyzer remains behavior-free.
[ ] Detector parameters remain unchanged.
[ ] AudioSignal / AudioOnsetDetector candidate generation remains unchanged.
[ ] No advanced chirp/frequency/overlap logic is added.
[ ] Autonomous behavior is not made more aggressive yet.

Definition of done:
Pass F is done when Resonant mode can run with detection and PatternResult flow active, behavior/output suppressed or enabled by mode, and every valid PatternResult that reaches behavior results in an explainable decision such as emitted, would emit, detection-only blocked, refractory blocked, waiting, output busy, self-suppressed, invalid, or ambiguous.