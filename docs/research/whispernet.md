# Whispernet: a room that whispers back, a gated delay line on a whisper detector

```text
Status:  idea (2026-10-30; owner's discussion of 2026-10-30, written up from it)
Lab:     none. The echoSpace repo (oysterboy/echoSpace, 48f74a5) is the nearest thing: an always-open 500 ms delay
         line that howled or faded (docs/research/echo-room.md, "Why echoSpace howled or faded")
```

Research only. The code as it stands is 195438a. A second piece next to the echo room (`echo-room.md`), not a
re-think of it: the echo room answers a clap with a frozen clip, Whispernet echoes a whisper through a gated delay
line. The two share most of the tree; see "Shared with the echo room" at the end.

## The idea (owner, 2026-10-30)

The room replays whispers, not claps. Nodes pass a whisper along and lose a little of it each time: the
whispering game, Stille Post. The name is a reading of the chain; Whispernet is also Amazon's mark for the Kindle's
wireless delivery, which matters for a public title, not for a mode name.

## What a whisper is, for the detector

Not a transient. No sharp attack, no pitch (unvoiced: broadband noise shaped by the mouth, most energy between
1 and 8 kHz, the fricatives above that), a little above the room's noise floor, so it reaches a node only from
close by, modulated at syllable rate (roughly 2-10 Hz), half a second to a few seconds long. The onset gate of the
echo room would miss it, and the selectiveness is the opposite kind: **the room answers what is quiet and ignores
what is loud.** Quiet is the password.

Four tests, cheap, in order of value:

1. **A level window, not a threshold.** Above the floor for long enough, and below a loud bound. The loud bound is
   the whole idea: speech, a clap, a door, a neighbour's chirp are all rejected for being too loud. It is also the
   feedback guard (below).
2. **A duration window**, 300 ms to about 3 s. `ScalarTransientDetector` already has minimum and maximum duration,
   coverage, longest island and maximum gap (`ScalarTransientConfig.h`); it lacks the loud bound, one added reject
   reason. Its shell fits; its onset logic matters little here.
3. **Modulation.** The envelope must move at syllable rate, or it is a fan, the HVAC, traffic: the envelope through
   a slow high-pass, then energy or a peak count. A few operations per block.
4. **Unvoiced**, optional. Distant quiet speech passes 1-3. The Goertzel bank (`FreqBandStream`) run the other way
   round could reject anything with a narrow peak. Whether to reject it is an artistic call: a room that whispers
   back what it half-heard from a conversation across the hall is not obviously wrong.

Sample rate: whisper fricatives live above 8 kHz, so 16 kHz sampling dulls exactly what makes a whisper a whisper.
48 kHz is the natural choice; 24 kHz the compromise if memory decides (the audio-quality argument in
`echo-room.md`, "Audio quality first, a simpler detector", holds here with more force).

## Why a gated delay line, not a frozen clip

The echo room freezes a short clip and plays it after a wait. A whisper is a phrase of unknown length, half a
second to several seconds: a clip of it needs a ring plus a copy (at 48 kHz int16, 96 KB per second: a 2 s clip
alone outgrows the ~110 KB heap block, roadmap-detection.md ANA-008) and a wait after the phrase ends. A gated
delay line handles it with one buffer of fixed size and no wait: the ring holds D ms of audio, the output is the
ring read D ms back, multiplied by a gate with ramps, and the detector decides per block whether the input is a
whisper.

Two details make it good rather than merely simple:

- **The gate is stored next to the audio and delayed with it.** The detector needs about 300 ms of evidence before
  it can say "whisper". With D at least that long, the start of the whisper is still in the ring when the decision
  lands, and the gate bits for those samples are set after the fact. The phrase plays from its first syllable,
  without a pre window and without a copy. The gate closes the same way, D after the release.
- **D is the artistic knob, bounded by memory.** At 300 ms the room echoes; at 2 s it answers. Same code, one
  buffer. The frozen clip is this with a copy, so the echo room's shape is the special case.

```text
format            bytes / s   D = 500 ms   D = 1 s   D = 2 s   the ~110 KB block holds
48 kHz, int16        96 KB        48 KB      96 KB    192 KB    ~1.1 s
24 kHz, int16        48 KB        24 KB      48 KB     96 KB    ~2.3 s
48 kHz, ADPCM 4-bit  24 KB        12 KB      24 KB     48 KB    ~4.5 s
```

Without ESP-NOW the block grows by a good margin; a WROVER module (PSRAM, same DevKit footprint and pins) would
make D of minutes possible but is a hardware change.

## Why the howl does not come back

echoSpace had an always-open gate with nothing to shape what passed through (`echo-room.md`, "Why echoSpace
howled or faded"). Here the gate is open only while the input passes the whisper window, and the window has a loud
bound. A loop with gain above 1 grows a little every D ms; the first pass that crosses the bound shuts the gate
within one block. The worst case is one short pass at roughly the bound times the gain: whisper level, not a
shriek. With a gain below 1 per pass it never gets there, and a node hearing its own output gives a decaying
multi-tap echo, each pass quieter, until it drops under the floor. That is echoSpace's "died off", but as a tail
behind a live whisper rather than the whole piece dying.

What this costs: the node must listen while it plays, so own-emit suppression is off in this mode and the
detector sees the live whisper plus the replay; the loud bound sits above that sum. The modulation test sees a
smeared envelope, which is fine. Gating needs 10-20 ms ramps or it clicks. Without the selectiveness tests this is
echoSpace again; they are what make the gate safe.

## Behavior

`ResonantBehavior` fits with two readings changed:

- A gate-open episode is a heard train (+1), the node's own replay another (+1); the level climbs, and at
  levelMax the rest forces the gate closed for restMs, so a room full of whisperers goes quiet for a while.
  p = level / L becomes the probability that the gate opens for an episode. The echo build's own
  `BehaviorGateConfig`, as for the echo room; the Node's defaults are tuned for hop trains.
- Jitter has no meaning when D is fixed, so it goes. A per-node D (say 250-600 ms) does its job of spreading the
  room, and fits better anyway.

Between nodes the chain is as in the echo room: B hears A's replay as a whisper (it is quiet, so it passes the
loud bound) and replays it after its own D, each hop a little duller, until a hop no longer clears the floor. Only
nodes close enough to hear a whisper take part, a nice property in a room. The idle call probably goes off.

## Where each piece sits

- **Capture and delay**: the ring in `src/audio`, fed by the mode's loop from the same `AudioBlock` it hands to the
  envelope, with the gate bits beside the samples, indexed by sample index (`AudioSourceI2S::stampBlock`). Not
  detection's (it produces facts).
- **Detector**: the envelope from `AudioSignal`, the modulation measure, and `ScalarTransientDetector` run per DMA
  block with a loud-bound reject added; no `DetectionRuntime`, no history, no inspectors.
- **Behavior**: `ResonantBehavior`, a `SignalShape` for the gated replay, suppression off.
- **Output**: the writer task in `I2sToneOutput` reads the ring D back and applies the delayed gate with ramps; a
  request kind for it in `SoundOutput`, since Behavior decides and SoundOutput performs (myspec 7.2).
- **Build**: a mode under `src/modes/` with its own env at 48 kHz (or 24), beside the echo room's; not a fork,
  see "In the repo or a fork" below.

## In the repo or a fork (owner's question, 2026-10-30)

No. The echo room's answer (`echo-room.md`, "In the repo or a fork") holds, with more force, because Whispernet
adds even less of its own.

Shared with the Node: the I2S HAL and the port's writer task, the envelope in `AudioSignal`, the
`ScalarTransientDetector` as the gate's shell, `ResonantBehavior` with the level automaton and the rest, the param
registry, OTA, the serial command path, CI, and the bench tooling's serial and chain-metric layers.

Its own, and all of it fits the mode layer:

- the ring with the gate bits, one file in `src/audio`;
- the loud-bound reject and the modulation measure, a few lines beside the detector;
- a renderer that reads the ring D back and applies the delayed gate, a second output class next to the tone
  renderer in `src/hal`, sharing the TX port;
- the mode's wiring, commands and configs under `src/modes/`.

Two settings look like departures but are not: suppression off is a config on the existing Behavior, and the
48 kHz rate is a build define the HAL already takes.

It would become a fork only if it dropped Behavior altogether (a pure gated line with no budget or rest) or moved
to other hardware (a WROVER for a long D). Neither is in this note. The echo room and Whispernet are the fourth and
fifth modes of one repo; open question 6 below, one mode with a profile switch or two, is a question inside the
repo either way.

## Verification

The Emitter cannot whisper. The test source is a recorded whisper on a small speaker at fixed distances, plus a
loud control (speech, a clap) that must be ignored. A RAW capture that includes the replay, and ears;
`tools/bench/nodegroup.py`'s chain metrics unchanged if the mode prints the same emission log lines. A fan or the
HVAC as the steady-noise control for the modulation test.

## Shared with the echo room

Both modes reuse: the I2S HAL at a higher rate, the envelope, `ScalarTransientDetector` as the gate's shell with
one added reject each (attack slope there, loud bound here), `ResonantBehavior` with the level automaton and rest,
the writer task, the param registry and the tooling. The capture ring in `src/audio` serves both, once as a clip
source and once as the delay line. The mode layer takes a fifth mode as easily as a fourth.

```text
                 echo room (echo-room.md)             Whispernet (this note)
trigger          a transient: clap, knock, step       a whisper: quiet, long, modulated, unvoiced
selectiveness    onset, strength, short duration,     a level window with a loud bound, duration,
                 attack slope                         modulation
shape            triggered replay of a frozen clip    gated delay line, the gate stored with the audio
delay            Behavior's wait plus jitter          D, fixed per node, 250 ms to seconds
loop guard       own-emit suppression (deaf while     the loud bound and a gain below 1 (keeps
                 it plays)                            listening)
memory           ring plus clip, both short           one ring of D
feel             an answer                            an echo, or an answer at a long D
```

## Open questions (the owner's calls)

1. D, and whether it varies per node.
2. 48 kHz with D about 1 s, 24 kHz with D about 2 s, or ADPCM.
3. The loud bound: a fixed level, or relative to the floor the node measures.
4. Whether distant quiet speech is rejected (test 4) or whispered back.
5. The gain per pass, which sets the self-echo tail.
6. Whether the echo room and Whispernet are one mode with a profile switch or two modes. Two, unless the detector
   and the output path turn out to share more than expected.
