# 2026-11-16 the cricket call in the field: detection rate

The trill-climb call (main 6e56f53b, `tools/bench/presets/cricket.preset`): 10 pulses of 15 ms on the base pitch with
15 ms gaps, then the 5-step glide (15 ms, no gap), 375 ms; `detection.scalar_release_debounce_ms` 20 so the trill
stays one sound. The owner liked it by ear.

## Setup

- Gateway E0 (COM12); R1, E2 play, E3 listens (saved muted: it reboots when it plays, see
  2026-11-16-low-window-field). All three on 6e56f53b by radio OTA, the preset live, not saved.
- Window as stored: one window 2500-3500 Hz (band_per_node 1, base 0, nothing moves). **Not the window of the
  earlier run** (250-1250 Hz): the call and the window both differ from it.
- 905 s logged (`run1.log`), boardview via E0; `changes.txt` has the times.

## Result

With two players every valid detection at one is of the other's call, so the rate is the listener's valid
detections over the other's calls (window-free):

| | glide, 250-1250 Hz (earlier run) | cricket, 2500-3500 Hz |
|---|---|---|
| R1 -> E2 | 105 / 118 = 89 % | 98 / 116 = 84 % |
| E2 -> R1 | 102 / 125 = 82 % | 97 / 117 = 83 % |
| E3 valid (muted listener) | 11 of 243 calls | 106 of 233 calls |

- R1 and E2 logged no rejected detection at all; every miss is a call with no detection started (the node deaf
  after its own call, an overlap, or below the onset). Valid detections lasted 377-410 ms: the whole call is read
  as one sound, trill included.
- `heard_by.py run1.log 0.9`: the 0.4 s window of the earlier analysis is too short for a 375 ms call (the verdict
  lands after the call ends); wider windows start to catch the next call's verdict, so the per-window table moves
  with the window (81-98 %). The count above is the figure to compare.
- E3 still calls about half of what it hears a single tone (95 rejected `single_pulse`), but on 2500-3500 it
  accepts 106, against 11 on 250-1250: E3's trouble is worse low.
- The verdict name prints `single_pulse` on valid detections too; it is the occurrence's shape label, not the
  outcome (`valid=` is).

## Open

- The glide on this same window, 15 min, so only the call differs.
- Whether the trill costs anything in the inspection's first 100 ms (contrast, amp): no such rejection here.
