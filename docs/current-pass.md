Pass H3 Remove OccurrenceDetailKind and correct canonical occurrence vocabulary

Goal:
Correct the current canonical DetectionTypes vocabulary before more refactor work builds on the wrong split.

This is a corrective pass to the current refactor, not a broad runtime migration.

Decision

For the current refactor, keep only these canonical enums:

enum class DetectorId : uint8_t {
    Unknown = 0,
    ScalarTransient,
    FrequencyMatch,
};

enum class OccurrenceType : uint8_t {
    None = 0,
    Transient,
    FrequencyMatch,
};

Remove OccurrenceDetailKind.

Do not replace it with another detail-kind enum in this pass.

Rationale

OccurrenceDetailKind is currently unnecessary because the detail payload layout is implied by OccurrenceType.

Current intended meanings:

DetectorId = detector implementation/family that produced the occurrence.
OccurrenceType = public event category consumed by PatternMatcher / Analyzer / Behavior.
Carrier feature/config may vary independently from occurrence type.
Detail payload layout is implied by OccurrenceType for now.

Examples:

DetectorId::ScalarTransient
OccurrenceType::Transient

This may represent scalar transient detection over AMP, frequency score, frequency contrast, or another scalar carrier. The public occurrence type remains Transient.

DetectorId::FrequencyMatch
OccurrenceType::FrequencyMatch

This represents the specialized frequency-match detector path with its own frequency-specific payload.

Required code changes
Remove OccurrenceDetailKind from DetectionTypes.
Remove any fields, comments, defaults, or switch branches that depend on OccurrenceDetailKind.
Rename / replace any current OccurrenceType::AmpTransient usage with OccurrenceType::Transient.
Do not introduce AmpTransient, FrequencyTransient, or another payload-kind enum in this pass.
Do not encode carrier/source feature identity into OccurrenceType.
Do not migrate unrelated detection runtime behavior.
Required docs updates

Update the active roadmap and contract docs so they match the corrected vocabulary.

At minimum, inspect and update:

docs/detection_minimal_contracts.md
docs/detection_contract_trim_inventory.md
docs/roadmaps/roadmap-detection-refactor-clean-architecture.md
any current-pass / DetectionTypes notes created during Pass A / G work

Docs should state:

DetectorId identifies the detector implementation/family.
OccurrenceType identifies the public event category.
Occurrence detail payload is implied by OccurrenceType.
OccurrenceDetailKind is intentionally not part of the current contract.
Carrier feature identity must remain separate from OccurrenceType.
Acceptance criteria
OccurrenceDetailKind no longer exists in code.
OccurrenceType contains only:
None
Transient
FrequencyMatch
No references remain to OccurrenceType::AmpTransient.
Scalar AMP transient code compiles using OccurrenceType::Transient.
Roadmap and minimal contract docs explicitly reflect the lean contract.
No unrelated detection behavior changes are made.
Compile after the change.
Report touched files and any remaining grep hits for:
OccurrenceDetailKind
AmpTransient
FrequencyTransient