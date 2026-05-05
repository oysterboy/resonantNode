We are doing A–G Resonant stabilizing refactor Pass B: Resonant drain parity.

Goal:
Make Resonant mode use the same AudioSource → AudioSignal → DetectionPipeline → Behavior drain order as Analyzer.

Context:
Analyzer path is now the reference.
AudioSource produces AudioBlock.
AudioSignal / AMP produces AmpCandidate.
DetectionPipeline wraps/enriches AmpCandidate into ChirpCandidate.
SEQ consumes ChirpCandidate in Analyzer mode.

Now Resonant mode must consume the same ChirpCandidate path, not bypass it.

Target Resonant loop order:

audioSource.update();

while (audioSource.hasBlock()) {
  audioSignal.processBlock(audioSource.popBlock());
}

while (audioSignal.hasAmpCandidate()) {
  detectionPipeline.handleAmpCandidate(audioSignal.popAmpCandidate());
}

while (detectionPipeline.hasChirpCandidate()) {
  resonantBehavior.handleCandidate(detectionPipeline.popChirpCandidate());
}

resonantBehavior.update();
logger.update();

Tasks:

1. Locate the Resonant mode update/loop function.

2. Compare it against Analyzer loop order.

Analyzer order should be treated as the reference:
- AudioSource first
- process all AudioBlocks
- process all AmpCandidates through DetectionPipeline
- process all ChirpCandidates
- behavior/control update after candidate drain
- logging last

3. Replace any old direct path:

AudioSignal → ResonantBehavior

with:

AudioSignal → DetectionPipeline → ResonantBehavior

4. Ensure ResonantBehavior consumes ChirpCandidate.

If ResonantBehavior still expects the old candidate type, either:
- update it to accept ChirpCandidate, preferred
- or add a temporary adapter using candidate.amp fields

Use:
candidate.amp.durationMs
candidate.amp.peakStrength
candidate.amp.onsetSample
candidate.amp.releaseSample
candidate.amp.audioOverflowDuringCandidate

5. Drain all available candidates.

Do not do:

if (hasCandidate()) handleOne();

Do:

while (hasCandidate()) handleAll();

6. Keep logging last.

Allowed per-candidate compact log:

RB candidate n=<n> gap=<ms> dur=<ms> strength=<f> freq=<unchecked|ok|reject> audio=<ok|overflow> action=<...>

Do not print inside:
- AudioSource loop
- AudioSignal sample processing
- DetectionPipeline critical path

7. Add or keep summaries:

RB summary:
candidates=...
actions=...
overflowCandidates=...
avg_strength=...
avg_duration=...

PIPELINE summary:
ampIn=...
chirpOut=...
chirpDropped=...
freqChecked=...
freqRejected=...

AUDIO summary:
...

SIGNAL summary:
...

8. Do not change behavior rules yet.

Do not tune:
- AMP thresholds
- min/max duration
- strength threshold
- cooldown/refractory
- behavior timing
- frequency settings

This pass is only drain parity.

Acceptance:
- Resonant mode compiles.
- Resonant mode uses DetectionPipeline.
- ResonantBehavior receives ChirpCandidate or compatible adapter.
- All AudioBlocks are drained before behavior update.
- All AmpCandidates are drained into DetectionPipeline.
- All ChirpCandidates are drained into ResonantBehavior.
- Logger runs last.
- No one-candidate-per-loop backlog.
- No new verbose logs in audio-critical path.