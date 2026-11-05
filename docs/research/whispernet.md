# Whispernet: a room that whispers back, a gated delay line on a whisper detector

```text
Status:  promoted (2026-10-30; owner's discussion of 2026-10-30, written up from it; a tryout slot the same evening,
         layout 1)
Became:  ECHO-002 (roadmap-echo.md), step 20 (roadmap-0-steps.md, issue #84), 2026-10-30
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

Without ESP-NOW the block would grow by a good margin, but the radio stays: NET OTA is the way in and out on
location (ECHO-003, 2026-10-30), so the budget is the block with the radio up. A WROVER module (PSRAM, same DevKit
footprint and pins) would make D of minutes possible but is a hardware change.

## Why the howl does not come back

echoSpace had an always-open gate with nothing to shape what passed through (`echo-room.md`, "Why echoSpace
howled or faded"). Here the gate is open only while the input passes the whisper window, and the window has a loud
bound. A loop with gain above 1 grows a little every D ms; the first pass that crosses the bound shuts the gate
within one block. The worst case is one short pass at roughly the bound times the gain: whisper level, not a
shriek. With a gain below 1 per pass it never gets there, and a node hearing its own output gives a decaying
multi-tap echo, each pass quieter, until it drops under the floor. That is echoSpace's "died off", but as a tail
behind a live whisper rather than the whole piece dying.

What this costs: the node must listen while it plays, so own-emit suppression is off in this mode and the
detector sees the live whisper plus the replay; the loud bound sits above that sum ("Suppression off: who hears
the replay" below). The modulation test sees a
smeared envelope, which is fine. Gating needs 10-20 ms ramps or it clicks. Without the selectiveness tests this is
echoSpace again; they are what make the gate safe.

## Behavior

`ResonantBehavior` fits with two readings changed:

- A gate-open episode is a heard train (+1), the node's own replay another (+1); the level climbs, and at
  levelMax the rest forces the gate closed for restMs, so a room full of whisperers goes quiet for a while.
  p = level / L becomes the probability that the gate opens for an episode. The echo build's own
  `BehaviorGateConfig`, as for the echo room; the Node's defaults are tuned for hop trains.
- Jitter has no meaning when D is fixed, so it goes. A per-node D does its job of spreading the room: decided
  400 ms .. 2 s (decisions/2026-11-01-whispernet-delay-per-node.md; the gate needs ~400 ms to open).

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

## Suppression off: who hears the replay (owner's question, 2026-10-30)

With suppression off, either the node's mic hears its own replay, or the speaker sits away from the own mic and
near a neighbour's. Both work, and the firmware is the same for both: the difference is one number per node, how
much of its own output its mic hears, and that number can be measured rather than assumed.

**Case 1: the mic hears the whisper and the own replay.** The ring holds the mix, so the replay is replayed D
later at gain g, then at g squared: a decaying tail of repeats at spacing D. With g below 1 and the loud bound this
is stable, and it is a sound, a whisper that smears and fades. Two things to do for it:

- Count the tail as one episode, not one own emission per pass, or the level automaton runs to the top on a single
  whisper.
- Make the floor and the loud bound aware of the own output. The node knows what it plays and when. If it measures
  its coupling at boot (a short burst and the level the mic reports, the way SELFTEST's amp check hears its own
  tone), it raises the floor by the expected own level while it plays. Cheap, and it keeps a reflective room from
  pinning the gate open. Full echo cancellation is not needed.

**Case 2: the speaker sits away from the own mic, near the neighbour's.** The speaker moves, not the mic: the
amp's output pair runs a metre or two of wire without complaint, while the mic shares BCLK and WS with the amp on
the one I2S port and its lines must stay short (a mic on a long cable corrupts the timing). With the speaker at the
next node's mic the chain is given by geometry: whisper into node 1, out at node 2's mic after D1, out at node 3's
mic after D2. Self-coupling is low, the tail of case 1 nearly absent, each hop one re-recording through air. Closed
into a ring, the whisper goes round and fades: Stille Post in its most literal form. The loop guard is the same loud
bound and g below 1, now across nodes instead of inside one.

**What it means for the design.** Build for case 1, because any room has reflections and the node must survive
some self-coupling anyway. Measure the coupling at boot and feed it to the floor and the bound. Then the layout
decides the character: speakers at their own nodes gives a room that smears, speakers at the neighbours gives a
line or a ring that passes the whisper on. A mixed layout is a set of different coupling numbers.

One point to say out loud: Behavior counts heard trains and own emissions per node. In case 2 a node's output is
mostly heard by its neighbour, so the level rises where the sound arrives, not where it was played. Probably right
for a chain; to be watched on the bench.

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
7. The layout: speakers at their own nodes (a room that smears) or at the neighbours' mics (a line or a ring);
   the coupling measured at boot either way.

## The detector in the inspection framework (step 20.3, 2026-11-01)

As the echo room's attack (decisions in roadmap-0-steps.md 20.2: one framework for the echo modes' detection), the
whisper tests become inspection modules judged by makeVerdict, run by EnvelopeRuntime (src/detection), the chain the
echo shell and native-echoreplay share:

- **The detector**: ScalarTransientDetector on the envelope, its duration window 300 ms .. 3 s, its levels just above
  the room's floor (the onset then marks a phrase's start, not a transient).
- **LevelWindow** (new module, target Loudness): the occurrence's envelope (P90 over its span) between a floor and the
  **loud bound**; above the bound class None (speech, a clap, a door, the own loop's growth: the howl guard).
- **Modulation** (new module, target Modulation): the envelope's movement at syllable rate over the occurrence (or the
  history's last 512 ms): the envelope minus its slow mean, its RMS against the mean (a modulation index), classed by
  thresholds; a fan or HVAC reads near 0.
- **A provisional inspection while the phrase runs**: the detector's open candidate (a read-only accessor: active,
  since when) is inspected once it is 300 ms old, and again every ~100 ms; a valid verdict opens the gate from the
  candidate's start (the delay line still holds it, D >= the evidence time), the release closes it. Behavior counts a
  gate-open episode as a heard train and its own replay as an emission (roadmap-echo.md ECHO-002 item 4).
- **The history**: 64 bins of 8 ms (FEATURE_HISTORY_BIN_MS 8), ~0.5 s of envelope, enough for 2-10 Hz.

Test set: tools/bench/synthwhisper.py (a noise vocoder on bench_audio.wav's speech: whispers at -45 dBFS, the same
speech voiced at -25, music, a fan, claps, a mix), replayed with native-echoreplay.

First host result (2026-11-01, LevelWindow and Modulation built; `native-echoreplay --plan whisper --rate 24000` on
the synthetic set; a phrase is one occurrence, syllable gaps bridged by a 300 ms release debounce, judged when it
closes, levels and modulation over its last 512 ms on an 8 ms grid; loud bound 20000, modulation Medium from 0.3):

| file | phrases | passed as whisper |
|---|---|---|
| whisper_female, whisper_male | 7, 7 | 7, 7 (all; level P90 3400-15700, modulation 0.47-0.93) |
| speech_male, music | 1, 1 | 0 (P90 32767, saturated) |
| speech_female | 5 | 1 (one phrase's P90 18730, under the bound) |
| fan | 1 | 0 (modulation 0.13) |
| claps | 0 | - (shorter than 300 ms) |
| mixed | 5 | 2 of the 2 whispers; whisper over the fan missed (modulation 0.29) |

Readings: per phrase, voiced speech is as modulated as a whisper (0.4-0.7; the file-wide 0.3 of the prototype
averaged the pauses away), so only the level separates speech from whisper here, by the 20 dB between the files.
At a closer or quieter talker that margin goes: a voicing check (the optional third module: a voiced/unvoiced
spectral reading) is the one that would hold. A fan under a whisper lowers the modulation index (the fan's constant
level enters the mean): the floor's level, not 0, is what the mean should be taken against. Both are for the tuning
on recordings, not the synthetic set.

The provisional inspection (2026-11-01, built): EnvelopeRuntime::setProvisional(300, 100) judges the detector's open
candidate (ScalarTransientDetector::openCandidate, read-only) at 300 ms and every 100 ms after, skipped while the
candidate is below release (a clap's tail sits in the release debounce and would read as modulated). The gate is the
shell's rule, modelled in native-echoreplay: opened by a valid provisional verdict (the delay line passes the phrase
from its start, at most 2 s back), closed by a failing one or the phrase's end, and kept shut to the phrase's end
once the phrase was too loud (speech has quiet half-seconds that read inside the window). On the synthetic set:

| file | gate opens | gated | first opening after |
|---|---|---|---|
| whisper_female, whisper_male | 9, 7 | 24.9 of 24.9 s, 22.3 of 22.3 s | 308, 341 ms |
| speech_female, speech_male, music, fan, claps | 0 | 0 s | - |
| mixed (whisper, speech, fan, whisper over fan) | 11 | 17.7 s: the whisper 7.8 s, whisper over fan ~9.9 s in 9 episodes, speech and fan 0 | 377 ms |

The female speech phrase the closed-phrase judgement let through (above) does not open the gate: it was too loud
earlier in the phrase. Whisper over the fan flickers at the modulation threshold (0.3); the floor-referenced mean
(above) is the candidate fix.

The delay line (2026-11-01, built): GatedDelayLine (src/audio, pure, test_gated_delay): int16 samples delayed by D, a
gate byte per 128-sample block that can be set after the samples came in while they are unplayed (pass from the
phrase's start), set for each new block while open, the gain ramped over 10 ms at each edge. At 24 kHz and D = 2 s it
holds 96 KB of samples and 377 bytes of gate. native-echoreplay --plan whisper --out-dir DIR writes what it would play
(DIR/<name>.gated.wav): on the synthetic set the whispers come out whole at their input level (-45.4 dBFS from -45),
speech, music, fan and claps as silence, the mixed file 17.5 s (its whisper and its whisper over the fan).

The howl simulation (2026-11-01): native-echoreplay --loop-gain G feeds the delay line's output back into the input
at G (one round = D = 2 s, so G > 1 grows by 20 log G dB per round), 20 s of silence after the file. whisper_female
(-45 dBFS):

| loop gain | gate opens | gated | output, loudest second | output, last 5 s |
|---|---|---|---|---|
| 0.5 | 9 | 34.7 s (the echo's echo, decaying) | -39.6 dBFS | silent |
| 1.0 | 8 | 17.6 s | -36.6 | silent |
| 1.5 | 5 | 15.9 s | -37.1 | silent |
| 2.0 | 5 | 10.8 s | -35.9 | silent |
| 4.0 | 5 | 9.3 s | -36.4 | silent |

No gain makes it howl: a grown replay crosses the loud bound within a round or two and keeps the gate shut to the
phrase's end, and the fan never opens it. The loop here is a flat gain; a room's loop has peaks (a howl there is a
tone, which reads unmodulated and shuts the gate as well). The two-board test on the desk is still the decision.

## A phrase longer than D (design note, 2026-11-04)

Since f3ddba37 a node does not listen while its own replay plays: at an audible gain (64) it heard itself louder
than the whisper and answered itself. The replay of a phrase starts D after the phrase's start, so a phrase longer
than D is cut where the replay begins: from there the mic holds the node's own output, and passing it would feed
the replay back into the line (a loop at gain 64 x the coupling, a howl). Ways out, none built: D near its top (2 s)
on the nodes meant for long phrases; echo cancellation (the node knows exactly what it plays: an adaptive filter
subtracting it from the mic, ~64-128 taps at 24 kHz, a few MAC/s million, the ESP32 can) which would also let the
node hear others while it plays; or accepting the cut as part of the piece. Owner's call; the cut stays until then.

## Whisper against the room's noises: what separates them (bench, 2026-11-05)

The desk session on E3 (bench/sessions/2026-11-05-whisper-listen-e3): the owner found whispers and rustle passed
about equally often. The board answers 0.3-1.4 s into a sound, before the flatness range (0.15-0.40) tells them
apart; only 4 of 31 phrases passed at their end; phrases ran 9-18 s because the release level is not reached between
sentences. Labelled recordings from a C920 webcam beside E3 (bench/sessions/2026-11-05-labels-c920, one speaker,
one room, a camera with its own level processing, nothing above 8 kHz): against the room's own spectrum, a whisper
adds 7-14 dB in 1.5-4.5 kHz and only 2-9 dB below 700 Hz; speech, typing, chair, paper and clothes add 12-21 dB
(median) below 700 Hz. On the full band a whisper is only ~4 dB over this room, whose power is low-frequency.
Candidates (the owner, 2026-11-05: Whispernet "as time allows"; the first two built the same evening as params, off
by default: whisper.level_band, whisper.timbre_check 1 with whisper.low_max_db; not yet on a board): the onset and release levels on a
1.5-4.5 kHz band instead of the full band; a check that little is added below 700 Hz (a param) in place of or next
to the flatness range; the decision about 1 s in rather than 0.3 s. Established alternatives looked at: speech /
non-speech features (4 Hz syllable modulation), WebRTC VAD (voiced speech, weak on a whisper), Silero VAD and
ESP-SR (too big for the ESP32 without PSRAM), a small trained classifier on mel bands (needs 30-60 min of labelled
audio). To check on the board's own mic first.
