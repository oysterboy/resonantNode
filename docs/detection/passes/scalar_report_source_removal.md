Scalar Report Source Removal
============================

Pass G completes the scalar canonical report-truth move away from
`ScalarOccurrenceSource` and into `ScalarTransientDetector`.

What moved
----------

The canonical scalar `DetectorReport` path now reads these fields directly from
`ScalarTransientDetector`:

- `acceptedPresent`
- `acceptedOccurrence`
- `scalarTransient`
- `selectedRejectPresent`
- `selectedReject`

`AcceptedOccurrenceSummary` stays the canonical accepted-occurrence contract.
This pass did not introduce a duplicate type.

Accepted occurrence semantics
-----------------------------

The detector-owned accepted summary is populated at scalar accept time with the
same semantics previously emitted through the wrapper-backed occurrence path:

- start/open time = detector peak start
- peak time = strongest observed sample time
- end/release time = debounced release-observed time
- duration = release minus start
- strength = peak strength
- score = peak strength
- contrast = `0.0f`
- confidence = `1.0f`

Reset lifecycle
---------------

The accepted summary now resets in the same lifecycle as the selected reject
summary:

- detector `resetState()`
- wrapper `resetRejectSummary()`
- runtime `resetSourceRejectSummaries()`

This keeps per-trial scalar canonical report truth from leaking across Analyzer
SEQ trials.

Wrapper status after Pass G
---------------------------

`ScalarOccurrenceSource` no longer contributes canonical truth to scalar
`DetectorReport` accepted/detail/reject fields.

The wrapper still temporarily owns:

- emitted `Occurrence` payload construction
- scalar candidate sample-time bookkeeping used by emitted occurrences
- legacy aggregate rejected-candidate diagnostics
- rejected counts, gap totals, island counts, and best/second-best summaries

Deletion blockers
-----------------

`ScalarOccurrenceSource` still cannot be removed yet because it still owns:

- the emitted `Occurrence` payload bridge consumed by the runtime pipeline
- transient payload/sample-index packaging not yet split into detector/core vs
  emission layers
- legacy aggregate scalar reject diagnostics that are still read by Analyzer
  compatibility output

Compile result
--------------

`platformio run -e esp32dev-analyzer`

- result: success

Hardware rerun
--------------

No hardware rerun was performed in this pass. This pass only verified compile
success; device validation remains pending if output-stability confirmation is
needed.
