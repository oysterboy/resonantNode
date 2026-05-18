# Detection Refactor Pass K — Inspector Config + AMP Support Analyzer Output

Version: Detection Roadmap v0.3 — Pass K  
Scope: Simplify AMP support handling, make SignalInspector configuration explicit, add profile-owned inspection config, and update Analyzer output

---

## Goal

Refactor the AMP support / inspection setup so it is simpler, profile-configurable, and consistently reported.

Primary decisions:

```text
Remove legacy distance labels.
Use AmpSupportClass directly.
Centralize AMP support classification.
Configure SignalInspector through simple InspectionConfig.
Let DetectionProfile provide that InspectionConfig.
Reflect amp_support in Analyzer output.
```

This pass replaces the earlier Analyzer-only framing.

The focus is now:

```text
DetectionProfile
→ InspectionConfig
→ SignalInspector
→ InspectedSignal.ampSupport
→ PatternRules / PatternResult
→ Analyzer output
```

---

## Why this pass exists

The current code has two related problems:

```text
1. AMP support classification is more complex than needed.
2. AMP support thresholds are duplicated in more than one place.
```

The architecture does not need spatial labels right now.

AMP evidence should be reported as what it actually is:

```text
strong / medium / weak / none / unknown AMP support
```

not as a spatial distance claim.

---

## Current issue

Current code likely has AMP support logic in places such as:

```text
SignalInspector.cpp
SignalWindowEvaluator.h
Analyzer output / logs
PatternRules / PatternResult fields
```

The previous model used both:

```text
AmpSupportClass
legacy distance labels
```

This pass removes legacy distance labels from the active path and makes `AmpSupportClass` the inspected fact.

---

## Target flow

```text
DetectionProfile
→ InspectionConfig
→ SignalInspector
→ InspectionRules / WindowEvaluators
→ InspectedSignal.ampSupport
→ PatternAssembler
→ PatternCandidate.ampSupport
→ PatternRules
→ PatternResult.ampSupport
→ Analyzer output
```

Boundary:

```text
FrequencyMatchDetector creates SignalCandidate.
SignalInspector evaluates AMP support.
PatternRules interpret PatternCandidate.
Analyzer reports the result.
Behavior consumes PatternResult + FieldState.
```

---

## 1. Remove legacy distance labels

### Target

Remove legacy distance labels from the active detection/analyzer path.

Remove or replace fields such as:

```cpp
AmpSupportClass ampSupport;
```

from active structs where present:

```text
SignalCandidate
InspectedSignal
PatternCandidate
PatternResult
Analyzer output
logs
```

Replace with:

```cpp
AmpSupportClass ampSupport;
```

### Remove mapping logic

Remove mapping logic like:

```text
Strong  → Near
Medium  → Mid
Weak    → Far
None    → Far / Unknown
```

Do not keep legacy distance labels under another name.

### Accepted remaining usage

Temporary compatibility aliases are acceptable only if needed to compile, but they should not appear in the active Analyzer output or new profile configuration.

---

## 2. Keep `AmpSupportClass` as the primary inspected fact

### Target enum

Use or normalize:

```cpp
enum class AmpSupportClass {
  Unknown,
  None,
  Weak,
  Medium,
  Strong
};
```

Existing enum order may remain if already established.

### Meaning

```text
Strong:
AMP clearly supports the signal.

Medium:
AMP support exists but is moderate.

Weak:
AMP support exists but is weak / indirect.

None:
No meaningful AMP support found.

Unknown:
AMP support was not evaluated or evidence was unavailable.
```

### Documentation wording

Use:

```text
AMP support indicates physical amplitude support for a detected signal.
It may correlate with local strength but is not a reliable distance measurement.
```

Avoid:

```text
Strong does not imply distance.
Weak does not imply distance.
```

---

## 3. Centralize AMP support classification

### Target

There should be one shared AMP support classifier.

Both of these should use the same classifier:

```text
ScalarWindow AMP evaluation
snapshot / fallback AMP evaluation
```

Do not duplicate thresholds in:

```text
SignalInspector.cpp
SignalWindowEvaluator.h
```

or equivalent files.

### Suggested config

```cpp
struct AmpSupportConfig {
  float strongLift;
  float strongNorm;

  float mediumLift;
  float mediumNorm;

  float weakLift;
  float weakNorm;
};
```

### Suggested helper

```cpp
AmpSupportClass classifyAmpSupport(
  float ampLift,
  float ampNormalized,
  const AmpSupportConfig& config
);
```

Preferred name:

```text
AmpSupportClassifier
```

or, if style prefers functions:

```text
classifyAmpSupport(...)
```

### Initial default values

Use the existing thresholds as defaults.

Do not tune thresholds in this pass.

---

## 4. Add simple `InspectionConfig`

### Target

Make inspector setup explicit and simple.

Add a small config object used by `SignalInspector`.

Suggested shape:

```cpp
struct InspectionConfig {
  AmpSupportConfig ampSupport;

  uint32_t ampWindowPreMs;
  uint32_t ampWindowPostMs;

  bool enableAmpSupportInspection;
  bool enableDuplicateRiskInspection;
};
```

Keep this minimal.

Do not introduce dynamic rule registry or external config.

### Ownership

`InspectionConfig` configures how `SignalInspector` inspects signals.

It should not configure:

```text
PatternRules
Behavior
FrequencyMatchDetector lifecycle
FieldState thresholds
```

---

## 5. Configure `SignalInspector` through `InspectionConfig`

### Target

`SignalInspector` should receive or hold an `InspectionConfig`.

Possible API:

```cpp
void SignalInspector::configure(const InspectionConfig& config);
```

or:

```cpp
SignalInspector inspector(config);
```

or:

```cpp
SignalInspector::SignalInspector(const InspectionConfig& config);
```

Use the existing project style.

### Required behavior

When inspecting a frequency-first signal:

```text
FrequencyMatch SignalCandidate
→ SignalInspector
→ AMP ScalarWindow or fallback AMP snapshot
→ AmpSupportClassifier using InspectionConfig.ampSupport
→ InspectedSignal.ampSupport
```

### Boundary

Do not compute AMP support in:

```text
FrequencyMatchDetector
PatternRules
Behavior
Analyzer
```

Analyzer only reports the result.

---

## 6. Add `InspectionConfig` to `DetectionProfile`

### Target

Each `DetectionProfile` should provide the inspector config.

Example:

```cpp
struct DetectionProfile {
  DetectionProfileKind kind;
  const char* name;

  InspectionConfig inspectionConfig;
  FieldStateConfig fieldStateConfig;

  // other existing profile choices...
};
```

### Profile examples

#### `FreqAmpProfile`

```text
enableAmpSupportInspection = true
ampWindowPreMs / ampWindowPostMs configured for frequency candidates
AmpSupportConfig uses default thresholds initially
```

#### `AmpStateProfile`

```text
enableAmpSupportInspection may be true or minimal
duplicate risk / basic AMP stats may be active
```

#### `ChirpProfile`

```text
may reuse FreqAmp inspection config initially
```

### Important

Profile owns config.

Inspector applies config.

PatternRules consumes inspected facts.

---

## 7. Keep configuration code-defined

### Target

Config is code-defined through profile factories.

Do not add:

```text
JSON config
YAML config
runtime profile files
dynamic plugin registry
external threshold files
```

Simple factory example:

```cpp
DetectionProfile makeFreqAmpProfile() {
  DetectionProfile p;
  p.kind = DetectionProfileKind::FreqAmp;
  p.name = "FreqAmp";

  p.inspectionConfig.enableAmpSupportInspection = true;
  p.inspectionConfig.ampSupport = defaultAmpSupportConfig();
  p.inspectionConfig.ampWindowPreMs = 20;
  p.inspectionConfig.ampWindowPostMs = 120;

  return p;
}
```

Exact names may differ.

---

## 8. Update `PatternRules` to consume `ampSupport` directly

### Target

`PatternRules` should use:

```text
PatternCandidate.ampSupport
```

not:

```text
PatternCandidate.ampSupport
```

Preferred result shape:

```cpp
PatternResult.kind = PatternResultKind::TonalPulse;
PatternResult.ampSupport = AmpSupportClass::Strong;
```

rather than:

```text
Legacy distance-label variants
```

If old result kinds exist, keep minimal compatibility only if needed, but do not expand them.

### Boundary

PatternRules may interpret `ampSupport`.

PatternRules should not recompute AMP support from raw metrics.

---

## 9. Update `PatternResult`

### Target

Ensure `PatternResult` can carry AMP support as a field.

Suggested:

```cpp
struct PatternResult {
  PatternResultKind kind;
  bool valid;
  float confidence;
  AmpSupportClass ampSupport;
  PatternRejectReason rejectReason;
};
```

Use existing project structure where possible.

### Preferred output vocabulary

Prefer:

```text
kind=TonalPulse amp_support=Strong
```

over:

```text
kind=TonalPulse with amp_support=Strong
```

This keeps pattern kind and AMP evidence separate.

---

## 10. Update Analyzer output

### Target

Analyzer should report AMP support directly.

Use:

```text
amp_support=strong|medium|weak|none|unknown
```

Do not report:

```text
legacy distance labels
```

### Compact trial output

Suggested fields:

```text
trial=...
profile=...
result=...
pattern=...
source=...
dt=...
confidence=...
amp_support=...
reason=...
field=...
```

### Explain output

Suggested shape:

```text
signal{...}
inspection{amp_support=... amp_peak=... amp_lift=... amp_norm=...}
pattern{kind=... amp_support=...}
field{quiet=... busy=... density=...}
classification{result=... reason=...}
```

### AMP metrics

Include numeric AMP metrics when useful:

```text
amp_peak
amp_mean
amp_lift
amp_norm
amp_window_status
```

But avoid flooding compact output.

---

## 11. Update logs around AMP inspection

### Target

Use clean AMP support vocabulary in stage logs.

Recommended `INSPECTED` fields:

```text
source=...
kind=...
accepted=...
reason=...
amp_support=Strong/Medium/Weak/None/Unknown
amp_peak=...
amp_mean=...
amp_lift=...
amp_norm=...
amp_window=valid/invalid/fallback
```

Recommended `PATTERN_RESULT` fields:

```text
kind=...
valid=...
confidence=...
amp_support=...
reason=...
```

Avoid:

```text
legacy distance labels
```

---

## 12. Update code comments / docs touched by this pass

### Target

Update comments around AMP inspection, classifier, profile config, Analyzer output.

Use:

```text
AMP support is an inspected amplitude-support class.
It should not be treated as an exact distance classifier.
```

Avoid:

```text
legacy distance labels
```

unless explicitly discussing removed legacy terminology.

---

## 13. Success criteria

After this pass:

```text
Legacy distance labels are removed from the active detection/analyzer path.

AmpSupportClass is the primary AMP inspection output.

AMP support classification logic exists in one shared place.

SignalInspector is configured via InspectionConfig.

DetectionProfile provides InspectionConfig.

FreqAmpProfile can configure AMP support inspection.

ScalarWindow AMP evaluation and snapshot fallback use the same classifier.

PatternRules consumes ampSupport directly.

PatternResult carries ampSupport or equivalent.

Analyzer output reports amp_support, not legacy distance labels.

Logs no longer imply AMP is a precise distance classifier.

FrequencyMatchDetector remains unchanged in responsibility.
```

---

## 14. Do not do in this pass

Do not:

```text
tune AMP thresholds
rewrite FrequencyMatchDetector
move AMP support into FrequencyMatchDetector
move AMP support evaluation into PatternRules
introduce legacy distance labels under a different name
remove legacy AMP
implement chirp grouping
implement white-noise/object detection
change behavior strategy unless compile requires field rename
introduce external profile config
```

This pass is inspector configuration + AMP support simplification + Analyzer output alignment.
