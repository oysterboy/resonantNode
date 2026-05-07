# Task: Fix I2S MEMS raw sample unpacking / slot selection

Context:
We added RAW dump modes for ResonantNode / Analyzer. Current full CSV RAW dump shows a perfect alternating pattern:

- 16000 sample rows, indices continuous
- Every odd index has `raw=0, abs=0`
- Even indices contain the actual signal, with a few natural zeros
- `dropped=0` in the latest chunk dump
- Chunk mode confirms the 100 ms chirp is captured, but full RAW sample output is not valid continuous mono PCM yet

Diagnosis:
The I2S input path is probably dumping two slots/channels while only one slot contains MEMS data:

```text
active slot, empty zero slot, active slot, empty zero slot...

This is likely an I2S slot/channel unpacking issue, not an acoustic/detector issue.

Goal:
Fix the I2S audio input path so all downstream code receives contiguous mono samples from the active MEMS slot only.

Do not tune detector parameters.

Do not change behavior logic.

Do not change the AMP detector thresholds.

Requirements
Find the actual I2S read/unpack path used by Analyzer RAW dump and normal analyzer/detector processing.
Confirm whether the code currently treats stereo/slot data as sequential mono samples.
Introduce explicit MEMS slot handling:
selectable slot: left, right, auto
default can be auto
auto mode should inspect a short buffer and choose the slot with higher RMS / mean_abs
log the selected slot
Ensure the exported mono sample stream is contiguous:
output sample index 0 = selected slot frame 0
output sample index 1 = selected slot frame 1
output sample index 2 = selected slot frame 2
no empty/padding slot rows
Preserve sample rate semantics:
sr=16000 should mean 16000 mono samples per second after slot selection
samples=16000 should mean 1 second of mono audio, not 8000 real samples plus 8000 zero slots
Add / keep RAW validation stats:
zero_count
odd_zero_count
even_zero_count
positive_count
negative_count
min
max
mean_abs
rms
selected slot info
Update RAW_BEGIN or RAW_INFO to include selected slot and source format, e.g.:
RAW_INFO i2s_slot_mode=auto selected_slot=left slot_width=32 valid_bits=24 shift=...
Rerun success criteria:
full RAW dump has 16000 rows
indices continuous
dropped=0
odd indices are not all zero anymore
raw contains both positive and negative values
main chirp still appears around expected timing
chunk mode still works
Important constraints
Keep current AMP/transient detector params frozen.
Do not “fix” this by just skipping odd samples inside the dump layer if the normal detector path still sees alternating zeros.
The correction must happen at the shared audio input / sample unpacking layer used by both RAW dump and detector processing.
Avoid hardcoding only one MEMS model. The fix should tolerate left/right slot differences via config or auto-detection.
Keep changes minimal and localized.
Useful current observations

Previous full CSV dump:

every odd sample row was exactly zero
even rows carried signal
signal max_abs around 802–904 depending on run
main 100 ms beep visible around 20–120 ms after trigger
latest chunk dump reported dropped=0
chunk_samples=800 at sr=16000 means 50 ms chunks
late energy around 350–650 ms was visible in chunk mode, but precise acoustic interpretation is blocked until the raw mono stream is fixed
Output expected from Codex
Identify the files/functions involved in I2S read/unpack.
Explain the current bug briefly.
Patch the code.
Add diagnostic logging.
State how to test with RAW full and RAW chunks.
Do not refactor unrelated architecture.
