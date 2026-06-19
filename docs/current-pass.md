# Current Pass — PHILIPS I²S Periodic PCM Boundary Spike

## Goal

Inspect and isolate the periodic PCM discontinuity in the current PHILIPS I²S input path.

Observed pattern:

- strongest sample-to-sample jump at `sampleIndex % 64 == 32`
- period: 64 selected mono PCM samples = 4 ms at 16 kHz
- discontinuity is much larger than normal adjacent-sample changes
- separate from slower DC drift
- no dropped RAW samples reported
- de-accumulation / first difference is diagnostic only and must not become a production fix

Ignore the LEFT_ALIGNED path in this pass. Treat it as legacy and do not modify, compare, or clean it up.

## Scope

Follow the actual PHILIPS data path from:

1. Arduino I²S / DMA read
2. read buffer and `bytesRead`
3. 32-bit slot-word indexing
4. stereo-frame and selected-slot handling
5. PCM decode
6. block/sample buffering
7. RAW capture
8. feature/detector feeding

Determine whether the discontinuity:

- already exists in the raw 32-bit I²S word, or
- is introduced by frame selection, indexing, decoding, copying, buffering, or downstream consumption.

## Primary Suspected Boundary

Inspect `AudioSourceI2S::refillBlock()` and related PHILIPS code.

Current likely relationship:

- 128 × 32-bit slot words per maximum refill
- 2 slot words per stereo frame
- maximum 64 selected mono samples per refill
- a 256-byte partial read would contain:
  - 64 × 32-bit words
  - 32 stereo frames
  - 32 selected mono samples

This matches the observed spike phase `32 mod 64`.

Do not assume this is the root cause. Confirm it from code and diagnostic output.

## Required Code Review

Check specifically:

### Read and block sizing

- DMA/read buffer capacity
- requested byte count
- actual `bytesRead`
- words per read
- stereo frames per read
- selected mono samples produced per read
- whether regular reads are 256 bytes / 32 selected samples
- whether the 64-sample period corresponds to alternating or repeated read sizes

### Arithmetic and alignment

- byte-count versus word-count versus frame-count versus mono-sample-count arithmetic
- all divisions by 4 and 8
- whether requested reads are aligned only to 4-byte words instead of complete 8-byte stereo frames
- handling of `bytesRead % 4`
- handling of `bytesRead % 8`
- handling of `bytesRead != requestedBytes`

### Slot and frame continuity

- whether every read incorrectly assumes its first word is slot 0
- whether slot phase is preserved across partial reads
- whether an odd number of slot words can be returned
- whether an incomplete stereo frame is silently discarded
- whether the following read then starts with the opposite slot
- pointer increments and indexes for 32-bit words, frames, and selected mono samples
- whether one word/frame/sample is inserted, skipped, duplicated, overwritten, or paired incorrectly at a refill boundary

### Buffer lifetime

- read-buffer reuse before all words are consumed
- output-block reuse before all selected samples are consumed
- ring-buffer wrap/copy boundaries
- stale values left in buffers after short reads
- block counters or state resets occurring every 32 or 64 selected samples

### RAW and feature parity

Confirm whether RAW capture and feature/detector processing consume the exact same decoded sample sequence from the same PHILIPS source block.

Identify any branch where:

- RAW sees different samples,
- samples are consumed twice,
- one path advances the source independently,
- or feature processing uses a copied/reordered sequence.

## Minimal Temporary Diagnostics

Add temporary diagnostics as close as possible to the PHILIPS read/decode boundary.

For each selected mono sample log:

- global monotonic selected-source sample index
- `sampleIndex % 64`
- I²S read-call index
- selected-sample index inside this read
- raw-word index inside this read
- bytes requested
- bytes read
- total 32-bit words read
- total complete stereo frames read
- selected slot/channel
- raw 32-bit word
- decoded PCM
- previous decoded PCM
- delta from previous decoded PCM

Also log one compact read-header line per I²S read containing:

- read-call index
- requested bytes
- returned bytes
- returned word count
- complete frame count
- remainder bytes
- remainder words / incomplete-frame state
- first expected slot phase
- last consumed slot phase
- number of selected mono samples emitted

Limit detailed sample logging to these global source-index ranges:

- `28..36`
- `92..100`
- `156..164`

Equivalent modulo-based logging around phase 32 is acceptable if it produces the same three boundary windows.

Diagnostics must not change:

- sample values
- sample ordering
- read cadence
- detector parameters
- feature calculations
- baseline handling

## Decision Tests

### A — Read boundary confirmed

If index 32, 96, 160 is also `selectedIndexInRead == 0`, the periodic discontinuity aligns with a new I²S read.

Report the corresponding:

- previous and current read-call IDs
- `bytesRead` values
- last raw word before boundary
- first raw word after boundary
- selected slot phase

### B — Discontinuity already in raw word

If the raw 32-bit words themselves show the discontinuity before decode, classify the problem as upstream of PCM decoding.

Then isolate with the smallest test:

- capture consecutive raw PHILIPS slot words directly across read boundaries
- bypass feature processing and RAW-history copying
- keep normal I²S configuration and timing unchanged

Do not compensate for the discontinuity.

### C — Raw words continuous, decoded PCM discontinuous

Inspect only:

- bit shift
- sign extension
- payload alignment
- masking
- cast order
- per-read decode state

Provide the exact expression producing the incorrect value.

### D — Slot/frame phase breaks

If an incomplete frame or odd word count causes the next read to restart with the wrong assumed slot, implement the smallest isolated correction:

Preferred robust correction:

- preserve incomplete bytes/words across reads
- assemble only complete 8-byte stereo frames
- maintain explicit slot/frame phase
- select the configured PHILIPS slot only after a complete frame exists

A simpler 8-byte request alignment may be used as an isolated experiment, but do not present it as sufficient unless diagnostics prove that every actual `bytesRead` is also frame-aligned.

### E — Copy or consumption boundary breaks

If raw words and decoded values are correct inside `refillBlock()` but the next consumer sees a discontinuity, locate the exact copy, wrap, or block transition and correct only that transition.

## Allowed Changes

- temporary boundary diagnostics
- counters used only for diagnostics
- assertions or warnings for invalid byte/frame alignment
- a narrowly targeted PHILIPS frame-continuity correction once proven
- a focused regression test for sample-sequence continuity across reads

## Do Not

- touch LEFT_ALIGNED code
- tune detectors
- smooth or suppress spikes
- recenter the baseline
- interpolate samples
- apply first difference / de-accumulation as a production transform
- alter acoustic feature processing
- change detector thresholds
- refactor unrelated audio, Analyzer, DetectionRuntime, or architecture
- silently discard partial bytes or incomplete frames as a “fix”
- claim DMA/I²S hardware fault without proving that the discontinuity exists in the returned raw word

## Required Result

Return:

1. **Exact boundary**
   - function
   - read-call transition
   - byte/word/frame/mono-sample position
   - relationship to phase `32 mod 64`

2. **Evidence**
   - relevant code expressions
   - diagnostic rows immediately before and after at least three boundaries
   - raw-word and decoded-PCM comparison

3. **Root-cause ranking**
   - ranked by evidence, not speculation

4. **Smallest isolated test or fix**
   - no unrelated cleanup
   - explain why it preserves normal samples

5. **Sequence-integrity confirmation**
   - confirm that diagnostics do not modify the stream
   - after a correction, verify that the raw selected PCM sequence is unchanged except at the proven faulty boundary
   - verify RAW capture and feature/detector input receive the same corrected sequence

## Acceptance Criteria

Pass succeeds only when all of the following are established:

- the code boundary corresponding to `32 mod 64` is identified or conclusively ruled out
- diagnostic output shows whether the jump exists in the raw I²S word
- byte, word, stereo-frame, and mono-sample counts are explicitly reported
- PHILIPS slot continuity across reads is verified
- RAW and feature/detector sequence parity is verified
- no signal-processing workaround is introduced
- LEFT_ALIGNED remains untouched
