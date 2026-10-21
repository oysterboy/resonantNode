# Listening through a comb: how a node hears a chirp, and why one pitch is not enough

Source material for a blog post and its graphs, 2026-10-15. Plain-language
explanation first, then the data behind each point. Every table is either
measured on the bench (session named, in `bench/sessions/` on the `bench`
branch) or computed from a formula or from measured data (marked
**derived**). Not a spec: the binding docs are `docs/specs/myspec.md`,
`docs/decisions/2026-10-15-signal-variation-inside-a-shared-band.md` and
`docs/research/frequency-drift-detection.md`.

---

## 1. The setup in one paragraph

ResonantNode is a set of small sound nodes in a room (ESP32, a 3 W speaker
behind a MAX98357A amplifier, an I2S MEMS microphone). A node listens. When
it hears another node's chirp, it may answer with a chirp of its own. The
whole piece is that conversation. So the core technical question is simple
to say: *did I just hear another node?*

Hardware facts used below: 16,000 samples per second (so nothing above
8 kHz exists for the node), chirp at 3,200 Hz, about 100 ms long, ramped
in and out over 5 ms.

## 2. How a node listens: one narrow window

The node does not "hear" the way we do. Every 4 samples (a quarter of a
millisecond) it looks at the last 64 samples (4 ms) and asks one question:
*how much 3,200 Hz is in here?* The tool for that is the Goertzel
algorithm: a single-frequency filter, cheaper than a full spectrum (FFT)
when you only care about one pitch.

With a 4 ms window the filter is not razor-sharp. It is about 250 Hz
wide, and tones close to 3,200 Hz still get through, partly:

**Table 1 - how much of a tone the detector sees, by how far off pitch it
is (derived: standard response of a 64-sample DFT bin at 16 kHz).**

| offset from 3,200 Hz (Hz) | seen, plain window | seen, Hann window |
|---|---|---|
| 0 | 1.000 | 1.000 |
| 25 | 0.984 | 0.994 |
| 50 | 0.936 | 0.975 |
| 75 | 0.858 | 0.945 |
| 100 | 0.757 | 0.904 |
| 125 | 0.637 | 0.853 |
| 150 | 0.505 | 0.794 |
| 175 | 0.368 | 0.729 |
| 200 | 0.234 | 0.659 |
| 225 | 0.109 | 0.586 |
| 250 | 0.000 | 0.512 |
| 275 | 0.089 | 0.438 |
| 300 | 0.156 | 0.367 |
| 325 | 0.198 | 0.300 |
| 350 | 0.216 | 0.238 |
| 375 | 0.212 | 0.182 |
| 400 | 0.189 | 0.133 |

Reading: today's detector (plain window) tolerates about +-100 Hz. At
250 Hz off it is blind. A Hann window (a soft fade at the window edges)
is wider and smoother, at the cost of selectivity.

*Graph idea:* line chart, x = offset (Hz), y = fraction seen, two lines.
Mark the +-100 Hz "comfortable" zone.

What the node does with that number: it watches the value over time.
A chirp is a short burst that rises above a threshold, stays there long
enough (60-300 ms), and falls again. Later checks ("inspectors") look at
the whole burst once it is over: was it strong, was it clean.

## 3. Why the room matters: the comb

The nodes stand on a surface with speaker and microphone facing up, so
nobody sees a "direction" in the network. The cost: the microphone hears
almost no direct sound. It hears reflections, from the ceiling, walls,
table. Each reflection travels a different distance. At some pitches the
copies arrive in step and add up, at others they arrive out of step and
cancel. Plotted over pitch, the received level looks like a comb with
broken teeth.

Measured: the Emitter played a 100 ms tone at each pitch, the receiver
recorded the raw microphone signal, and the tone's amplitude was computed
directly from the recording (detector not involved; `tools/bench/rawtone.py`).

**Table 2 - received tone amplitude by pitch, two spacings, same room,
same boards, same day (measured; sessions `2026-10-14-up-90cm-rawscan`,
`2026-10-14-up-100cm-rawscan`; E3 -> E2, facing up, 0.5 of full scale).**

| pitch (Hz) | 90 cm | 100 cm |
|---|---|---|
| 2800 | 38,655 | 60,457 |
| 2850 | 43,934 | 19,494 |
| 2900 | 42,772 | 48,674 |
| 2950 | 33,415 | 60,358 |
| 3000 | 31,828 | 71,565 |
| 3050 | 7,306 | 46,932 |
| 3100 | 22,397 | 28,222 |
| 3150 | 63,736 | 12,359 |
| 3200 | 59,735 | 11,065 |
| 3250 | 44,706 | 17,945 |
| 3300 | 69,620 | 35,276 |
| 3350 | 72,685 | 22,306 |
| 3400 | 36,279 | 3,632 |
| 3450 | 77,909 | 68,270 |
| 3500 | 75,345 | 71,454 |
| 3550 | 16,946 | 11,941 |
| 3600 | 58,636 | 36,052 |

Numbers worth quoting (derived from Table 2):

- Strongest / weakest pitch: 10.7x at 90 cm, 19.7x at 100 cm.
- 3,200 Hz, the pitch the nodes use: 59,735 at 90 cm, 11,065 at 100 cm.
  Moving the boards 10 cm costs a factor of 5.4.
- Correlation of the two curves (log amplitude): 0.02. In plain words:
  knowing the comb at 90 cm tells you nothing about the comb at 100 cm.
- A few features stay put (3,450-3,500 strong, 3,550 weak at both), a
  hint of a small fixed part from speaker and mic themselves.

Caveat: 50 Hz steps. Narrower notches than that are not resolved.

*Graph idea:* two lines over pitch, log y-axis, a vertical marker at
3,200 Hz. The point lands on a peak in one line and in a valley in the
other.

### Why the teeth move (derived)

The comb comes from path differences. A reflection that travels an extra
distance d cancels at pitches spaced c / d apart (c = speed of sound).
Teeth a few hundred Hz apart mean extra paths around a metre, which fits
a ceiling bounce. Move a board and d changes, so every tooth moves.

Temperature moves them too, without touching anything: all comb
frequencies scale with the speed of sound.

**Table 3 - speed of sound by air temperature, and how far the comb
moves at 3,200 Hz relative to 20 °C (derived: c = 331.3 + 0.606 * T).**

| air (°C) | c (m/s) | comb shift at 3,200 Hz |
|---|---|---|
| 15 | 340.4 | -28 Hz |
| 20 | 343.4 | 0 |
| 25 | 346.4 | +28 Hz |
| 30 | 349.5 | +57 Hz |

A notch in Table 2 is roughly 50-100 Hz wide, so a warm afternoon can
slide one onto or off the chirp. Consistent with the 24 h soak: same
boards, nothing moved, received strength drifted from 9.1k (08:00) to
11.2k (midday) and back to ~9.5k (14:00), about ten times the
block-to-block variation (session `2026-10-12-soak24h-70cm`, 300 chirps,
299 detected; no room temperature was logged, so the cause is not proven).

## 4. What the comb does to detection

Same story, now through the detector. 10 chirps per spacing, two pitches
100 Hz apart.

**Table 4 - chirps detected out of 10, by spacing and pitch (measured;
`2026-10-14-up-<N>cm-0p5` and `-0p5-f3300`, index in
`2026-10-14-up-spacing-map/README.md`).**

| spacing | 3,200 Hz | 3,300 Hz | 3,200 Hz strength |
|---|---|---|---|
| 60 cm | 0 | 10 | ~3,400 |
| 70 cm | 0 | 10 | 5,800-6,200 |
| 80 cm | 0 | 8 | ~4,500 |
| 90 cm | 10 | 10 | 7,600-8,100 |
| 100 cm | 0 | 0 | ~3,700 |
| 110 cm | 10 (and 50/50 in a longer run) | 10 | 5,900-6,600 |

Reading: at 3,200 Hz the node hears its neighbour at 2 of 6 spacings.
Moving the pitch by 100 Hz rescues 3 more and still loses one. No single
pitch works at every spacing, and a room of 5 nodes has 10 pairs, each
with its own comb. Making the chirp louder does not help: at 70 cm,
0.3 of full scale was worse than 0.5 (0 of 50, session
`2026-10-14-up-70cm-0p3`).

Contrast, same hardware with speaker facing microphone (line of sight,
110 cm, 0.3 of full scale; `docs/refactors/archive/damp-bench-check.md`):
4,000 / 4,800 / 5,600 / 6,400 Hz each detected 50/50. Facing each other,
the direct sound dominates and the comb hardly matters. The ripple is the
price of the omni-directional layout, which is an artistic choice that
stays.

*Graph idea:* small grid, spacing x pitch, filled = detected. Or a dot
plot of Table 4 next to Table 2.

## 5. The idea: let the pitch move

If any single pitch can land in a notch, do not use a single pitch. Two
ways to spread a signal over pitch:

- **sweep**: the pitch glides inside one chirp, say 2,800 -> 3,600 Hz in
  100 ms;
- **hopping**: a train of short pulses, each at a different pitch.

At any instant the tone is still at one pitch, and if that pitch is in a
notch, nothing can hear it. But it does not stay there. The notch turns
from "the whole chirp is missing" into "a few milliseconds are missing".

**Table 5 - what the receiver would get during a sweep 2,800 -> 3,600 Hz
over 100 ms, at the two measured spacings (derived: Table 2 interpolated
in log amplitude along the sweep; a model, not a recording).**

| time (ms) | pitch (Hz) | 90 cm | 100 cm |
|---|---|---|---|
| 0 | 2800 | 38,655 | 60,457 |
| 5 | 2840 | 42,823 | 24,446 |
| 10 | 2880 | 43,233 | 33,755 |
| 15 | 2920 | 38,750 | 53,048 |
| 20 | 2960 | 33,091 | 62,449 |
| 25 | 3000 | 31,828 | 71,565 |
| 30 | 3040 | 9,806 | 51,064 |
| 35 | 3080 | 14,308 | 34,589 |
| 40 | 3120 | 34,030 | 20,284 |
| 45 | 3160 | 62,915 | 12,089 |
| 50 | 3200 | 59,735 | 11,065 |
| 55 | 3240 | 47,374 | 16,291 |
| 60 | 3280 | 58,316 | 26,919 |
| 65 | 3320 | 70,830 | 29,367 |
| 70 | 3360 | 63,254 | 15,516 |
| 75 | 3400 | 36,279 | 3,632 |
| 80 | 3440 | 66,865 | 37,968 |
| 85 | 3480 | 76,360 | 70,163 |
| 90 | 3520 | 41,482 | 34,933 |
| 95 | 3560 | 21,721 | 14,894 |
| 100 | 3600 | 58,636 | 36,052 |

Reading: at 100 cm a fixed 3,200 Hz chirp sits at 11,065 for its whole
length, the level that failed 10 of 10 times. The sweep through the same
room spends most of its time well above that, with short dips (around
45-55 ms and at 75 ms). The detector already tolerates short gaps inside
a chirp (it was built for that), so dips of 5-10 ms are bridgeable.

*Graph idea:* the money chart. x = time, y = received level, three lines
at 100 cm: fixed 3,200 Hz (flat at 11,065), sweep (Table 5), and a
dashed detection threshold. Or: the comb from Table 2 with the sweep
drawn as a moving dot.

## 6. What the detector has to learn

A detector tuned to one 250 Hz-wide window cannot follow a sweep from
2,800 to 3,600 Hz. It needs to watch a band. The plan:

- several narrow windows side by side across a shared "network band",
  and at each moment take the strongest one;
- keep the 4 ms window (a sweep moves only ~30 Hz within 4 ms, so
  nothing smears; the problem is coverage, not resolution);
- add checks that look at the whole burst afterwards: was it a clean
  tone rather than broadband noise ("tonality"), did the pitch move
  smoothly like a sweep or jump like speech ("trajectory").

The trade-off is noise. A wider ear hears more of everything. The bench
already shows how sensitive this is: lowering the single pitch, which
moves it into the range where speech and music live, costs false
answers.

**Table 6 - false answers (node would have replied) in 50 windows of
speech or music, by chirp pitch (measured; 90 cm, facing up, DC blocker;
sessions `2026-10-14-dc-90cm-noise`, `-lowfreq-90cm-noise`,
`-midfreq-90cm-noise`; one run each).**

| pitch (Hz) | speech | music |
|---|---|---|
| 3,200 | 0 | 0 |
| 2,800 | 4 | 1 |
| 2,600 | 3 | 4 |
| 2,000 | 3 | 4 |
| 1,500 | 1 | 30 |

*Graph idea:* grouped bars by pitch. The cliff below 3,200 Hz is the
reason the band starts around 3 kHz unless the new checks pay for it.

## 7. Variation as art, robustness as a side effect

The owner's goal is not a better beep. Nodes should sound different from
each other and change over time; whether one plays sweeps and another
hops is an artistic choice. The technical decision is what makes that
possible: the detector stops looking for one known signal and listens
for "something tonal in the band", whatever its shape. Everything inside
the band is free to vary.

A second idea extends this: two layers per signal.

- an **aesthetic layer**, free in pitch. Musically the interesting
  range starts below 1 kHz;
- a **detection layer** in the band (around 3 kHz), possibly an overtone
  of the low note so it reads as timbre rather than a separate beep.

The hardware likes this. The speaker is far louder in the low range:

**Table 7 - received tone amplitude, 500-3,600 Hz, 90 cm, facing up
(measured; session `2026-10-14-up-90cm-rawscan-wide`, recomputed from the
raw captures with the method of `tools/bench/rawtone.py`; boards
re-plugged since Table 2, so 3,200 Hz reads 34,625 here vs 59,735 there).**

| pitch (Hz) | amplitude | pitch (Hz) | amplitude |
|---|---|---|---|
| 500 | 34,537 | 2100 | 45,572 |
| 600 | 134,301 | 2200 | 86,248 |
| 700 | 240,051 | 2300 | 14,177 |
| 800 | 273,850 | 2400 | 16,676 |
| 900 | 91,313 | 2500 | 43,353 |
| 1000 | 127,892 | 2600 | 83,452 |
| 1100 | 88,418 | 2700 | 85,601 |
| 1200 | 71,288 | 2800 | 37,354 |
| 1300 | 112,953 | 2900 | 34,449 |
| 1400 | 79,959 | 3000 | 30,741 |
| 1500 | 37,208 | 3100 | 26,325 |
| 1600 | 26,393 | 3200 | 34,625 |
| 1700 | 51,816 | 3300 | 33,908 |
| 1800 | 50,559 | 3400 | 55,084 |
| 1900 | 57,023 | 3500 | 57,469 |
| 2000 | 81,096 | 3600 | 66,866 |

Reading: around 700-800 Hz the same electrical level arrives 7-8x
stronger than at 3,200 Hz (speaker resonance plus room). The low layer
can be quiet in the mix and still carry the music. The same table shows
the comb at every pitch, not only near 3 kHz (2,300-2,400 Hz is a deep
valley at this placement). Two of the measured caveats: at 800 and
1,000 Hz the detector saturated and rang longer than its 200 ms limit
(session `2026-10-14-lowfreq-90cm`), and a loud low layer may leak a
little into the detection band (to be measured).

*Graph idea:* one line, log y, 500-3,600 Hz, shaded "aesthetic" (<1 kHz)
and "detection band" (~3 kHz) regions.

## 8. What it costs the little computer (derived)

The ESP32 has 62.5 µs per sample at 16 kHz and the node already uses
about 39 µs of it. That, not memory, is the limit.

**Table 8 - options for watching a band (derived: operation counts, not
timed on the board).**

| approach | work per sample | memory | change to the system |
|---|---|---|---|
| today: 3 windows, recomputed every 4 samples | ~64 loop steps | 512 B | - |
| 8 windows, computed in one pass | ~1.5-2x today | +~300 B | small |
| 8 windows, sliding DFT (updated, not recomputed) | about today or less | +~200 B | small, needs care with rounding |
| FFT, 64 points every 1 ms | bursty, similar on average | +1.3 KB | large: everything becomes block-based |
| ultrasound (>20 kHz) | 3x the samples, 20.8 µs budget | - | new sample rate, new task layout, unknown speaker/mic response |

Ultrasound was considered and dropped: it does not fit the time budget,
the parts are not specified for it, and visitors would no longer hear the
nodes talk.

## 9. Status and honesty notes

- Tables 2, 4, 6, 7 and the soak numbers are single sessions in one room
  with one pair of boards (E2, E3). Not yet repeated across rooms.
- The comb explanation (multipath via ceiling and walls) fits all the
  data but is not proven by a separate test (absorber above the pair, or
  the same scan facing each other).
- Tables 1, 3, 5, 8 are computed. Table 5 is a model built from Table 2,
  not a recording of a real sweep. The real sweep test needs the output
  side to play sweeps first (planned, step 5b of `roadmap-0-steps.md`).
- Nothing here is shipped yet. The plan: band detection and per-node
  variation before the 5-node field trial; the two-layer signal and
  variation over time after it.
