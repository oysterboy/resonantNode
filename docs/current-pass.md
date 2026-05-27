# Pass 7 - Drop Duplicate Risk

Goal:

```text
remove duplicate-risk from the active detection pipeline and keep the plan/reporting surface lean
```

Done:

```text
[LANDED] Duplicate-risk state has been removed from the inspector, occurrence, pattern, and analyzer reporting paths.
[LANDED] DetectionProfile now composes only the scalar inspector module for each profile.
[LANDED] Analyzer and node profile reporting now describe the remaining active inspector module composition.
```

Follow-up:

```text
[TODO] Re-run TonalPulse and Amp SEQ logs to confirm the reports no longer mention duplicate-risk fields.
[TODO] Keep `InspectionConfig` only if we still need it as a legacy helper, otherwise remove the dead config shape later.
[TODO] Continue using the explicit plan shape when adding any future inspector modules.
```
