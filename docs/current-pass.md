# Codex Instruction — Make Frequency Reject Diagnostics Actually Useful

## Goal

Replace the current broad/cumulative `SEQ_REJECTS` output with a **trial-local reject diagnostic** that explains why an expected emitted pulse did or did not become an `Occurrence`.

Status:

```text
implemented in code: compact trial-local `SEQ_FREQ_DIAG` gated by explicit `log=...+diag`
```

The current diagnostic is too ambiguous:

```text
SEQ_REJECTS trial=2 result=expected ...
freq_would=matched
freq_match=1
freq_state=rejected
freq_count=54944
freq_match_count=3438
freq_reject_count=51506

This is misleading because the same trial may have an accepted FrequencyMatch occurrence in SEQ_EXPLAIN. The line is currently mixing accepted-trial state, background/rejected frames, and cumulative counters.

The new diagnostic must answer:

For this trial window, why did FrequencyMatch emit or not emit an Occurrence?
Rename Output

Rename:

SEQ_REJECTS

to:

SEQ_FREQ_DIAG

or, if it prints only failure cases:

SEQ_FREQ_REJECT

Preferred for now:

SEQ_FREQ_DIAG

because it can print both accepted and missed trial windows.

Core Rule

Diagnostics must be trial-local and window-local.

Do not print cumulative run counters in per-trial diagnostics.

Reset the diagnostic accumulator at the start of each SEQ trial / expected window.

Track Two Separate Things

For each trial, track:

A. accepted occurrence, if one exists
B. rejected frequency evidence inside the expected window

Do not collapse these into the same fields.

Accepted occurrence fields
accepted_present
accepted_start_ms
accepted_peak_ms
accepted_release_ms
accepted_dt_ms
accepted_dur_ms
accepted_score_peak
accepted_contrast_peak
accepted_strength

If no occurrence was emitted:

accepted_present=0
Rejected / non-occurrence evidence fields
frames
valid_frames
score_ok_frames
contrast_ok_frames
both_ok_frames
match_frames
reject_frames

max_score
max_score_ms
max_contrast
max_contrast_ms
mean_score
mean_contrast

best_reject_reason
last_reject_reason
Useful Failure Classification

The diagnostic must classify the failure into one clear reason.

Add:

enum class FrequencyDiagReason {
    None,
    NoFrames,
    NoValidFrames,
    ScoreTooLow,
    ContrastTooLow,
    ScoreAndContrastTooLow,
    MatchTooShort,
    Suppressed,
    NotReady,
    GateClosed,
    TimingOutsideWindow,
    OccurrenceEmitted,
    Unknown,
};

For misses, best_reject_reason should usually be one of:

NoFrames
NoValidFrames
ScoreTooLow
ContrastTooLow
ScoreAndContrastTooLow
MatchTooShort
Suppressed
NotReady
GateClosed
TimingOutsideWindow

For expected trials with accepted occurrence:

best_reject_reason=occurrence_emitted

or:

best_reject_reason=none

Do not print freq_state=rejected when accepted_present=1 unless it is clearly labeled as background/context rejection.

Identify Near-Misses

Add a simple near-miss classification.

near_miss=0/1
near_miss_reason=...

Recommended logic:

near_miss=1 if:
  max_score >= 0.75 * score_threshold
  OR max_contrast >= 0.75 * contrast_threshold
  OR score_ok_frames > 0
  OR contrast_ok_frames > 0

This helps distinguish:

no useful tone seen

from:

almost enough frequency evidence, but gate did not open
Important: Score and Contrast Must Use Real Thresholds

Current output sometimes shows:

freq_score=2320.4/0.0
freq_contrast=220.16/0.00

That is not useful for reject analysis.

Print real thresholds:

score_threshold=10000.0
contrast_threshold=50.0

And use fields like:

max_score=...
score_threshold=...
max_contrast=...
contrast_threshold=...

Do not print /0.0 unless the threshold is actually disabled.

Suggested Output: Expected Trial
SEQ_FREQ_DIAG trial=12 result=expected
accepted_present=1 accepted_dt=33ms accepted_dur=98ms accepted_strength=25624.2
frames=1800 valid=1800 match_frames=120 reject_frames=1680
score_ok_frames=120 contrast_ok_frames=120 both_ok_frames=120
max_score=25624.2 score_threshold=10000.0
max_contrast=820.4 contrast_threshold=50.0
mean_score=1934.0 mean_contrast=229.5
best_reject_reason=occurrence_emitted near_miss=0
Suggested Output: Miss Trial
SEQ_FREQ_DIAG trial=34 result=miss
accepted_present=0
frames=1800 valid=1800 match_frames=0 reject_frames=1800
score_ok_frames=0 contrast_ok_frames=8 both_ok_frames=0
max_score=4200.0 score_threshold=10000.0
max_contrast=73.0 contrast_threshold=50.0
mean_score=1707.4 mean_contrast=223.3
best_reject_reason=score_too_low near_miss=1 near_miss_reason=contrast_ok_score_low

Another miss case:

SEQ_FREQ_DIAG trial=34 result=miss
accepted_present=0
frames=1800 valid=1800 match_frames=0 reject_frames=1800
score_ok_frames=0 contrast_ok_frames=0 both_ok_frames=0
max_score=1200.0 score_threshold=10000.0
max_contrast=12.0 contrast_threshold=50.0
mean_score=80.0 mean_contrast=1.4
best_reject_reason=score_and_contrast_too_low near_miss=0
Required Diagnostic Logic

For each live frequency frame inside the trial window:

diag.frames++;

if (frame.valid) {
    diag.validFrames++;
}

if (frame.score >= config.scoreThreshold) {
    diag.scoreOkFrames++;
}

if (frame.contrast >= config.contrastThreshold) {
    diag.contrastOkFrames++;
}

if (frame.score >= config.scoreThreshold &&
    frame.contrast >= config.contrastThreshold) {
    diag.bothOkFrames++;
}

if (frame.matched) {
    diag.matchFrames++;
}

if (!frame.matched) {
    diag.rejectFrames++;
    diag.lastRejectReason = classifyFrameReject(frame);
}

diag.maxScore = max(diag.maxScore, frame.score);
diag.maxContrast = max(diag.maxContrast, frame.contrast);
diag.sumScore += frame.score;
diag.sumContrast += frame.contrast;

At trial end:

diag.meanScore = diag.frames > 0 ? diag.sumScore / diag.frames : 0;
diag.meanContrast = diag.frames > 0 ? diag.sumContrast / diag.frames : 0;
diag.bestRejectReason = classifyTrialReject(diag, acceptedPresent);
diag.nearMiss = classifyNearMiss(diag);
Trial Reject Classification

Use this priority:

if accepted_present:
    OccurrenceEmitted

else if frames == 0:
    NoFrames

else if valid_frames == 0:
    NoValidFrames

else if gate_closed:
    GateClosed

else if not_ready:
    NotReady

else if suppressed:
    Suppressed

else if both_ok_frames > 0 but match_frames == 0:
    MatchTooShort

else if score_ok_frames == 0 and contrast_ok_frames == 0:
    ScoreAndContrastTooLow

else if score_ok_frames == 0:
    ScoreTooLow

else if contrast_ok_frames == 0:
    ContrastTooLow

else:
    Unknown
Print Policy

For active debugging:

diagnostics=1 → print SEQ_FREQ_DIAG for every trial

Later default:

diagnostics=0 → no SEQ_FREQ_DIAG
diagnostics=miss → print only for miss/late/duplicate/rejected

Do not print per-frame diagnostics.

Performance / Safety Rules

Do not reintroduce the diagnostic-collapse bug.

Avoid:

large snapshots
heap allocations per trial
large report buffers
per-frame Serial output
copying frame arrays
state mutation from diagnostic reads

Use:

small fixed-size struct
simple counters
sum/min/max
one line per trial
no dynamic allocation

Diagnostics must be passive:

Observe FrequencyMatch.
Do not influence FrequencyMatch.
Success Criteria
- Output renamed from SEQ_REJECTS to SEQ_FREQ_DIAG.
- Per-trial counters reset every trial.
- No cumulative counters in per-trial line.
- Expected trials clearly show accepted_present=1.
- Miss trials clearly show accepted_present=0 and a meaningful best_reject_reason.
- Real score/contrast thresholds are printed.
- Near-miss classification exists.
- No contradictory fields like freq_match=1 + freq_state=rejected without context.
- diagnostics=1 does not collapse detection rate.
- 100-trial diagnostics=1 run stays close to diagnostics=0 behavior.
