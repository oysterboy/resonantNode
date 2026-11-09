# Operator Guide

Status: current operator guide, rewritten 2026-10-25 (replaces the 2026-09 Setup Notes, whose
2026-10-10 drift banner listed names that no longer existed).
Scope: how to run, read and change the firmware that exists today, in all three build modes.
Purpose: answer "how does what we built work" and "where do I change this knob". For "where does
this concept belong" read `docs/specs/myspec.md`; for "what next" read `docs/roadmaps/`.

This file points, it does not copy. A value quoted here is the value on the date given; the
source of truth for a number is `PARAM DUMP` on the board or the config header named next to it.
When you find a stale name here, fix the pointer, do not add another table of values.

---

## Builds and boards

```text
platformio run -e esp32dev            Node (RB, the field firmware)
platformio run -e esp32dev-analyzer   Analyzer (default env)
platformio run -e esp32dev-emitter    Emitter (signal source only)
platformio run -e esp32dev-echo       the echo room at 24 kHz since 2026-11-05 (48 kHz before; D-AMP only; ECHO-003, ECHO-004)
platformio run -e esp32dev-whisper    the same shell as Whispernet at 24 kHz (D-AMP only; ECHO-002, 2026-11-01)
```

These build the D-AMP board (MAX98357A amp and I2S mic on one full-duplex port), the node hardware
since 2026-10-09. The discontinued piezo boards build as `esp32dev-piezo`, `esp32dev-piezo-analyzer`,
`esp32dev-piezo-emitter` (`-D BOARD_PIEZO`). Pins and I2S slots per board: `src/app/BoardConfig.h`.
Never flash a piezo build onto a D-AMP board or the reverse: the pinouts collide on GPIO 25/26.

What each image is built from (2026-11-01): the Node, Analyzer and Emitter builds leave out the echo shell
(`src/modes/echo/`) and the echo modes' inspection modules (`src/detection/inspection/EchoInspection.cpp`, built
only with `ECHO_INSPECTION`); the echo and whisper builds leave out the Node's mode, the band bank
(`FreqBandStream.cpp`) and `DetectionRuntime.cpp`. The filters and flags are the envs in `platformio.ini`.

Build-time knobs (not params, they need a rebuild): `platformio.ini` build flags
(`CHIRP_FREQUENCY_HZ`, `BUILD_VERSION`), tone level `I2S_TONE_AMPLITUDE` and the other defaults in
`src/app/RuntimeDefaults.h`, the node roster (name and the hop train's base bin per board MAC; 0..3 on the 8-bin band since 2026-10-30) in
`src/app/NodeRoster.h`.

Host-side builds and tests (no board): `test/README`.

## The three modes

- **Node / RB** (`src/modes/resonant/`): listens, detects, and answers through `ResonantBehavior`.
  Use it for live behavior checks and for the field.
- **Analyzer** (`src/modes/analyzer/`): runs SEQ / OBS trials against a known signal and prints
  per-trial reports. No reactive output. Use it for detection validation and tuning.
- **Emitter** (`src/modes/emitter/`): plays chirps, pulse trains and sweeps on command or on a
  timer. Use it as the signal source for Analyzer runs.

Node and Analyzer share the detection profiles and `DetectionRuntime`; they differ in what happens
after the verdict (behavior and output on the Node, reporting on the Analyzer).

Serial lines, every mode and every port (USB console, the Analyzer <-> Emitter
link): one reader (`src/app/SerialLine.h`). A line ends at `\n`; CR and
non-printable bytes are dropped. A line longer than the buffer (255 characters
on the Node's and the Analyzer's USB console, 95 on both Emitter ports, USB and the
link from the Analyzer; 2026-10-26) is refused whole with
`ERR line_too_long max=N` (`RB ERR ...` on the Node), never run cut. A number
in a command that is not a plain decimal is refused with `... bad value
token=<token>`; nothing is read as 0 (since 2026-10-26;
decisions/2026-10-25-analyzer-params-on-registry-and-line-policy.md).

## Node (RB)

`RB help` prints the command surface. As of 2026-10-26:

```text
RB STATUS                       state, profile, output.* status, silent=
RB PROFILE [name=...]           show or switch the detection profile (TonalPulseScalar | AmpExperimental)
RB CALL                         play the node's network signal now (a kick for multi-node runs)
RB log off|minimal|full         serial log level
RB summary | RB stop | RB rebase [force]
PARAM LIST|GET|SET|DUMP         every registered knob (see below)
PARAM SAVE|LOAD|RESET           persistence to NVS
NET <target|*> <command>        any Node command by radio (PARAM, RB, SELFTEST to one named node), NET PING
NET STATUS                      the gateway's own radio counters
SELFTEST [external wait_ms=N | chirp n=N gap_ms=MS dur_ms=MS | help]
```

A line the Node does not know gets `RB ERR unknown_command` (since 2026-10-28; before, no reply).

`RB PARAM` and `RB BEHAV` are gone since 2026-10-22. Old session headers map through
`docs/decisions/2026-10-22-live-params-profile-reset-no-rb-behav.md`.

### Params

Every tunable knob on the Node is a registered param in `lib/ParamRegistry/` (an in-repo library
that includes nothing from `src/`). `PARAM LIST` prints the paths with their ranges, `PARAM DUMP`
the current values. Each config describes its own params (`param::describeParams`, next to the struct; the
list of sites is in `src/app/ParamBindings.h`); `NodeParams::describe()` in
`src/modes/resonant/ResonantNodeParams.cpp` registers them in the PARAM LIST order. The groups:

```text
detection.scalar_*        ScalarTransientDetector thresholds and timing (13 params)
detection.band_low_hz / band_high_hz / amp_medium / contrast_medium
detection.shape_*         the shape inspector (DET-015): look-back, tail, run and step limits (8 params;
                          its level is detection.scalar_release_threshold)
behavior.*                wait, refractory, self-suppression, idle call, rest, the level automaton
                          (chain_length, level_fall_ms, answer_spread_ms, start_chance_pct since 2026-11-05, plateau_pct and spread_shrinks
                          since 2026-11-06; the Behavior paths were renamed on
                          2026-11-06, the old-to-new table is in decisions/2026-11-06-param-review-owner-answers.md)
output.*                  hop train, idle announce, silent (8 params)
node.radio_enabled        ESP-NOW on or off
node.radio_log            console lines per second mirrored to the radio (0 off; 2026-11-05)
```

Edge values (review 12.0, 2026-10-29): `detection.scalar_max_duration_ms 0` is no limit (the detector and RB
hop_check). `output.idle_announce_hz` 1..99 plays at 100 Hz, the lowest tone the output plays (0 is no
announcement). A band wider than the bank (8 bins) is clamped at its high edge, and `RB plan_check` prints
`shape_level_out_of_domain` (status=warn) when the shape rule's level, the release threshold, is not a
FrequencyTarget score (another stream, or above 32767).

Where the defaults live: `detection.*` in `src/detection/DetectionProfile.h` (the profile
structs), `behavior.*` in `src/behavior/BehaviorProfile.h` (`BehaviorGateConfig`), `output.*` in
`src/app/RuntimeDefaults.h` and the roster. The Node's shipped `behavior.*` values are `shippedNodeBehavior()` in
`BehaviorProfile.h` (the 2026-11-05 overnight field, confirmed as the defaults on 2026-11-09); the struct's own
defaults stay for the host tests, the field simulation and the echo modes. Retuning a default is its own pass, logged
through `tools/logging/`; do not change these headers as a side effect of other work.

### Param reference (Node)

What each param does, its unit and when it acts. Values are not copied here: `PARAM DUMP` on the board shows them,
the sites are listed above. Paths renamed on 2026-11-06 are mapped in
`docs/decisions/2026-11-06-param-review-owner-answers.md`; a `PARAM SET` with an old path answers `unknown_param`.
**cond.** means the param acts only in the setting named.

`behavior.*`: how the node answers and calls.

| path | what it does | unit | acts |
|---|---|---|---|
| `wait_after_heard_ms` | fixed wait before an answer | ms | yes |
| `answer_spread_ms` | a random 0..x added to that wait, shrinking with the level (`spread_shrinks`) | ms | cond.: `chain_length` > 0 |
| `spread_shrinks` | the spread shrinks as the level rises; 0 keeps the full window | bool | cond.: `chain_length` > 0 |
| `after_own_deaf_ms` | detection deaf while the own sound plays, plus this | ms | yes |
| `after_own_tail_ms` | detection deaf for the room's tail after the own sound | ms | yes |
| `after_own_no_answer_ms` | after the own sound, heard trains count but are not answered | ms | yes |
| `call_enabled` | the node calls on its own after a silence | bool | yes |
| `call_after_silence_ms` | the silence before the own call | ms | yes |
| `call_after_silence_spread_ms` | plus or minus a random amount on that | ms | yes |
| `call_blocked_after_heard_ms` | no own call this soon after hearing a train | ms | yes |
| `call_blocked_after_own_ms` | no own call this soon after the own sound | ms | yes |
| `chain_length` | events (heard trains plus own sounds) until the rest; 0 switches the level automaton off | count | yes |
| `level_fall_ms` | the level drops by 1 per this much silence | ms | cond.: `chain_length` > 0 |
| `start_chance_pct` | chance of answering the first train in a quiet field; it rises evenly to 100 % | % | yes (0 = 1 in `chain_length`) |
| `plateau_pct` | the chance reaches 100 % after this share of the ramp, certain beyond | % | cond.: `start_chance_pct` > 0 |
| `rest_ms` | the pause at the top of the level | ms | cond.: `chain_length` > 0 |
| `band_per_node` | each node its own window on the range, so the windows differ per node | bool | yes |
| `band_range_low_hz`, `band_range_high_hz` | the range the windows sit on | Hz | cond.: `band_per_node` 1 |
| `base_scatter_bins` | how far the base moves at a rest (0 = anywhere) | bins | cond.: `band_per_node` 1 |

`output.*` and `node.*`: the sound and the node's own switches.

| path | what it does | unit | acts |
|---|---|---|---|
| `output.hop_enabled` | the signal is the climbing train (else the plain pattern) | bool | yes |
| `output.hop_rotation` | the start base from the roster | index | yes |
| `output.hop_pulse_ms` | one step's length | ms | yes |
| `output.hop_gap_ms` | silence between steps (0 = one glide) | ms | yes |
| `output.hop_steps` | steps per signal | count | yes |
| `output.idle_announce_hz`, `output.idle_announce_ms` | the call's lead-in tone and its length (0 Hz = none) | Hz, ms | yes |
| `output.silent` | outputs off, detection keeps running | bool | yes |
| `node.radio_enabled` | the radio at boot | bool | yes, read at boot |
| `node.radio_log` | console lines per second mirrored to the radio, 0 off | lines/s | yes |

`detection.*` is tuning, a pass of its own: `scalar_onset_threshold` and `scalar_release_threshold` (a sound starts
above, ends below), `scalar_cooldown_ms`, `scalar_min_duration_ms` / `scalar_max_duration_ms` (allowed length, 0 =
no limit), `scalar_release_debounce_ms`, `scalar_max_gap_ms`, `scalar_min_release_coverage_ms` and
`scalar_min_longest_island_ms` (time above release, summed and longest), `scalar_min_peak_strength`, the two
switches `scalar_require_min_strength` and `scalar_require_carrier_quality` (each turns its limits on), `band_low_hz`
/ `band_high_hz` (the shared band; the window rules while `behavior.band_per_node` is 1), `amp_medium` and
`contrast_medium` (the inspectors' limits), and `shape_*` (the climb the hop train must show).

### Apply points, in boot order

- `Node::begin()`: `configureParameters()`, audio source and signal start, detection reset, then
  `applyActiveProfiles()` (detection profile and behavior gate config from the headers), then the
  roster lookup (node name, base bin of the hop train; MAC fallback), then `captureDefaults()` so the registry
  knows the firmware defaults, then `loadStoredParams()`: a valid stored value wins, `PARAM stale`
  is printed per entry saved against a default that has since changed. The radio comes up last, so
  a stored `node.radio_enabled 0` keeps it off.
- `PARAM SET <path> <value>`: writes the bound field and applies its module in place, no detector
  reset. Live only, lost on reboot.
- `PARAM SAVE`: the only flash write; stores the params whose value differs from the firmware default and drops the
  stored entry of one that is back at its default, so a later default is not hidden by a value nobody chose (PAR-018,
  `docs/decisions/2026-11-09-save-only-what-differs.md`). The line reads `written= unchanged= removed= failed=`.
  `PARAM LOAD` returns every param to its default and then applies the store (so it also undoes a live
  `PARAM SET` of a param that has no entry), `PARAM RESET` clears it and restores the firmware defaults
  (`docs/decisions/2026-10-23-param-persistence-save-only.md`). A board saved before 2026-11-09 still holds an entry
  for every param; `PARAM SAVE` once drops those that equal the default.
- `RB PROFILE name=...`: switches the profile and reapplies detection and behavior from the profile
  structs, which replaces the bound `detection.*` fields wholesale. Live `PARAM SET` overrides on
  them are gone after a switch; check `PARAM DUMP`.
- Boot prints `PARAM load stored=N applied=N stale=N invalid=N` plus one line per stored value that
  differs from its default.

### Silent mode

`PARAM SET output.silent 1` keeps the node listening (verdicts counted, `decision=outputs_off` in
the log) but it never sounds: no answer, no idle call, `RB CALL` ignored. With `PARAM SAVE` it boots
silent (`RB silent=1` at boot) until `PARAM SET output.silent 0` plus SAVE, or `PARAM RESET`.
`RB STATUS` shows `silent=`. SELFTEST's amp check still plays its tones (an explicit command); the own-chirp check
goes through Behavior and is skipped on a silent node (`own_chirp_suppressed result=SKIP reason=silent`), and
`SELFTEST chirp` answers `SELFTEST_CHIRP skip reason=silent`: set `output.silent 0` first to run them.

### Firmware over WiFi (OTA)

The boards pull a new image over WiFi (decisions/2026-10-28-ota-firmware-pull-over-http.md):

1. Credentials and token: `include/wifi_secrets.h` (copy `include/wifi_secrets.example.h`; git ignores it) holds
   `WIFI_SSID`, `WIFI_PASSWORD` and `OTA_TOKEN`, the shared token every OTA command must carry (owner 2026-10-29). A
   build without the file or without `OTA_TOKEN` answers every OTA with `OTA result=no_credentials`; a command with a
   missing or wrong `token=` answers `OTA result=bad_token` and changes nothing. A url longer than 159 characters is
   refused (`bad_command`), not cut.
2. Build, then serve the images from the lab PC: `python tools/bench/otaserve.py` (prints the full command lines,
   token included, and binds to the LAN address it prints). The PC's firewall must allow inbound TCP 8070 on the
   private network (rule "resonantNode OTA (8070)"). The images hold the WiFi password in plain text: stop the
   server (Ctrl+C) once the boards have updated.
3. On a USB Node: `NET * OTA url=http://<pc>:8070/node.bin token=<t>` (all other Nodes by radio), then
   `OTA url=http://<pc>:8070/node.bin token=<t>` (itself). On the Analyzer `.../analyzer.bin token=<t>`, on the
   Emitter's USB `.../emitter.bin token=<t>`. The Node closes a sounding emission first, the Emitter stops its tone.
4. Each board prints `OTA start`, `OTA wifi ip=`, `OTA progress pct=`, `OTA result=ok rebooting`, boots the new
   image (`OTA pending_verify confirm_after_ms=20000 reboot_deadline_ms=60000` at boot), and after 20 s of uptime,
   from its mode's loop, `OTA confirmed partition=app0|app1`. A new image that crashes, resets, or has not confirmed
   by 60 s of uptime (a hang: the deadline restarts it) falls back to its previous image (`src/app/FirmwareOta.h`).
   `OTA result=download_failed|wifi_timeout` leaves the old image running and the radio on its channel (WiFi joins
   without auto-reconnect and without storing the credentials in NVS). A board that sounds closes its emission
   first; the echo modes free their big buffers before the pull (`ECHO ota largest_block=`, 2026-11-05) and take
   them back after a failed one (`ECHO ERR ring_or_clip_alloc_failed` / `whisper_alloc_failed` if the heap refuses).
   The Analyzer refuses `OTA` during a SEQ run: `OTA result=busy reason=seq_active` (2026-11-05).

### Remote commands (radio)

Any Node on USB is the gateway: `NET <target> <command>`, target `*` (all) or a roster name
(`E1`..`E5`). Every Node command goes (PARAM, RB, SELFTEST, OTA) plus `NET PING`; each node answers with
what it would print on serial, shown as `NET reply from=E3 seq=N part=P <line>` and
`NET reply from=E3 seq=N done parts=K`. Since 2026-10-30 a lost frame shows: the line it cut is printed with `gap=1`, and the done line
adds `missing=M`. `NET STATUS` shows the gateway's own radio counters. The
radio is on by default; `PARAM SET node.radio_enabled 0`, `PARAM SAVE`, reboot turns it off.
The gateway prints `NET sent seq=N target=T text=<command>`, or `NET ERR outbox_full (nothing sent)` when the
radio's outbox is full; in `NET sent` and in the receiver's `NET cmd from=` an OTA `token=` value prints as
`<hidden>` (2026-10-29, review 12.0).

Operator rules (the firmware does not check these, except where noted):

- `SELFTEST` by radio: one node at a time, all others silent (`NET * PARAM SET output.silent 1`
  first). The gateway and the node refuse `NET * SELFTEST`. The test blocks its node for seconds
  and plays tones.
- `RB CALL` makes the node sound now (the start of a chain): only when you want the network to run.
- `RB PROFILE name=...` resets live detection and behavior values to the profile's (it lists them).
- `RB rebase` blocks its node for about a second (audio baseline), as at boot.
- `PARAM SAVE` costs every node it reaches about 8 audio buffers (the flash write); `NET * PARAM
  SAVE` hits all nodes at once. Save outside a session.
- Large replies are slow on purpose: a node sends one reply frame per 150 ms, so `NET * PARAM DUMP`
  takes about 2 s and `NET * RB STATUS` about 1 s; the gateway keeps its audio. Do not cut the wait
  short with another command: a new command to a node ends its reply still in progress.
- Anyone with ESP-NOW on channel 1 and the network id can send these commands: fine for the
  installation, not for a public space (`docs/decisions/2026-10-25-remote-params-espnow.md`).

The radio gateway board (2026-11-05, `esp32dev-gateway`, `docs/decisions/2026-11-05-radio-gateway-board.md`): a bare
ESP32 with no mic and no amp on a USB data port, named by the roster (E0). Its console takes `NET <name|*> <command>`
as on a Node (PARAM, RB, ECHO, WHISPER, SELFTEST, OTA, NET PING), `NET STATUS` and `OTA url=... token=...` for its
own update; it prints `NET reply from=...` and the boards' `NET log from=...` lines. It runs no commands of the
others and plays nothing, so every board with a mic and an amp can play in any mode. Its first flash needs the
BOOT button held (a CH340 board without auto-reset: `esptool ... --connect-attempts 80` with BOOT down); after that
the update goes over WiFi like the others'. A new board prints `GATEWAY efuse_mac=0x...` for its roster line.

`boardview.py --boards E0=COMx --radio E1,E2,E3,E4,E5 --poll "RB STATUS"` shows every board through it.

The log by radio (2026-11-05, `docs/decisions/2026-11-05-radio-log-mirror.md`): `node.radio_log` N (0 off, the
default; up to 20) makes a board send every console line it prints, outside a remote command's reply, as one radio
frame, at most N lines per second on average (the rest counted as dropped); in all three modes, with the same path.
Every board on USB prints the others' lines as `NET log from=E1 <line>` (lines starting `NET ` are not sent, a line
is cut at 233 characters). Lossy by design: no repeats, nothing resent. `NET STATUS` / `NET PING` add `log=`,
`log_sent=`, `log_dropped=`. For boards on a power-only charger: `NET * PARAM SET node.radio_log 5`, and the board
view with `--radio`. What reaches the log is what the board prints: `RB log full` prints each verdict.

### Startup quiet gate

The Node waits for the room to be quiet before it arms: `NodeBaseline::Config::quietThreshold` in
`src/modes/resonant/ResonantNodeBaseline.h`, one value per board (D-AMP and piezo differ); the warmup, hold,
timeout and settle times sit beside it.

## Analyzer

`HELP` prints the top-level commands, `SEQ help` the SEQ surface. As of 2026-10-26:

```text
SEQ start [N|tries=N] [period=MS] [window=MS] [freq=HZ] [dur=MS] [delay=MS] [report_settle=MS] [test=LABEL]
                                                             tries >= 1, freq= and hop= entries 1..7900 (2026-10-29)
SEQ start ... band=LOW-HIGH|off                              score the strongest bin in the band (default off)
SEQ OBS start ...               passive observe mode for an external emitter already running
SEQ start ... pulses=N gap=MS [ramp=MS] [hop=F1,F2,...]      pulse train per trial (DET-011)
SEQ start ... pulses=N ... [level=FS] [att=DB1,DB2,...]       the train at FS (0-0.999) and each pulse
                                                             DB below it, cycled (the margin test; D-AMP only)
SEQ start ... freq=START_HZ dur=MS sweep=END_HZ              sweep inside the chirp (OUT-005)
SEQ stop | SEQ STATUS | SEQ SUMMARY | SEQ REPORT | SEQ TRIES N
SEQ REPORT                                                   the detection values the current (or last) run
                                                             started with, not a PARAM SET since (2026-10-29)
SEQ start while a run is active                              refused: ERR SEQ start refused reason=run_active
                                                             (SEQ STOP first; STOP also cancels a start queued
                                                             with it; since 11g C1, 2026-10-27)
SEQ MODE quiet|trial|inspect|source|system|detail            which report lines print
SEQ PROFILE TonalPulseScalar|AmpExperimental
SEQ DIAG on|off | SEQ WHEN off|miss|all | SEQ VERBOSE 0|1|2   (SEQ FREQBAND, a no-op, removed 2026-10-29)
PARAM LIST|GET|SET|DUMP|RESET   the detection.scalar_* knobs, same paths and grammar as the Node
PARAM SAVE|LOAD                 answered with status=no_store (no store on the Analyzer)
RAW trigger f=HZ dur=MS post=MS [pre=MS] [decim=N] mode=feat|pcm|i2s|both [pulses=N gap=MS hop=...]
RAW trigger ... [sweep=END_HZ] [band=LOW-HIGH|off]          sweep the chirp; band for this capture only
                                  pre + post: at most 6,000 rows, fewer when the largest free block is smaller
                                  (RAW_ALLOC max_rows= says how many; AnalyzerRawCapture.cpp planRawCapture, 2026-10-29)
RAW ... during a SEQ run (active or queued)                 refused: RAW_ERR busy reason=seq_active (2026-10-29)
OTA ... during a SEQ run (active or queued)                 refused: OTA result=busy reason=seq_active (2026-11-05)
EMIT CHIRP|MODE AUTO|MODE REMOTE|SWEEP|PULSES ...            the Analyzer drives its own output too
SELFTEST [external | chirp n=N gap_ms=MS dur_ms=MS | help]   external: board only, external_chirp SKIP
```

The Analyzer's `PARAM` is the Node's since 2026-10-26 (`src/app/ParamCommands`): the 13
`detection.scalar_*` knobs of the TonalPulseScalar profile are on a `ParamRegistry` through the same
description the Node uses (`param::describeParams` in `src/detection/detectors/scalar/ScalarTransientParams.h`,
gathered by `src/app/ParamBindings.h`), so `PARAM SET detection.scalar_onset_threshold 2000`
is the same line on both boards and `PARAM DUMP` shows the values. Differences from the Node: no
store (`SAVE` / `LOAD` answer `status=no_store`; `RESET` returns to the profile's values), and a value
takes effect at the next `SEQ start`, not live. The observed stream is the profile's (`SEQ PROFILE`),
not a param. The old `PARAM scalar_<key>=<value>` form and `PARAM STATUS` are gone
(`docs/decisions/2026-10-25-analyzer-params-on-registry-and-line-policy.md`).

What the report lines mean (SEQ_TRIAL, SEQ_SOURCE_CORE / SEQ_SOURCE_SPEC, SEQ_INSPECT, SEQ_EXPLAIN,
SEQ_SUMMARY, and which output is neutral tooling rather than analyzer truth): `docs/specs/myspec.md`
5.9. That section defines the line classes; the exact fields are whatever the current firmware prints.
Since 11g C3b (2026-10-27) the bare `SEQ_SOURCE` line (a subset of SEQ_SOURCE_CORE) and `SEQ_TIMING`
(SEQ DIAG on) are gone, and keys that were constant over every committed bench run and read by no tool
left the lines (the list: `docs/refactors/archive/analyzer-run-trial-split.md`, Progress, C3b).
Since 13.0 (2026-10-29) SEQ_EXPLAIN carries the inspected occurrence's shape (DET-015): `shape.applicable`,
`shape.distinct` (bins visited), `shape.up` (steps one bin up), `shape.other`, `shape.runs`, `shape.first_hz`,
`shape.last_hz`, `shape.class`; a hop train reads 5 / 4 / strong, a plain chirp 1 / 0 / none.

### Sample dump (detector-input curve tooling)

Neutral developer tooling: bounded rows of the selected detector-input stream around chosen
trials, between `SAMPLES_BEGIN` and `SAMPLES_END`, with a `SAMPLES_NOTE` line naming the input
and the budget. It is not RAW capture, not the audio envelope, not a report line.

Knobs on the `SEQ start` line:

```text
sampleFirst=N  sampleEvery=N  sampleLead=MS  sampleTail=MS  sampleStep=MS  sampleMax=N
```

`sampleMax` is the row budget over all dumped trials (5,000 on 2026-10-25). Each trial must also
fit the fixed per-trial buffer (`kMaxSampleRows`, 256 rows, `AnalyzerModeApp.h`); the defaults
(lead 50 ms, tail 800 ms, step 4 ms) give 213 rows per trial. `SEQ help` prints the current
defaults.

### RAW capture

`RAW trigger ...` records PCM or feature values around one played signal. The numeric rule for its
values: `docs/specs/myspec.md` 5.9, "RAW capture". One CSV layout per `mode=` (2026-10-26, the header
row before the data, `src/modes/analyzer/tools/AnalyzerRawCapture.cpp`):

```text
mode=feat (default)  ms,amp_strength,env_strength,freq_score,freq_fresh
mode=pcm             ms,pcm,baseline,centered
mode=i2s             ms,pcm               decim=N keeps every Nth I2S word
mode=both            ms,amp,pcm,freq,contrast,fresh
```

The header row is what `tools/bench/rawcorpus.py` reads. The `fields=` key on the `RAW_BEGIN` line
names the same columns (for `mode=pcm` since 2026-10-28; before, it said `ms,pcm,baseline`).

## Emitter

Commands over USB, as of 2026-10-29 (level= / att= on the D-AMP build only):

```text
EMIT CHIRP freq=HZ dur=MS
EMIT PULSES n pulse_ms gap_ms [freq=HZ] [ramp=MS] [hop=F1,F2,...]    up to 8 playable frequencies
EMIT PULSES ... [level=FS] [att=DB1,...]    train level and per-pulse dB (0-60), up to 8 entries
EMIT SWEEP start=HZ stop=HZ step=HZ dur=MS pause=MS
EMIT MODE AUTO interval=MS freq=HZ dur=MS | EMIT MODE REMOTE
SELFTEST
```

From the Analyzer, an `EMIT ...` whose forwarded part is longer than 95 characters (the Emitter link's line) is
refused with `ERR EMIT line_too_long max=95` (2026-10-29).

The Emitter prints `EVT emitter_ready`, `EVT emitter_cmd` and the trial markers `EMIT_START`,
`EMIT_DRIVE_ON`, `EMIT_DONE`, `EMIT_DRIVE_OFF` (each `trial=... t_ms=...`), so a log of an Emitter-driven run can be aligned with the Analyzer's trials.

## What runs: the board view (2026-11-04)

`python tools/bench/boardview.py --boards E1=COM8,... [--every 5] [--html page.html] [--log file]`: one row per board,
refreshed every few seconds in the terminal and, with --html, as a page that reloads itself in the browser: the mode
(node, echo, whisper), Behavior's state, the band (a Node's base and its train's Hz) or Whispernet's delay and room
level, the level, the last action and its age, counters. It asks RB STATUS and ECHO STATUS (a board ignores the
other's) and reads what the boards print. It holds the ports: stop it before nodegroup or an OTA script.
`--radio E1,E4,E5` (2026-11-05) adds rows for boards without USB, from the `NET log from=` lines (node.radio_log) and
NET replies the first `--boards` board prints; a Node there also sends `NET * RB STATUS` each round.

## Echo shell (esp32dev-echo, 2026-10-30)

The shell the echo room and Whispernet will grow in (`src/modes/echo/`, roadmap-echo.md ECHO-003). It does not
detect or replay yet: it runs the per-sample path both modes need at 48 kHz (the mic block, an envelope, an int16
ring of `ECHO_RING_MS`, 500 by default) with the radio up, and counts what that costs. Rate, DMA buffers and ring
length are build flags (the env in `platformio.ini`); it has no params and leaves the Node's stored ones alone.

```text
ECHO STATUS (or STATUS)   one line: dropped_bufs since ECHO RESET, pass_us_mean / max, block_us_max,
                          samples, measured_mhz, env_peak, free_heap, largest_block, net counters
ECHO RESET                the load counters
ECHO LOG <s>              the status line every s seconds (10 at boot), 0 = on request only
ECHO GATE [onset=] [release=] [floor=] [peak=] [attack=MS] [pre=MS] [post=MS] [gain=] [on=0|1]
          [levelmax=] [start=PCT] [plateau=PCT] [decay=MS] [rest=MS] [wait=MS] [jitter=MS]   (the echo room's Behavior, 2026-11-01)
                          the echo room's values, live (Strength16 levels; the clip pre + post <= 600 ms since
                          2026-11-05, 300 ms at 48 kHz before)
REC <secs 1..600>|STOP    the mic as framed PCM over USB at 921600 baud (the board switches the baud and back);
                          tools/bench/longrec.py reads it into a WAV. Raw mic, so run with the own playback off.
                          Untested on a board until one sits on USB (2026-11-06: compiled and host-tested only).
TONE <hz> <ms>            a test tone through the amp, at the shell's rate
OTA url=... token=...     as on the Node; the image confirms after 20 s once its radio started
NET STATUS / NET PING     the radio's counters
```

The echo room (2026-10-31, ECHO-001): the scalar detector runs once per DMA block on the envelope; each occurrence is
inspected by the echo room's one-module plan (AttackRise: the envelope rose from `floor` to `onset` within `attack` ms,
read from a 64 ms FeatureHistory) and judged by makeVerdict; a clap goes to a ResonantBehavior
with the echo room's own values (no idle call, the first clap answered with 3 / 4, an answer 350 ms after it plus up to
300 ms, a rest after a level of 4); the answer is the clap itself, `pre` ms before its onset to `post` ms after, cut
from the ring and played at `gain`. Since 2026-11-05 (ECHO-004, `audio/ClipShape.h`) the copied clip is shaped
first: `echo.damping_pct` a low-pass (0 = as recorded; the corner from the clip's top at 0 % to 1 kHz at 100 %), which
compounds over the generations since each replay records a replay, and `echo.fade_ms` a fade over its end. Lines: `ECHO clap valid= attack_ms= strength= duration_ms= decision=`, `ECHO replay
onset_index= ms= gain=`; ECHO STATUS adds `level= occurrences= claps= slow= replays= clip_missing= behavior=`.
Since 2026-11-04 every value is a param (ECHO-003): `echo.*` in the echo build, `whisper.*` in the whisper build, with
`PARAM LIST|GET|SET|DUMP|SAVE|LOAD|RESET` as on the Node (also by radio: `NET <name> PARAM ...`). The defaults are in
`EchoApp.h` (Gate) and `Whispernet.h` (Values), the paths in `EchoApp::configureParams` and
`Whispernet::describeParams`; `PARAM DUMP` lists the running ones. Saves share the Node's NVS namespace under the
modes' own ids: a mode loads only its own, and PARAM RESET in any mode clears every mode's. ECHO GATE and WHISPER
GATE stay as shorthands for the same fields.

Whispernet's thresholds follow the room (2026-11-04, `whisper.adaptive`, on by default): the onset stands at
`whisper.room_factor` times the room's quiet level, learned while no phrase runs and the board does not play (it
falls within `whisper.room_fall_ms`, rises over `whisper.room_rise_ms`), never under `whisper.onset`; release and
floor keep their ratio to the onset. WHISPER STATUS shows `room=` and `onset_now=`.

The whisper band (2026-11-05, off by default, `audio/BandBalance.h`): `whisper.level_band 1` makes the level (onset,
release, the room's learning) follow 1.5-4.5 kHz instead of the full band; `whisper.timbre_check 1` makes the timbre
module read how many dB the 200-700 Hz band stands over the room's (learned with it) and pass a phrase whose median
is at most `whisper.low_max_db`, in place of the flatness range (`flat=` in the WHISPER phrase line then prints that
excess). From the labelled recordings, not yet heard on a board (research/whispernet.md, 2026-11-05).

Echo cancellation (2026-11-04, `whisper.aec`, off by default): the whisper build writes the speaker in lockstep with
the mic (`DuplexStream`, no writer task, TONE refused), so the speaker runs a fixed `whisper.aec_delay` samples
behind the mic (1066 on E5); `EchoCanceller` takes the board's own output out of the mic before anything else hears
it (~10-11 dB on E5). `whisper.listen_while_playing` then lets the board keep listening during its replay once the
cancellation reaches `whisper.listen_erle_db`. `WHISPER AEC CAL` plays 0.4 s of noise and prints mic and output for
`tools/bench/aeccal.py` (the delay); `WHISPER AEC NOISE <ms>` plays noise to train it; WHISPER STATUS shows
`aec_erle_db=`, `aec_peak_tap=` (32 when aligned), `aec_delay=`, `short_writes=`, `refills=`.

By radio from a USB Node: `NET <name|*> OTA ...` (the way back to the Node image), `NET <name> ECHO STATUS`,
`NET <name> NET PING`. The shell is not a gateway (no NET to other boards from it). Its boot line
`ECHO boot ... ring_bytes=... largest_block_before_ring=... net_started=1` gives the memory numbers.

Whispernet (2026-11-01, ECHO-002; `esp32dev-whisper`, the same shell at 24 kHz): a whisper heard is whispered back
after the board's own delay D (400 .. 2000 ms, from the chip id; `delay_ms=` in the boot line). The mic runs through a
2 s delay line (96 KB, `whisper_bytes=` in the boot line, `ECHO ERR whisper_alloc_failed` when the heap had no such
block) that the amp streams continuously, silent while the gate is shut. A phrase (onset to release, gaps under 300 ms
bridged) is judged while it runs, 300 ms in and every 100 ms after: its level P90 between `floor` and `loud` and its
syllable modulation at least `mod`; the first passing check asks Behavior (no idle call, no wait, no jitter; the echo
room's level and rest), and an answered phrase is passed from its start. A failing check shuts the gate, a phrase once
louder than `loud` stays shut to its end. Detection keeps running while the board plays (it hears its own replay).
The rule is `src/behavior/WhisperGate.h`, the values code defaults in `Whispernet.h` (from the synthetic set, to be
tuned on the desk); `native-whisperreplay` runs the same chain and gate on WAV files, on the whisper board's history
geometry and from the shipped `whisper.*` values (`src/detection/EchoProfiles.h`; the flags override; the synthetic set
reads ~16 dB above the desk's mic: `--level-db -16`). `native-echoreplay` is the echo room's (2026-11-05).

```text
WHISPER STATUS            delay_ms phrases whispers checks asked answered opens gated_ms replays gate_open
                          fifo_wait fifo_dropped underruns line_bytes (also after each ECHO STATUS)
WHISPER GATE [onset=] [release=] [floor=] [loud=] [mod=] [delay=MS 400..2000] [gain=] [on=0|1]
                          live values (Strength16 levels; not saved)
```

Lines: `WHISPER heard after_ms= level_p90= mod= decision=` when a phrase first passes. TONE is silent in this build
(the stream has the amp). By radio: `NET <name> WHISPER STATUS` / `WHISPER GATE ...`.

## Runtime gate chain

One occurrence becomes one `OccurrenceVerdict` (`src/detection/evaluation/OccurrenceVerdict.h`),
rendered by `makeVerdict()` at the end of inspection (`src/detection/inspection/VerdictProjection.h`):

```text
accepted          the detector emitted an accepted Occurrence
proposalMatched   the inspected occurrence met the plan's requirements
supportMatched    the support requirements passed (the shipped plan: contrast, amp, and since
                  2026-10-26 shape: >= 3 band bins with 2 steps up, so a plain chirp is rejected;
                  modules[] in makeTonalPulseScalarProfile(), src/detection/DetectionProfile.h)
valid             the verdict is a usable pattern result
behaviorEligible  ResonantBehavior lets it trigger a reaction now
```

The first four are verdict fields, decided in detection. `behaviorEligible` is decided in
`src/behavior/ResonantBehavior.*` from the verdict and the behavior timing (wait, refractory,
self-suppression, idle, rest) and, since 2026-10-30 (BEH-008), the level automaton: a level counted from the
verdicts (FieldState is no longer read); a heard train is answered with p = level / chain_length (counted first)
after the 100 ms wait plus a random window of answer_spread_ms that shrinks as the level rises (`behavior.spread_shrinks 0` keeps it full)
(`decision=declined_by_level` otherwise), a pending answer is dropped with 1 - p when another train comes
(`yielded`), at chain_length the node rests (`rest_level`). RB STATUS shows `behavior.level`, `behavior.declined`,
`behavior.yielded`. `behavior.chain_length 0` switches it off. Since 2026-10-30 (BEH-009) Behavior also owns the
hop train's base bin (0..3 on the 8-bin band): seeded from `output.hop_rotation` (the roster at boot, a stored or
set value over it), it moves one bin toward the base last heard on each answer (the verdict's heard lowest bin) and
jumps to a random base at the level's rest; RB STATUS `behavior.base_bin` / `base_heard` / `base_glides` /
`base_scatters`, the boot line `RB signal ... base=`. The glide is config (`BehaviorGateConfig::baseGlide`), not a
param. `ResonantBehavior::BehaviorDecision` lists every
reason a valid verdict is not acted on; the Node prints it as `decision=` in the event log.
Older docs name the first two `patternAccepted` and `patternMatched`; same gates.

## Where do I change this knob

```text
detector thresholds, timing       PARAM SET detection.scalar_* (live)   defaults: DetectionProfile.h
listening band                    PARAM SET detection.band_*             defaults: RuntimeDefaults.h (kDefaultNetworkBand*)
inspector medium limits           PARAM SET detection.amp_medium | contrast_medium
shape rule (hop train only)       PARAM SET detection.shape_*            defaults: DetectionProfile.h (module 2)
answer timing, idle, rest         PARAM SET behavior.*                   defaults: BehaviorProfile.h (shippedNodeBehavior)
hop train, idle announce, silent  PARAM SET output.*                     defaults: RuntimeDefaults.h, NodeRoster.h
radio                             PARAM SET node.radio_enabled
tone level                        I2S_TONE_AMPLITUDE (build flag / RuntimeDefaults.h)
chirp frequency                   CHIRP_FREQUENCY_HZ (platformio.ini)
pins, I2S slots                   BoardConfig.h
startup quiet threshold           NodeBaseline::Config::quietThreshold (ResonantNodeBaseline.h)
Analyzer detector knobs           PARAM SET detection.scalar_* (per run, applied at SEQ start)
host replay overrides             --set detection.<path>=<value> (.pio/build/native-replay/program --help)
```

Live changes are lost on reboot unless `PARAM SAVE`d. Changing a default header is a tuning pass
with its own log entry, not a drive-by.

## Bench workflow

Run SEQ tests with `tools/bench/seqrun.py`, compare runs with `tools/bench/seqcmp.py`, build
replay corpora from RAW captures with `tools/bench/rawcorpus.py`. Hardware evidence is committed
on the `bench` branch, checked out as a worktree at `bench/` (its `README.md` has the session
layout). A gate result cites a `bench/sessions/...` path, never a run that only exists in `logs/`.

The standing board check after a change is `tools/bench/nodecheck.py` (the Nodes, 26-27 checks on 2026-11-06) and,
for Analyzer output, `tools/bench/analyzerab.py` against the last A/B set.

## Source map

```text
src/detection/      profiles, detectors, features, occurrences, inspection (+ makeVerdict), field state, DetectionRuntime
src/detection/analyzer/   pure Analyzer types and rules only (no mode code)
src/behavior/       ResonantBehavior and BehaviorGateConfig
src/output/         OutputRequest.h (the request vocabulary: ChirpPattern, OutputRequest,
                    OutputLimits, OutputStatus; no device), SoundOutput, ChirpOutput, ToneOutput
src/audio/, src/hal/  signal chain and I2S input / output
src/net/            ESP-NOW frames for NET commands, NodeNet (transport), NetGateway (reply text layer)
src/modes/resonant|analyzer|emitter|echo/   per-mode command handling and wiring
src/modes/resonant/  the Node, split by role: ResonantNodeApp.cpp (boot, loop, pipeline, SELFTEST glue),
                     ResonantNodeParams (NodeParams: registry, NVS store, registration order; the Node's
                     apply per module, persistence, profile), ResonantNodeCommands.cpp
                     (serial and the command dispatch), ResonantNodeNet.cpp (radio), ResonantNodeStatus.cpp
                     (diagnostics); its parts: ResonantNodeCounters.h (RB tallies),
                     ResonantNodeBaseline (quiet-gated rebase), NetGateway (radio replies, src/net/),
                     ResonantNodeSelfTest (SELFTEST phases), ResonantNodeSignalComposer (band grid,
                     output.hop_* / idle call -> OutputRequest)
src/app/            board config, runtime defaults, roster, param ids, NVS store, serial
lib/ParamRegistry/  the param registry (no src/ includes)
test/, test/fixtures/   on-device and host suites, replay corpus
```
