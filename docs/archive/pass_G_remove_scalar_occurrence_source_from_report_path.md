# Pass G — Remove ScalarOccurrenceSource from Canonical Scalar Report Path

## Goal

Move the remaining canonical scalar `DetectorReport` accepted-occurrence facts out of
`ScalarOccurrenceSource` / wrapper-held emitted occurrence state and into
`ScalarTransientDetector`.

After this pass, the canonical scalar report path should be:

```text
ScalarTransientDetector
-> accepted occurrence summary
-> scalar report detail
-> selected reject summary
-> DetectionRuntime::refreshScalarDetectorReport()
-> DetectionRuntime::scalarDetectorReport()
-> Analyzer scalar bridge
```

`ScalarOccurrenceSource` may still remain temporarily for actual `Occurrence`
emission and legacy aggregate diagnostics, but it must no longer provide canonical
scalar `DetectorReport` truth.

## Non-goals

Do not:

- change scalar detection behavior
- change thresholds, timing, duration gates, peak gates, or profile defaults
- delete `ScalarOccurrenceSource`
- delete `DetectionDiagnostics`
- rewrite Analyzer output
- migrate frequency report production
- rename profiles
- introduce stored config / OTA params
- resolve COM port / hardware rerun issues in this pass

## Required work

### 1. Add detector-owned accepted occurrence summary

Add or reuse a canonical accepted-occurrence summary owned by
`ScalarTransientDetector`.

Use the existing `AcceptedOccurrenceSummary` type from `DetectorReport.h` if it
already exists. Do not duplicate the type.

The detector-owned summary should contain the same facts currently used by
`DetectorReport.accepted`:

- present
- start/open time
- peak time
- end/release time
- duration
- strength

### 2. Populate accepted summary at scalar accept/emit time

When `ScalarTransientDetector` accepts a candidate, store the accepted occurrence
facts in the detector-owned report snapshot.

The stored values must match the current emitted occurrence semantics exactly:

- same start/open time
- same peak time
- same end/release time
- same duration
- same strength

No behavior change.

### 3. Reset accepted summary correctly

When detector/report state is reset, clear the detector-owned accepted summary.

Ensure this happens in the same reset lifecycle as the detector-owned selected
reject summary added in Pass F.

### 4. Update DetectionRuntime report assembly

Update `DetectionRuntime::refreshScalarDetectorReport()` so canonical scalar
report fields come from `ScalarTransientDetector`:

- `report.accepted`
- `report.scalarTransient`
- `report.selectedRejectPresent`
- `report.selectedReject`

After this pass, `ScalarOccurrenceSource` must not be used to populate canonical
scalar report accepted/detail/reject fields.

### 5. Keep ScalarOccurrenceSource alive only for temporary leftovers

Do not delete the wrapper yet.

It may still own:

- actual `Occurrence` emission
- accepted occurrence payload construction for the old runtime path
- legacy aggregate reject diagnostics
- reject counts
- best / second-best rejected duration aggregates
- rejected gap totals
- rejected island counts
- max rejected peak strength aggregates
- old compatibility fields

But these must remain legacy/temporary, not canonical detector report truth.

### 6. Update bridge comments

Update the `TEMP_SCALAR_REPORT_BRIDGE` comment.

It should now say roughly:

```cpp
// TEMP_SCALAR_REPORT_BRIDGE:
// Canonical scalar DetectorReport fields are detector-owned:
// accepted occurrence summary, scalar detail, and selected reject.
// ScalarOccurrenceSource remains temporarily for Occurrence emission and
// legacy DetectionDiagnostics aggregate compatibility only.
```

Move or split the comment if its current location becomes misleading.

### 7. Add pass documentation

Create:

```text
docs/detection/passes/scalar_report_source_removal.md
```

Document:

- what moved in Pass G
- which canonical scalar report fields are now detector-owned
- whether `ScalarOccurrenceSource` still contributes to canonical report truth
- what `ScalarOccurrenceSource` still owns temporarily
- remaining deletion blockers
- compile result
- whether hardware rerun was performed or still blocked

## Acceptance criteria

Pass G is successful if:

1. `platformio run -e esp32dev-analyzer` succeeds.
2. `DetectionRuntime::scalarDetectorReport()` API remains unchanged.
3. Analyzer scalar bridge still reads `scalarDetectorReport()`.
4. Scalar detection behavior is unchanged.
5. Canonical scalar `DetectorReport` fields now come from `ScalarTransientDetector`:
   - accepted occurrence summary
   - scalar detail
   - selected reject
6. `ScalarOccurrenceSource` no longer contributes to canonical scalar report truth.
7. `ScalarOccurrenceSource` may still exist only for:
   - temporary `Occurrence` emission
   - legacy aggregate diagnostics
   - compatibility leftovers
8. Legacy SEQ output remains intended-compatible.
9. No tuning/profile/default changes are included.

## Report back

Return:

- files changed
- whether accepted occurrence summary is now detector-owned
- whether `ScalarOccurrenceSource` still contributes to canonical `DetectorReport`
- remaining `ScalarOccurrenceSource` responsibilities
- remaining deletion blockers
- compile result
- hardware rerun status, if attempted

## Expected next step after Pass G

After Pass G, the likely next pass is:

```text
Pass H — Split occurrence emission from ScalarOccurrenceSource
```

Only move to Pass H after Pass G confirms that the canonical scalar report path is
fully detector-owned.
