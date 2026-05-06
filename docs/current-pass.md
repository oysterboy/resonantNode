Task: Pass D — Candidate / pattern validity parity

Context:
This is Pass D of the current ResonantNode refactor.

Status:
- Pass D is complete.
- SEQ now classifies by `onset_dt_ms`, logs `end_dt_ms` separately, and keeps DET/PAT parity logs unchanged.
- `early` now folds into `expected` on the onset-based baseline.
- Detector-to-pattern validity is centralized, reason codes are shared, and Resonant behavior stays simpler than Analyzer.

Pass A:
Analyzer was checked as the trusted reference path for current AMP/transient detection.

Pass B:
A shared DetectionPipeline scaffold was introduced.

Pass C:
Resonant mode was moved toward the same candidate → DetectionPipeline → PatternResult path as Analyzer, and signal/debug terminology was clarified.

Current refactor scope:
Introduce a shared detection/classification pipeline scaffold used by both Analyzer and Resonant mode, while keeping the current AMP/transient detector parameters frozen.

Target flow:
DetectorCandidate
→ PatternCandidate
→ PatternResult
→ Analyzer SEQ or ResonantBehavior

Pass D goal:
Make candidate validity and pattern validity explicit, consistent, and shared.

Result:
- D2: centralized `DetectorCandidate -> PatternResult` validity in `DetectionPipeline`.
- D3: kept Analyzer SEQ classification downstream and measurement-only.
- D4: kept runtime behavior simpler than Analyzer.
- D5: clarified shared reason codes and logging.

Pass E:
- Ready to begin.

The code should no longer contain hidden or duplicated validity decisions spread across:
- AudioSignal / detector drain
- DetectionPipeline
- Analyzer SEQ
- Resonant node glue
- ResonantBehavior

Spec principle:
Detection transforms feature streams into low-level acoustic candidates.
Classification transforms candidates into meaningful acoustic events.
Analyzer may classify strictly for measurement.
Runtime behavior may consume simpler behavior-facing PatternResult values.
Detection should not decide behavior.

Important constraints:
- Do not tune detector parameters.
- Do not change AudioOnsetDetector thresholds.
- Do not change AudioSignal candidate generation unless absolutely required for exposing existing validity facts.
- Do not implement advanced chirp grouping.
- Do not implement frequency matching.
- Do not implement family matching.
- Do not implement overlap/dominance handling.
- Do not implement VEKTOR transport, OSC, hub integration, or resource registry.
- Keep DetectionPipeline simple.
- Avoid broad rewrites.
- Do not redesign behavior logic beyond validity handling.

Frozen AMP detector baseline:
onsetThreshold = 36.0
releaseThreshold = 26.0
cooldownMs = 300
releaseDebounceMs = 30
minTransientDurationMs = 60
maxTransientDurationMs = 240
minTransientPeakStrength = 40.0

Core distinction to enforce:

1. DetectorCandidate validity

Question:
Was this a detector-level accepted transient-like event?

This belongs near:
- AudioSignal
- AudioOnsetDetector
- DetectorCandidate
- candidate queue / drain

Possible detector-level facts:
- accepted / rejected
- startMs
- acceptedMs
- endMs
- durationMs
- peakStrength
- release reason
- reject reason
- overflow
- blocked / peak active
- duration too short
- duration too long
- strength too low
- quiet / no onset

2. PatternResult validity

Question:
Is this a usable classified acoustic event for Analyzer or Behavior?

This belongs near:
- DetectionPipeline
- PatternCandidate
- PatternResult
- SimpleTransientPatternDetector

Possible pattern-level results:
- None
- ValidTransient
- Invalid
- Ambiguous

Later pattern types may include ValidChirp, ValidTone, Noise, etc., but do not implement those now unless already present as harmless placeholders.

3. Analyzer SEQ trial classification

Question:
How does this PatternResult relate to the controlled test trigger?

This belongs only in Analyzer SEQ.

Analyzer may classify:
- expected
- early
- late
- duplicate
- miss
- unexpected

Important:
expected / early / late / duplicate / miss are measurement classifications, not general runtime PatternTypes.

4. Runtime behavior validity

Question:
Should behavior treat this PatternResult as a valid heard event, ignore it, or block response?

This belongs in ResonantBehavior or a small behavior-facing adapter.

Runtime behavior should not need to know:
- exact detector reject internals
- SEQ trial category
- raw sample / amp-level details

Implementation target:

1. Audit current validity logic

Search for all places where the code decides that a detection/candidate/event is:
- valid
- invalid
- accepted
- rejected
- early
- late
- duplicate
- miss
- ignored
- unexpected
- transientDetected
- should respond
- should emit

Identify which layer each decision belongs to:
- detector-level
- pattern-level
- analyzer SEQ-level
- behavior-level

2. Centralize detector-to-pattern validity in DetectionPipeline

DetectionPipeline should be the single shared place where a DetectorCandidate becomes a PatternResult.

For now:
- accepted DetectorCandidate → PatternResult::ValidTransient
- rejected/invalid DetectorCandidate → PatternResult::Invalid or false/no result
- unclear candidate → PatternResult::Ambiguous if needed

Preserve reason fields.

Do not let Analyzer and Resonant each invent their own DetectorCandidate → valid-heard-event logic.

3. Keep Analyzer SEQ strict but downstream

Analyzer SEQ should consume PatternResult, then classify trial outcome.

Correct:
DetectorCandidate
→ DetectionPipeline
→ PatternResult::ValidTransient
→ SEQ classifies expected / early / late / duplicate

Wrong:
DetectorCandidate
→ Analyzer-only validity rule
→ expected / late
while Resonant uses a different validity rule

4. Keep runtime behavior simpler than Analyzer

Runtime behavior should consume only behavior-relevant validity.

For now:
- PatternResult::ValidTransient may become validHeardEvent
- PatternResult::Invalid should be ignored
- PatternResult::Ambiguous should probably be ignored or logged

Behavior should not consume Analyzer-only categories:
- expected
- early
- late
- duplicate
- miss

Those are test measurements, not behavior semantics.

5. Clarify reason codes

If reason strings/enums exist, make them clear enough for logging.

Suggested distinction:

DetectorRejectReason:
- None
- Quiet
- NoOnset
- DurationTooShort
- DurationTooLong
- StrengthTooLow
- PeakActive
- Cooldown
- Overflow
- Unknown

PatternReason:
- FromAcceptedTransient
- DetectorRejected
- InsufficientEvidence
- AmbiguousEvidence
- UnsupportedPattern
- Unknown

Do not overbuild if the current code only needs a smaller set.

6. Logging

Logs should make clear:
- DetectorCandidate accepted/rejected and why
- PatternResult type and reason
- Analyzer SEQ category, if in Analyzer
- Behavior handling decision, if in Resonant

Avoid conflating:
- detector validity
- pattern validity
- analyzer trial classification
- behavior blocking

Expected output:
1. Concise Pass D implementation summary.
2. Files changed.
3. List of validity decisions found and where they now live.
4. DetectorCandidate validity model.
5. PatternResult validity model.
6. Analyzer SEQ classification model.
7. Runtime behavior validity model.
8. Evidence that Analyzer and Resonant share the same DetectorCandidate → PatternResult validity path.
9. Evidence detector baseline is unchanged.
10. Notes on any temporary compatibility shims.
11. Whether Pass E can begin.

Acceptance checklist:
[ ] Validity logic has been audited.
[ ] Detector-level validity is distinct from pattern-level validity.
[ ] Analyzer SEQ categories remain Analyzer-only.
[ ] Runtime behavior does not consume expected/early/late/duplicate/miss as behavior semantics.
[ ] DetectionPipeline owns shared DetectorCandidate → PatternResult validity.
[ ] Analyzer and Resonant do not duplicate candidate validity interpretation.
[ ] PatternResult preserves enough reason/timing/strength data for logs.
[ ] Rejected/invalid/ambiguous cases are represented clearly enough.
[ ] Detector parameters remain unchanged.
[ ] AudioSignal / AudioOnsetDetector candidate generation remains unchanged.
[ ] No advanced chirp/frequency/overlap logic is added.
[ ] Logs distinguish detector validity, pattern validity, Analyzer SEQ result, and behavior handling.

Definition of done:
Pass D is done when candidate validity and pattern validity are explicit, Analyzer and Resonant share the same DetectorCandidate → PatternResult interpretation, Analyzer-only trial classifications remain separate from runtime behavior semantics, and logs can show where a detection was accepted, rejected, classified, ignored, or used.
