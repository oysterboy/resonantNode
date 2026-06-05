# Roadmap: Memory Optimization

Project: ResonantNode / Resonanzraum Detection Refactor  
Scope: `DetectionRuntime` allocation failure, runtime heap hygiene, and bounded data ownership  
Current failure:

```txt
RUNTIME_SIZE detection_runtime_bytes=104896
HEAP_STATUS when=begin_before_runtime_alloc free_8bit=146300 largest_8bit=90100 free_internal=216656
ERR MEMERROR reason=detection_runtime_alloc_failed
```

## Diagnosis

`DetectionRuntime` is currently **104,896 bytes** and is allocated with `new`. The largest available contiguous 8-bit heap block at boot is only **90,100 bytes**, so allocation fails even though total free heap is larger.

This is not primarily a total-memory problem. It is a **large contiguous heap allocation problem** plus a **data-layout/lifetime problem**.

Core issue:

```txt
DetectionRuntime object > largest allocatable heap block
```

Secondary issues:

- `FeatureHistory` is the dominant memory consumer.
- `FeatureHistory` currently stores a full history for `FeatureStreamId::Unknown`.
- `FeatureHistory::getWindow()` uses runtime `malloc()` for quantile/trimmed-mean calculation.
- Detection/result/pattern objects duplicate data across stages.
- Current queues are sized for more complexity than the active TonalPulse / SinglePulse profile needs.
- Large permanent runtime objects are heap-owned instead of statically/module-owned.

## Memory Architecture Rule

Detection data must be **explicitly owned, bounded, and lifetime-scoped**.

Rules:

1. No heap allocation in audio / detection / analyzer runtime paths.
2. Large permanent runtime objects are statically owned.
3. Feature histories are fixed-size buffers owned by `DetectionRuntime` or feature modules.
4. Occurrence candidates are source-owned active slots.
5. Closed occurrences enter small fixed pools.
6. Pattern candidates reference occurrences by id/handle, not by copying full objects.
7. Analyzer stores compact summaries and selected ids, not full duplicated object graphs.
8. Pool overflow must be reported explicitly; never fallback to heap.

---

# NOW Passes

These are the immediate fixes for the current boot failure.

## Implementation Status

```text
M1 DONE - DetectionRuntime now has direct ownership.
M2 DONE - FeatureHistory stores only real streams.
M3 DONE - FeatureBin sumSquares reduced to float.
M4 DONE - FeatureHistory::getWindow() no longer allocates heap scratch.
M5 DONE - Runtime and analyzer queues trimmed.
M6 DONE - Memory inventory diagnostics now print the ownership tree.
```

## Pass M1 — Stop Heap-Allocating `DetectionRuntime`

### Problem

Current shape:

```cpp
detection::DetectionRuntime* _detection = nullptr;
_detection = new (std::nothrow) detection::DetectionRuntime();
```

This requests one contiguous heap block of ~105 KB and fails.

### Target

Use direct/static ownership.

In `AnalyzerApp.h`, replace:

```cpp
detection::DetectionRuntime* _detection = nullptr;
```

with:

```cpp
detection::DetectionRuntime _detection;
```

Update calls from:

```cpp
_detection->resetState();
_detection->setFrequencyMatchConfig(...);
```

to:

```cpp
_detection.resetState();
_detection.setFrequencyMatchConfig(...);
```

Remove the `new (std::nothrow)` block and pointer-only null checks.

### Success

- No `detection_runtime_alloc_failed` boot error.
- `DetectionRuntime` is no longer allocated from heap.
- Boot still prints `RUNTIME_SIZE detection_runtime_bytes=...`.

### Commit

Commit after this pass.

---

## Pass M2 — Remove `Unknown` from `FeatureHistory` Storage

### Problem

Current stream count likely uses enum ordinal count:

```cpp
static constexpr size_t kStreamCount =
    static_cast<size_t>(FeatureStreamId::FrequencyContrast) + 1U;
```

Given:

```cpp
enum class FeatureStreamId {
    Unknown,
    AmpEnvelope,
    FrequencyScore,
    FrequencyContrast,
};
```

This allocates **4 streams**, including `Unknown`.

But only 3 streams are real:

```txt
AmpEnvelope
FrequencyScore
FrequencyContrast
```

With 512 bins and large `FeatureBin`s, `Unknown` wastes roughly 18–20 KB.

### Target

Use explicit stream mapping.

```cpp
static constexpr size_t kStreamCount = 3;

static bool isSupportedStream(FeatureStreamId stream) {
    return stream == FeatureStreamId::AmpEnvelope ||
           stream == FeatureStreamId::FrequencyScore ||
           stream == FeatureStreamId::FrequencyContrast;
}

static size_t streamIndex(FeatureStreamId stream) {
    switch (stream) {
        case FeatureStreamId::AmpEnvelope:
            return 0;
        case FeatureStreamId::FrequencyScore:
            return 1;
        case FeatureStreamId::FrequencyContrast:
            return 2;
        case FeatureStreamId::Unknown:
        default:
            return 0; // only reached after support check failed elsewhere
    }
}
```

All public methods must reject `Unknown` before indexing.

### Success

- `FeatureHistory` no longer allocates storage for `Unknown`.
- `sizeof(DetectionRuntime)` drops significantly.
- Existing Amp/FrequencyScore/FrequencyContrast behavior stays unchanged.

### Commit

Commit after this pass.

---

## Pass M3 — Reduce `FeatureBin` Size

### Problem

Current `FeatureBin` is large. It likely contains:

```cpp
struct FeatureBin {
    unsigned long timeMs;
    float first;
    float last;
    float min;
    float max;
    float sum;
    double sumSquares;
    size_t count;
};
```

`double sumSquares` is expensive and probably unnecessary for embedded diagnostics.

### Target

Change:

```cpp
double sumSquares = 0.0;
```

to:

```cpp
float sumSquares = 0.0f;
```

Keep RMS calculation correct enough:

```cpp
float rms = sqrtf(sumSquares / count);
```

or cast only at final calculation if needed.

### Success

- `sizeof(FeatureBin)` decreases.
- `sizeof(FeatureHistory)` decreases.
- Window stats remain stable enough for diagnostics and inspector support.

### Commit

Commit after this pass.

---

## Pass M4 — Remove Runtime `malloc()` from `FeatureHistory::getWindow()`

### Problem

`FeatureHistory::getWindow()` uses temporary heap allocation for quantile/median/trimmed mean calculation:

```cpp
float* values = static_cast<float*>(malloc(sizeof(float) * valueCount));
...
free(values);
```

This is dangerous in runtime inspector paths.

### Target

Remove heap allocation from `FeatureHistory::getWindow()`.

For now, replace quantile-derived values with cheap approximations:

```txt
median      = mean
p75         = peak or mean
p90         = peak
trimmedMean = mean
```

Keep primary stats:

```txt
mean
rms
min
max
peak
sampleCount
coverage
```

Do not introduce `std::vector`, `new`, `malloc`, `String`, or heap-backed containers.

### Success

- No `malloc/free` inside `FeatureHistory` runtime path.
- Inspector windows still produce mean/rms/min/max/peak/count/coverage.
- No behavior change expected for TonalPulse if quantiles are not primary gates.

### Commit

Commit after this pass.

---

## Pass M5 — Reduce Current Queue Capacities

### Problem

Current queues are sized larger than needed for the active TonalPulse / SinglePulse profile.

Likely current values:

```cpp
DetectionRuntime::kResultQueueCapacity = 8;
PatternAssembler::kQueueCapacity = 8;
PatternAssembler::kRecentOccurrenceCapacity = 4;
```

Current profile does not need dense multi-pattern buffering.

### Target

Reduce for the current pass:

```cpp
DetectionRuntime::kResultQueueCapacity = 2;
PatternAssembler::kQueueCapacity = 2;
PatternAssembler::kRecentOccurrenceCapacity = 2;
```

Add comments:

```cpp
// Current TonalPulse/SinglePulse profile only needs a small queue.
// Larger capacities belong to later dense/multi-pattern profiles.
```

### Success

- Reduced `DetectionRuntime` size.
- No normal TonalPulse run loses valid primary results.
- If overflow happens, it is logged clearly instead of silently allocating or dropping without reason.

### Commit

Commit after this pass.

---

## Pass M6 — Add Memory Inventory Diagnostics

### Problem

We need stable visibility into where memory goes.

### Target

At boot, print sizes for major structures as an ownership tree.

Top-level entries are the owning objects.
If a structure owns nested buffers or sub-objects, list those as indented sub-items under the owner instead of as peers.
Examples:

```text
DetectionRuntime
  FeatureHistory
  PatternAssembler queue
  PatternResult queue

AudioSignal
  RawSampleHistory

AnalyzerApp::SequenceTest
  CurveSnapshot sampleHistory
  CurveSnapshot sampleHistoryPending
  CurveSnapshot sampleRows
```

Print these ownership-tree sizes:

```cpp
Serial.printf("SIZE DetectionRuntime=%u\n", sizeof(detection::DetectionRuntime));
Serial.printf("  SIZE FeatureHistory=%u\n", sizeof(detection::FeatureHistory));
Serial.printf("    SIZE FeatureBin=%u\n", sizeof(detection::FeatureHistory::FeatureBin));
Serial.printf("  SIZE Occurrence=%u\n", sizeof(detection::Occurrence));
Serial.printf("  SIZE InspectedOccurrence=%u\n", sizeof(detection::InspectedOccurrence));
Serial.printf("  SIZE PatternAssembler=%u\n", sizeof(detection::PatternAssembler));
Serial.printf("  SIZE PatternCandidate=%u\n", sizeof(detection::PatternCandidate));
Serial.printf("  SIZE PatternResult=%u\n", sizeof(detection::PatternResult));
Serial.printf("SIZE AudioSignal=%u\n", sizeof(AudioSignal));
Serial.printf("  SIZE RawSampleHistory=%u\n", sizeof(RawSampleHistory));
Serial.printf("SIZE AnalyzerApp::SequenceTest=%u\n", sizeof(AnalyzerApp::SequenceTest));
Serial.printf("  SIZE CurveSnapshot sampleHistory=%u\n", sizeof(CurveSnapshot));
Serial.printf("  SIZE CurveSnapshot sampleHistoryPending=%u\n", sizeof(CurveSnapshot));
Serial.printf("  SIZE CurveSnapshot sampleRows=%u\n", sizeof(CurveSnapshot));
Serial.printf("SIZE AnalyzerReport=%u\n", sizeof(AnalyzerReport));
```

If nested types are private, add explicit debug helpers instead of making core internals public unnecessarily.

Also print heap caps:

```cpp
heap_caps_get_free_size(MALLOC_CAP_8BIT)
heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)
heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT)

heap_caps_get_free_size(MALLOC_CAP_INTERNAL)
heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)
heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL)

heap_caps_get_free_size(MALLOC_CAP_DMA)
heap_caps_get_largest_free_block(MALLOC_CAP_DMA)
heap_caps_get_minimum_free_size(MALLOC_CAP_DMA)
```

### Success

Boot log shows:

```txt
MEMORY_INVENTORY
SIZE DetectionRuntime=...
  SIZE FeatureHistory=...
    SIZE FeatureBin=...
  SIZE Occurrence=...
  SIZE InspectedOccurrence=...
  SIZE PatternAssembler=...
  SIZE PatternCandidate=...
  SIZE PatternResult=...
SIZE AudioSignal=...
  SIZE RawSampleHistory=...
SIZE AnalyzerApp::SequenceTest=...
  SIZE CurveSnapshot sampleHistory=...
  SIZE CurveSnapshot sampleHistoryPending=...
  SIZE CurveSnapshot sampleRows=...
SIZE AnalyzerReport=...
...
HEAP_CAPS cap=8BIT free=... largest=... min=...
HEAP_CAPS cap=INTERNAL free=... largest=... min=...
HEAP_CAPS cap=DMA free=... largest=... min=...
```

Private nested types stay private; the analyzer uses explicit debug helpers to print the inventory without widening core ownership APIs.

### Commit

Commit after this pass.

---

# MUST NOT Rules for NOW

Do not do these in the immediate memory-fix pass:

- Do not add fallback heap allocations.
- Do not move large arrays blindly into other heap regions.
- Do not introduce `std::vector`, `std::string`, `String`, `std::function`, `new`, `malloc`, or `free` in detection/analyzer runtime paths.
- Do not refactor behavior architecture during this pass.
- Do not optimize by deleting diagnostics blindly; first measure object sizes.
- Do not silently drop candidates/results on overflow.

---

# LATER Passes

These are important but should happen after the firmware boots reliably again.

## Pass L1 — Produce a Memory Map Table

Create a markdown table from actual `sizeof` logs.

Template:

```md
| Owner | Object | Count | Size each | Total | Lifetime | Placement | Notes |
|---|---:|---:|---:|---:|---|---|---|
| DetectionRuntime | FeatureHistory | 1 | ? | ? | permanent | static | 3 streams |
| DetectionRuntime | Occurrence pool | ? | ? | ? | rolling | static pool | accepted only |
| DetectionRuntime | Pattern queue | ? | ? | ? | rolling | static pool | current TonalPulse capacity |
| Analyzer | AnalyzerReport | 1 | ? | ? | per trial | static | compact report |
```

Success:

- Every major buffer has an owner.
- Every major buffer has a lifetime.
- Every major buffer has a placement decision.

---

## Pass L2 — Add Pool High-Water Diagnostics

For each fixed pool/queue, track:

```txt
capacity
current_count
high_water_count
overflow_count
last_overflow_reason
```

Example log:

```txt
POOL_STATUS name=PatternResult capacity=2 current=0 high_water=1 overflow=0
```

On overflow:

```txt
POOL_OVERFLOW name=PatternResult capacity=2 action=drop_oldest reason=too_many_results
```

Success:

- Pool pressure is visible without increasing memory.
- Dense field / duplicate cases can be diagnosed.

---

## Pass L3 — Replace Full Object Copies with Handles

### Problem

`PatternCandidate`, `PatternResult`, `AnalyzerReport`, and pipeline snapshots may copy full occurrences / inspected occurrences / frequency frames.

### Target

Use small ids/handles where possible.

Instead of:

```cpp
struct PatternCandidate {
    InspectedOccurrence occurrences[4];
};
```

Use:

```cpp
using OccurrenceId = uint8_t;

struct PatternCandidate {
    OccurrenceId occurrenceIds[4];
    uint8_t occurrenceCount;
    uint32_t startMs;
    uint32_t endMs;
    float confidence;
};
```

Instead of copying full selected chains into Analyzer, store:

```cpp
struct AnalyzerStageSelection {
    uint8_t occurrenceId;
    uint8_t patternId;
    AnalyzerStage failedStage;
    AnalyzerReason reason;
};
```

Success:

- Pattern and analyzer memory drops.
- Selected chain remains explainable.
- No full duplicated object graph across stages.

---

## Pass L4 — Compact Inspector Evidence

### Problem

Inspector evidence can become large if it stores too many per-window details.

### Target

Evidence should store summaries, not raw windows.

Preferred shape:

```cpp
struct ScalarStrengthEvidence {
    float mean;
    float rms;
    float peak;
    float lift;
    uint16_t windowMs;
    uint8_t coverageRatioPct;
    uint8_t strengthClass;
    bool valid;
};
```

Avoid:

```cpp
float windowValues[128];
```

Success:

- Evidence is compact and stable.
- Analyzer still has enough facts to explain inspect-stage decisions.

---

## Pass L5 — Re-evaluate FeatureHistory Length

### Problem

`512` bins may be larger than needed for all streams.

### Target

Choose history length from actual maximum window needs:

```txt
required_bins = max_window_ms / feature_update_ms + margin
```

For example:

```txt
AMP: dense updates, may need more bins
FrequencyScore: fresh updates only, maybe fewer bins
FrequencyContrast: fresh updates only, maybe fewer bins
```

Do not assume all streams need the same length.

Possible later shape:

```cpp
FeatureHistory<AmpEnvelope, 512>
FeatureHistory<FrequencyScore, 128>
FeatureHistory<FrequencyContrast, 128>
```

or separate stream buffers with different capacities.

Success:

- Feature history memory reflects real retrospective window needs.
- Frequency history does not pretend held/stale values are fresh evidence.

---

## Pass L6 — Bit-Pack Validity / Reduce Padding

### Problem

`FeatureBin` may waste memory through padding and boolean/count storage.

### Target options:

- Use `uint16_t` or `uint32_t` for count where appropriate instead of `size_t`.
- Store valid flags separately as a bitset if needed.
- Consider struct-of-arrays for hot histories:

```cpp
float values[CAPACITY];
uint32_t timestamps[CAPACITY];
uint16_t counts[CAPACITY];
```

instead of one large padded struct array.

Success:

- `sizeof(FeatureBin)` is justified.
- `sizeof(FeatureHistory)` is substantially lower.

---

# V2 / Future Passes

These belong after the memory baseline is stable.

## Pass V2.1 — Profile-Specific Memory Budgets

Different profiles get different capacities.

Example:

```txt
TonalPulse profile:
- result queue: 2
- recent occurrences: 2
- pattern candidates: 2

ChirpExperimental profile:
- result queue: 4
- recent occurrences: 8
- pattern candidates: 4

DenseField future profile:
- larger pools, explicitly budgeted
```

Rule:

> Profile capacity is explicit configuration, not accidental global bloat.

---

## Pass V2.2 — Analyzer Snapshot Policy

Analyzer should not copy full runtime objects by default.

Define snapshot levels:

```txt
SEQ_TRIAL:
  final classification + selected pattern summary + first failed stage

SEQ_INSPECT:
  selected occurrence + compact evidence summaries

SEQ_EXPLAIN:
  selected source → inspect → pattern → trial chain
  plus small reject summaries

RAW_SAMPLE_CAPTURE:
  separate diagnostic path only
```

Rule:

> Analyzer explains the selected chain. It does not archive the full runtime world.

---

## Pass V2.3 — Optional PSRAM Policy

If PSRAM exists, use it only for non-realtime diagnostics:

Allowed PSRAM candidates:

```txt
RAW_SAMPLE_CAPTURE buffers
large debug dump buffers
offline analysis snapshots
```

Avoid PSRAM for:

```txt
audio frame processing
candidate lifecycle
FeatureHistory used by inspectors
PatternAssembler hot path
Behavior timing state
```

Rule:

> PSRAM is for optional diagnostics, not for hiding core runtime bloat.

---

# Expected End State

After NOW passes:

```txt
- Firmware boots without DetectionRuntime allocation failure.
- DetectionRuntime is statically/module-owned.
- FeatureHistory stores only real streams.
- FeatureBin is smaller.
- FeatureHistory window evaluation does not allocate heap.
- Current queues are sized for TonalPulse / SinglePulse.
- Boot logs show object sizes and heap capability status.
```

After LATER passes:

```txt
- Full memory map exists.
- Pools report high-water and overflow.
- Pattern/analyzer stages use handles instead of full copies.
- Inspector evidence is compact.
- FeatureHistory length and layout match real timing needs.
```

After V2 passes:

```txt
- Profiles declare explicit memory budgets.
- Analyzer snapshot levels are clear and bounded.
- PSRAM, if used, is limited to optional diagnostics.
```

# Codex Safety Contract

For all memory work:

```txt
No silent fallback allocation.
No runtime heap allocation in detection/analyzer loop paths.
No heap-backed STL containers in core runtime paths.
No full-object duplication unless justified by memory map.
No hidden profile-wide capacity increases.
Every worked item must compile and be committed before moving on.
```
