# Detection Naming Glossary

Use these meanings in active detection and analyzer code:

- `Value` = bare scalar number
- `Sample` = timestamped scalar observation: value plus time, index, or freshness
- `Packet` = live pipeline handoff object; may be compound
- `Measurement` = derived measurement, not evidence yet
- `Window` = bounded range or slice
- `Frame` = timing, process, or protocol unit only
- `Block` = source or transport chunk
- `Evidence` = inspected support or reject payload
- `Label` = human-readable output only

Keep this glossary current with active code.
Do not apply these names retroactively to archive docs or historical snapshots.
