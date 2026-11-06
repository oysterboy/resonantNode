# Param review: what each knob does, whether it still acts, what it is called (step 20.5, issue #88)

Status: proposal for the owner, 2026-11-05; the owner answered on 2026-11-06 (decisions/2026-11-06-retire-the-old-rest-triggers.md and decisions/2026-11-06-param-review-owner-answers.md); the tables below are the state before that and still list the retired params. Before that, nothing was decided; a rename or a retirement the owner takes gets a
`docs/decisions/` file, and the annotated list then goes to `notes_manual.md` (pointers to the code sites, no copied
defaults). Values: `PARAM DUMP` on the board; the sites are listed in `src/app/ParamBindings.h` and the echo modes'
`EchoApp::configureParams` / `Whispernet::describeParams`.

Renaming is cheap on the board: the store keys params by id (`ParamIds.h`), not by path, so a saved value survives a
new name. What a rename costs: old bench session headers and tools (`nodepreset.py`, `deskrun` plans) name the old
paths; a mapping table here and in the decision file covers them, as for RB PARAM in 2026-10-22.

Legend for "acts": **yes**; **cond.** = only in some setting (named); **no** = no effect in any shipped setting.

## Node: behavior.* (21)

| path | what it does | unit | acts | name |
|---|---|---|---|---|
| behavior.wait_after_heard_ms | fixed wait before an answer | ms | yes | ok; with the echo modes' `wait_ms` one name for all three would help (`answer_wait_ms`) |
| behavior.answer_jitter_ms | random 0..x added to that wait, shrinking with the level | ms | cond.: not with level_max 0 | it is a spread, not jitter: `answer_spread_ms` (owner's question 2026-11-05) |
| behavior.refractory_after_emit_ms | after the own sound, heard trains count but are not answered | ms | yes | ok, but one of three "after the own sound" windows (below) |
| behavior.suppress_self_chirp_ms | detection deaf while the own sound plays, plus this | ms | yes | "chirp" is the old signal's word; `self_deaf_ms`? |
| behavior.detection_suppress_tail_ms | detection deaf for the room tail after the own sound | ms | yes | the three windows want one family: `after_own.deaf_ms`, `after_own.tail_ms`, `after_own.no_answer_ms` |
| behavior.idle_enabled | the node calls on its own after a silence | bool | yes | "idle" is code-speak; `call_enabled` |
| behavior.idle_timeout_ms | the silence before the own call | ms | yes | `call_after_silence_ms` |
| behavior.idle_timeout_variation_ms | +- random on that | ms | yes | `call_after_silence_spread_ms` |
| behavior.idle_blocked_after_heard_ms | no own call this soon after hearing a train | ms | yes | `call_blocked_after_heard_ms` |
| behavior.idle_blocked_after_own_emit_ms | no own call this soon after the own sound | ms | yes | `call_blocked_after_own_ms` |
| behavior.rest_after_field_patterns | rest after n trains in the field window | count | **no** (0 since BEH-008, "until retired") | retire |
| behavior.own_emit_budget | rest after n own sounds per window | count | **no** (0, same) | retire |
| behavior.own_emit_budget_window_ms | that window | ms | **no** | retire |
| behavior.rest_ms | the pause at the top of the level | ms | cond.: level_max > 0 (or the two above) | ok |
| behavior.level_max | events (heard trains + own sounds) until the rest; 0 = automaton off | count | yes | reads as a level, acts as the chain's length: `chain_length` |
| behavior.level_decay_ms | the level drops 1 per this much silence | ms | cond.: level_max > 0 | `level_fall_ms`? (as whisper.room_fall_ms) |
| behavior.start_chance_pct | first answer's chance in a quiet field, rising evenly to the top | % | yes (0 = 1 / level_max) | ok (new) |
| behavior.base_scatter_bins | how far the base moves at a rest | bins | cond.: band per node or base bins | ok |
| behavior.band_per_node | each node its own window on the range | bool | yes | ok |
| behavior.band_range_low_hz / high_hz | the range the windows move on | Hz | cond.: band_per_node 1 | ok |

Not a param, but behaving like one (named on 2026-11-05): the jitter's shrink with the level (`answerJitterShrinks`),
no floor under it; the level's offset on the Node (`levelOffset`, a param only in the echo modes); a ramp's end (the
chance reaching 100 % before the top, the owner's plateau idea).

## Node: output.* and node.* (10)

| path | what it does | unit | acts | name |
|---|---|---|---|---|
| output.hop_enabled | the signal is the climbing train (else the plain pattern) | bool | yes | `signal_climbs`? or retire (nothing ships with it off) |
| output.hop_rotation | the start base from the roster | index | yes (the first window / base) | `start_base` |
| output.hop_pulse_ms | one step's length | ms | yes | `signal_step_ms` |
| output.hop_gap_ms | silence between steps (0 = one glide) | ms | yes | `signal_gap_ms` |
| output.hop_steps | steps per signal | count | yes | `signal_steps` |
| output.idle_announce_hz | the call's lead-in tone, 0 = none | Hz | yes | `call_lead_hz` |
| output.idle_announce_ms | its length | ms | yes | `call_lead_ms` |
| output.silent | outputs off, detection runs | bool | yes | ok |
| node.radio_enabled | the radio at boot | bool | yes (read at boot) | ok |
| node.radio_log | console lines per second by radio, 0 off | lines/s | yes | ok |

## Node: detection.* (24)

Detection params are tuning, a pass of their own (CLAUDE.md); names only here.

| path | what it does | acts | name |
|---|---|---|---|
| detection.scalar_onset_threshold / release_threshold | a sound starts above / ends below | yes | ok |
| detection.scalar_cooldown_ms | no new start this soon after one | yes | ok |
| detection.scalar_min_duration_ms / max_duration_ms | allowed length | yes | ok |
| detection.scalar_min_peak_strength | the peak's minimum | cond.: require_min_strength | ok |
| detection.scalar_require_min_strength | switches min_peak and min_strength on | yes | a switch for two limits; drop it (a limit of 0 is off) |
| detection.scalar_min_strength | the matched mean's minimum | **no** at 0 (shipped) | `min_mean_strength`; or retire |
| detection.scalar_release_debounce_ms | a dip shorter than this does not end the sound | yes | ok |
| detection.scalar_require_carrier_quality | switches coverage, island and gap checks on | yes | same as require_min_strength |
| detection.scalar_min_release_coverage_ms | time above release, summed | yes | `min_time_above_ms` |
| detection.scalar_min_longest_island_ms | the longest stretch above release | yes | ok |
| detection.scalar_max_gap_ms | the longest dip allowed inside | yes | ok |
| detection.band_low_hz / band_high_hz | the shared band | cond.: band_per_node 0 (else the window rules) | say so: `shared_band_*` |
| detection.contrast_medium / amp_medium | the inspectors' Medium limits | yes | ok |
| detection.shape_pre_ms / tail_ms | the shape window around the sound | yes | ok |
| detection.shape_max_rising_step_bins | the largest step up still counted | yes | ok |
| detection.shape_min_run_ms | a bin counts after this long | yes | ok |
| detection.shape_medium_min_bins / medium_min_rising_steps | the climb the Node needs | yes | ok |
| detection.shape_strong_min_bins / strong_min_rising_steps | the Strong class | **no** gate (the requirement is Medium; label only) | keep or retire with the Strong class |

## Echo room: echo.* (17) and Whispernet: whisper.* (33)

The echo modes repeat Behavior's knobs under other names; one vocabulary across the three would read better:

| meaning | Node | echo | whisper |
|---|---|---|---|
| fixed wait | behavior.wait_after_heard_ms | echo.wait_ms | (none: the delay line) |
| spread | behavior.answer_jitter_ms | echo.jitter_ms | (none) |
| after the own sound | behavior.refractory_after_emit_ms | echo.refractory_ms (+ echo.tail_ms) | (deaf while playing) |
| chain length | behavior.level_max | echo.level_max | whisper.level_max |
| start | behavior.start_chance_pct | echo.level_offset | whisper.level_offset |
| level fall | behavior.level_decay_ms | echo.level_decay_ms | whisper.level_decay_ms |
| rest | behavior.rest_ms | echo.rest_ms | whisper.rest_ms |
| loudness | (output level) | echo.gain | whisper.gain |

Proposal: the echo modes take start_chance_pct in place of level_offset (one rule in Behavior, levelOffset retired),
and the shared meanings take the Node's names under the mode's prefix.

Echo-only: onset / release / floor / min_peak / attack_ms (the clap), pre_ms / post_ms (the clip, capped at 300 ms
at 48 kHz: ECHO-004), enabled. Whisper-only: onset / release / floor / loud, modulation, flat_min / flat_max (the
timbre range, to change with the band check, research/whispernet.md 2026-11-05), delay_ms, pre / hold / tail /
ramp_ms (the gate), first_check / check_every / min_phrase / gap / max_phrase_ms (the phrase), adaptive /
room_factor / room_rise / room_fall_ms (the room level), aec / aec_taps / aec_mu / aec_delay / listen_while_playing
/ listen_erle_db (the canceller; listen_while_playing does not work yet). All act; names fine except `whisper.floor`
and `echo.floor` (a minimum level under the room-following onset, and the attack's floor: two meanings).

## For the owner to decide

1. Retire the two old rest triggers (3 params) and `detection.scalar_min_strength`.
2. The renames above, as one set with a mapping table (or a subset: the behavior names first).
3. One vocabulary in the echo modes; start_chance_pct there too.
4. The three hidden knobs as params: the jitter's floor (or its shrink on / off), a ramp's end (the plateau), and
   whether level_max 0 should keep ignoring the spread.
