# Node self-test, SELFTEST (step 3b, issue #28, NODE-015)

Status: active. Firmware and runner written 2026-10-10 evening; **compile
verified only** (all six envs build), nothing has run on hardware yet.
Roadmap: `docs/roadmaps/roadmap-node.md` NODE-015, step 3b of
`docs/roadmaps/roadmap-0-steps.md`.
Replaces: the throwaway amp-check sketch
(`bench:sessions/2026-10-10-issue20-damp-10cm/amp-check/sketch`) and the
manual R1 leftovers of `damp-bench-check.md` (Node check on E3, own-echo).

Measure, don't tune: no threshold in `DetectionProfile.h` /
`BehaviorProfile.h` changed. The SELFTEST pass/fail limits below are
provisional and live in `src/selftest/AudioSelfTest.h` and
`src/modes/resonant/ResonantNodeSelfTest.cpp`.

---

## 1. Goal

On 2026-10-10 E1's amp went silent, E2's mic read 0 in both I2S slots until
re-seated, and during the soak the Analyzer heard nothing for 7 h while
every run "completed". The firmware reported none of it. SELFTEST turns
each of these into a loud FAIL line, on every build that ships, and
`tools/bench/selftest.py` runs it on every attached board and logs it to a
bench session.

Gate (NODE-015): SELFTEST passes on every board going into NODE-011, logged
on `bench`.

## 2. Where the code lives, and why

| Part | File | Builds |
|---|---|---|
| Output format, command parsing, `board` check | `src/selftest/SelfTestReport.*` | all |
| `mic` and `amp` checks, blocking chirp helper | `src/selftest/AudioSelfTest.*` | all |
| Node phases: `quiet_boot`, `own_chirp_suppressed`, `external_chirp` | `src/modes/resonant/ResonantNodeSelfTest.cpp` | Node |
| Analyzer command (board, mic, amp; chirp helper) | `src/modes/analyzer/AnalyzerSelfTest.cpp` | Analyzer |
| Emitter command (USB console) | `src/modes/emitter/EmitterApp.cpp` | Emitter |
| Host runner | `tools/bench/selftest.py`, sample `tools/bench/samples/selftest_node.sample.log` | - |

Boundaries (myspec):

- `src/selftest/` depends only on the HAL (`AudioSourceI2S`), the
  `ToneOutput` interface and `BuildInfo`. It is outside `detection/` and
  `modes/`, so the Node and Emitter builds get it without the analyzer
  tooling their `build_src_filter` excludes.
- The hardware checks read the I2S source directly (as the AC sketch did),
  never a detector. They block the caller's loop; afterwards the caller
  resets its detection chain (`DetectionRuntime::resetState` /
  `resetDetectionState`, `AudioSignal::resetSignalState`, `FreqBandStream`)
  because the samples read never reached it. The Node then goes through
  `SETTLE` (500 ms) again if it was `ACTIVE`; the AudioSignal baseline and
  the Behavior config are left alone.
- The Node runtime checks only watch what the Node already receives:
  verdicts popped from `DetectionRuntime` (in `processDetectionFrame`) and
  its own chirp start/finish events. No detector internals, no
  reconstruction of detector truth.
- Behavior still decides: own test chirps are requested with the new
  `ResonantBehavior::requestTestChirp()`. Behavior takes the request the
  next time it is `Idle` (never over a pending response, a chirp or the
  refractory time) and emits a single chirp through the normal
  request -> `ChirpOutput` -> `notifyChirpStarted/Finished` path, with the
  same own-emit suppression as a heard-pattern response
  (`chirpRequestSourceName() == "selftest"`).
- SoundOutput performs: the `amp` check plays its 100 ms tones on the
  board's real output device, reached through the new
  `ChirpOutput::toneOutput()` (Node, Emitter). The Analyzer has no output
  object; on D-AMP it gets an `I2sToneOutput` on its own port whose writer
  task runs only during the check (new `I2sToneOutput::end()`).

Other small changes: the Emitter now reads its USB console for `SELFTEST`
and `EMIT <control line>` (runs the line as if it came over Serial2, the
Analyzer's EMIT syntax; the runner uses `EMIT MODE REMOTE` to stop AUTO
chirps). Node `RB help` and Analyzer `HELP` list SELFTEST.

## 3. Commands

| Command | Node | Analyzer | Emitter |
|---|---|---|---|
| `SELFTEST` | board, quiet_boot, mic, amp, own_chirp_suppressed, external_chirp=SKIP not_requested | board, mic, amp | board, mic, amp |
| `SELFTEST external wait_ms=N` (default 10000, max 120000) | board, external_chirp | board, external_chirp=SKIP not_node | same as Analyzer |
| `SELFTEST chirp n=5 gap_ms=1500 dur_ms=100` | N single chirps through Behavior | N tones (D-AMP; piezo: none) | N tones |
| `SELFTEST help` | usage | usage | usage |

Case-insensitive. A second SELFTEST while the Node's is running prints
`SELFTEST busy`; the Analyzer refuses during a SEQ run
(`SELFTEST busy reason=seq_active`).

## 4. Output format

The repo's `KEYWORD key=value ...` serial convention (like `SEQ_SUMMARY`,
`EVT`). One line per check, values never contain spaces, a FAIL or SKIP
carries `reason=<word>` right after `result=`:

```text
SELFTEST_BEGIN role=<node|analyzer|emitter> mode=<full|external> board=<damp|piezo>
SELFTEST check=<name> result=PASS|FAIL|SKIP [reason=<word>] key=value ...
SELFTEST_SUMMARY result=PASS|FAIL pass=N fail=N skip=N role=<role>
```

Detail and marker lines (not counted): `SELFTEST_AMP` (one per amp chirp),
`SELFTEST_RESTORE` (detection reset after the hardware checks),
`SELFTEST_EXTERNAL waiting wait_ms=N` (the Node starts listening; the
runner makes the helper chirp now), `SELFTEST_CHIRP start ...` /
`SELFTEST_CHIRP done n=N started=N [reason=...]` (chirp helper, no summary
line), `SELFTEST_HELP`, `SELFTEST busy`. `SELFTEST_SUMMARY result=PASS`
means no FAIL; SKIPs do not fail it.

Example (Node, fabricated numbers; full sample in
`tools/bench/samples/selftest_node.sample.log`):

```text
SELFTEST_BEGIN role=node mode=full board=damp
SELFTEST check=board result=PASS mac=24:dc:c3:4b:4c:e8 board=damp role=node version=0.4.0 git=0000000 built=Oct_10_2026 built_time=20:55:00 uptime_ms=61234
SELFTEST check=quiet_boot result=PASS state=ACTIVE boot=done last=done last_at_ms=3512 smooth_at_rebase=231.5 quiet_rebases=1 forced_rebases=0 baseline=-3.2
SELFTEST check=mic result=PASS frames=12000 nonzero_pct=99.9 span=41234 floor_mean=412 floor_peak=1630 bit8_pct=49.7 framing=ok slot=0
SELFTEST_AMP n=1 floor=1010 tone=301233 snr_db=49.5 samples=6528 valid=1
...
SELFTEST check=amp result=PASS chirps=5 valid=5 freq_hz=3200 tone_ms=100 path=i2s tone=300120 floor=1010 snr_db=49.5 snr_min_db=37.2 snr_max_db=50.1 pass_db=30.0
SELFTEST_RESTORE detection=reset baseline=SETTLE
SELFTEST check=own_chirp_suppressed result=PASS chirps=5 started=5 verdicts=0 valid=0 other_verdicts=0 other_valid=0 echo_window_ms=300 suppress_tail_ms=60
SELFTEST check=external_chirp result=SKIP reason=not_requested
SELFTEST_SUMMARY result=PASS pass=5 fail=0 skip=1 role=node
```

## 5. Checks and criteria

`board` - MAC (chip base MAC, `esptool read_mac` format, lower case),
`BOARD_NAME`, role, `BUILD_VERSION`, git hash (`BUILD_GIT_SHA`, as in the
BUILD banner), build date/time, uptime. PASS unless the MAC cannot be read.

`mic` - 750 ms (12,000 frames) with nothing of ours sounding, read frame by
frame with `AudioSourceI2S::readRawSample()` (decoded = slot word >> 8, so
decoded bit 0 is the raw word's bit 8; with `RAW_I2S_UNDECODED` the raw
word is used directly). FAIL reasons, in order:
- `no_data`: fewer than half the frames arrived (I2S not delivering);
- `all_zero`: non-zero samples < 50 % (E2, 2026-10-10);
- `stuck`: raw span < 4 (floating data line reading one value);
- `zero_floor`: FD envelope mean < 1;
- `floor_high`: FD envelope mean > 100,000 (healthy D-AMP 850-5,900);
- `framing_bit8`: bit 8 set in < 1 % or > 99 % of samples (#24). Judged
  only where `I2S_RX_MSB_ALIGN` is built in (D-AMP); piezo reports
  `framing=not_judged`.
Reported: `frames nonzero_pct span floor_mean floor_peak bit8_pct framing slot`.
`floor_*` use the amp check's envelope (moving max of |FD| over 6 samples).

`amp` - 5 trials of the AC sketch measurement: 100 ms quiet, a 100 ms tone
at the profile chirp frequency (`ChirpOutput::toneHz()`, 3200 Hz) on the
board's real output, 200 ms tail, 1 s gap. Per trial on the FD stream:
floor = max envelope from start+20 ms to toneOn-5 ms, tone = mean envelope
toneOn+50..90 ms, SNR = 20 log10((tone+1)/(floor+1)). PASS when the
**median SNR >= 30 dB**; FAIL `low_snr` or `no_data`. Reported: median
`tone`, median `floor`, median `snr_db`, min/max. The tone-level half of the
AC rule (not more than 10 dB below the session start) is judged by the
runner across runs (`tone_vs_prev`, DEGRADED below -10 dB), since one
board has no reference of its own. Path: `i2s` (D-AMP) or `piezo` (Node
piezo build: LEDC tone, same measurement). SKIP `no_output_in_build`
(piezo Analyzer) or `no_mic_in_build` (piezo Emitter, both mic and amp).

`quiet_boot` (Node) - result of the latest quiet-gated rebase since boot
(`RB rebase done` vs `RB rebase skipped reason=no_quiet`). If the boot
sequence is still running, SELFTEST waits up to 15 s for it. PASS `done`;
FAIL `no_quiet`, or `timeout` while still pending. `RB rebase force` is
counted (`forced_rebases`) but is not a quiet-gated result. Also reported:
current baseline state, boot result, time, smoothed level at the decision.

`own_chirp_suppressed` (Node) - after the hardware checks and SETTLE, 5
single chirps requested through Behavior (1.5 s after each one finished),
then a 1 s tail. Every verdict the Node pops is attributed: heard
(`primaryHeardAtMs`, else `primaryStartMs`) from 20 ms before an own chirp
(any source, including idle) to 300 ms after it finished = own. PASS when
all 5 chirps started and **0 own verdicts**; FAIL `own_verdict` or
`chirp_not_started`. Verdicts outside those windows (`other_verdicts`) are
reported only: a neighbour Node answering ~0.6 s later lands there.

`external_chirp` (Node, `SELFTEST external`) - prints
`SELFTEST_EXTERNAL waiting`, then waits `wait_ms` for a **valid** verdict
not attributable to an own chirp: PASS (with `latency_ms`, `responded=1`
if the Node answered within 1.5 s); SKIP `no_external_chirp` when nothing
valid arrived. Response is reported, not required (Behavior gates may
legitimately block it).

## 6. Runner and bench session

```text
python tools/bench/selftest.py [--ports COM3 COM4 ...] [--reset] [--external]
       [--external-wait-ms 10000] [--chirps 5] [--slug S | --session DIR]
       [--note ...] [--distance-cm N] [--keep-emitters] [--no-compare]
python tools/bench/selftest.py --dry-run [...]
python tools/bench/selftest.py --selfcheck <log> [...]
```

- Ports: given, or every CP210x (VID 10C4, PID EA60). Opened with DTR/RTS
  held low before `open()`, so boards are not reset; `--reset` pulses EN
  (`benchlib.reset_board`) and keeps the boot output in the log.
- First `EMIT MODE REMOTE` on every port (Analyzer forwards to its
  Emitter, an Emitter takes it on USB, a Node ignores it), unless
  `--keep-emitters`. Emitters stay in REMOTE afterwards.
- Pass 1: `SELFTEST` on one board at a time, until `SELFTEST_SUMMARY`.
- Pass 2 (`--external`, >= 2 boards): for each board that reported
  role=node, `SELFTEST external wait_ms=N`; on `SELFTEST_EXTERNAL waiting`
  the next board in the list gets `SELFTEST chirp n=5 gap_ms=1500`.
- MAC -> label from the Boards table in `bench/README.md` (`?` if absent).
- Table per board: label, port, MAC, role, hw, git, result, each check, amp
  SNR / tone / floor, `tone_vs_prev` (vs the newest earlier SELFTEST of the
  same MAC in any session).
- Session `bench/sessions/<date>-selftest[-slug]/`: one log per board
  (`selftest_<label>_<HHMMSS>.<git>.log`, `#` header with a `# firmware:`
  line that `benchlib.parse_banner` reads), `session.json` with the entries
  under `"selftest"` (label, MAC, port, file, firmware, result, all checks,
  amp trials, tone reference). Not under `"runs"`: `index.csv` is one row
  per SEQ run with `SEQ_SUMMARY` fields, which a self-test has none of, so
  `rebuild_index()` skips these entries unchanged.
- Exit code 0 only when every board passed.

## 7. Verified vs pending

Verified 2026-10-10 (no hardware attached, no port opened):
- `pio run` for esp32dev, -analyzer, -emitter, -piezo, -piezo-analyzer,
  -piezo-emitter: all SUCCESS. Size vs the same tree before the change:
  esp32dev flash 358,305 -> 368,337 (+10,032), RAM 60,084 -> 60,260
  (+176); esp32dev-analyzer flash 405,989 -> 415,145 (+9,156), RAM
  85,276 -> 85,352 (+76); esp32dev-emitter flash +8,440, RAM +144.
- `selftest.py --help`, `--dry-run`, `--selfcheck` on the sample log; the
  live path once against fake serial ports (session written, external
  rotation, combined summary).

Pending hardware (nothing above says it works on a board):
- every number and criterion; that the SELFTEST amp numbers match the AC
  sketch on the same board (they should: same measurement, same stream);
- the Node's restore (SETTLE, detection reset) and that a SEQ run right
  after an Analyzer SELFTEST is unaffected;
- `I2sToneOutput::end()` (task exit) on the Analyzer;
- the Emitter's USB console with an Analyzer linked (acks on Serial2).

Hardware plan (next bench visit): see the report of the pass that wrote
this file; summarised as dated results here once run.

## 8. Open questions

1. Provisional limits: mic floor 100,000, non-zero 50 %, span 4, bit 8
   1-99 %, amp 30 dB median. Re-check against the first runs on all five
   boards.
2. Neighbour Nodes answer the test tones and each other. Medians (amp) and
   the attribution window (own chirp) tolerate a few; with five Nodes in
   one room a chain of answers may still raise floors. If it does: a
   Behavior mute command, or test with the others out of earshot.
3. `external_chirp` passes on a verdict; should it also require the
   response (`responded=1`)?
4. Should SELFTEST entries get their own generated table (a
   `selftest.csv` next to `index.csv`) once there are several sessions?
5. Piezo framing: judge bit 8 there too, or keep reporting only?
