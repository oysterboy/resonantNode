Refactor PCM scaling into a permanent, format-derived signal contract. Do not normalize against an observed runtime peak such as `100000`.

## Goal

Establish one canonical PCM representation and one canonical feature-strength representation:

* `PcmSample`: signed `int32_t`, with a precisely defined valid range derived from the microphone’s decoded I²S bit depth.
* `Strength16`: unsigned `uint16_t`, range `0…32767`, used for amplitude-related detector, inspector, history-statistic, and report values.
* Frequency score may also use `0…32767`.
* Frequency contrast remains a separate ratio/quality unit and must not be treated as amplitude strength.

## 1. Define the decoded PCM contract

Inspect the actual microphone format and current I²S decoding.

Determine explicitly:

* source bit depth, likely 24 valid signed bits
* position of valid bits inside the 32-bit I²S word
* sign extension
* any current shifts applied during decoding
* whether de-accumulation can increase the theoretical range by one bit

Create one documented canonical type:

```cpp
using PcmSample = int32_t;
```

Define the theoretical limits from the real decoded format, not from measured captures.

Example for signed 24-bit source PCM:

```cpp
constexpr uint8_t kSourcePcmBits = 24;
constexpr int32_t kSourcePcmMax =
    (int32_t{1} << (kSourcePcmBits - 1)) - 1;
constexpr int32_t kSourcePcmMin =
    -(int32_t{1} << (kSourcePcmBits - 1));
```

If First Difference is enabled, calculate its theoretical intermediate range separately:

```cpp
using PcmIntermediate = int64_t;
```

Do not silently assume that the processed sample has the same theoretical range as the source sample.

Choose and document one permanent output policy for First Difference:

* either preserve the expanded processed range in `int32_t`
* or apply a fixed mathematically defined shift, for example divide by 2, so the processed signal remains inside the canonical PCM range

Prefer a fixed shift based on bit growth, not clipping based on observed audio.

Example:

```cpp
diff = current - previous;
processed = diff / 2;
```

Use the exact shift justified by the source bit-depth analysis.

## 2. Normalize amplitude from the canonical PCM range

Create one central conversion function:

```cpp
using Strength16 = uint16_t;

Strength16 pcmMagnitudeToStrength(PcmSample sample);
```

The mapping must be based on the documented canonical PCM maximum:

```cpp
strength =
    min(abs(sample), kCanonicalPcmMagnitudeMax)
    * 32767
    / kCanonicalPcmMagnitudeMax;
```

Requirements:

* use safe absolute-value handling for signed minimum
* use a wide intermediate type
* saturate only at the theoretical canonical maximum
* no observed-run constants
* no microphone-specific magic value such as `100000`
* no hidden `>> 8` normalization

## 3. Preserve signed waveform history efficiently

Do not store raw `int32_t` PCM history if RAM cost is unnecessary.

Store a mathematically scaled signed 16-bit waveform:

```cpp
using HistorySample = int16_t;
```

Convert using the same canonical PCM range:

```cpp
historySample =
    clamp(
        sample * 32767 / kCanonicalPcmMagnitudeMax,
        -32768,
        32767
    );
```

This preserves:

* waveform polarity
* relative amplitude
* Goertzel input shape
* RMS and window statistics
* predictable RAM usage

Rename the history or document clearly that it stores normalized signed PCM, not raw hardware PCM.

## 4. Separate units explicitly

Use distinct types or clearly named fields:

```cpp
PcmSample
HistorySample
Strength16
FrequencyScore16
FrequencyContrast
```

Do not mix:

* PCM counts
* normalized amplitude strength
* normalized frequency score
* frequency contrast ratio

Update field and tuning names where needed:

```text
amp_onset_strength
amp_release_strength
amp_min_peak_strength
freq_min_score
freq_min_contrast
```

## 5. Tuning values

After the permanent scaling contract is implemented:

* retune AMP thresholds in `Strength16` units
* retain frequency score in `0…32767`
* validate frequency contrast independently
* do not preserve old thresholds merely because they look familiar

Add temporary diagnostic output showing, for the same frame:

```text
pcm
pcm_abs
amp_strength
freq_score
freq_contrast
```

Use this only for validation, then keep or remove according to existing diagnostic architecture.

## 6. Static validation

Add compile-time or unit-level checks for known points:

```text
PCM 0                         → Strength 0
PCM half canonical magnitude  → approximately 16384
PCM canonical maximum         → 32767
negative and positive values  → equal magnitude strength
out-of-range intermediate     → safely saturated
```

Also verify:

* First Difference does not overflow
* state persists across DMA boundaries
* no artificial first-sample spike
* history conversion is symmetric
* `None` preprocessing preserves the original decoded PCM contract

## 7. Non-goals

Do not add:

* automatic gain control
* adaptive normalization
* run-dependent peak calibration
* dynamic microphone calibration
* percentile-based scaling
* baseline retuning
* detector logic changes beyond unit conversion
* new parameter infrastructure

## Final architecture rule

Raw and processed PCM scaling must be derived from the actual digital format and mathematically defined preprocessing gain.

Runtime measurements are for tuning thresholds, not for defining the numeric signal domain.
