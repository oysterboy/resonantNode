# FreeRTOS tasks: where they help in this firmware, and where they don't

```text
Status:  exploring (2026-10-10)
Became:  ANA-004 fix direction: Analyzer report task (roadmap-detection.md,
         2026-10-10)
Lab:     docs/lab/notes_lab.md 2026-10-09 (D-AMP bring-up);
         bench:sessions/2026-10-09-issue20-damp-110cm-b (g_drops_*: drops
         only with diagnostics + mode=detail on detected trials)
```

## Problem

The firmware is one Arduino `loop()` doing everything: reading I2S,
feature streams, detection, inspection, behavior, serial commands and, in
the Analyzer, multi-KB trial reports. Anything slow in that loop starves
the I2S driver, which then drops whole DMA buffers (8 ms each). That has
bitten three times: the inspection-history holes (issue #26, DET-009), the
diagnostic-report drops (ANA-004), and the need to feed the D-AMP speaker
on time (solved with the first explicit task, step 2). The CPU already
runs at its maximum (240 MHz) with flash at 80 MHz QIO (#26), so there is
no clock left to buy headroom with; the remaining lever is to stop slow,
non-urgent work from running in the audio loop.

## What runs where today (2026-10-10)

```text
loop()            core 1, Arduino loopTask: everything below unless named
i2s_tone          core 0, priority 5, D-AMP Node/Emitter only: renders the
                  chirp sine, blocks in i2s_write (src/hal/I2sToneOutput.cpp)
I2S / UART drivers  interrupts plus FreeRTOS queues (the I2S event queue
                  counts dropped buffers since #26)
port mutex        AudioSourceI2S: keeps i2s_tone out while begin()
                  reinstalls the driver
```

FreeRTOS was always underneath (Arduino-ESP32 runs `setup()`/`loop()` as a
FreeRTOS task); step 2 added the first task of our own.

## Rules (proposed)

1. A task is for a timing job: feeding or draining a peripheral on time,
   or moving slow work off the audio core. Logic (detection, inspection,
   behavior) stays in one place.
2. Tasks talk through queues (copies of small records) or single 32-bit
   values; a mutex only where two sides touch the same driver.
3. Name core, priority and stack where the task is created.
4. Core 0 hosts the helpers (tone, reports, later wireless); core 1 keeps
   the audio loop.

## Candidate 1: Analyzer report task

-> promoted to ANA-004 (roadmap-detection.md), 2026-10-10

Today a detected trial's report (~6.9 KB with diagnostics + mode=detail)
is formatted and printed inside the loop at trial end, and the driver
drops ~21-22 DMA buffers (~170 ms of audio) per detected trial.

Proposal: the loop fills a compact record per trial (the facts the
reporter prints today: trial classification, source/inspection fields,
summary counters) and pushes it into a FreeRTOS queue; a low-priority
task on core 0 formats and prints it. The loop never formats text or
waits on the UART during a run.

Steps:
1. Measure where the ~170 ms goes before building anything: time
   `finalizeSequenceTrial()` and the print calls with `micros()` in a
   detail + diagnostics run. Formatting cost (CPU on core 1) argues for a
   task on core 0; UART back-pressure alone would also be solved by a
   bigger TX buffer or chunking.
2. Define the record from what `AnalyzerSeqReporter` prints (one struct,
   fixed size; strings as enums/ids). Check RAM: queue depth x record size
   against the ~110 KB largest heap block (RAW capture needs 72 KB of it).
3. Reporter task on core 0, priority below i2s_tone (the Analyzer has no
   tone task today, the Node does not report SEQ at all).
4. Output order: trial records, then SEQ_SUMMARY / SEQ REPORT after the
   queue drains; bench tools parse line types, so the text format stays.
5. Verify: detail + diagnostics run with 0 dropped buffers on detected
   trials (g_drops_detail reproduced: 198 / 10 trials today).

Cheaper alternatives considered: SEQ DIAG off as the default (hides the
problem, loses detail); printing a few lines per loop iteration (spreads
the cost but keeps it on the audio core); formatting on the PC from a
compact record stream (also needs the record; could follow the task).

## Candidate 2: audio path as its own task (later, conditional)

A high-priority task on core 1 reads I2S, runs feature streams and the
detector, and pushes detected occurrences into a queue; the loop takes
them and runs inspection, behavior and serial. Matches the spec's flow
("Detection produces facts ... Behavior decides") with the queue as the
boundary, and would end the starved-audio class of problems for the Node
too. Invasive: touches the Node's core loop and everything that shares
detection state (FeatureHistory is read by inspection). Only if the field
trial or wireless shows the loop running out of time: today the Node uses
~39 of 62.5 us per sample (Analyzer path, D-AMP; issue #26 had 51-54 on
piezo).

## Candidate 3: wireless (later)

ESP-NOW or Wi-Fi bring their own tasks on core 0, next to i2s_tone and a
report task. Same rule: incoming messages via a queue, the loop decides.
Needs a measured budget so the speaker task never starves.

## Not candidates

Serial command parsing, LED patterns, idle timing, behavior: cheap, and
the loop handles them; a task would add shared state for no gain.

## Open questions

- Where the 170 ms goes (step 1 of candidate 1).
- Whether the Node has a similar stall anywhere (it prints far less; no
  dropped-buffer count is logged in Node mode yet).
- Stack sizes: measure with `uxTaskGetStackHighWaterMark()` once tasks
  exist, rather than guessing.
