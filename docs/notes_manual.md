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
```

These build the D-AMP board (MAX98357A amp and I2S mic on one full-duplex port), the node hardware
since 2026-10-09. The discontinued piezo boards build as `esp32dev-piezo`, `esp32dev-piezo-analyzer`,
`esp32dev-piezo-emitter` (`-D BOARD_PIEZO`). Pins and I2S slots per board: `src/app/BoardConfig.h`.
Never flash a piezo build onto a D-AMP board or the reverse: the pinouts collide on GPIO 25/26.

Build-time knobs (not params, they need a rebuild): `platformio.ini` build flags
(`CHIRP_FREQUENCY_HZ`, `BUILD_VERSION`), tone level `I2S_TONE_AMPLITUDE` and the other defaults in
`src/app/RuntimeDefaults.h`, the node roster (name and hop rotation per board MAC) in
`src/app/NodeRoster.h`.

Host-side builds and tests (no board): see "Build & verify" in `CLAUDE.md`.

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
token=<token>`; nothing is read as 0 (since 2026-10-26, step 11e;
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

`RB PARAM` and `RB BEHAV` are gone since step 10 (2026-10-22). Old session headers map through
`docs/decisions/2026-10-22-live-params-profile-reset-no-rb-behav.md`.

### Params

Every tunable knob on the Node is a registered param in `lib/ParamRegistry/` (an in-repo library
that includes nothing from `src/`). `PARAM LIST` prints the paths with their ranges, `PARAM DUMP`
the current values. The groups, registered in `src/modes/resonant/ResonantNodeParams.cpp`:

```text
detection.scalar_*        ScalarTransientDetector thresholds and timing (13 params)
detection.band_low_hz / band_high_hz / amp_medium / contrast_medium
detection.shape_*         the shape inspector (DET-015): look-back, tail, run and step limits (8 params;
                          its level is detection.scalar_release_threshold)
behavior.*                wait, refractory, self-suppression, idle call, rest (13 params)
output.*                  hop train, idle announce, silent (8 params)
node.radio_enabled        ESP-NOW on or off
```

Where the defaults live: `detection.*` in `src/detection/DetectionProfile.h` (the profile
structs), `behavior.*` in `src/behavior/BehaviorProfile.h` (`BehaviorGateConfig`), `output.*` in
`src/app/RuntimeDefaults.h` and the roster. Retuning a default is its own pass, logged through
`tools/logging/`; do not change these headers as a side effect of other work.

### Apply points, in boot order

- `Node::begin()`: `configureParameters()`, audio source and signal start, detection reset, then
  `applyActiveProfiles()` (detection profile and behavior gate config from the headers), then the
  roster lookup (node name, hop rotation; MAC fallback), then `captureDefaults()` so the registry
  knows the firmware defaults, then `loadStoredParams()`: a valid stored value wins, `PARAM stale`
  is printed per entry saved against a default that has since changed. The radio comes up last, so
  a stored `node.radio_enabled 0` keeps it off.
- `PARAM SET <path> <value>`: writes the bound field and applies its module in place, no detector
  reset. Live only, lost on reboot.
- `PARAM SAVE`: the only flash write; stores every registered param. `PARAM LOAD` re-reads and
  applies the store, `PARAM RESET` clears it and restores the firmware defaults
  (`docs/decisions/2026-10-23-param-persistence-save-only.md`).
- `RB PROFILE name=...`: switches the profile and reapplies detection and behavior from the profile
  structs, which replaces the bound `detection.*` fields wholesale. Live `PARAM SET` overrides on
  them are gone after a switch; check `PARAM DUMP`.
- Boot prints `PARAM load stored=N applied=N stale=N invalid=N` plus one line per stored value that
  differs from its default.

### Silent mode

`PARAM SET output.silent 1` keeps the node listening (verdicts counted, `decision=outputs_off` in
the log) but it never sounds: no answer, no idle call, `RB CALL` ignored. With `PARAM SAVE` it boots
silent (`RB silent=1` at boot) until `PARAM SET output.silent 0` plus SAVE, or `PARAM RESET`.
`RB STATUS` shows `silent=`. SELFTEST still plays its tones, it is an explicit command.

### Remote commands (radio, step 11a)

Any Node on USB is the gateway: `NET <target> <command>`, target `*` (all) or a roster name
(`E1`..`E5`). Every Node command goes (PARAM, RB, SELFTEST) plus `NET PING`; each node answers with
what it would print on serial, shown as `NET reply from=E3 seq=N part=P <line>` and
`NET reply from=E3 seq=N done parts=K`. `NET STATUS` shows the gateway's own radio counters. The
radio is on by default; `PARAM SET node.radio_enabled 0`, `PARAM SAVE`, reboot turns it off.

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

### Startup quiet gate

The Node waits for the room to be quiet before it arms: `kRbStartupQuietThreshold` in
`src/modes/resonant/ResonantNodeApp.cpp`, one value per board (D-AMP and piezo differ).

## Analyzer

`HELP` prints the top-level commands, `SEQ help` the SEQ surface. As of 2026-10-26:

```text
SEQ start [N|tries=N] [period=MS] [window=MS] [freq=HZ] [dur=MS] [delay=MS] [report_settle=MS] [test=LABEL]
SEQ start ... band=LOW-HIGH|off                              score the strongest bin in the band (default off)
SEQ OBS start ...               passive observe mode for an external emitter already running
SEQ start ... pulses=N gap=MS [ramp=MS] [hop=F1,F2,...]      pulse train per trial (DET-011)
SEQ start ... freq=START_HZ dur=MS sweep=END_HZ              sweep inside the chirp (OUT-005)
SEQ stop | SEQ STATUS | SEQ SUMMARY | SEQ REPORT | SEQ TRIES N
SEQ MODE quiet|trial|inspect|source|system|detail            which report lines print
SEQ PROFILE TonalPulseScalar|AmpExperimental
SEQ DIAG on|off | SEQ FREQBAND on|off | SEQ WHEN off|miss|all | SEQ VERBOSE 0|1|2
PARAM LIST|GET|SET|DUMP|RESET   the detection.scalar_* knobs, same paths and grammar as the Node
PARAM SAVE|LOAD                 answered with status=no_store (no store on the Analyzer)
RAW trigger f=HZ dur=MS post=MS [pre=MS] [decim=N] mode=feat|pcm|i2s|both [pulses=N gap=MS hop=...]
RAW trigger ... [sweep=END_HZ] [band=LOW-HIGH|off]          sweep the chirp; band for this capture only
EMIT CHIRP|MODE AUTO|MODE REMOTE|SWEEP|PULSES ...            the Analyzer drives its own output too
SELFTEST [external | chirp n=N gap_ms=MS dur_ms=MS | help]   external: board only, external_chirp SKIP
```

The Analyzer's `PARAM` is the Node's since step 11e (2026-10-26, `src/app/ParamCommands`): the 13
`detection.scalar_*` knobs of the TonalPulseScalar profile are on a `ParamRegistry` through the same
binding the Node uses (`src/app/ParamBindings.h`), so `PARAM SET detection.scalar_onset_threshold 2000`
is the same line on both boards and `PARAM DUMP` shows the values. Differences from the Node: no
store (`SAVE` / `LOAD` answer `status=no_store`; `RESET` returns to the profile's values), and a value
takes effect at the next `SEQ start`, not live. The observed stream is the profile's (`SEQ PROFILE`),
not a param. The old `PARAM scalar_<key>=<value>` form and `PARAM STATUS` are gone
(`docs/decisions/2026-10-25-analyzer-params-on-registry-and-line-policy.md`).

What the report lines mean (SEQ_TRIAL, SEQ_SOURCE, SEQ_INSPECT, SEQ_EXPLAIN, SEQ_SUMMARY, and
which output is neutral tooling rather than analyzer truth): `docs/specs/myspec.md` 5.9. That
section defines the line classes; the exact fields are whatever the current firmware prints.

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
says `ms,pcm,baseline` for `mode=pcm`, one column short of the header (review D3.3, left for the
code batch).

## Emitter

Commands over USB, as of 2026-10-26:

```text
EMIT CHIRP freq=HZ dur=MS
EMIT PULSES n pulse_ms gap_ms [freq=HZ] [ramp=MS] [hop=F1,F2,...]    up to 8 playable frequencies
EMIT SWEEP start=HZ stop=HZ step=HZ dur=MS pause=MS
EMIT MODE AUTO interval=MS freq=HZ dur=MS | EMIT MODE REMOTE
SELFTEST
```

The Emitter prints `EVT emitter_ready`, `EVT emitter_cmd` and the trial markers `EMIT_START`,
`EMIT_DRIVE_ON`, `EMIT_DONE`, `EMIT_DRIVE_OFF` (each `trial=... t_ms=...`), so a log of an Emitter-driven run can be aligned with the Analyzer's trials.

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
`src/behavior/ResonantBehavior.*` from the verdict plus `FieldState` and the behavior timing
(wait, refractory, self-suppression, idle, rest). `ResonantBehavior::BehaviorDecision` lists every
reason a valid verdict is not acted on; the Node prints it as `decision=` in the event log.
Older docs name the first two `patternAccepted` and `patternMatched`; same gates.

## Where do I change this knob

```text
detector thresholds, timing       PARAM SET detection.scalar_* (live)   defaults: DetectionProfile.h
listening band                    PARAM SET detection.band_*             defaults: RuntimeDefaults.h (kDefaultNetworkBand*)
inspector medium limits           PARAM SET detection.amp_medium | contrast_medium
shape rule (hop train only)       PARAM SET detection.shape_*            defaults: DetectionProfile.h (module 2)
answer timing, idle, rest         PARAM SET behavior.*                   defaults: BehaviorProfile.h
hop train, idle announce, silent  PARAM SET output.*                     defaults: RuntimeDefaults.h, NodeRoster.h
radio                             PARAM SET node.radio_enabled
tone level                        I2S_TONE_AMPLITUDE (build flag / RuntimeDefaults.h)
chirp frequency                   CHIRP_FREQUENCY_HZ (platformio.ini)
pins, I2S slots                   BoardConfig.h
startup quiet threshold           kRbStartupQuietThreshold (ResonantNodeApp.cpp)
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

## Source map

```text
src/detection/      profiles, detectors, features, occurrences, inspection (+ makeVerdict), field state, DetectionRuntime
src/detection/analyzer/   pure Analyzer types and rules only (no mode code)
src/behavior/       ResonantBehavior and BehaviorGateConfig
src/output/         OutputRequest.h (the request vocabulary: ChirpPattern, OutputRequest,
                    OutputLimits, OutputStatus; no device), SoundOutput, ChirpOutput, ToneOutput
src/audio/, src/hal/  signal chain and I2S input / output
src/net/            ESP-NOW frames for NET commands
src/modes/resonant|analyzer|emitter/   per-mode command handling and wiring
src/modes/resonant/  the Node, split by role: ResonantNodeApp.cpp (boot, loop, pipeline, baseline),
                     ResonantNodeParams.cpp (params, persistence, profile), ResonantNodeCommands.cpp
                     (serial and the command dispatch), ResonantNodeNet.cpp (radio), ResonantNodeStatus.cpp
                     + ResonantNodeSelfTest.cpp (diagnostics)
src/app/            board config, runtime defaults, roster, param ids, NVS store, serial
lib/ParamRegistry/  the param registry (no src/ includes)
test/, test/fixtures/   on-device and host suites, replay corpus
```
