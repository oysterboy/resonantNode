# Normalization

## Goal

Establish one clear numeric contract for PCM, amplitude strength, and frequency score.

This pass must **replace incorrect existing scaling**, remove redundant helpers, and reduce wrapper layers. Do not add a second normalization path beside the current one.

## Core principles

1. Raw PCM keeps its full valid dynamic range.
2. AMP strength and frequency score are separate derived feature domains.
3. Both derived domains use `uint16_t` values in `0…32767`.
4. Frequency score is amplitude-like and therefore based on `sqrt(power)`.
5. Existing incorrect scaling must be corrected at its source.
6. Prefer one canonical conversion per feature domain.
7. Remove obsolete aliases, wrappers, and duplicate conversion helpers after migration.

## Canonical types

```cpp
using PcmSample = int32_t;
using Strength16 = uint16_t;
using FrequencyScore16 = uint16_t;
```

Optional only where waveform history needs a compact signed representation:

```cpp
using HistorySample = int16_t;
```

Meanings:

- `PcmSample`: decoded and preprocessed signed PCM
- `Strength16`: amplitude strength, `0…32767`
- `FrequencyScore16`: linearized frequency amplitude score, `0…32767`
- `HistorySample`: compact signed stored waveform, with explicitly documented scaling
- frequency contrast remains a separate ratio/quality value

Do not introduce additional wrapper structs unless they carry real semantic state or multiple related fields.

## PCM domain

The PCM domain is derived from the actual I²S format:

- valid source bit depth
- bit alignment
- sign extension
- decode shift
- preprocessing gain

Do not normalize raw PCM against observed capture peaks such as `100000`.

Raw PCM may later approach the real 24-bit range. Therefore, do not force the entire live audio path through an early normalized `int16_t` representation.

Ownership and target flow:

```text
AudioSourceI2S
→ transport read
→ I²S word decoding
→ correctly aligned and sign-extended PcmSample

AudioSourceI2S preprocessing
→ First Difference / de-accumulation
→ canonical preprocessed PcmSample

AudioSignal
→ baseline / centering where applicable
→ centered PcmSample

Feature producers
├─ AMP producer:
│  PcmSample → Strength16
├─ frequency producer:
│  PcmSample → Goertzel power → sqrt(power) → FrequencyScore16
├─ waveform history
└─ RAW diagnostics
```

Ownership rule:

```text
AudioSourceI2S owns hardware transport, sample-format decoding, and First Difference preprocessing.
AudioSignal owns baseline/centering.
Feature producers own feature-specific normalization.
```

Do not place AMP or frequency normalization inside `AudioSourceI2S`.

Do not place feature-specific normalization inside the generic `AudioSource` unless the class is explicitly the feature producer itself.

## Ownership boundaries

### AudioSourceI2S

`AudioSourceI2S` owns:

- I²S driver interaction
- DMA/read handling
- channel and slot selection
- bit alignment
- sign extension
- conversion from transport word to canonical `PcmSample`
- First Difference / de-accumulation state and fixed gain correction

It must not own:

- baseline interpretation
- AMP scaling
- envelope scaling
- Goertzel score normalization
- detector thresholds
- Analyzer-facing feature semantics

### AudioSourceI2S preprocessing

The I²S source preprocessing stage owns:

- First Difference / de-accumulation
- continuity of preprocessing state across DMA reads
- publication of the canonical preprocessed `PcmSample`

This stage must preserve the valid PCM dynamic range and must not collapse the live signal into a detector-specific `int16_t` domain.

### Feature producers

Each feature producer owns its semantic conversion:

- AMP producer owns `PcmSample → Strength16`
- envelope producer owns smoothing in the AMP-strength domain
- frequency producer owns `power → sqrt(power) → FrequencyScore16`
- history owns its own explicitly documented storage mapping

Detector and Analyzer consume these values. They do not normalize them again.

## Preprocessing

First Difference / de-accumulation remains part of the canonical PCM path and belongs in `AudioSourceI2S`.

Use a wide intermediate:

```cpp
using PcmIntermediate = int64_t;
```

Example:

```cpp
const PcmIntermediate diff =
    PcmIntermediate(current) - PcmIntermediate(previous);
```

If a fixed gain correction is needed after First Difference, it must be mathematically documented and applied once.

Do not add a later compensating normalization merely to undo an earlier arbitrary shift.

## AMP strength

AMP strength uses one fixed engineering reference, not the complete 24-bit digital full scale.

```cpp
constexpr PcmSample kAmpStrengthGainReferencePcm = ...;
```

This value means:

> PCM magnitude at which AMP strength intentionally reaches `32767`.

It does not mean that larger PCM values are invalid.

Current starting reference:

```cpp
constexpr PcmSample kAmpStrengthGainReferencePcm = 262143;
```

This keeps common observed centered PCM peaks in the useful middle of the
`Strength16` range while still allowing loud events to saturate intentionally.

Canonical conversion:

```cpp
Strength16 pcmMagnitudeToStrength(PcmSample sample);
```

Required behavior:

```cpp
Strength16 pcmMagnitudeToStrength(PcmSample sample) {
    const uint64_t magnitude = safeAbsToUint64(sample);
    const uint64_t scaled =
        magnitude * uint64_t(32767) /
        uint64_t(kAmpStrengthGainReferencePcm);

    return Strength16(std::min<uint64_t>(scaled, 32767));
}
```

Requirements:

- safe handling of the most negative signed value
- wide multiplication intermediate
- intentional saturation above `kAmpStrengthGainReferencePcm`
- fixed compile-time or profile-owned reference
- no runtime peak normalization
- no hidden `>> 8`
- no duplicate conversion helper with slightly different semantics

## AMP envelope

The AMP envelope must operate in one clearly defined domain.

Preferred:

```text
PcmSample magnitude
→ Strength16
→ envelope smoothing
→ Strength16 envelope
```

Alternative internal float smoothing is acceptable:

```text
Strength16 input
→ internal float state
→ clamped Strength16 output
```

The public feature value remains `Strength16`.

Do not keep both:

```text
raw amp
normalized amp
detector amp
display amp
```

unless each has a clearly necessary and documented role.

The stable feature streams should be:

```text
AmpMagnitude : Strength16
AmpEnvelope  : Strength16
```

## Frequency score

Frequency DSP continues to consume full-range `PcmSample` values.

Do not feed Goertzel from an already saturated shared `int16_t` detector signal.

The Goertzel result is power-like. Convert it to an amplitude-like score:

```text
target power
→ sqrt(target power)
→ fixed reference scaling
→ FrequencyScore16
```

Canonical reference:

```cpp
constexpr double kFrequencyAmplitudeReference = ...;
```

Canonical conversion:

```cpp
FrequencyScore16 frequencyAmplitudeToScore(double amplitude);
```

Example:

```cpp
FrequencyScore16 frequencyAmplitudeToScore(double amplitude) {
    if (!(amplitude > 0.0)) {
        return 0;
    }

    const double scaled =
        amplitude * 32767.0 /
        kFrequencyAmplitudeReference;

    return FrequencyScore16(
        std::clamp(scaled, 0.0, 32767.0)
    );
}
```

Requirements:

- `FrequencyScore16` is based on `sqrt(power)`
- the reference is fixed and explicit
- score saturation is intentional
- target power remains available only where diagnostics need it
- frequency contrast remains separate
- no second score conversion later in Detector or Analyzer

Stable frequency feature fields:

```cpp
FrequencyScore16 score;
float contrast;
```

Optional diagnostics:

```cpp
double targetPower;
double neighborPower;
double totalPower;
```

## Separate references are intentional

AMP and frequency use the same public numeric range:

```text
0…32767
```

They do not need the same underlying PCM reference or formula.

```cpp
kAmpStrengthGainReferencePcm
kFrequencyAmplitudeReference
```

This is correct because:

- AMP strength measures broadband/time-domain magnitude
- frequency score measures target-band amplitude derived from spectral power

The shared range provides consistent storage and threshold handling, not identical physical meaning.

## Waveform history

Waveform history must have one explicit purpose.

### If used for later signal inspection or Goertzel

Prefer storing canonical `PcmSample` if RAM permits.

### If compact `int16_t` history is required

Use one documented fixed mapping:

```cpp
HistorySample pcmToHistorySample(PcmSample sample);
```

Its saturation/reference must be explicit and independent from AMP strength semantics.

Do not silently reuse `Strength16` conversion for signed waveform history.

Rename misleading types such as `RawSampleHistory` if the stored values are normalized or saturated.

Use:

```text
PcmHistory
```

for true `PcmSample` storage, or:

```text
NormalizedPcmHistory
```

for fixed scaled `int16_t` storage.

## Remove and consolidate existing conversions

Before adding any new helper, inventory all existing functions that perform:

- PCM shifting
- absolute magnitude conversion
- detector scaling
- amplitude normalization
- frequency power scaling
- score clamping
- history compression

Choose one canonical implementation for each semantic conversion.

Likely cleanup pattern:

```text
decodePcmSample()              keep
applyPreprocessing()           keep
pcmMagnitudeToStrength()       canonical AMP conversion
frequencyAmplitudeToScore()    canonical frequency conversion
pcmToHistorySample()           only if compact history remains
```

Remove, inline, or redirect obsolete helpers such as:

```text
normalizeDetectorMagnitude()
pcmMagnitudeToDetectorStrength()
scaleAmpForDetector()
normalizeFrequencyScore()
toFeatureStrength()
```

Do not leave compatibility wrappers unless an external API genuinely requires them.

If a wrapper only forwards one argument to another helper and adds no validation, state, or semantic boundary, delete it.

## Reduce wrapper layers

Prefer:

```cpp
const Strength16 amp =
    pcmMagnitudeToStrength(centeredPcm);
```

over chains such as:

```cpp
const auto magnitude = getMagnitude(centeredPcm);
const auto normalized = normalizeMagnitude(magnitude);
const auto detectorValue = toDetectorStrength(normalized);
const auto streamValue = wrapFeatureValue(detectorValue);
```

A wrapper is justified only when it owns one of:

- state
- buffering
- smoothing
- lifecycle
- validation
- semantic composition
- hardware or module boundary

Pure renaming wrappers should be removed.

## Feature-stream contract

Stable feature values:

```text
AmpMagnitude    Strength16
AmpEnvelope     Strength16
FrequencyScore  FrequencyScore16
FrequencyContrast float
```

Generic feature storage may remain temporarily float if changing it would cause broad unrelated churn.

However:

- producers must use the canonical conversions
- public typed fields should use the explicit types
- Detector and Analyzer must not rescale the values again
- float storage must not change the numeric contract

## Detector contract

Detectors consume normalized feature values.

They must not:

- know PCM full-scale
- apply bit shifts
- normalize AMP again
- take another square root of frequency score
- convert frequency power into score
- maintain private alternative scales

Thresholds are expressed directly in the stable feature domains:

```text
amp_onset_strength
amp_release_strength
amp_min_peak_strength
freq_min_score
freq_min_contrast
```

## RAW and Analyzer reporting

RAW output must name the actual value being printed.

Correct fields:

```text
ms,amp_strength,env_strength,freq_score,freq_fresh
```

`freq_score` must read:

```cpp
FeatureStreamId::FrequencyScore
```

not `FrequencyTargetBand`, target power, or another diagnostic field.

Print integer feature values directly.

Do not multiply and divide by `1000` merely to print integer strength values.

RAW PCM remains separate:

```text
ms,pcm,baseline,centered
```

Analyzer consumes the already-normalized feature/report values and must not introduce display-only rescaling that can be mistaken for detector truth.

## Choosing references

Do not derive references from one current run.

Use the full capture matrix:

- 50 cm
- 100 cm
- 200 cm
- angle variations
- quiet/background runs
- expected close/loud cases
- potential near-saturation cases

Choose references with documented headroom.

### AMP reference goal

Typical valid tones should occupy a useful multi-thousand range.

Very loud events may saturate intentionally.

### Frequency reference goal

Valid target tones should occupy a useful score range without routine hard saturation.

The score should preserve useful separation between:

- quiet
- weak/far tone
- normal tone
- strong/near tone

## Validation

### Conversion tests

```text
AMP:
0                         → 0
half AMP reference        → approximately 16384
AMP reference             → 32767
above AMP reference       → 32767
positive/negative PCM     → equal strength

Frequency:
power <= 0                → 0
quarter reference power   → approximately 16384
reference power           → 32767
above reference power     → 32767
```

Because frequency score uses square root:

```text
quarter power → half amplitude → half score
```

### Runtime checks

Verify:

- RAW PCM is unchanged
- First Difference has no overflow
- preprocessing state persists across DMA boundaries
- no startup spike
- AMP quiet values remain near zero
- valid tones use a meaningful strength range
- frequency score is actual `sqrt(power)`-based score
- no routine saturation in normal cases
- Detector sees the same values that RAW FEAT prints
- Analyzer reports those values without another conversion
- old scaling helpers are no longer referenced

## Migration order

1. Inventory current scaling and wrapper functions.
2. Mark the canonical PCM, AMP, frequency, and history conversions.
3. Correct existing producer calculations in place.
4. Route feature streams to the corrected canonical values.
5. Remove Detector-side and Analyzer-side rescaling.
6. Fix RAW FEAT field selection and naming.
7. Delete obsolete wrappers and duplicate helpers.
8. Compile and run unit/static checks.
9. Run RAW PCM and RAW FEAT.
10. Only then retune Detector and Inspector thresholds.

## Non-goals

Do not add:

- AGC
- adaptive normalization
- runtime peak calibration
- percentile-based scaling
- automatic threshold tuning
- new Param infrastructure
- a generic normalization framework
- additional wrapper classes
- a shared normalized `int16_t` live audio path
- detector behavior tuning in the normalization pass

## Final architecture rule

`AudioSourceI2S` ends at correctly decoded and FirstDifference-preprocessed `PcmSample`.

`AudioSignal` may center that PCM against baseline, but it does not own feature normalization.

AMP and frequency producers each perform exactly one explicit, fixed, semantically correct conversion into `0…32767`.

Detector, Analyzer, and reporting consume those canonical feature values without rescaling.

Correct the existing producers and remove redundant conversions. Do not stack new normalization layers on top of old ones.
