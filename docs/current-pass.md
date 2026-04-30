Do not make Analyzer SEQ responsible for buffer freshness.

AudioSignal/I2S source must own buffer hygiene:
- read chunked I2S input
- preserve chronological sample order
- reconstruct per-sample timestamps
- detect backlog/stale samples
- drop stale backlog at source level if necessary
- report dropped samples/backlog stats
- I2S source should keep a max buffered depth stat and a dropped-sample stat

AudioOnsetDetector consumes a source-agnostic stream:
`update(float level, uint32_t sampleTimeUs)`

Detector must produce the same result for the same timed sample stream, independent of chunk size.

Analyzer SEQ only classifies detector events relative to controlled chirp timing.
ResonantBehaviour has no concept of trial or expected event and must not depend on analyzer-only timing.

Progress:
- I2S source now buffers chunked input and reconstructs per-sample timestamps.
- AudioOnsetDetector now consumes a timed sample stream directly.
- Analyzer SEQ reports source drop/backlog stats in its final summary.
