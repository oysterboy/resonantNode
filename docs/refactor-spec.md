# Refactor Spec: AudioSourceI2S
## Sample Now, Stream Later
### Next step for the audio source abstraction

## Fix steps

Before proceeding to additional source or signal refactors, the behavior boundary shall be tightened as follows:

1. Self-chirp suppression policy shall be removed from `Node`.
2. `Node` shall be limited to orchestration, wiring, and output control.
3. Timing rules for response suppression shall reside in `ResonantBehavior` or in a dedicated behavior-adjacent policy object.
4. Debug latches may remain in `Node` provided they do not influence behavior decisions.
5. Further source or signal refactors shall proceed only after the behavior boundary is clarified and stabilized.

## Goal

Extend the current audio input path so the system can support both:

- current analog mic input
- future digital MEMS / I2S mic input

without changing downstream behavior logic.

This refactor should keep the current `AudioSource` contract sample-based for now and prepare the codebase for a later stream or window API.

## Current contract

The source layer currently behaves like this:

`AudioSource::readSample()`

This means:

- one call returns one sample
- `AudioSignal` consumes one sample at a time
- downstream detection and behavior stay unchanged

## Next step

Add `AudioSourceI2S` as a new implementation of `AudioSource`.

The I2S implementation may:

- read from a hardware buffer internally
- average or reduce buffered values into one returned sample
- keep the public contract unchanged for now

## Later step

After the I2S source exists and is stable, introduce a stream or frame-based API.

That future API should allow:

- windowed reads
- buffered processing
- richer signal analysis

Do not implement that now.

## Architectural intent

### HAL / Source

Owns raw acquisition only.

Examples:

- ADC-backed source now
- I2S-backed source later

### AudioSignal

Keeps sample-based signal conditioning:

- baseline
- magnitude
- smoothing

### AudioOnsetDetector

Keeps transient / onset detection.

### Behavior

Keeps state and response logic.

### Node

Orchestrates updates and wiring only.

## AudioSource contract

`AudioSource` is the shared acquisition abstraction for the current pipeline.

Its public contract stays minimal and stable:

- `begin()`
- `readSample()`

That allows `AudioSignal` and `Node` to use different source implementations interchangeably.

### Rule

- shared public contract: stable
- internal implementation: source-specific

That means:

- `AudioSourceAnalog` may implement sampling via ADC
- `AudioSourceI2S` may implement sampling via I2S, buffering, averaging, or other internal mechanisms

As long as both provide the same public contract, they remain interchangeable in the existing pipeline.

### Important constraint

If `AudioSourceI2S` appears to need additional public methods, then one of two things is true:

1. the methods are implementation-specific and should remain private
2. the system is ready for a later, separate abstraction, such as window or buffer input, which should be introduced explicitly rather than leaked into the current `AudioSource` contract

### Current design principle

For the current stage:

- `AudioSource` stays stable
- subclasses may differ internally
- subclasses must not require different public methods for the current sample-based pipeline

## Refactor target

Current:

`AudioSourceAnalog -> AudioSignal`

Next:

`AudioSourceAnalog`
`AudioSourceI2S`
`-> AudioSignal`

Later:

`AudioSourceStream`
or
`AudioFrameSource`
`-> AudioSignal` or a new stream-oriented signal stage

## Implementation notes

### 1. AudioSource interface

Keep the existing interface stable:

```cpp
class AudioSource {
public:
    virtual ~AudioSource() = default;
    virtual void begin() = 0;
    virtual int readSample() = 0;
};
```

### 2. AudioSourceI2S

Add a new implementation for digital MEMS / I2S input.

It may internally:

- fill a DMA or software buffer
- convert the buffer into one sample value
- expose only `readSample()` for now

### 3. AudioSignal

Keep `AudioSignal` sample-based.

Do not change its external behavior in this refactor.

### 4. Node

Keep `Node` wiring simple:

- create the chosen source
- pass it into `AudioSignal`
- leave detection and behavior unchanged

## Result

The codebase should support:

- `AudioSourceAnalog -> AudioSignal -> AudioOnsetDetector -> Behavior`
- `AudioSourceI2S -> AudioSignal -> AudioOnsetDetector -> Behavior`

while preserving the current one-sample contract.

## Summary

Add `AudioSourceI2S` next.
Keep the public source contract sample-based.
Use internal buffering or averaging only inside the I2S implementation.
Defer stream or window APIs to a later refactor.
