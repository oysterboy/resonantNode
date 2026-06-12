# Pass PAR-001 - Analyzer tuning surface

Status: completed
Scope: analyzer tuning surface only
Goal: expose a small module-owned tuning surface for `TonalPulseScalar` so the analyzer can adjust profile parameters through `PARAM`, and print the scalar profile parameters in the sequence header.

---

## Landed

- `AnalyzerApp` now carries module-owned tuning state for frequency-match and scalar transient settings.
- `PARAM` parsing can update scalar tuning values alongside the existing frequency-match fields.
- The active sequence profile merges the tuning surface before runtime applies it.
- Sequence start and `SEQ` reporting print the scalar profile fields explicitly.
- User-facing tuning text now says `Scalar` instead of leaking the internal `AmpEnvelope` stream name.

## Verification

- Analyzer build passes with `platformio run -e esp32dev-analyzer`.

## Next

- `PAR-002` minimal Param core and registry work.
