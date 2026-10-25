// Node status output (step 10 item 9, NODE-004): RB STATUS, the RB summaries, the
// boot-time signal config, and the hop_check / plan_check lines. Moved verbatim
// from ResonantNodeApp.cpp; handleDetectCommand is now printRbStatus (it was
// RB STATUS under the wrong name, boundary map docs/refactors/archive/step10-boundary-map.md).
#include "ResonantNodeApp.h"
#include <Arduino.h>
#include <string.h>
#include <esp_system.h>
#include "../../app/RuntimeDefaults.h"
#include "../../app/TimingUtils.h"
#include "../../app/BuildInfo.h"
#include "../../app/NodeRoster.h"
#include "../../detection/HopGapContract.h"
#include "../../detection/InspectionPlanContract.h"
#include "../../detection/features/FrequencyMeasurementPacketBuilder.h"
#include "../../detection/inspection/InspectionNames.h"
#include "../../detection/evaluation/VerdictNames.h"
#include <ParamRegistry.h>
#include "../../app/ParamIds.h"
#include "../../app/TimeBase.h"
#include "../../app/SerialOut.h"

void Node::printSignalConfig() {
    uint32_t hz[8];
    const size_t n = hopFrequencies(hz, sizeof(hz) / sizeof(hz[0]));
    serialOut().print("RB signal band=");
    serialOut().print(_bandLowHz);
    serialOut().print('-');
    serialOut().print(_bandHighHz);
    serialOut().print(" bins=");
    serialOut().print(static_cast<unsigned long>(bandBinCount()));
    serialOut().print(" hop=");
    serialOut().print(_hopEnabled ? "on" : "off");
    serialOut().print(" rotation=");
    serialOut().print(_hopRotation);
    serialOut().print(" steps=");
    serialOut().print(_hopSteps);
    serialOut().print(" pulse_ms=");
    serialOut().print(_hopPulseMs);
    serialOut().print(" gap_ms=");
    serialOut().print(_hopGapMs);
    serialOut().print(" order=");
    for (size_t k = 0; k < n; ++k) {
        if (k > 0) {
            serialOut().print(',');
        }
        serialOut().print(hz[k]);
    }
    serialOut().println();
}

void Node::printRbStatus(const char* line) {
    (void)line;
    const detection::DetectionProfile& detectionProfile = activeDetectionProfile();
    serialOut().print("RB STATUS profile=");
    serialOut().print(detection::detectionProfileName(detectionProfile.kind));
    serialOut().print(" emitters=");
    serialOut().print(detection::detectorSelectionName(detectionProfile.detectorSelection));
    serialOut().print(" inspectionPlan=");
    serialOut().print(detection::inspectionPlanName(detectionProfile.inspectionPlan));
    const detection::InspectionModuleConfig* supportRequirement =
        detection::firstEnabledRequirement(detectionProfile.inspectionPlan);
    const bool supportRequired = supportRequirement != nullptr;
    serialOut().print(" has_requirement=");
    serialOut().print(supportRequired ? 1 : 0);
    serialOut().print(" required_label=");
    serialOut().print(supportRequired ? detection::inspectionTargetName(supportRequirement->target) : "none");
    serialOut().print(" behavior_state=");
    serialOut().print(_behavior.stateName());
    serialOut().print(" behavior_idle=");
    serialOut().print(_behavior.idleEnabled() ? 1 : 0);
    serialOut().print(" behavior_suppressSelfChirpMs=");
    serialOut().print(_behavior.behaviorSuppressSelfChirpMs());
    serialOut().print(" behavior_detectionSuppressTailMsOwnEmit=");
    serialOut().print(_behavior.detectionSuppressTailMsOwnEmit());
    serialOut().print(" output_busy=");
    serialOut().print(_behavior.outputBusy() ? 1 : 0);
    serialOut().print(" silent=");
    serialOut().print(_silent ? 1 : 0);
    // OutputStatus (OUT-002, step 10f): what SoundOutput is doing and has done.
    {
        const OutputStatus& out = _soundOutput.status();
        serialOut().print(" output.phase=");
        serialOut().print(outputPhaseName(out.phase));
        serialOut().print(" output.started=");
        serialOut().print(static_cast<unsigned long>(out.started));
        serialOut().print(" output.done=");
        serialOut().print(static_cast<unsigned long>(out.done));
        serialOut().print(" output.rejected=");
        serialOut().print(static_cast<unsigned long>(out.rejected));
        serialOut().print(" output.last_reject=");
        serialOut().print(outputRejectName(out.lastReject));
        serialOut().print(" output.last_start_ms=");
        serialOut().print(out.lastStartMs);
        serialOut().print(" output.last_done_ms=");
        serialOut().print(out.lastDoneMs);
        serialOut().print(" output.span_ms=");
        serialOut().print(out.spanMs);
    }
    serialOut().print(" band=");
    serialOut().print(_freqBandStream.bandLowHz());
    serialOut().print('-');
    serialOut().print(_freqBandStream.bandHighHz());
    serialOut().print(" hop_enabled=");
    serialOut().print(_hopEnabled ? 1 : 0);
    serialOut().print(" hop_rotation=");
    serialOut().print(_hopRotation);
    serialOut().print(" hop_steps=");
    serialOut().print(_hopSteps);
    serialOut().print(" hop_pulse_ms=");
    serialOut().print(_hopPulseMs);
    serialOut().print(" hop_gap_ms=");
    serialOut().print(_hopGapMs);
    const unsigned long sampleRateHz = _audioSource.sampleRateHz() > 0 ? _audioSource.sampleRateHz() : 16000UL;
    const unsigned long windowSizeSamples = _freqBandStream.windowSizeSamples();
    const unsigned long frequencyUpdateEverySamples = _freqBandStream.frequencyUpdateEverySamples();
    const unsigned long ageSamples = _freqBandStream.lastPacketAgeSamples();
    const float windowMs = sampleRateHz > 0
        ? (static_cast<float>(windowSizeSamples) * 1000.0f) / static_cast<float>(sampleRateHz)
        : 0.0f;
    const float updateStepMs = sampleRateHz > 0
        ? (static_cast<float>(frequencyUpdateEverySamples) * 1000.0f) / static_cast<float>(sampleRateHz)
        : 0.0f;
    const float ageMs = sampleRateHz > 0
        ? (static_cast<float>(ageSamples) * 1000.0f) / static_cast<float>(sampleRateHz)
        : 0.0f;
    serialOut().print(" freq.window_samples=");
    serialOut().print(windowSizeSamples);
    serialOut().print(" freq.window_ms=");
    serialOut().print(windowMs, 2);
    serialOut().print(" freq.update_every_samples=");
    serialOut().print(frequencyUpdateEverySamples);
    serialOut().print(" freq.update_period_ms=");
    serialOut().print(updateStepMs, 3);
    serialOut().print(" freq.target_hz=");
    serialOut().print(_freqBandStream.targetFrequencyHz());
    serialOut().print(" freq.produced_fresh_packet=");
    serialOut().print(_freqBandStream.producedFreshPacketOnLastObserve() ? 1 : 0);
    serialOut().print(" freq.packet_age_samples=");
    serialOut().print(ageSamples);
    serialOut().print(" freq.packet_age_ms=");
    serialOut().print(ageMs, 3);
    const AudioSourceStats& audioStats = _audioSource.stats();
    serialOut().print(" i2s.samples_read=");
    serialOut().print(static_cast<unsigned long>(audioStats.totalSamplesRead));
    serialOut().print(" i2s.dropped_dma_buffers=");
    serialOut().print(audioStats.droppedDmaBuffers);
    serialOut().print(" i2s.clock_rate_mhz=");
    serialOut().print(audioStats.sampleClockRateMilliHz);
    // Audio-source health without the Analyzer (2026-10-16: a Node that
    // heard nothing had no field saying why). overflows: DMA overflow
    // events; backsteps: blocks whose time went backwards; clock
    // corrections: sample-clock re-anchors and the largest one; deficit: how
    // many seconds of audio are missing against the uptime (reads, drops,
    // stalls), 0.0 when the stream is whole.
    const unsigned long uptimeMs = timebase::nowMs();
    const uint64_t expectedSamples = (static_cast<uint64_t>(uptimeMs) * sampleRateHz) / 1000ULL;
    const double deficitS = expectedSamples > audioStats.totalSamplesRead
        ? static_cast<double>(expectedSamples - audioStats.totalSamplesRead) / static_cast<double>(sampleRateHz)
        : 0.0;
    serialOut().print(" i2s.overflows=");
    serialOut().print(audioStats.overflowCount);
    serialOut().print(" i2s.backsteps=");
    serialOut().print(audioStats.timestampBacksteps);
    serialOut().print(" i2s.max_backstep_us=");
    serialOut().print(audioStats.maxTimestampBackstepUs);
    serialOut().print(" i2s.clock_corrections=");
    serialOut().print(audioStats.sampleClockCorrections);
    serialOut().print(" i2s.max_clock_correction_us=");
    serialOut().print(audioStats.maxSampleClockCorrectionUs);
    serialOut().print(" i2s.read_errors=");
    serialOut().print(audioStats.readErrors);
    serialOut().print(" audio.uptime_ms=");
    serialOut().print(uptimeMs);
    serialOut().print(" audio.deficit_s=");
    serialOut().print(deficitS, 1);
    // Step 8b: verdicts the 4-slot queue dropped (was Analyzer-only), and the
    // loop task's lowest free stack since boot (bytes; getWindow() puts 2 KB
    // on it per inspection module, audit 3.7).
    // Cumulative since boot or rebase (resetRbCounters): the mean over a span is
    // the difference of two STATUS lines (step 10c, per-board strength drift).
    // The room as the mic hears it (smoothed magnitude, the same value the
    // quiet baseline watches): now, the loudest since the last STATUS, and the
    // baseline. Shows whether a run had noise at all (2026-10-23 drilling log).
    serialOut().print(" audio.level=");
    serialOut().print(_audioSignal.smoothedSignalMagnitude());
    serialOut().print(" audio.level_max=");
    serialOut().print(_rbLevelMaxSinceStatus);
    _rbLevelMaxSinceStatus = 0;
    serialOut().print(" audio.baseline=");
    serialOut().print(_audioSignal.baseline(), 1);
    serialOut().print(" heard.valid=");
    serialOut().print(_rbValidPatternCount);
    serialOut().print(" heard.strength_sum=");
    serialOut().print(static_cast<unsigned long long>(_rbValidStrengthSum));
    // The radio's cost (step 11a): frames, and the heap it took.
    serialOut().print(" net.started=");
    serialOut().print(_net.started() ? 1 : 0);
    serialOut().print(" net.rx=");
    serialOut().print(_net.stats().rxFrames);
    serialOut().print(" net.tx_failed=");
    serialOut().print(_net.stats().txFailed);
    serialOut().print(" heap.free=");
    serialOut().print(static_cast<unsigned long>(esp_get_free_heap_size()));
    serialOut().print(" heap.free_min=");
    serialOut().print(static_cast<unsigned long>(esp_get_minimum_free_heap_size()));
    serialOut().print(" detection.verdict_queue_overflows=");
    serialOut().print(_detection.verdictQueueOverflowCount());
    serialOut().print(" loop.stack_free_min_bytes=");
    serialOut().print(static_cast<unsigned long>(uxTaskGetStackHighWaterMark(nullptr)));
    // Why a Node might not hear (2026-10-17): the sample clock against the
    // system clock, the own-emit suppression deadline on that axis, whether
    // the newest sample would be suppressed right now, how many samples the
    // suppression has swallowed since boot, and whether the chirp output
    // still counts as active (a train that never "finishes" keeps Behavior
    // in Chirping, which suppresses every sample).
    serialOut().print(" clock.now_ms=");
    serialOut().print(uptimeMs);
    serialOut().print(" audio.last_sample_ms=");
    serialOut().print(_lastSampleTimeMs);
    serialOut().print(" behavior.suppress_armed=");
    serialOut().print(_behavior.ownEmitSuppressArmed() ? 1 : 0);
    serialOut().print(" behavior.suppress_until_ms=");
    serialOut().print(_behavior.ownEmitSuppressUntilMs());
    serialOut().print(" behavior.suppressed_now=");
    serialOut().print(_behavior.ownEmitDetectionSuppressed(_lastSampleTimeMs) ? 1 : 0);
    serialOut().print(" behavior.suppressed_samples=");
    serialOut().print(_suppressedSampleCount);
    serialOut().print(" chirp_active=");
    serialOut().print(_chirpOutput.isActive() ? 1 : 0);
    serialOut().print(" field.patterns=");
    serialOut().print(_detection.fieldState().recentPatternCount);
    serialOut().print(" behavior.resting=");
    serialOut().print(_behavior.resting(uptimeMs) ? 1 : 0);
    serialOut().print(" behavior.rest_until_ms=");
    serialOut().print(_behavior.restUntilMs());
    serialOut().print(" behavior.rests=");
    serialOut().print(_behavior.restsEntered());
    serialOut().print(" behavior.last_rest=");
    serialOut().print(ResonantBehavior::behaviorDecisionName(_behavior.lastRestReason()));
    serialOut().print(" behavior.blocked_resting=");
    serialOut().print(_behavior.blockedResting());
    serialOut().print(" ");
    printBuildIdentity(serialOut(), "node");
    serialOut().println();
}

// One line on the fit between the network signal and the scalar detector
// (detection/HopGapContract.h). status=fail: the train can never be one
// occurrence; status=warn: only where the room tail bridges the gaps (the
// shipped 10 ms gap against release debounce 10 is warn, step 5h item 1).
void Node::printHopGapCheck() {
    if (!_hopEnabled) {
        return;
    }
    uint32_t hz[8];
    detection::HopTrainShape shape;
    shape.steps = static_cast<unsigned long>(hopFrequencies(hz, sizeof(hz) / sizeof(hz[0])));
    shape.pulseMs = _hopPulseMs;
    shape.gapMs = _hopGapMs;
    const detection::ScalarTransientConfig& sc = _activeDetectionProfile.scalarTransient;
    const detection::HopGapCheck c = detection::checkHopGap(shape, sc);
    serialOut().print("RB hop_check status=");
    serialOut().print(!c.hardOk() ? "fail" : (!c.softOk() ? "warn" : "ok"));
    serialOut().print(" train_ms=");
    serialOut().print(c.trainMs);
    serialOut().print(" gap_ms=");
    serialOut().print(shape.gapMs);
    serialOut().print(" release_debounce_ms=");
    serialOut().print(sc.releaseDebounceMs);
    serialOut().print(" max_gap_ms=");
    serialOut().print(sc.maxGapMs);
    serialOut().print(" duration_ms=");
    serialOut().print(sc.minTransientDurationMs);
    serialOut().print("..");
    serialOut().print(sc.maxTransientDurationMs);
    serialOut().print(" issues=");
    bool any = false;
    const struct {
        bool ok;
        const char* name;
    } items[] = {
        {c.durationFits, "duration"},
        {c.coverageFits, "coverage"},
        {c.islandFits, "island"},
        {c.gapWithinMaxGap, "max_gap"},
        {c.gapBelowDebounce, "gap_not_below_debounce"},
        {c.tailFits, "no_tail_margin"},
    };
    for (const auto& item : items) {
        if (!item.ok) {
            serialOut().print(any ? "," : "");
            serialOut().print(item.name);
            any = true;
        }
    }
    serialOut().println(any ? "" : "none");
}

// RB plan_check: detection.scalar_* durations against the inspection plan's
// windows and the FeatureHistory depth (detection/InspectionPlanContract.h,
// step 8b item 2). status=fail: inspection reads past the history;
// status=warn: occurrences near the min duration are inspected before their
// window has arrived (shipped: early_window_ms=5, audit 3.1, DET-017).
void Node::printInspectionPlanCheck() {
    const detection::InspectionPlanCheck c =
        detection::checkInspectionPlan(_activeDetectionProfile.scalarTransient, _activeDetectionProfile.inspectionPlan);
    serialOut().print("RB plan_check status=");
    serialOut().print(!c.hardOk() ? "fail" : (!c.softOk() ? "warn" : "ok"));
    serialOut().print(" history_needed_ms=");
    serialOut().print(c.historyNeededMs);
    serialOut().print(" history_ms=");
    serialOut().print(c.historySpanMs);
    serialOut().print(" early_window_ms=");
    serialOut().print(c.earlyWindowMs);
    serialOut().print(" issues=");
    if (c.hardOk() && c.softOk()) {
        serialOut().println("none");
        return;
    }
    serialOut().print(!c.historyFits ? "history_too_short" : "");
    serialOut().print(!c.historyFits && !c.windowCompleteAtRelease ? "," : "");
    serialOut().println(!c.windowCompleteAtRelease ? "window_after_inspection" : "");
}

void Node::printRbSummary() const {
    const float avgStrength = _rbPendingCount > 0 ? (static_cast<float>(_rbStrengthSumScaled) / 100.0f) / static_cast<float>(_rbPendingCount) : 0.0f;
    const float avgDuration = _rbPendingCount > 0 ? static_cast<float>(_rbDurationSumMs) / static_cast<float>(_rbPendingCount) : 0.0f;
    const AudioSignalStats& stats = _audioSignal.stats();

    serialOut().print("RB summary: pending=");
    serialOut().print(_rbPendingCount);
    serialOut().print(" accepted=");
    serialOut().print(_rbPatternAcceptedCount);
    serialOut().print(" validPatterns=");
    serialOut().print(_rbValidPatternCount);
    serialOut().print(" chirpStarted=");
    serialOut().print(_rbChirpStartedCount);
    serialOut().print(" overflowPending=");
    serialOut().print(_rbOverflowPending);
    serialOut().print(" avg_strength=");
    serialOut().print(avgStrength, 1);
    serialOut().print(" avg_duration=");
    serialOut().print(avgDuration, 1);
    serialOut().print("ms");
    serialOut().print(" profile=");
    serialOut().print(profileName());
    printBuildIdentity(serialOut(), "node");
    serialOut().println();
    serialOut().print("RB baseline state=");
    serialOut().println(rbBaselineStateName());
    serialOut().print("RB log mode=");
    serialOut().println(rbLogModeName());

    printRbBehaviorSummary();
    printRbSignalSummary();
}

void Node::printRbBehaviorSummary() const {
    serialOut().print("RB behavior idle=");
    serialOut().print(_behavior.idleEnabled() ? 1 : 0);
    serialOut().print(" wait=");
    serialOut().print(_behavior.waitAfterHeardMs());
    serialOut().print(" refractory=");
    serialOut().print(_behavior.refractoryAfterEmitMs());
    serialOut().print(" suppressSelfChirp=");
    serialOut().print(_behavior.behaviorSuppressSelfChirpMs());
    serialOut().print(" detectionSuppressTail=");
    serialOut().print(_behavior.detectionSuppressTailMsOwnEmit());
    serialOut().print(" idleTimeout=");
    serialOut().print(_behavior.idleTimeoutMs());
    serialOut().print(" idleTimeoutVariation=");
    serialOut().print(_behavior.idleTimeVariationMs());
    serialOut().print(" idleBlockedHeard=");
    serialOut().print(_behavior.idleBlockedAfterHeardMs());
    serialOut().print(" idleBlockedOwnEmit=");
    serialOut().print(_behavior.idleBlockedAfterOwnEmitMs());
    serialOut().print(" idleEnabled=");
    serialOut().print(_behavior.idleEnabled() ? 1 : 0);
    serialOut().print(" lastDecision=");
    serialOut().print(_behavior.lastDecisionName());
    serialOut().print(" lastBlock=");
    serialOut().print(_behavior.lastBlockReasonName());
    serialOut().print(" lastPattern=");
    serialOut().print(_behavior.lastVerdictTypeName());
    serialOut().print(" lastHeardMs=");
    serialOut().print(_behavior.lastHeardMs());
    serialOut().print(" lastEmitMs=");
    serialOut().print(_behavior.lastEmitMs());
    serialOut().print(" waitUntilMs=");
    serialOut().print(_behavior.waitUntilMs());
    serialOut().print(" refractoryUntilMs=");
    serialOut().print(_behavior.refractoryUntilMs());
    serialOut().print(" ownEmitDetectionSuppressUntilMs=");
    serialOut().print(_behavior.ownEmitDetectionSuppressUntilMs());
    serialOut().print(" outputBusy=");
    serialOut().print(_behavior.outputBusy() ? 1 : 0);
    serialOut().print(" patternsReceived=");
    serialOut().print(_behavior.patternsReceived());
    serialOut().print(" ignoredInvalid=");
    serialOut().print(_behavior.patternsIgnoredInvalid());
    serialOut().print(" blockedOutputBusy=");
    serialOut().print(_behavior.blockedOutputBusy());
    serialOut().print(" blockedWaiting=");
    serialOut().print(_behavior.blockedWaiting());
    serialOut().print(" blockedRefractory=");
    serialOut().print(_behavior.blockedRefractory());
    serialOut().print(" blockedSelfSuppressed=");
    serialOut().print(_behavior.blockedSelfSuppressed());
    serialOut().print(" wouldEmit=");
    serialOut().print(_behavior.wouldEmitCount());
    serialOut().print(" emitted=");
    serialOut().println(_behavior.emittedCount());
}

void Node::printRbSignalSummary() const {
    serialOut().print("RB occurrence baseline=");
    serialOut().print(_audioSignal.baseline(), 1);
    serialOut().print(" mag=");
    serialOut().print(_audioSignal.signalMagnitude());
    serialOut().print(" smooth=");
    serialOut().println(_audioSignal.smoothedSignalMagnitude());
}
