# Pass 0 - Analyzer Boundary Cleanup

Goal:

```text
make the analyzer report read like a report, not like a second detector
```

Still to do:

```text
[PARTIAL] 1 Keep analyzer-owned SEQ labels prefixed and clearly separated from detection-profile and PatternResult facts.
[PARTIAL] 2 Keep SEQ_TRIAL focused on results + measurements, with the wiring repeated only in the header or summary.
[PARTIAL] 3 Keep SEQ_EXPLAIN ordered as pattern, detector, inspectors, field, debug, with detector showing source occurrence fields and measurements showing detector strength plus inspector strength instead of raw frequency-window data.
[PARTIAL] 3a Only analyzer-computed SEQ fields should use the A_ prefix; non-analyzer facts stay unmarked.
[PARTIAL] 4 Move pipeline facts into PatternResult and print them only when the current trial actually has pipeline evidence, so the analyzer does not re-derive detector truth.
[TODO] 5 Make rejects, duplicates, and debug details come forward through PatternResult and trial diagnostics instead of being recomputed in the analyzer.
```

## Pass 1 - TonalPulse Sanity Check

Goal:

```text
keep TonalPulse behavior unchanged while using the new source/config/inspection split
```

Still to do:

```text
[LANDED] Keep TonalPulse on FrequencyMatch with the current tuned defaults.
[LANDED] Keep TonalPulse inspection on AmpEnvelope with AmpStrength evidence.
[LANDED] Make PatternRules consume Occurrence.valid instead of checking frequency-specific candidate shape.
[LANDED] PatternRulesConfig now asks for a required evidence target directly.
[PARTIAL] Verify the live SEQ output still matches the intended TonalPulse wiring and wording.
```

## Pass 2 - Amp Test Profile

Goal:

```text
add a new Amp test profile that exercises the scalar path on the updated architecture
```

Still to do:

```text
[LANDED] Use ScalarTransient as the occurrence source.
[LANDED] Use AmpEnvelope as the observed stream for the scalar source.
[LANDED] Use FrequencyScore as the first scalar inspector module.
[LANDED] Add a second useful inspector module so we can test ordered multi-module inspection.
[LANDED] Keep the profile self-contained and explicit about its config defaults and overrides.
[PARTIAL] Verify the Amp profile output makes the scalar path obvious without looking like a second TonalPulse.
```

## Pass 3 - Validation

Goal:

```text
confirm the new architecture behaves as intended without broadening scope
```

Still to do:

```text
[DONE] Verify TonalPulse output is still stable in runtime logs and analyzer reports.
[DONE] Verify Amp test profile produces the expected source / inspector wiring.
[TODO] Keep the extra frequency-history projections commented out for now to reduce analyzer memory pressure.
```



## Pass 4 - Analyzer Hot Path Stabilization

Goal:

```text
keep the analyzer diagnostics useful without letting debug output perturb timing
```

Done:

```text
[LANDED] Moved SEQ_CAND emission out of the audio hot path and into trial finalization.
[LANDED] Kept candidate facts in trial diagnostics so the analyzer can still print full candidate detail.
[LANDED] Reduced analyzer reporting to one source confidence and one pattern confidence.
[LANDED] Simplified SEQ measurements to detector strength plus inspector strength.
[LANDED] Kept the detector / inspector / pattern split visible in SEQ_EXPLAIN without duplicating raw window data.
```

Follow-up:

```text
[DONE] Re-run the 50-trial TonalPulse SEQ check and confirm the miss rate stays stable without relying on inline candidate logging.
[TODO] Watch for any remaining timing skew between trial scheduling and pattern finalization.
```

## Pass 5 - Inspector Acceptance Simplification

Goal:

```text
derive inspector acceptance from the selected source/profile kind instead of carrying a separate inspectionRules field
```

Done:

```text
[LANDED] Removed the stored inspectionRules field from DetectionProfile.
[LANDED] Made OccurrenceInspector derive candidate-family acceptance from the emitted occurrence source.
[LANDED] Removed the runtime inspectionRules setter from DetectionRuntime and the sequence/session wiring.
[LANDED] Updated analyzer and node profile details to describe the derived inspection acceptance instead of a duplicated rule field.
```

Follow-up:

```text
[TODO] Re-run TonalPulse and Amp SEQ logs to confirm the simplified wiring still reads correctly.
[TODO] Keep the runtime boundary clear: source emits, inspector annotates, pattern rules decide.
```
