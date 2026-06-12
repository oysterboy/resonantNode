# Future FREQ_DIAG

Status: future tooling note
Scope: FrequencyMatch tuning diagnostics

Pass X1 removed the legacy `DetectionDiagnostics` bridge and stopped copying
deep FrequencyMatch diagnostic material into clean Analyzer output.

If deep frequency tuning output is needed again, add it as an explicit
`FREQ_DIAG` / tooling surface that reads from detector-owned frequency tooling
or `FrequencyMatchDetector` internals directly.

Do not route these fields through:

- `SEQ_SOURCE`
- `SEQ_INSPECT`
- `SEQ_EXPLAIN`
- `SEQ_SUMMARY`
- `AnalyzerReport` clean fields

Candidate future fields:

- score and contrast means / extrema
- target and neighbor band power means / extrema
- release reject frame counters
- longest match streak details
- deep candidate/frame summaries
- near-miss tuning explanations
