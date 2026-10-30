# Echo room: a node that replays what it hears, on the Node's detector, Behavior and output

```text
Status:  idea (2026-10-30; owner's discussion of 2026-10-30, written up from it; the audio-quality trade and the
         repo-or-fork question added the same day)
Lab:     the echoSpace repo (oysterboy/echoSpace, 48f74a5): a continuous 500 ms delay line, mic to amp, at 48 kHz;
         the owner's report of what it did in a room ("shrill feedback, or died off"); no bench session
```

Research only. The code as it stands is 195438a. Nothing here is a step, a roadmap item or a decision.

## The idea (owner, 2026-10-30)

An EchoRoom build that uses the Node's features: an audio buffer of 200 ms or as much as fits, triggered by
transients, so a node replays what it heard, with a budget and rest behaviour like the Node's. The earlier
EchoRoom (echoSpace) ran into shrill feedback or died off, so selectiveness might help.

## What exists

- **echoSpace** (`src/main.cpp` there, 117 lines): a continuous delay line on the D-AMP board. The mic feeds a mono
  ring of 500 ms at 48 kHz, 32-bit words (96 KB static), gain 0.9, a one-pole low-pass (alpha 0.35) against
  feedback, and the ring's output goes to both amp slots. Always on, nothing decides anything.
- **The Node** runs the same board at 16 kHz. Its per-sample path costs about 37 us of the 62.5 us a sample allows
  (decisions/2026-10-22-serial-lines-and-iram-hot-path.md); at 48 kHz it would not keep up. The largest free heap
  block with ESP-NOW up is about 110 KB (roadmap-detection.md, ANA-008). The DevKit's WROOM module has no PSRAM.
- The pieces the idea names are all in the Node, and all behind the spec's boundaries (myspec.md: Detection
  produces facts, Behavior decides, SoundOutput performs):
  - `ScalarTransientDetector` (`src/detection/detectors/scalar/`) gates one scalar stream with an onset threshold,
    a release threshold, a cooldown, a maximum duration and a minimum peak strength (`ScalarTransientConfig.h`).
    Which stream is the profile's choice (`src/detection/DetectionProfile.h`): the shipped TonalPulseScalar watches
    `FrequencyTarget`, the 3.2 kHz band; AmpExperimental watches `AmpEnvelope`, broadband.
  - `ResonantBehavior` (`src/behavior/`) decides when a heard thing is answered: wait plus jitter, refractory,
    own-emit suppression for the whole emission plus a 60 ms tail, and since BEH-008 the level automaton (+1 per
    heard train, +1 per own emission, decay, p = level / L, rest at levelMax). It names a `SignalShape` (Chirp or
    NetworkSignal) and does not know what sounds.
  - `SoundOutput` (`src/output/`) performs one `OutputRequest` (Pattern, HopTrain, IdleCall) on the output;
    `I2sToneOutput` (`src/hal/`) renders it one DMA block at a time in a writer task on the TX side of the port
    `AudioSourceI2S` owns, which stamps every block with its sample index.

## Why echoSpace howled or faded

A continuous mic-to-amp path has a loop gain. Above 1 at any frequency it howls at the room's and the comb
filter's resonances (the shrill part); below 1 every pass loses signal and the echo fades in a few rounds. There is
no stable middle: loop gain near 1 is unstable by definition, so the gain and the low-pass only choose which failure
happens. Selectiveness cannot fix a continuous loop; it has to break it.

## Two shapes

**A. Delay line with a gate.** The ring keeps writing. A transient opens the output gate for a window, and the
listener hears what happened delay ms ago. No copy, simple. But while the gate is open the mic hears the amp, so
the loop is back for the length of the window, and the delay is the ring length.

**B. Triggered replay.** On a transient the node freezes a clip (a pre window before the onset plus a post window
after it), waits, and plays the clip back once at a fixed gain. The wait is Behavior's wait plus jitter, not the
ring length; the ring only needs pre plus post.

B is the one that maps onto the Node, and it answers both failures:

- **No node ever has a live mic-to-amp path.** Playback comes from a frozen clip. The node's own mic hears it, but
  detection is suppressed for the emission plus the tail, so nothing of it enters the next clip. A howl needs a
  continuous path and there is none.
- **Fading becomes a decision, not physics.** Every replay plays at full level. Whether the room goes on depends on
  the neighbours and on p, not on how much signal survived the last pass.
- **Selectiveness is the detector, already built.** The onset threshold, the minimum peak strength, the 120 ms
  maximum duration and the 50 ms cooldown separate a clap, a knock or a short word from speech, music and a steady
  tone (a steady tone gives one reject and then silence). What the room answers is tuned there; everything else is
  ignored instead of fed around.

-> the proposal below assumes B.

## Buffer size

The question is the sample format, not 200 ms:

```text
format                             bytes / s   200 ms   500 ms   the 110 KB block holds
48 kHz, int32 (echoSpace today)      192 KB     38 KB    96 KB    ~570 ms
16 kHz, int16 (the Node's rate)       32 KB    6.4 KB    16 KB    ~3.4 s
```

At the Node's 16 kHz stored as int16, seconds are cheap and the shipped detection keeps running; the replay is
band-limited to 8 kHz. At 48 kHz the replay sounds fuller, but the shipped detection would have to be decimated or
dropped and the ring eats the heap. With B the ring holds pre plus post and a copied clip of up to 500 ms is 16 KB
at 16 kHz, so the wait can be any length, including the 3 s jitter, with no second buffer.

First proposal: 16 kHz, int16, clip <= 500 ms. Amended the same day: the owner prefers the audio quality and a
simpler detector; see "Audio quality first, a simpler detector" below for 48 kHz and its budget.

## Which transients

- `AmpEnvelope` (AmpExperimental's stream): broadband onsets, so claps, knocks, footsteps, a word. This is what an
  echo room wants: it answers the room.
- `FrequencyTarget` (TonalPulseScalar's stream): the 3.2 kHz band only, the network detector. An echo room on it
  would echo only the nodes themselves.

The echo build does not need the verdict chain (proposalMatched, supportMatched); an accepted occurrence from the
onset gate with a minimum peak strength is the trigger, the inspection plan nearly empty. One consequence: a
replayed clap is indistinguishable from a real clap, so the old rest rule "only valid network patterns count"
(decisions/2026-10-17-two-node-exchange-chain-length.md) cannot apply. Every transient counts, heard or own, which
is what the level automaton counts anyway.

## The loop that remains: the chain between nodes

Each node's replay is a transient for every neighbour, so one clap can chain through the room. That is the wanted
effect, and it is a different kind of loop from the howl: delayed by wait plus jitter, gated by the onset detector,
probabilistic through p = level / L, capped by the rest at levelMax. Two things to expect:

1. **Natural decay across generations.** Each hop is a recording of a replay through a room, a mic and an amp; the
   clip gets duller and softer per generation until it no longer clears the onset threshold. A free, organic fade,
   and the chain ends on its own even where the automaton would allow more.
2. **The Node's defaults are tuned for hop trains, not echoes.** p = level / L with offset 0 answers the first heard
   train 1 in 6 (BehaviorProfile.h), so a lone clap would mostly go unanswered. The echo build wants its own
   `BehaviorGateConfig`: a higher first-answer probability, a lower levelMax and a shorter rest, so a clap reliably
   gets one or two echoes and a busy room is damped. The idle call (every 40-80 s) probably goes off; or the idle
   voice replays the last clip ("the room remembers"), a variant worth trying.

## Where each piece sits

- **Capture**: a small ring in `src/audio`, fed by the Node loop from the same `AudioBlock` it hands to detection,
  indexed by sample index (which `AudioSourceI2S::stampBlock` already provides), so "pre ms before the onset" is a
  subtraction from the occurrence's start time. Detection must not own it (it produces facts); the loop writes it.
- **Behavior**: a third `SignalShape`, Replay. Nothing else in `ResonantBehavior` changes.
- **SoundOutput**: a fourth `OutputRequest::Kind`, Clip, holding a pointer and a length; `I2sToneOutput` renders it
  by copying samples with gain and a short ramp to both slots instead of computing a sine. The Node freezes the clip
  and composes the request, as it composes the hop train today.
- **Timing**: the post window must finish recording before playback starts. With `waitAfterHeardMs` 100 ms and a
  post window of 200 ms or more the Node is in a short recording state before Chirping. A copied clip makes this a
  deadline, not a race.
- **The ring and the own emission**: the ring keeps recording while the node plays. A trigger just after the
  suppression tail could reach its pre window back into the own emission. Either keep the pre window shorter than
  the tail, or mark the ring region written during own emission and skip it. Cheap, but it has to be deliberate.
- **Build**: a fourth mode under `src/modes/echo/` with its own env, next to resonant, analyzer and emitter; the
  case and the layout in "In the repo or a fork" below.

## Audio quality first, a simpler detector (owner, 2026-10-30)

The owner's preference: better audio quality at the cost of a simpler detector, a scalar transient gate on the
amplitude alone, with no feature history and no inspectors. The numbers support it.

**Where the Node's 37 us go.** The Goertzel bank for the band (`FreqBandStream`), the feature history record per
sample, and the inspection on top. The amplitude path itself (`AudioSignal::processSample`) is a DC blocker, an
absolute value and a one-pole smoother, three operations. With the detector's compares added that is a few us,
inside the 20.8 us a sample allows at 48 kHz.

**How the simple detector runs.** Today a packet is built and `ScalarTransientDetector::update()` is called once
per sample. At 48 kHz the onset gate runs once per DMA block on the block's envelope instead (every 1.3-2.7 ms,
by the block length), still far finer than an onset needs; the per-sample loop shrinks to the three operations plus
the ring write. The detector is reused unchanged: it wants a packet and a scalar, and its ms timers do not care that
packets come per block. The echo mode skips `DetectionRuntime` entirely: envelope, detector, occurrence, Behavior.
The spec's boundary holds (the detector states facts, Behavior decides).

**Given up, kept, to add.**

- Gone: frequency selectivity, so the echo node cannot tell a node from the room (the room is the point); the
  inspectors' quality checks and the history behind them.
- Kept: the gates inside the detector: onset and release thresholds, cooldown, maximum duration, minimum peak
  strength, coverage. These separate a clap from a steady tone.
- To add, cheap: an attack-slope gate. The detector thresholds level, not rise time. A clap crosses the threshold
  in a millisecond or two; a voice swelling or a door scraping takes tens. For an echo room this is the
  selectiveness that matters, and with per-block envelopes it is one subtraction.
- To re-tune: the smoothing factor and the threshold values were set for 16 kHz per-sample updates. Same knobs,
  new constants.

**Audio quality, honestly.** Sample rate buys bandwidth above 8 kHz; whether that is audible is decided by the rest
of the chain. The MEMS mic reaches 20 kHz and the MAX98357A is fine; the small speaker and the room decide. The
mic's 24 bits stored as int16 after the shift lose nothing the amp reproduces, so the ring and the clip hold int16.
RX and TX share BCLK and WS, so mic and amp run at one rate: 48 kHz is the natural choice, 24 kHz a middle path if
memory bites.

**Memory at 48 kHz, int16, 96 KB per second:**

```text
piece                 300 ms   500 ms
ring, pre plus post    29 KB    48 KB
clip copy              29 KB    48 KB
both                   58 KB    96 KB
```

The 500 ms pair just fits the ~110 KB block and leaves nothing. Two ways out: play from the ring with no copy,
which works as long as wait plus clip stays shorter than the ring; or leave ESP-NOW out of the echo build, which
frees a good margin. A 300 ms clip with a copy is comfortable either way.

**A cost that is neither CPU nor RAM.** The Analyzer's SEQ tooling and the replay corpus are built around the band
and the hop train. An amplitude-only echo node has none of that, so its verification is a RAW capture and ears, not
the existing bench battery. Acceptable for an idea; the thing to notice before the first board run.

## In the repo or a fork (owner's question, 2026-10-30)

In the repo, as a fourth mode. The question was answered once before: the Emitter is a signal-source-only mode
that shares everything, and the mode layer (step 10) exists so that builds differ in `src/modes/` and share the
rest.

**Why in-repo.**

- It reuses most of the tree: the I2S HAL, the envelope in `AudioSignal`, the `ScalarTransientDetector`,
  `ResonantBehavior` with the level automaton, `SoundOutput` and the writer task, the param registry, OTA, the
  serial command path. A fork copies all of that and the copies drift; the automaton is still being tuned (step
  16), and a fork would freeze a snapshot of it.
- The build system already supports it: the sample rate is a build define (`AUDIO_I2S_SAMPLE_RATE_HZ`), and
  `AudioSignal` and `I2sToneOutput` take the rate at runtime, so `esp32dev-echo` at 48 kHz next to `esp32dev` at
  16 kHz is an env, not a branch. CI builds every env, so the same run keeps the echo build compiling.
- The repo's rules cost little and help: nothing under `src/detection/` includes `src/modes/`, the registry
  includes nothing from `src/`, the format check. The echo mode holds its own `BehaviorGateConfig` and
  `ScalarTransientConfig` instances, so it never touches the Node's tuning.

**What is separate, and where it goes.**

- `src/modes/echo/`: the mode's wiring, its command surface, its detector and behavior configs.
- `src/audio/`: the capture ring (audio, not detection).
- `src/output/` and `src/hal/`: the Clip request kind and its renderer.
- A `roadmap-echo.md` with ECHO- IDs, and a step in `roadmap-0-steps.md` when it starts. The milestone decision
  (decisions/2026-10-26-milestone-installation-ready-node.md) sequences at the cut-over, so the echo room lands
  after the installation-ready node, not inside step 16.
- Bench sessions on the `bench` branch like every other session, tagged echo.

**The tooling is smaller than "separate" suggests.** Three pieces carry over:

1. The Emitter as the clap source: its pulse train at a 5 ms pulse is nearly a click, repeatable and scriptable,
   so the echo tests need no new hardware.
2. `tools/bench/nodegroup.py`'s chain metrics (chain length, emissions per minute, time to quiet) apply unchanged
   if the echo node prints the same emission log lines.
3. `tools/bench/seqrun.py`'s serial layer for a new `echocheck.py`, the echo equivalent of `nodecheck.py`: trigger
   on a click, clip captured, replay heard, level and rest counters move.

Genuinely new: a RAW capture that includes the replay, so the clip that was played can be read against the sound
that triggered it. That and ears are the verification until an echo-specific pass rule exists.

**When a fork would be right:** a different board, or a Behavior that stops being "answer what you heard, with a
budget". Neither is in the idea as it stands. echoSpace retires to a reference once the mode plays its first clip.

## Open questions (the owner's calls)

1. Shape A or B. This note argues B.
2. 16 kHz with the shipped detection, or 48 kHz with the amplitude-only detector. The owner leans to 48 kHz
   (2026-10-30, the section above).
3. Clip length, and whether the pre window is in the clip.
4. Whether chains across neighbours are wanted and how long; levelMax and restMs set that.
5. Whether ESP-NOW stays in the build. Without it the heap grows by a good margin.
6. Which transients: broadband (claps, voice) or the tonal band only.
