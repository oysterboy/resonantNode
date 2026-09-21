# Architecture Proposal — Detector Family Is a Build, Profile Is Config, Thresholds Are Params

Status: proposal, decided in principle 2026-09-21, not yet implemented.
Supersedes: `docs/refactors/cleanup-detector-ownership.md` (the union-of-two-
detectors proposal, deferred on measurement). This gets everything that
proposal wanted, and more, without any of its lifetime risk.
Related to: `docs/refactors/cleanup-analyzer-node-isolation.md` (same
mechanism, same principle), `docs/roadmaps/roadmap-param-config.md`
(PAR-013, PAR-014: this is that split applied to detection).

---

## The model in one table

| Layer | Decided by | Changes via | Example |
|---|---|---|---|
| **Detector family** | build flag | Firmware OTA (FWOTA) | `FrequencyMatch` vs `ScalarTransient` |
| **Profile** | Config, per node | OTA + reboot to apply | `TonalPulseScalar` vs `AmpExperimental` |
| **Thresholds** | Params, per node | live, no reboot | `freqAttackScore=18000` |

Each layer changes only what the layer below can't: a family decides which
detector class exists at all, a profile decides the inspection plan and
which feature streams are worth recording, params tune numbers inside a
plan that is already running.

This is not a new idea layered onto the roadmap. `roadmap-param-config.md`
already draws these lines: PAR-013 keeps *Firmware OTA changes firmware*
apart from *Remote Param Update changes params*, and PAR-014 says *Config is
identity, boot, network, hardware, and may require reboot* while *Params are
live tuning*. A detector family is firmware. A profile is config. That is
the whole proposal; the rest of this document is what falls out of it.

The two runtime modes differ in what they may switch:

- **Analyzer**: switches profiles *within* the compiled family, live, in one
  session (`TonalPulseScalar` vs `AmpExperimental`). Cross-family comparison
  is two firmwares.
- **Node**: receives a profile as config, stores it, reboots, applies it. A
  profile carries its family; a node rejects a profile from a family it
  wasn't built for, instead of silently doing something else.

---

## Why this is the right replacement for the union proposal

`cleanup-detector-ownership.md` measured its own saving at 1,440 bytes and
found a stale-report dispatch that would be undefined behavior under a
union on the profile-switch path. It was deferred on those numbers. This
proposal gets the same object-count reduction by not compiling the other
detector, which is:

- **the full saving, not `max`**: the family you don't build costs nothing,
  not `max(1832, 1440)`. Frequency-family Node saves 1,440 (no
  `ScalarTransientDetector`); scalar-family Node saves 1,832.
- **no lifetime management**: one concrete member, constructed once, no
  placement-new, no inactive union member to read by accident. The
  hazard that sank the union simply has no object to hit.
- **dispatch removed, not unified**: every `switch (_detectorSelection)`,
  the `DetectorSelection` runtime state, and Phase 3's
  `ActiveDetectorAdapter` go away in *both* builds, since the Analyzer is
  also per-family. Phase 3 unified two branches into one path; this deletes
  the path's reason to exist.
- **Phase 0 stops being a deletion decision**: "keep or consolidate
  `FrequencyMatchDetector`" becomes "which families do we still build." If
  field data says scalar wins, stop building the frequency firmware. Nothing
  has to be deleted to find out.

---

## What it saves, measured

Sizes from `xtensa-esp32-elf-g++` on the target, after Phase 5c
(`FeatureHistory` at 3 slots x 256 bins x 24 bytes):

| | Frequency-family Node | Scalar-family Node |
|---|---|---|
| detector not compiled | -1,440 (`ScalarTransientDetector`) | -1,832 (`FrequencyMatchDetector`) |
| `FeatureHistory` slots | 3 -> 2 (`TonalPulseFreq` reads 2 streams): **-6,184** | 3 -> 3 (both scalar profiles read 3): 0 |
| `DetectorSelection` dispatch, the other family's detector code | flash only | flash only |
| **RAM, approximately** | **-7,600** | **-1,800** |

The asymmetry matters: `TonalPulseFreq` is the stable production profile
(`implementation-status.md`), so the *production* Node is the frequency
family and gets the larger saving. Node RAM after this session's other work
is 59,996 bytes; the frequency-family Node would land around 52,400.

`FeatureHistory::kMaxActiveStreams` becomes a per-family constant: the
maximum over that family's profiles, still `static_assert`ed against
`kMaxInspectionModules` so it can't be under-sized.

### What does not shrink: bin depth for the frequency family

`kBinsPerStream = 256` is sized to the longest accepted occurrence (240 ms,
`AmpExperimental`) plus a 10 ms look-back. One might expect the frequency
family to need less. It needs *more*, or rather it has no bound to size
against: `FrequencyMatchDetector::closePending()` accepts on
`_pendingDurationMs >= minDurationMs` alone. `_pendingMaxDurationMs` is set
to 0 and reported in `DetectorReport.thresholds.maxDurationMs`, but never
enforced. A frequency occurrence can run indefinitely.

That means a latent, pre-existing behavior worth recording here even though
this proposal doesn't change it: a frequency occurrence longer than about
246 ms already outruns the history buffer, and inspection anchored at its
start degrades to `HistoryWindowIncomplete`. Nobody has reported it, which
probably means real frequency occurrences are well under 246 ms in practice;
but it is unbounded by code, only by acoustics. Either bound it (a
`maxDurationMs` the detector actually enforces, which changes detection
behavior and needs its own SEQ run) or accept that frequency-family bin
depth stays at 256. This proposal takes the second option and leaves the
first as a separate decision.

---

## Proposed Change

### 1. A build flag per family, one Node and one Analyzer env each

`platformio.ini` grows a family dimension, following the existing
`ANALYZER_MODE` pattern:

```ini
[env:esp32dev]               ; Node, frequency family (production default)
build_flags = ... -D DETECTOR_FAMILY_FREQUENCY

[env:esp32dev-scalar]        ; Node, scalar family
build_flags = ... -D DETECTOR_FAMILY_SCALAR

[env:esp32dev-analyzer]      ; Analyzer, frequency family
[env:esp32dev-analyzer-scalar]

[env:esp32dev-emitter]       ; no detection, no family
```

Five environments instead of three. That is the cost of the design and
should be stated plainly rather than hidden: T1 becomes "all five link,"
and the SEQ battery is already per-profile so it doesn't grow.

`build_src_filter` extends the Phase 5a pattern: `detectors/frequency/`
is excluded from scalar-family builds and `detectors/scalar/` from
frequency-family builds. "Absent, not merely unreachable," same as the
Analyzer tooling.

### 2. One header carries the switch: `DetectionFamily.h`

The family flag is inspected in exactly one source file. Everything else
is family-agnostic by construction, not by discipline.

Today the family leaks into 12 files, but almost all of that is the
`DetectorSelection` *enum* used as a value: profile tags, `switch`es that
print a name. That is the axis-1 vocabulary this proposal keeps (section
5), and a firmware that only ever sees one enum value still compiles a
`switch` over both. Those files need no directive. The only file that
touches the detector *classes* is `DetectionRuntime.h/.cpp` (51
references), and even it can be kept clean by putting the `#if` one level
down:

```cpp
// src/detection/DetectionFamily.h -- the only file that reads DETECTOR_FAMILY_*.
#if defined(DETECTOR_FAMILY_FREQUENCY)
  #include "detectors/frequency/FrequencyMatchDetector.h"
  namespace detection {
    using ActiveDetector       = FrequencyMatchDetector;
    using ActiveDetectorConfig = FrequencyMatchConfig;
    constexpr DetectorSelection kCompiledFamily = DetectorSelection::FrequencyMatch;
    constexpr size_t kFamilyMaxActiveStreams = 2;   // TonalPulseFreq reads 2 streams
    // The three places the two detectors genuinely differ, as inline shims:
    inline void updateActiveDetector(ActiveDetector&, const AudioSamplePacket&,
        const FrequencyBandMeasurementPacket&, const ActiveDetectorConfig&, unsigned long nowMs);
    inline void applyActiveDetectorConfig(ActiveDetector&, const ActiveDetectorConfig&);
    inline void resetActiveDetectorRejectSummaries(ActiveDetector&);
  }
#elif defined(DETECTOR_FAMILY_SCALAR)
  #include "detectors/scalar/ScalarTransientDetector.h"
  namespace detection {
    using ActiveDetector       = ScalarTransientDetector;
    using ActiveDetectorConfig = ScalarTransientConfig;
    constexpr DetectorSelection kCompiledFamily = DetectorSelection::ScalarTransient;
    constexpr size_t kFamilyMaxActiveStreams = 3;   // both scalar profiles read 3
    // ...same three shims, scalar bodies
  }
#else
#  error "define exactly one of DETECTOR_FAMILY_FREQUENCY / DETECTOR_FAMILY_SCALAR"
#endif
```

`DetectionRuntime.h` then says `ActiveDetector _detector;` and holds an
`ActiveDetectorConfig`. Every call that was already identical across the
two detectors (`popOccurrence`, `hasPendingOccurrence`, `resetState`,
`latestReport`, `reportGeneration`, `setDiagnosticsEnabled`) becomes a
plain method call on `_detector`. `_detectorSelection`,
`ActiveDetectorAdapter`, `hasPendingDetectorOutput()`'s switch,
`captureLatestDetectorReportIfChanged()`'s switch, and both
`reportGeneration()` ternaries in the Analyzer layer are deleted, not
gated.

The three shims are the only family-specific glue that has to exist, and
they are exactly the three things today's `switch`es were written around:

- `update()` has a different signature per detector (the spec allows this
  and this proposal keeps it).
- config application differs: scalar has `applyScalarTransientConfig()`,
  frequency builds a `FrequencyMatchCriteria::Values` inline each frame.
- reject-summary reset uses different method names per detector
  (`resetRejectSummary()` vs `resetAcceptedOccurrenceSummary()` +
  `resetSelectedRejectSummary()`).

Each shim's body lives inside the matching `#if` branch of
`DetectionFamily.h`. `DetectionRuntime.cpp` calls the shim and contains
no `#if DETECTOR_FAMILY` at all. This is the compile-time descendant of
Phase 3's runtime adapter: same idea, zero runtime cost, and no second
branch to keep in sync.

Count, for the record: **one source file** (`DetectionFamily.h`) plus
`platformio.ini` (build flags and the `build_src_filter` that excludes
the other family's `detectors/` directory), which is configuration, not
source.

One temptation to resist, because it would make a second directive site:
compiling out the other family's profile factories in `DetectionProfile.h`.
Those factories are small config-struct builders; leaving all of them in
every build costs a few hundred bytes of flash and no RAM, and it keeps
the single-header property. The wrong-family profiles are refused at
runtime by `setDetectorSelection()` (section 3), which is the validation
hook OTA needs anyway. So `DetectionProfile.h` stays directive-free and
family-agnostic.

### 3. The Node-facing API keeps its shape; `setDetectorSelection()` validates

`cleanup-analyzer-node-isolation.md`'s non-negotiable constraint holds:
`ResonantNodeApp` calls the same methods with the same signatures.
`setDetectorSelection(DetectorSelection)` stays, but its job changes from
"switch" to "check": if the requested family is not the compiled one, it
refuses, loudly (return `false`, or set a rejected-profile flag the mode
shell prints). `applyActiveDetectionProfile()` checks the family before
applying anything else, so a mismatched profile never half-applies.

This is the hook the OTA layer needs: a profile arriving over the air is
checked against the compiled family before it is stored, not after reboot.

### 4. Profiles become per-family config, thresholds stay params

`DetectionProfile.h` is **not** split by family (see the end of section 2:
that would be a second directive site for no RAM). Every build compiles
every factory. A profile's `detectorSelection` field is its family tag,
already present today; `setDetectorSelection()` compares it against
`kCompiledFamily` and refuses a mismatch. So `TonalPulseFreq` exists in a
scalar-family firmware as a profile the firmware will name but never run.

The existing `ParamRegistry` (`052b03e`) already binds detection thresholds
as live params on the Node. Nothing changes there; it is already the
"Params" row of the table. What's new is the "Config" row: a persisted
profile choice, applied at boot. That is PAR-010's persistence backend
(NVS) applied to one value, and it is explicitly *after* PAR-010 in the
roadmap's own ordering, so this proposal does not pull persistence forward.
Until PAR-010 lands, the Node's profile is whatever the build's default
factory returns, which is what it is today.

### 5. Axis-1 vocabulary stays

`DetectorId::ScalarTransient`/`FrequencyMatch`, `OccurrenceType::Scalar`/
`Frequency`, and `DetectorReport.scalar`/`.frequency` are unchanged. They
are shared vocabulary across firmwares: an Analyzer log from a scalar-family
node and one from a frequency-family node must still name their detector
the same way. Only the *runtime dispatch* between the two values disappears;
in any given firmware exactly one value ever appears.

---

## Worked example: the third family is the MVP detector

`cleanup-analyzer-node-isolation.md` sketched a `SimpleThresholdDetector`
as an acceptance test for the four-method core contract, and
`cleanup-0-plan.md` carries it as Phase 6. `docs/specs/mvp-app-structure.md`
§3 describes, step by step, the same algorithm, then says in §3 and §5 that
no new detector class is required: reuse `ScalarTransientDetector` with one
input. The two documents were written independently and never reconciled.
This section reconciles them: **the third family is the MVP detector**, and
`mvp-app-structure.md` is amended to say so (see the note at the end).

### Why not just reuse `ScalarTransientDetector`, as the MVP spec said

It works. It is also a superset, and the size of the superset is the
argument:

| | `ScalarTransientDetector` | what MVP §3 asks for |
|---|---|---|
| source | 1,209 lines across 4 files | ~140 lines |
| config setters | 14 | 6 |
| RAM | 1,440 bytes | ~400, of which 360 is the `Occurrence` it hands over |
| carries | carrier-quality gating, matched-mean strength, coverage/island/gap bookkeeping, best-rejected summary, diagnostics counters | onset, hold, release, duration gate, cooldown |

Reusing it does not answer the question Phase 6 exists to ask, which is
whether a detector that implements *only* the core contract can run in
production. A minimal class does, in ~150 lines, and doubles as the MVP
detector. The first family added *after* `DetectionFamily.h` exists is also
the real test of that header; the two existing families were refactored
into it, this one is born into it.

### What the family consists of

```text
src/detection/detectors/simple/
  SimpleThresholdDetector.h        the class, ~60 lines
  SimpleThresholdDetector.cpp      update() / popOccurrence(), ~80 lines
src/detection/DetectionFamily.h    +1 #elif branch: aliases + the three shims, ~20 lines
DetectorId / DetectorSelection     +1 enum value each (axis-1 vocabulary, section 5)
src/detection/DetectionProfile.h   +1 config struct, +1 factory (compiled in every build)
platformio.ini                     +2 envs (Node, Analyzer), src_filter entries
DetectorReportPrinter              nothing, unless it should appear in SEQ output
```

No other file changes. If adding this family touches `DetectionRuntime`,
the shim set in section 2 was incomplete and that is the bug to fix, not
the runtime.

### The class, sized to MVP §3

```cpp
namespace detection {

class SimpleThresholdDetector {
public:
    // Core contract: everything the Node build calls.
    void resetState();
    void update(float envelope, unsigned long nowMs);   // family-specific input, as the spec allows
    bool hasPendingOccurrence() const { return _pendingPresent; }
    bool popOccurrence(Occurrence& out);

    // Diagnostics contract, deliberately trivial. reportGeneration() never
    // changes, so DetectionRuntime's captureLatestDetectorReportIfChanged()
    // never fires and the Analyzer records MissingDetectorReport, an
    // integrity state that already exists and already prints. Zero RAM.
    const DetectorReport& latestReport() const { static const DetectorReport none{}; return none; }
    uint32_t reportGeneration() const { return 0; }
    void setDiagnosticsEnabled(bool) {}

    // MVP §3 knobs, all six.
    void setOnThreshold(float v)             { _onThreshold = v; }
    void setOffThreshold(float v)            { _offThreshold = v; }      // < on: hysteresis
    void setMinOnsetMs(unsigned long v)      { _minOnsetMs = v; }        // debounce a 1-sample spike
    void setMinDurationMs(unsigned long v)   { _minDurationMs = v; }     // too short: noise
    void setMaxDurationMs(unsigned long v)   { _maxDurationMs = v; }     // too long: continuous, not a burst
    void setCooldownMs(unsigned long v)      { _cooldownMs = v; }        // merge one physical event

private:
    float _onThreshold = 0.0f, _offThreshold = 0.0f;
    unsigned long _minOnsetMs = 0, _minDurationMs = 0, _maxDurationMs = 0, _cooldownMs = 0;

    bool _above = false;
    unsigned long _aboveSinceMs = 0, _startMs = 0, _peakMs = 0, _cooldownUntilMs = 0;
    float _peak = 0.0f;

    bool _pendingPresent = false;
    Occurrence _pending = {};
    unsigned long _nextOccurrenceId = 0;
};

} // namespace detection
```

`update()` is MVP §3 steps 3–7 literally: crossing `onThreshold` starts a
candidate once it has held for `minOnsetMs`; dropping below `offThreshold`
or reaching `maxDurationMs` closes it; a closed candidate with duration in
`[minDurationMs, maxDurationMs]` becomes `_pending` with
`startMs`/`peakMs`/`endMs`/`strength = peak`; anything else is dropped;
either way `_cooldownUntilMs` is set. There is no reject-summary state
because nothing reads one; the family's `resetActiveDetectorRejectSummaries`
shim is empty.

The `DetectionFamily.h` branch:

```cpp
#elif defined(DETECTOR_FAMILY_SIMPLE)
  #include "detectors/simple/SimpleThresholdDetector.h"
  namespace detection {
    using ActiveDetector       = SimpleThresholdDetector;
    using ActiveDetectorConfig = SimpleThresholdConfig;
    constexpr DetectorSelection kCompiledFamily = DetectorSelection::SimpleThreshold;
    constexpr size_t kFamilyMaxActiveStreams = 0;   // MVP has no Inspector; see below
    inline void updateActiveDetector(ActiveDetector& d, const AudioSamplePacket& p,
        const FrequencyBandMeasurementPacket&, const ActiveDetectorConfig&, unsigned long nowMs) {
        d.update(static_cast<float>(p.smoothedLevel), nowMs);   // AmpEnvelope, and only that
    }
    inline void applyActiveDetectorConfig(ActiveDetector&, const ActiveDetectorConfig&);  // six setters
    inline void resetActiveDetectorRejectSummaries(ActiveDetector&) {}
  }
```

### What it saves

MVP §2 drops the Inspector, so the simple family has no inspection plan and
`kFamilyMaxActiveStreams = 0`. That is the number that matters:

| | bytes |
|---|---|
| neither existing detector compiled | -3,272 |
| `FeatureHistory` with zero slots | **-18,616** |
| `SimpleThresholdDetector` | +~400 |
| **net, against today's 59,996** | **about -21,500, to roughly 38,500** |

Two wrinkles, both small. A zero-length array is ill-formed C++, so the
`FeatureHistory` member becomes conditional on `kFamilyMaxActiveStreams > 0`
(one more line in the family header, and `featureHistory()` disappears from
the Analyzer surface for this family). And MVP §2 also drops
`PatternMatcher` (3,912 bytes) and `FieldStateTracker` (96), but those are
`DetectionRuntime` structure, not family; whether the MVP is a *family* or
a whole *mode* like `EMITTER_MODE` is a larger question than this document,
and as a family it already gets the detector and history savings without
touching the runtime's shape. Leave that one.

### Reconciliation note for `mvp-app-structure.md`

That document's §3 and §5 are amended (same commit as this section) to
replace "no new detector class is required, reuse `ScalarTransientDetector`
with one input" with a pointer here. Its algorithm description in §3 is
unchanged and is the spec this class implements; only the "which class"
sentence moves. The reuse option is recorded there as the fallback it is:
correct, heavier, and it leaves Phase 6's question unanswered.

---

## What this does not do

- Does not consolidate to one family. Both remain buildable; which ones are
  *built* is a release decision, informed by Phase 0's field data.
- Does not touch detector internals, thresholds, or lifecycle logic.
- Does not change `Occurrence`, `DetectorReport`, `PatternResult`, or
  `FieldState` shapes.
- Does not implement OTA or NVS persistence. It defines what a delivered
  profile must carry (a family) and where the check goes. PAR-010/PAR-013
  own the transport and storage.
- Does not bound frequency occurrence duration (see above). Separate
  decision, separate SEQ run.

---

## What it removes, said plainly

The Analyzer loses same-session cross-family comparison: `TonalPulseFreq`
against `TonalPulseScalar` is two flashes, not one command. In practice the
SEQ battery already runs them as separate 50-trial sessions (T2 and T3), and
Phase 0's matched-condition trials were never going to interleave the two
mid-sequence, so nothing in the current verification workflow depends on
it. But it is a real affordance going away, and if someone is in the habit
of `RB PROFILE`-flipping between families to eyeball behavior, that habit
breaks.

The alternative, keeping both detectors in the Analyzer build only, would
preserve it at the cost of keeping all the `DetectorSelection` dispatch
alive under `#ifdef ANALYZER_MODE`. That's a coherent design too; it just
gives up the "dispatch removed, not unified" half of the win and keeps two
code shapes for `DetectionRuntime` to reason about. Not chosen, recorded
here in case the cross-family habit turns out to matter.

---

## Risks

- **Environment matrix.** Three envs become five. Every "build all"
  instruction in the docs and any CI needs updating, and T1 means all five.
  Manageable, but it is the thing most likely to be forgotten.
- **The family check must be unmissable.** A profile from the wrong family
  reaching a node must fail visibly at the point of arrival, not degrade
  quietly at boot. Until OTA exists this is only `RB PROFILE` on Serial,
  where a printed refusal is enough.
- **`FeatureHistory` sizing is now per-family.** Get the per-family maximum
  wrong and inspection degrades to `HistoryWindowIncomplete` for the profile
  that needed the extra slot. The `static_assert` against
  `kMaxInspectionModules` bounds it above; a per-family assert against the
  actual profile factories would bound it exactly.
- **Ordering with PAR-010.** The "Config" row is not real until profile
  choice persists across reboot. Landing this before persistence means the
  Node's profile is build-default-only, which is today's behavior; that's
  fine, but don't describe OTA profiles as available until NVS is.

---

## Suggested approach

1. Write `DetectionFamily.h` (aliases, `kCompiledFamily`,
   `kFamilyMaxActiveStreams`, the three shims) and add the family build
   flags and the two new envs; extend `build_src_filter`. Build all five.
2. Point `DetectionRuntime` at `ActiveDetector`/`ActiveDetectorConfig` and
   route the three differing calls through the shims; delete
   `_detectorSelection`, `ActiveDetectorAdapter`, and every `switch` on
   the selection. `grep -c "DETECTOR_FAMILY" src/` must return exactly 1
   file when this step is done; if it doesn't, the shim set is incomplete.
   This step delivers the RAM and the dispatch removal and is
   compile-verifiable end to end.
3. Make `setDetectorSelection()` validate against `kCompiledFamily` instead
   of switch; make `applyActiveDetectionProfile()` check family first.
   Confirm the Node API is unchanged in shape. `DetectionProfile.h` is not
   touched.
4. Make `FeatureHistory::kMaxActiveStreams` take `kFamilyMaxActiveStreams`,
   with a per-family `static_assert` against that family's profile
   factories so under-sizing can't compile.
5. Measure: `sizeof(DetectionRuntime)` and real linked RAM for both Node
   families, recorded in this document.
6. Hardware: T2 on the frequency-family Analyzer, T3 on the scalar-family
   Analyzer, T7 on the frequency-family Node. T6 (profile switch) becomes
   "reboot into the other profile" for the Node and "switch within family"
   for the Analyzer; the cross-family switch no longer exists to test.
7. Decide the frequency-duration bound separately, with its own SEQ run.
