Implement a minimal, firmware-selectable PCM de-accumulation step directly inside `AudioSourceI2s`.

## Scope

Apply preprocessing immediately after incoming I²S samples have been decoded into the normal PCM sample type, before they enter:

* `AudioSourceFrame`
* feature extraction
* feature history
* detectors
* inspectors
* Analyzer processing

Do not move this logic into downstream modules.

## Required design

Add an explicit preprocessing mode:

```cpp
enum class PcmPreprocessMode : uint8_t {
    None,
    FirstDifference
};
```

Add one firmware/default selection close to the existing I²S configuration:

```cpp
constexpr PcmPreprocessMode kPcmPreprocessMode =
    PcmPreprocessMode::FirstDifference;
```

Changing this to `None` must restore the original unchanged PCM path.

## Processing

For every decoded PCM sample, in true stream order:

```cpp
output = current - previous;
previous = current;
```

Requirements:

* previous-sample state persists across DMA/read-buffer boundaries
* use a wider intermediate type to avoid overflow
* clamp to the valid output PCM range if necessary
* initialize from the first valid incoming sample
* output `0` for the first `FirstDifference` sample to avoid a startup spike
* reset state whenever `AudioSourceI2s` is initialized, restarted, reconfigured, or stopped

Example shape:

```cpp
int32_t AudioSourceI2s::preprocessSample(int32_t current) {
    if (preprocessMode_ == PcmPreprocessMode::None) {
        return current;
    }

    if (!hasPreviousSample_) {
        previousSample_ = current;
        hasPreviousSample_ = true;
        return 0;
    }

    const int64_t diff =
        static_cast<int64_t>(current) -
        static_cast<int64_t>(previousSample_);

    previousSample_ = current;

    return clampToPcmRange(diff);
}
```

Adapt names and types to the actual codebase. Do not introduce unnecessary abstractions.

## Diagnostics

Preserve the distinction between:

* direct decoded I²S PCM before preprocessing
* processed PCM passed into `AudioSourceFrame`

`RAW mode=i2s` should continue to show preprocessed-input truth from the driver side.

`RAW mode=pcm` should show the processed PCM that actually enters the runtime pipeline.

Do not silently label processed samples as raw I²S samples.

## Non-goals

Do not add or modify:

* high-pass filters
* baseline subtraction
* baseline tracking
* startup compensation
* smoothing
* interpolation
* detector tuning
* inspector tuning
* Analyzer architecture
* runtime Param infrastructure
* I²S driver configuration

## Verification

Verify:

1. Build succeeds.
2. `None` reproduces the previous PCM stream exactly.
3. `FirstDifference` removes the slow accumulated drift.
4. No discontinuity appears at DMA/read boundaries.
5. No artificial first-sample spike appears.
6. The 3.2 kHz test tone remains visible and usable.
7. Downstream pipeline APIs remain unchanged.

Keep the patch small, explicit, local to `AudioSourceI2s`, and easy to remove later.
