Pass 01 — Fix scalar frequency freshness

Do this first because it affects architectural correctness.

Add helper:
bool streamRequiresFreshFrequency(FeatureStreamId stream)
In scalar occurrence path, skip update unless fresh when observing frequency streams.
Do not let held frequency values drive scalar detector lifecycle.

Commit:

DetectionCleanup [f01] Fresh-gate scalar frequency occurrence source

- Skip ScalarTransient updates for frequency-derived streams unless the frequency packet is fresh.
- Preserve AMP scalar behavior.
- Prevent held frequency values from extending scalar candidate duration.
Pass 02 — Remove remaining Analyzer heap allocation
Replace AnalyzerReport* _sequenceReportScratch with direct member.
Remove new.
Keep memory inventory print for sizeof(AnalyzerReport).

Commit:

DetectionCleanup [f02] Direct-own analyzer report scratch

- Replace heap-allocated AnalyzerReport scratch with direct AnalyzerApp ownership.
- Remove runtime new allocation from sequence report path.
- Keep report reset behavior unchanged.
Pass 03 — Compute exact window quantiles
Use the stored feature values in each retained window to compute median, p75, p90, and trimmed mean directly.
Do not alias them to max or mean.

Commit:

DetectionCleanup [f03] Remove misleading scalar quantile placeholders

- Compute p75/p90/median/trimmed mean from the stored window values.
- Keep mean, peak, RMS, count, and coverage fields valid.
- Prepare bounded quantile implementation for a later pass.
Pass 04 — Add overflow and stale-pointer safety
Add `_resultQueueOverflowCount`.
Store inspected occurrence data by value inside `PatternResult` so queued results stay safe.

Commit:

DetectionCleanup [f04] Harden pattern result queue diagnostics

- Count dropped `PatternResult`s when the queue is full.
- Keep inspected-occurrence data inside `PatternResult` as a value snapshot.
- Expose overflow state in Analyzer diagnostics.
