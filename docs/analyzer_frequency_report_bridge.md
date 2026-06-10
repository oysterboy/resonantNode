# Analyzer Frequency Report Bridge

## Purpose

Bridge overlapping frequency analyzer source fields from the canonical
`DetectorReport` path while keeping the printed legacy SEQ output stable.

## Previous Frequency Analyzer Source

Before this pass, analyzer frequency synthesis primarily used:

```text
DetectionRuntime::captureDiagnostics()
-> DetectionDiagnostics.frequency*
-> AnalyzerApp::buildSequenceAnalyzerReport()
-> AnalyzerFrequencyDiagnostic / AnalyzerSourceStageReport
```

That meant the analyzer still reconstructed detector truth from the legacy
diagnostic dump even after Pass I added a detector-owned frequency report path.

## New Frequency Analyzer Source

After this pass, the overlapping frequency detector-truth fields now flow from:

```text
FrequencyMatchDetector::buildReport(...)
-> DetectionRuntime::frequencyDetectorReport()
-> AnalyzerApp::buildSequenceAnalyzerReport()
-> AnalyzerFrequencyDiagnostic / AnalyzerSourceStageReport
```

`DetectionDiagnostics` remains the fallback source for frequency data that is
not yet present in `DetectorReport`.

## Fields Now Populated from DetectorReport

Analyzer frequency fields now sourced from `DetectorReport` when the active
detector report is `DetectorId::FrequencyMatch`:

- accepted present / trial timing summary
- accepted start / peak / release / duration
- accepted strength
- accepted score / contrast
- score threshold
- contrast threshold
- score-ok / contrast-ok / both-ok / match counters
- source occurrence emitted
- runtime occurrence received
- source last reject reason
- selected reject reason when a canonical selected reject exists
- selected reject gate reason
- lifecycle state:
  `opened`, `released`, `emitted`, `validRelease`, `emitAllowed`
- lifecycle timing:
  `openMs`, `peakMs`, `releaseMs`, `durationMs`
- generic relevant thresholds:
  `minDurationMs`, `maxDurationMs`
- live frequency inspect facts:
  `rejectReason`, `candidateState`, `readyOk`, `gateOpen`
- detection gate blocked / reason
- selected reject summary overlaps:
  `rejectCount`, `bestDurationMs`, `bestOpenMs`, `bestPeakMs`,
  `bestLastMatchMs`, `bestCloseMs`, `bestPeakPrimary`,
  `bestPeakSecondary`, `bestRejectReason`

## Fields Still Populated from DetectionDiagnostics

Remaining frequency analyzer fields still using `DetectionDiagnostics` fallback:

- frame/window bookkeeping:
  `frames`, `validFrames`, `freshFrames`, `heldFrames`, `diagLongestMatchStreak*`,
  `windowMs`, `updateStepMs`, `bucketCount`, `valueCount`,
  `freshUpdateCount`, `heldUpdateCount`, `matchedUpdateCount`,
  `latestValueAgeMs`, `freshCoverageRatio`
- release / near-miss counters:
  `rejectFrames`, `releaseScoreOkFrames`, `releaseContrastOkFrames`,
  `releaseBothOkFrames`, `releaseScoreTooLowFrames`,
  `releaseContrastTooLowFrames`, `releaseScoreAndContrastTooLowFrames`,
  `releaseNoEvidenceFrames`, `nearMiss`, `nearMissReason`
- amplitude / audio health side metrics:
  `audioHealth`, `audioZeroishFrames`, `audioFlatlineFrames`,
  `audioLargeJumpFrames`, `audioRmsTooLowFrames`, `audioRmsTooHighFrames`,
  `audioMaxAbsDelta`, `ampPeak`, `ampMean`, `ampPeakMs`
- full scalar/statistical frequency summaries:
  `meanScore`, `sumScore`, `minScore`, `maxScore`,
  `meanContrast`, `sumContrast`, `minContrast`, `maxContrast`,
  target/lower/upper/neighbor power stats,
  lower/upper score stats,
  `peakScore`, `peakContrast`, `peakSampleCount`
- richer source-summary leftovers not yet in `DetectorReport`:
  `secondBestDurationMs`, `bestGateReason`, `closeCause`,
  `scoreTooLowFrames`, `contrastTooLowFrames`,
  `scoreAndContrastTooLowFrames`,
  `maxPeakPrimary`, `maxPeakPrimaryMs`,
  `maxPeakSecondary`, `maxPeakSecondaryMs`,
  `totalMatchMs`, `totalGapMs`, `maxGapMs`, `islandCount`
- last-candidate snapshot:
  `sourceLastCandidate.*`
- candidate-id / duration-decision compatibility fields:
  `acceptedCandidateId`, `selectedRejectCandidateId`, `lastCandidateId`,
  `lifecycleCandidateId`, `candidateLastMatchMs`,
  `fmDurationOk`, `fmDurationUsedMs`, `fmDurationPrintedMs`,
  `fmMinDurationUsedMs`, `fmMinDurationReportedMs`,
  `fmDurationInconsistent`, `fmPrintedDurationInconsistent`,
  `fmCloseCause`
- live-only compatibility fields not yet in `DetectorReport`:
  `liveFreqWould`, `liveFreqPresent`, `liveFreqValid`, `liveFreqMatch`

## Legacy Output Compatibility

- No SEQ field names changed.
- No legacy reporter formatting changed.
- No frequency analyzer output mode was removed.

The bridge only changes where overlapping truth is sourced from.

## What Did Not Change

- no analyzer format redesign
- no canonical `SEQ_INSPECT` yet
- no frequency occurrence-emission migration
- no `DetectionDiagnostics` deletion
- no scalar path behavior change
- no tuning

## Remaining Gaps

- analyzer still keeps a large fallback dependency on `DetectionDiagnostics`
- `DetectorReport` still lacks several rich frequency stats and compatibility
  counters that analyzer legacy output expects
- `FrequencyOccurrenceSource` still owns accepted occurrence emission
- canonical inspect/explain output is still pending

## Recommended Next Pass

Recommended next pass: `Pass K - Add canonical SEQ_INSPECT from DetectorReport`.
