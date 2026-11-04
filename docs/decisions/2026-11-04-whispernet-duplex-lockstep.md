# Whispernet's mic and speaker run in lockstep (one I/O owner), for the echo cancellation

Status: decided (owner 2026-11-04, "Ja. update spec, build"; ECHO-002)
Date: 2026-11-04

## Decision

In the whisper build the full-duplex I2S port is read and written in one place, `DuplexStream` (src/hal/): for each
mic block read, the same number of output frames is written, and what was written is kept by the mic's index. The
speaker then runs a fixed number of frames behind the mic; the echo canceller (an input-conditioning stage, myspec
5.2) reads the played samples at that offset. I2sToneOutput's writer task and the FIFO are not used in that build.

## Why

- The canceller needs to know which played sample is reaching the mic. With the writer task on its own pace and a
  FIFO between, neither time stamps (an output latency of -19 to +40 ms read from them) nor the two sample counters
  (an offset different in every dump) gave a fixed relation on E5 (bench/sessions/2026-11-04-aec-desk).
- Lockstep fixes the offset by construction, as embedded echo cancellers usually do; it is measured once per start.
- Whispernet's output is a stream anyway, not a request a stalled loop could cut short.

## Rules out

- The mode writing the port itself: the port keeps one owner per build (myspec 7.3).
- Lockstep in the Node or the echo room: their output stays request-driven through SoundOutput and the writer task.

## Revisit when

- A stalled loop audibly underruns the stream in the room (then a deeper TX queue, or the loop's stalls removed).
- The echo room or the Node want the canceller too.
