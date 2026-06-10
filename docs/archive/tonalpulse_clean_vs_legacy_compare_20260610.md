# TonalPulse Clean vs Legacy Compare

Date: 2026-06-10  
Log root: `logs/seq-tests/20260610_210404-tonalpulse-compare`

---

## Purpose

Capture one short SEQ comparison run for the TonalPulse-family profiles using:

- clean path: `SEQ MODE inspect`
- remaining legacy compare path: `SEQ MODE LEG_full`

This is not an exact A/B replay of the same detector timeline. Each mode was
run separately, so trial timing and live input variation can differ.

---

## Profiles Tested

- `tonalpulse`
- `scalar_freq_experimental`

For each profile, the run also issued `SEQ REPORT` so the neutral runtime
bundle was captured alongside the clean or legacy compare output.

---

## Output Files

- combined session:
  - `logs/seq-tests/20260610_210404-tonalpulse-compare/session.log`
- scenario logs:
  - `logs/seq-tests/20260610_210404-tonalpulse-compare/tonalpulse_clean_inspect.log`
  - `logs/seq-tests/20260610_210404-tonalpulse-compare/tonalpulse_legacy_compare.log`
  - `logs/seq-tests/20260610_210404-tonalpulse-compare/scalar_freq_experimental_clean_inspect.log`
  - `logs/seq-tests/20260610_210404-tonalpulse-compare/scalar_freq_experimental_legacy_compare.log`

---

## TonalPulse

### Clean path

Source log:

- `logs/seq-tests/20260610_210404-tonalpulse-compare/tonalpulse_clean_inspect.log`

Observed clean outputs:

- `SEQ_TRIAL`
- `SEQ_INSPECT`
- `SEQ_SUMMARY`
- `SEQ REPORT` neutral bundle

Run result:

```text
trials=2
expected=1
late=1
detector_accepted=0
detector_rejects=0
patterns_valid=2
avg_dt_ms=575
```

Notable details:

- trial 1 was `late` with `dt=1131ms`
- trial 2 was `expected` with `dt=18ms`
- clean inspect showed frequency aggregate and threshold data
- clean inspect did not expose legacy source-summary blocks

### Legacy compare path

Source log:

- `logs/seq-tests/20260610_210404-tonalpulse-compare/tonalpulse_legacy_compare.log`

Observed outputs:

- same clean `SEQ_TRIAL`
- same clean `SEQ_INSPECT`
- plus legacy compare blocks:
  - `SEQ_SOURCE`
  - `SEQ_SOURCE_REJECTS`
  - `SEQ_SOURCE_LIFECYCLE`
  - `SEQ_SOURCE_DIAG`
  - `SEQ_SOURCE_TRACE`
  - `SYSTEM_HEALTH`

Run result:

```text
trials=2
expected=1
late=1
detector_accepted=0
detector_rejects=0
patterns_valid=2
avg_dt_ms=144
```

Important note:

```text
This avg_dt_ms differs from the clean run because the legacy compare path was
executed as a separate live run, not because LEG_full rewrites the clean
summary fields.
```

### TonalPulse comparison conclusion

```text
LEG_full no longer provides separate legacy trial/inspect/explain ownership.
It mainly adds the old source-summary/source-detail family plus system-health
and runtime context.
```

That supports the current decision map:

- clean analyzer truth is already on the clean path
- the remaining legacy ownership is the source-summary/source-detail family

Open issue still visible:

```text
TonalPulse can produce valid clean trial results while SEQ_SUMMARY still shows
detector_accepted=0.
```

This remains a detector/report consistency issue, not a printer-ownership
issue.

---

## scalar_freq_experimental

### Clean path

Source log:

- `logs/seq-tests/20260610_210404-tonalpulse-compare/scalar_freq_experimental_clean_inspect.log`

Observed clean outputs:

- `SEQ_TRIAL`
- `SEQ_INSPECT`
- `SEQ_SUMMARY`
- `SEQ REPORT` neutral bundle

Run result:

```text
trials=2
late=2
detector_accepted=2
detector_rejects=1
patterns_valid=2
avg_dt_ms=412
```

Important clean evidence:

- trial 1 clean inspect showed:
  - accepted scalar occurrence present
  - selected reject present
  - `reject.class=timing`
  - `reject.detector_reason=duration_too_long`
- trial 2 stayed late without a selected reject

This confirms the clean path already explains the main scalar late/reject shape
without needing legacy explain printers.

### Legacy compare path

Source log:

- `logs/seq-tests/20260610_210404-tonalpulse-compare/scalar_freq_experimental_legacy_compare.log`

Observed outputs:

- same clean `SEQ_TRIAL`
- same clean `SEQ_INSPECT`
- plus legacy compare blocks:
  - `SEQ_SOURCE`
  - `SEQ_SOURCE_REJECTS`
  - `SEQ_SOURCE_LIFECYCLE`
  - `SEQ_SOURCE_LAST_CANDIDATE`
  - `SEQ_SOURCE_DIAG`
  - `SEQ_SOURCE_TRACE`
  - `SYSTEM_HEALTH`

Run result:

```text
trials=2
late=2
detector_accepted=2
detector_rejects=1
patterns_valid=2
avg_dt_ms=671
```

What the legacy compare path adds:

```text
- old source.freq.* carrier dumps
- analyzer-local source summary wording
- lifecycle and trace blocks tied to the legacy source-summary model
```

### scalar_freq_experimental comparison conclusion

```text
The clean path already captures the important scalar decision:
accepted event plus timing reject reason = duration_too_long.
```

The remaining legacy value is mostly:

- legacy source-summary formatting
- old source carrier dumps
- mixed lifecycle/trace wording

This again points to the same next cleanup target:

```text
retire the remaining legacy source-summary/source-detail family
```

---

## Runtime Notes

- flash/upload succeeded before the run
- `COM6` was used
- one startup line in the first clean TonalPulse run showed:

```text
SEQ remote claim: ... status=timeout
```

The run still executed normally afterward, so this looked like a transient
claim/ack timing issue rather than a failed test.

---

## Overall Conclusion

The comparison run supports three conclusions:

```text
1. Clean analyzer truth is already readable enough for TonalPulse-family runs.
2. LEG_full is now mostly a shell that appends legacy source-summary/source-
   detail output to an otherwise clean trial/inspect flow.
3. The next SEQ printer cleanup should target the remaining source-summary/
   source-detail family, not attempt to resurrect removed legacy comparison
   printers.
```

Separate from printer cleanup, the run also reconfirms:

```text
TonalPulse detector_accepted=0 in summary despite valid pattern/trial results.
```

That should be investigated as a detector/report consistency issue in a later
pass, not folded into the printer-ownership cleanup.
