# Pass Tooling-2 MVP — Switch SEQ sample dump to detector input

## Goal

Keep the existing `SAMPLES_*` tooling path, but change what it displays:

```text
AudioSignal envelope curve → detector input scalar curve
```

This pass is intentionally small. It does **not** introduce a generalized trace architecture.

---

## Current problem

The existing SEQ sample dump prints curve rows with fields like:

```text
fields=t,current,env,peak,open
```

Those fields came from `AudioSignal` / `CurveSnapshot` and are misleading if the useful diagnostic target is now the detector input.

For detector tuning, the more useful output is a simple scalar time curve:

```text
fields=t,value
```

---

## Target output

Replace the current sample row header:

```text
SAMPLES_BEGIN trial=<n> trigger_ms=<ms> sample_rate_ms=<step> fields=t,current,env,peak,open
```

with:

```text
SAMPLES_BEGIN trial=<n> source=detector_input trigger_ms=<ms> sample_rate_ms=<step> fields=t,value
```

Rows become:

```text
<t>,<value>
```

Example:

```text
SAMPLES_BEGIN trial=3 source=detector_input trigger_ms=123456 sample_rate_ms=1 fields=t,value
-50,0.12
-49,0.13
...
0,1.84
1,2.11
...
SAMPLES_END trial=3
```

---

## Keep existing command parameters

Keep and document only the real `sample*` parameters:

```text
sampleFirst=<n>
sampleEvery=<n>
sampleLead=<ms>
sampleTail=<ms>
sampleStep=<ms>
sampleMax=<n>
```

Meanings:

```text
sampleFirst  dump the first N trials
sampleEvery  dump every Nth trial
sampleLead   capture window before trigger, in ms
sampleTail   capture window after trigger, in ms
sampleStep   output decimation step, in ms
sampleMax    requested/effective maximum rows
```

Remove or stop documenting stale/unclear parameters:

```text
dumpSamples
curveFormat
```

---

## Implementation scope

### 1. Preserve sample dump mechanics

Keep existing mechanics:

```text
trial selection
lead/tail capture window
sampleStep decimation
sampleRows buffer
SAMPLES_BEGIN / rows / SAMPLES_END printer
```

Do not rewrite row storage yet.

---

### 2. Change source from AudioSignal to detector input

Stop using the existing `AudioSignal` envelope/current/peak/open snapshot as the printed source.

Instead, feed the sample dump with the current detector input scalar.

The detector/runtime owns what `detector_input` means for the active profile.

Examples:

```text
TonalPulse / FrequencyMatch: frequency match score or equivalent detector input scalar
Scalar detector: selected scalar input value
AMP detector: amp envelope / scalar detector input
```

Analyzer must not interpret the meaning.

---

### 3. Simplify row shape

Old row shape:

```text
t,current,env,peak,open
```

New row shape:

```text
t,value
```

No fake `current`, `env`, `peak`, or `open` fields.

---

### 4. Update help/docs

Help/docs must say:

```text
SAMPLES is detector-input curve tooling.
It is not raw audio.
It is not the audio envelope curve.
It is not DetectorReport.
It is not SEQ_SOURCE / SEQ_INSPECT / SEQ_EXPLAIN.
It is not canonical Analyzer truth.
```

Also document the actual row cap behavior:

```text
sampleMax must be clamped to the real sampleRows capacity.
The printed/effective max should not imply more rows than storage allows.
```

---

## Non-goals

Do not add in this pass:

```text
curve=audio_env|detector_input selection
fresh-only trace mode
every-N-fresh decimation
compact timestamp storage
multi-channel trace rows
feature registry
DetectorReport changes
PatternResult changes
SEQ_SOURCE changes
SEQ_INSPECT changes
SEQ_EXPLAIN changes
Analyzer classification changes
```

---

## Guardrails

Analyzer remains a dumb recorder/printer:

```text
Detector/runtime produces detector input value.
Analyzer records/windows/decimates/prints it.
Analyzer does not derive trial truth from SAMPLES.
```

Canonical analyzer truth remains:

```text
SEQ_TRIAL
SEQ_SOURCE
SEQ_INSPECT
SEQ_EXPLAIN
SEQ_SUMMARY
DetectorReport
RejectedCandidateSummary
PatternResult
```

`SAMPLES_*` remains neutral diagnostic tooling.

---

## Minimal Codex goal mode

```text
Switch the existing SEQ sample dump tooling from AudioSignal curve snapshots to the current detector input scalar. Simplify SAMPLES output fields from t,current,env,peak,open to t,value. Preserve the existing sample dump command parameters: sampleFirst, sampleEvery, sampleLead, sampleTail, sampleStep, sampleMax. Update help/docs so SAMPLES is described as detector-input curve tooling, not raw audio, not audio envelope, and not Analyzer truth. Do not add selectable curve sources, fresh-only tracing, compact storage redesign, feature registry, or DetectorReport changes in this pass.
```

---

## Commit message

```text
AnalyzerTooling: show detector input in SEQ sample dump
```
