# Pass: Wiring / MEMS Dropout Diagnostics

## Context

We are investigating movement-triggered miss streaks in the ResonantNode / Resonanzraum detection path.

Current known fact:

```text
Strike 1: it is not a full ESP32 reboot.
```

Therefore, do not focus on brownout / watchdog / reboot as the primary explanation in this pass.

The next diagnostic target is:

```text
ESP32 stays alive
  ↓
Is I2S still returning data?
  ↓
Is raw MEMS audio still valid?
  ↓
Is frequency evidence still being produced?
  ↓
Is the detector rejecting because evidence is absent / weak / too short?
```

This pass is diagnostic-first. Do not change detection behavior except where explicitly marked as optional recovery handling.

---

## Goal

When a 10–20 second miss streak happens after cable/movement, the logs must classify the failure as one of:

```text
NO_I2S_DATA
FLATLINE_AUDIO
INVALID_AUDIO
LOW_COVERAGE
STALE_AUDIO
FREQ_NO_UPDATES
FREQ_SCORE_LOW
FREQ_TOO_SHORT
DETECTOR_REJECTED
NORMAL_AUDIO_BUT_NO_TONE
```

The Analyzer should stop reporting all of these as generic `miss`.

---

## Non-goals

Do not tune thresholds in this pass.

Do not redesign AMP support in this pass.

Do not change Behavior/RB suppression/refractory logic.

Do not treat this as an acoustic-distance / AMP deviation pass.

Do not assume reboot/brownout unless `uptime_ms` proves it.

---

## Pass A — Minimal system continuity proof

### Task

Add a lightweight `SYSTEM_HEALTH` diagnostic line.

Print every 500 ms during SEQ / Analyzer runs.

### Fields

```text
SYSTEM_HEALTH
t_ms=...
uptime_ms=...
boot_count=...
reset_reason=...
loop_max_us=...
free_heap=...
```

### Requirements

`uptime_ms` must be monotonic during the miss streak.

If `uptime_ms` resets, classify separately as real reboot. But current working assumption is that it does not reset.

### Acceptance

During a movement-triggered miss streak, logs show:

```text
SYSTEM_HEALTH ... uptime_ms continues increasing
```

Then full ESP32 reboot is ruled out.

---

## Pass B — I2S input health

### Task

Add `AUDIO_IO_HEALTH`.

Print every 500 ms during SEQ / Analyzer runs.

### Fields

```text
AUDIO_IO_HEALTH
t_ms=...
i2s_reads=...
i2s_bytes_read=...
processed_samples=...
expected_samples=...
processed_ratio=...
frames_emitted=...
partial_frames=...
duplicate_frames=...
gap_samples=...
overlap_samples=...
max_block_age_ms=...
max_processing_lag_ms=...
```

### Meaning

If `i2s_bytes_read` drops to zero or near-zero during the dead period:

```text
audio_state=NO_I2S_DATA
```

If `processed_ratio` drops badly:

```text
audio_state=LOW_COVERAGE
```

If frame invariants break:

```text
audio_state=INVALID_FRAME_STREAM
```

### Required invariants

```text
partial_frames == 0
duplicate_frames == 0
unexpected_overlap == 0
```

If overlap is intentional, log it explicitly as intentional. Otherwise treat as a diagnostic failure.

### Acceptance

During a dead period, we can answer:

```text
Is I2S still returning bytes? yes/no
Is the frame stream still valid? yes/no
```

---

## Pass C — Raw MEMS audio health

### Task

Add raw-audio statistics from the audio frames.

Print every 500 ms during SEQ / Analyzer runs.

### Diagnostic line

```text
RAW_AUDIO_HEALTH
t_ms=...
frames=...
min_sample=...
max_sample=...
dc_mean=...
mean_abs=...
max_abs=...
zero_ratio=...
clip_count=...
flatline=0|1
```

### Detection rules

Use conservative thresholds first. The goal is not perfect classification; the goal is visibility.

Suggested states:

```text
FLATLINE_AUDIO
- max_abs below tiny threshold for > 250 ms

CLIPPING_AUDIO
- clip_count above threshold

DC_STUCK_AUDIO
- dc_mean large and mean_abs very small, or sample range tiny around a non-zero DC value

RAW_AUDIO_OK
- max_abs / mean_abs plausible and not flatline
```

### Interpretation

```text
I2S bytes normal + flatline=1
→ MEMS output/contact/power/data dropout

I2S bytes normal + clipping/noise burst
→ cable/contact instability or bad data line

I2S bytes normal + raw audio normal
→ not a raw MEMS dropout; continue to frequency diagnostics
```

### Acceptance

During a dead period, we can answer:

```text
Is the MEMS producing plausible changing audio? yes/no
```

---

## Pass D — AudioHealthState

### Task

Introduce a small diagnostic-only state enum.

### Suggested enum

```cpp
enum class AudioHealthState {
    OK,
    NoI2SData,
    LowCoverage,
    InvalidFrameStream,
    FlatlineAudio,
    DcStuckAudio,
    ClippingAudio,
    StaleAudio,
    Unknown
};
```

### Priority order

When multiple problems are present, classify in this order:

```text
NoI2SData
InvalidFrameStream
LowCoverage
FlatlineAudio
DcStuckAudio
ClippingAudio
StaleAudio
OK
Unknown
```

### Diagnostic line

When state changes, print:

```text
AUDIO_HEALTH_CHANGE
t_ms=...
from=...
to=...
reason=...
```

When a bad state recovers, print:

```text
AUDIO_RECOVERED
t_ms=...
previous_state=...
duration_ms=...
```

### Acceptance

A 20 second dead period produces a clear start/recovery pair if audio health is abnormal:

```text
AUDIO_HEALTH_CHANGE ... to=FlatlineAudio
AUDIO_RECOVERED ... duration_ms=20000
```

---

## Pass E — Frequency source health

### Task

Add `FREQ_HEALTH` to show whether frequency evidence exists during the dead period.

Print every 500 ms during SEQ / Analyzer runs.

### Fields

```text
FREQ_HEALTH
t_ms=...
target_hz=...
target_generation=...
window_samples=...
window_ms=...
update_step_ms=...
fresh_updates=...
latest_age_ms=...
score_peak_recent=...
contrast_peak_recent=...
score_ok_recent=...
contrast_ok_recent=...
matched_ms_recent=...
```

### Interpretation

```text
fresh_updates stop
→ frequency extractor not producing fresh evidence

fresh_updates continue but score_peak_recent low
→ audio exists, but target frequency is absent / weak / shifted

score_peak_recent ok but matched_ms_recent too short
→ detector gate / release / min-duration issue

score and contrast ok, but no candidate
→ detector lifecycle bug or expected-window mismatch
```

### Acceptance

During a miss streak, we can answer:

```text
Is frequency evidence missing, weak, or too short?
```

---

## Pass F — FrequencyMatch rejected candidate summary

### Task

Carry FrequencyMatch failure reasons out of the source/detector.

Do not only report `no candidate`.

### Required reject reasons

```text
NoFreshFrequencyFrames
ScoreBelowThreshold
ContrastBelowThreshold
TooShort
ReleasedBeforeMinDuration
TooLong
OutsideExpectedWindow
TargetGenerationChanged
LowCoverage
AudioInvalid
Unknown
```

### Suggested rejected summary

```cpp
struct FrequencyRejectSummary {
    bool present = false;

    FrequencyRejectReason reason = FrequencyRejectReason::Unknown;

    uint32_t openMs = 0;
    uint32_t peakMs = 0;
    uint32_t closeMs = 0;
    uint32_t durationMs = 0;

    float scorePeak = 0.0f;
    float contrastPeak = 0.0f;

    uint32_t matchedMs = 0;
    uint32_t requiredMs = 0;

    uint16_t freshUpdates = 0;
    uint16_t scoreOkUpdates = 0;
    uint16_t contrastOkUpdates = 0;
    uint16_t matchedUpdates = 0;
};
```

### Acceptance

A failed expected event can distinguish:

```text
no frequency data
score low
contrast low
too short
audio invalid
```

---

## Pass G — Carry health + reject reason into SEQ_INSPECT

### Task

Update `SEQ_INSPECT` so each trial includes source failure class, audio state, and frequency reject reason.

### Example: raw audio dropout

```text
SEQ_INSPECT
trial=42
result=miss
source=freq
candidate=none
audio_state=FlatlineAudio
source_failure=audio_invalid
freq_reject=AudioInvalid
reason=no_frequency_candidate_because_audio_invalid
```

### Example: frequency evidence too short

```text
SEQ_INSPECT
trial=43
result=miss
source=freq
candidate=rejected
audio_state=OK
source_failure=freq_too_short
freq_reject=TooShort
score_peak=...
contrast_peak=...
matched_ms=18
required_ms=30
reason=frequency_match_too_short
```

### Example: normal audio, no target tone

```text
SEQ_INSPECT
trial=44
result=miss
source=freq
candidate=none
audio_state=OK
source_failure=no_target_frequency
freq_reject=ScoreBelowThreshold
score_peak=...
contrast_peak=...
reason=score_below_threshold
```

### Acceptance

Generic miss output is no longer acceptable for these cases:

```text
audio invalid
frequency score absent
frequency score weak
frequency match too short
```

---

## Pass H — Optional recovery behavior

Only after diagnostics prove a real stuck I2S / MEMS stream.

### Optional behavior

If bad audio state persists for more than a threshold:

```text
NoI2SData > 1000 ms
FlatlineAudio > 2000 ms
InvalidFrameStream > 1000 ms
```

then log:

```text
AUDIO_RECOVERY_ATTEMPT
state=...
action=flush_i2s
```

Possible recovery actions:

```text
flush I2S DMA buffers
reset internal frame assembler
clear stale feature buffers
optionally reinitialize I2S driver
```

### Important

Recovery must be explicitly logged.

Do not hide recovery inside normal detection.

Do not reset detector state silently in a way that makes Analyzer truth ambiguous.

### Acceptance

If recovery is implemented, logs show:

```text
AUDIO_HEALTH_CHANGE
AUDIO_RECOVERY_ATTEMPT
AUDIO_RECOVERED
```

---

## Pass I — Test protocol

### Test 1: stable no-touch run

Goal:

```text
Baseline health logs under stable conditions.
```

Expected:

```text
AudioHealthState=OK
processed_ratio near 1.0
partial_frames=0
duplicate_frames=0
raw audio not flat
freq fresh updates present
normal candidate acceptance
```

### Test 2: gentle cable movement

Goal:

```text
Trigger the known miss streak.
```

Record:

```text
Does I2S byte count continue?
Does raw audio flatline or clip?
Do frequency updates continue?
What reject reason appears?
Does AudioHealthState recover?
```

### Test 3: intentional mic occlusion / acoustic weakening

Goal:

```text
Differentiate acoustic weak signal from wiring dropout.
```

Expected:

```text
audio_state=OK
raw audio still plausible
freq score low or matched_ms too short
not NoI2SData / FlatlineAudio
```

### Test 4: unplug / disturb MEMS line if safe

Goal:

```text
Capture known bad electrical signature.
```

Expected:

```text
NoI2SData or FlatlineAudio or ClippingAudio
```

Use only if safe for hardware.

---

## Success criteria

This pass succeeds when one movement-triggered 10–20 second miss streak can be classified as one of:

```text
NoI2SData
FlatlineAudio
DcStuckAudio
ClippingAudio
LowCoverage
FreqNoUpdates
ScoreLow
TooShort
```

And the trial output says why:

```text
not generic miss
not behavior suppression
not reboot
```

---

## Final architecture implication

The detection chain should expose physical/wiring failures as input health states:

```text
I2S / MEMS / Raw Audio Health
  ↓
Frequency Evidence Health
  ↓
FrequencyMatch Detector Reject Reason
  ↓
SEQ_INSPECT Failure Class
```

Software should not pretend wiring dropouts are acoustic misses.
