Investigate whether Arduino-ESP32’s legacy `I2S.h` RX implementation can cause the repeatable `sampleIndex % 64 == 32` discontinuity seen with:

* `I2S_PHILIPS_MODE`
* 16 kHz
* 32-bit slots
* stereo transport
* INMP441 active on slot 0
* application reads 512 bytes = 128 words = 64 stereo frames
* application selects every second 32-bit word and decodes `int32_t >> 8`

Do not tune detection or apply a signal-processing workaround. This is an investigation pass.

## 1. Identify the exact framework

Inspect the actual PlatformIO environment and report:

* platform version
* Arduino-ESP32 version
* ESP-IDF version
* exact `I2S.h` and `I2S.cpp` paths used by the compiler
* relevant compile definitions

Do not infer the version merely from the API.

## 2. Trace the complete RX path

Starting at:

```cpp
I2S.begin(I2S_PHILIPS_MODE, 16000, 32);
I2S.available();
I2S.read(buffer, 512);
```

trace through the actual installed Arduino and ESP-IDF source:

```text
I2S peripheral
→ DMA descriptors/buffers
→ ESP-IDF legacy i2s driver
→ Arduino I2S wrapper
→ Arduino stream/ring buffer, if present
→ AudioSourceI2S::refillBlock()
→ slot selection
→ decodePcmSample()
```

For every layer, record:

* configured channel/slot mode
* data width and slot width
* DMA buffer count and length
* units of every length field: bytes, words, frames, or per-channel samples
* callback/read chunk sizes
* whether 256-byte or 64-word internal boundaries exist
* whether stereo size calculations include both channels
* any copy, format conversion, byte swap, alignment correction, or remainder handling
* whether reads can begin in the middle of a DMA descriptor

## 3. Verify application assumptions

Review `AudioSourceI2S::refillBlock()` specifically.

Confirm or disprove:

* returned words alternate slot 0 / slot 1
* `samplesToProcess / 2` always means complete stereo frames
* the first returned word is always slot 0
* a 512-byte read cannot cross an internal boundary with changed alignment
* selecting indices `0,2,4,...` remains valid across successive reads
* `sample32 >> 8` is correct for this actual Philips/32-bit configuration

Note: current code discards any concept of persistent slot phase between reads. Determine whether that is safe according to the actual library contract.

## 4. Build a minimal diagnostic

Add a compile-time diagnostic mode only. Avoid Serial printing inside the RX loop.

For each library read, store a small fixed-size record containing:

* read sequence
* requested and returned bytes
* `I2S.available()` before/after
* first and last 4 raw 32-bit words
* words around offsets 60–68
* global raw-word index
* application mono-sample index
* inferred slot phase
* DMA descriptor/buffer identity or offset, if accessible
* adjacent raw-word and selected-sample deltas

Print records only after capture.

Also aggregate `mean_abs_delta`, `max_abs_delta`, and count by:

* mono sample phase modulo 64
* raw-word phase modulo 128
* byte phase modulo 256 and 512
* DMA descriptor position, if available

## 5. Controlled comparisons

Run the same capture while changing one variable at a time:

1. application read size: 256, 512, 1024 bytes
2. DMA buffer length, if configurable
3. DMA buffer count
4. one-slot/mono RX configuration versus stereo RX
5. direct ESP-IDF `i2s_read()` versus Arduino `I2S.read()`
6. repeated small reads versus one block read

Keep sample rate, Philips framing, 32-bit slot width, microphone, wiring, and emitted tone unchanged.

## 6. Decision criteria

Classify the result as one of:

* application slot-phase bug
* Arduino wrapper chunk/copy bug
* ESP-IDF DMA boundary issue
* peripheral configuration mismatch
* microphone-originated discontinuity
* unproven

A library-origin conclusion requires the discontinuity phase to move predictably when read size or DMA geometry changes.

A MEMS-origin conclusion requires it to remain fixed in the continuous sample stream independently of application read size and DMA geometry.

## Deliverable

Produce a short report containing:

1. exact installed versions
2. exact RX configuration
3. verified buffer-size arithmetic
4. diagram of the actual data path
5. suspicious code locations with file and line references
6. diagnostic results table
7. strongest supported conclusion
8. smallest next test

Do not implement de-accumulation, baseline subtraction, filtering, or spike suppression as a fix.
