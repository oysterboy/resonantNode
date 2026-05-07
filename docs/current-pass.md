# Codex Pass Notes — H3a Analyzer Log Modes

## Goal

Introduce selectable Analyzer log modes before expanding frequency classification logs.

The Analyzer currently mixes several responsibilities in one output stream:

```text
test runner
detector verifier
classification system
frequency evidence logger
debug console
```

H3a separates output by purpose so future H3/H4 tests are readable and grep-/CSV-friendly.

This is an **observability architecture pass**.

It must not change detector behavior, Analyzer classification logic, or ResonantBehavior.

---

## Why This Pass Exists

Upcoming work needs clean data for:

```text
expected_primary
duplicate
late
self_suppressed
unexpected_noise
```

and later:

```text
SEQ frequency sweeps
candidate-local frequency windows
frequency quality flags
```

Without log modes, those outputs will be buried in detector spam.

The goal is not “more logs”.

The goal is:

```text
select the kind of evidence being inspected
```

---

## H3a Scope

### Implement now

Add semantic Analyzer log modes / flags for:

```text
summary
trial
candidate
freq_class
raw_debug
```

Allow combinations, for example:

```text
summary + trial
summary + freq_class
candidate + raw_debug
freq_class only
```

### Do not implement yet

```text
do not add new frequency classification logic
do not add SEQ frequency sweeps
do not change detector thresholds
do not change PatternResult validity
do not change Analyzer classification decisions
do not change ResonantBehavior
do not add frequency gating
```

---

## Recommended Design

Use bit flags, not a single enum, because combinations are useful.

Example:

```cpp
enum AnalyzerLogFlags : uint32_t {
    ANALYZER_LOG_NONE       = 0,
    ANALYZER_LOG_SUMMARY    = 1 << 0,
    ANALYZER_LOG_TRIAL      = 1 << 1,
    ANALYZER_LOG_CANDIDATE  = 1 << 2,
    ANALYZER_LOG_FREQ_CLASS = 1 << 3,
    ANALYZER_LOG_RAW_DEBUG  = 1 << 4,
};
```

Helper:

```cpp
inline bool analyzerLogEnabled(uint32_t flags, AnalyzerLogFlags flag) {
    return (flags & static_cast<uint32_t>(flag)) != 0;
}
```

If plain C-style constants are easier in the current codebase, that is fine.

Keep names stable and readable.

---

## Suggested Default

Default Analyzer output should remain readable:

```text
summary + trial
```

Recommended default flags:

```cpp
static constexpr uint32_t DEFAULT_ANALYZER_LOG_FLAGS =
    ANALYZER_LOG_SUMMARY |
    ANALYZER_LOG_TRIAL;
```

For H3 frequency comparison later:

```text
summary + freq_class
```

For clean export-style analysis:

```text
freq_class only
```

For detector debugging:

```text
summary + candidate + raw_debug
```

---

## Modes

## 1. Summary Mode

Purpose:

```text
quick regression check after a run
```

Output one compact summary line after a run or sequence block.

Example:

```text
SEQ_SUMMARY trials=100 expected=63 late=17 early=1 misses=19 duplicates=35 unexpected=11 avg_strength=59.4 avg_dur=161.0
```

This mode should stay on by default.

---

## 2. Trial Mode

Purpose:

```text
normal tuning and per-trial overview
```

Output one compact line per trial.

Example:

```text
SEQ_TRIAL trial=42 result=expected dt_ms=154 dur_ms=132 strength=58.0 duplicates=1 late=0
```

This mode should stay on by default.

---

## 3. Candidate Mode

Purpose:

```text
understand detector candidates and Analyzer classification
```

Output one line per accepted/rejected/classified candidate.

Example:

```text
SEQ_CAND trial=42 class=expected_primary valid=1 type=ValidTransient reason=FromAcceptedTransient dt_ms=154 dur_ms=132 strength=58.0
```

This mode should be off by default.

---

## 4. Frequency Class Mode

Purpose:

```text
compare frequency evidence by candidate class
```

Output one line per classified candidate/result with frequency fields.

Example:

```text
SEQ_FREQ_CLASS trial=42 class=expected_primary valid=1 type=ValidTransient reason=FromAcceptedTransient dt_ms=154 dur_ms=132 strength=58.0 freq_present=1 freq_match=0 freq_hz=1800 freq_score=0.63 freq_conf=0.42 freq_contrast=0.31 freq_age_ms=12 freq_valid_window=1
```

This mode should be off by default for normal use.

It will become the main output mode for H3b.

---

## 5. Raw Debug Mode

Purpose:

```text
inspect noisy detector internals when something breaks
```

Examples:

```text
SEQ_RAW onset_seen=1
SEQ_RAW below_threshold_count=27
SEQ_RAW duration_too_short dur_ms=31
SEQ_RAW duration_too_long dur_ms=318
SEQ_RAW blocked_peak_active=1
SEQ_RAW release_debounce=1
```

This mode must be off by default.

Do not allow raw debug logs to appear in clean frequency classification output unless explicitly enabled.

---

## Config / Control

Use the simplest configuration mechanism that fits the current codebase.

Acceptable options:

### Option A — Compile-time constant

```cpp
static constexpr uint32_t ANALYZER_LOG_FLAGS =
    ANALYZER_LOG_SUMMARY |
    ANALYZER_LOG_TRIAL;
```

This is enough for H3a if runtime commands would complicate the pass.

### Option B — Serial command / runtime param

If there is already a parameter or command system, optionally allow:

```text
log summary
log trial
log candidate
log freq_class
log raw
log default
log quiet
```

But do not build a large command system just for H3a.

Recommendation:

```text
start with compile-time flags
add runtime switching later if useful
```

---

## Helper Functions

Create small helper functions instead of scattering `Serial.print()` checks everywhere.

Example shape:

```cpp
void logSeqSummary(...);
void logSeqTrial(...);
void logSeqCandidate(...);
void logSeqFreqClass(...);
void logSeqRawDebug(...);
```

Each helper should first check the active log flags.

Example:

```cpp
void logSeqTrial(...) {
    if (!analyzerLogEnabled(activeAnalyzerLogFlags, ANALYZER_LOG_TRIAL)) {
        return;
    }

    Serial.print("SEQ_TRIAL ");
    // key=value fields
}
```

Goal:

```text
Analyzer logic stays readable.
Log mode checks are centralized.
```

---

## Prefixes

Use stable prefixes so output can be filtered easily:

```text
SEQ_SUMMARY
SEQ_TRIAL
SEQ_CAND
SEQ_FREQ_CLASS
SEQ_RAW
```

Do not change these casually once introduced.

---

## Field Style

Use key-value logs:

```text
key=value key=value key=value
```

Avoid prose.

Good:

```text
SEQ_TRIAL trial=42 result=expected dt_ms=154 dur_ms=132 strength=58.0
```

Bad:

```text
Trial 42 was expected and had a duration of 132 ms
```

Reason:

```text
key-value logs are grep-friendly and CSV-convertible
```

---

## Relationship to H3b

H3a only creates the logging structure.

H3b will use `SEQ_FREQ_CLASS` for:

```text
expected_primary
duplicate
late
self_suppressed
unexpected_noise
```

H3a may create the prefix and helper, but it does not need to fully populate all H3b frequency comparison classes yet.

---

## Relationship to H4 SEQ Frequency Sweep

Later H4 will likely add logs such as:

```text
SEQ_FREQ_SWEEP
```

or extend `SEQ_FREQ_CLASS` with:

```text
emit_hz
emit_duration_ms
trial_interval_ms
sweep_index
```

H3a should make this easy by keeping log helpers and mode flags modular.

Do not implement sweep mode in H3a.

---

## Success Checks

### Compile

```text
Analyzer builds
Resonant builds if shared headers are touched
```

### Default readability

Default output should still show:

```text
SEQ_SUMMARY
SEQ_TRIAL
```

and should not show raw debug spam.

### Mode isolation

With only frequency-class mode enabled later, output should be possible without raw debug noise.

### No behavior change

These must remain unchanged:

```text
DetectorCandidate creation
PatternResult validity
Analyzer timing classification
SEQ trial logic
ResonantBehavior response logic
```

---

## Explicit Non-Goals

```text
do not tune detector params
do not change AudioSignal math
do not change AudioFrequencyDetector behavior
do not change candidate classification logic
do not make frequency required
do not add behavior gating
do not add ValidTone or ValidChirp behavior
do not add SEQ frequency sweep yet
do not add candidate-local frequency windows yet
```

---

## Commit Message Suggestion

```text
H3a add analyzer log modes
```

or:

```text
Add semantic Analyzer logging flags
```

---

## Compact Codex Instruction

```text
Implement Pass H3a: Analyzer Log Modes.

Add semantic Analyzer log flags/modes so Analyzer output can be selected by purpose. Required modes are: summary, trial, candidate, freq_class, raw_debug. Prefer bit flags so modes can be combined.

Default output should remain readable and include summary + trial only. Raw debug must be off by default.

Use stable log prefixes:
SEQ_SUMMARY
SEQ_TRIAL
SEQ_CAND
SEQ_FREQ_CLASS
SEQ_RAW

Use key-value log fields, not prose.

Create small log helper functions where practical so Analyzer logic does not become cluttered with Serial.print checks.

Do not change detector behavior, PatternResult validity, Analyzer classification logic, SEQ timing, ResonantBehavior, frequency evidence semantics, or detector parameters.

Do not implement frequency classification comparison or SEQ frequency sweep yet. This pass only creates the logging mode structure that H3b and H4 will use.
```
