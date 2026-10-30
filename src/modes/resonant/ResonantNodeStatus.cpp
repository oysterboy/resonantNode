// Node status output (step 10 item 9, NODE-004): RB STATUS, the RB summaries, the
// boot-time signal config, and the hop_check / plan_check lines. Moved verbatim
// from ResonantNodeApp.cpp; handleDetectCommand is now printRbStatus (it was
// RB STATUS under the wrong name, boundary map docs/refactors/archive/step10-boundary-map.md).
#include "ResonantNodeApp.h"
#include <Arduino.h>
#include <esp_system.h>
#include "../../app/RuntimeDefaults.h"
#include "../../app/BuildInfo.h"
#include "../../detection/HopGapContract.h"
#include "../../detection/InspectionPlanContract.h"
#include "../../detection/inspection/InspectionNames.h"
#include "../../app/TimeBase.h"
#include "../../app/SerialOut.h"
#include "../../app/PrintKeyValue.h"
#include "../../app/FreqStatusFields.h"

void Node::printSignalConfig() {
    const NodeSignalComposer::Values& hop = _composer.values();
    const NodeSignalComposer::BandGrid grid = bandGrid();
    uint32_t hz[8];
    const size_t n = _composer.hopFrequencies(grid, hz, sizeof(hz) / sizeof(hz[0]));
    serialOut().print("RB signal band=");
    serialOut().print(_band.lowHz);
    serialOut().print('-');
    serialOut().print(_band.highHz);
    printKeyValue("bins", static_cast<unsigned long>(grid.bins));
    printKeyValue("hop", hop.hopEnabled ? "on" : "off");
    printKeyValue("rotation", hop.hopRotation);
    printKeyValue("steps", hop.hopSteps);
    printKeyValue("pulse_ms", hop.hopPulseMs);
    printKeyValue("gap_ms", hop.hopGapMs);
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
    printKeyValue("emitters", detection::detectorSelectionName(detectionProfile.detectorSelection));
    printKeyValue("inspectionPlan", detection::inspectionPlanName(detectionProfile.inspectionPlan));
    const detection::InspectionModuleConfig* supportRequirement =
        detection::firstEnabledRequirement(detectionProfile.inspectionPlan);
    const bool supportRequired = supportRequirement != nullptr;
    printKeyValue("has_requirement", supportRequired ? 1 : 0);
    printKeyValue("required_label",
                  supportRequired ? detection::inspectionTargetName(supportRequirement->target) : "none");
    // Behavior's snapshot (step 11i N6, review A9); its time fields are taken at uptimeMs.
    const unsigned long uptimeMs = timebase::nowMs();
    const BehaviorStatus b = _behavior.status(uptimeMs);
    printKeyValue("behavior_state", b.stateName);
    printKeyValue("behavior_idle", b.config.idleEnabled ? 1 : 0);
    printKeyValue("behavior_suppressSelfChirpMs", b.config.behaviorSuppressSelfChirpMs);
    printKeyValue("behavior_detectionSuppressTailMsOwnEmit", b.config.detectionSuppressTailMsOwnEmit);
    printKeyValue("output_busy", b.outputBusy ? 1 : 0);
    printKeyValue("silent", _switches.silent ? 1 : 0);
    // OutputStatus (OUT-002, step 10f): what SoundOutput is doing and has done.
    {
        const OutputStatus& out = _soundOutput.status();
        printKeyValue("output.phase", outputPhaseName(out.phase));
        printKeyValue("output.started", static_cast<unsigned long>(out.started));
        printKeyValue("output.done", static_cast<unsigned long>(out.done));
        printKeyValue("output.rejected", static_cast<unsigned long>(out.rejected));
        printKeyValue("output.last_reject", outputRejectName(out.lastReject));
        printKeyValue("output.last_start_ms", out.lastStartMs);
        printKeyValue("output.last_done_ms", out.lastDoneMs);
        printKeyValue("output.span_ms", out.spanMs);
    }
    printKeyValue("band", _freqBandStream.bandLowHz());
    serialOut().print('-');
    serialOut().print(_freqBandStream.bandHighHz());
    {
        const NodeSignalComposer::Values& hop = _composer.values();
        printKeyValue("hop_enabled", hop.hopEnabled ? 1 : 0);
        printKeyValue("hop_rotation", hop.hopRotation);
        printKeyValue("hop_steps", hop.hopSteps);
        printKeyValue("hop_pulse_ms", hop.hopPulseMs);
        printKeyValue("hop_gap_ms", hop.hopGapMs);
    }
    const unsigned long sampleRateHz =
        _audioSource.sampleRateHz() > 0 ? _audioSource.sampleRateHz() : runtime::kDefaultAudioI2SSampleRateHz;
    printFreqStatusFields(serialOut(), _freqBandStream, sampleRateHz);
    const AudioSourceStats& audioStats = _audioSource.stats();
    printKeyValue("i2s.samples_read", static_cast<unsigned long>(audioStats.totalSamplesRead));
    printKeyValue("i2s.dropped_dma_buffers", audioStats.droppedDmaBuffers);
    printKeyValue("i2s.clock_rate_mhz", audioStats.sampleClockRateMilliHz);
    // Audio-source health without the Analyzer (2026-10-16: a Node that
    // heard nothing had no field saying why). overflows: DMA overflow
    // events; backsteps: blocks whose time went backwards; clock
    // corrections: sample-clock re-anchors and the largest one; deficit: how
    // many seconds of audio are missing against the uptime (reads, drops,
    // stalls), 0.0 when the stream is whole.
    const uint64_t expectedSamples = (static_cast<uint64_t>(uptimeMs) * sampleRateHz) / 1000ULL;
    const double deficitS = expectedSamples > audioStats.totalSamplesRead
        ? static_cast<double>(expectedSamples - audioStats.totalSamplesRead) / static_cast<double>(sampleRateHz)
        : 0.0;
    printKeyValue("i2s.overflows", audioStats.overflowCount);
    printKeyValue("i2s.backsteps", audioStats.timestampBacksteps);
    printKeyValue("i2s.max_backstep_us", audioStats.maxTimestampBackstepUs);
    printKeyValue("i2s.clock_corrections", audioStats.sampleClockCorrections);
    printKeyValue("i2s.max_clock_correction_us", audioStats.maxSampleClockCorrectionUs);
    printKeyValue("i2s.read_errors", audioStats.readErrors);
    printKeyValue("audio.uptime_ms", uptimeMs);
    printKeyValue("audio.deficit_s", deficitS, 1);
    // Step 8b: verdicts the 4-slot queue dropped (was Analyzer-only), and the
    // loop task's lowest free stack since boot (bytes; getWindow() puts 2 KB
    // on it per inspection module, audit 3.7).
    // Cumulative since boot or rebase (NodeCounters, reset by assignment): the mean over a span is
    // the difference of two STATUS lines (step 10c, per-board strength drift).
    // The room as the mic hears it (smoothed magnitude, the same value the
    // quiet baseline watches): now, the loudest since the last STATUS, and the
    // baseline. Shows whether a run had noise at all (2026-10-23 drilling log).
    printKeyValue("audio.level", _audioSignal.smoothedSignalMagnitude());
    printKeyValue("audio.level_max", _rbLevelMaxSinceStatus);
    _rbLevelMaxSinceStatus = 0;
    printKeyValue("audio.baseline", _audioSignal.baseline(), 1);
    printKeyValue("heard.valid", _counters.validPatterns);
    printKeyValue("heard.strength_sum", static_cast<unsigned long long>(_counters.validStrengthSum));
    // The radio's cost (step 11a): frames, and the heap it took.
    printKeyValue("net.started", _net.started() ? 1 : 0);
    printKeyValue("net.rx", _net.stats().rxFrames);
    printKeyValue("net.tx_failed", _net.stats().txFailed);
    printKeyValue("heap.free", static_cast<unsigned long>(esp_get_free_heap_size()));
    printKeyValue("heap.free_min", static_cast<unsigned long>(esp_get_minimum_free_heap_size()));
    printKeyValue("detection.verdict_queue_overflows", _detection.verdictQueueOverflowCount());
    printKeyValue("loop.stack_free_min_bytes", static_cast<unsigned long>(uxTaskGetStackHighWaterMark(nullptr)));
    // Why a Node might not hear (2026-10-17): the sample clock against the
    // system clock, the own-emit suppression deadline on that axis, whether
    // the newest sample would be suppressed right now, how many samples the
    // suppression has swallowed since boot, and whether the chirp output
    // still counts as active (a train that never "finishes" keeps Behavior
    // in Chirping, which suppresses every sample).
    printKeyValue("clock.now_ms", uptimeMs);
    printKeyValue("audio.last_sample_ms", _lastSampleTimeMs);
    printKeyValue("behavior.suppress_armed", b.ownEmitSuppressArmed ? 1 : 0);
    printKeyValue("behavior.suppress_until_ms", b.ownEmitSuppressUntilMs);
    printKeyValue("behavior.suppressed_now", _behavior.ownEmitDetectionSuppressed(_lastSampleTimeMs) ? 1 : 0);
    printKeyValue("behavior.suppressed_samples", _suppressedSampleCount);
    printKeyValue("chirp_active", _chirpOutput.isActive() ? 1 : 0);
    printKeyValue("field.patterns", _detection.fieldState().recentPatternCount);
    printKeyValue("behavior.resting", b.resting ? 1 : 0);
    printKeyValue("behavior.rest_until_ms", b.restUntilMs);
    printKeyValue("behavior.rests", b.restsEntered);
    printKeyValue("behavior.level", b.level);
    printKeyValue("behavior.declined", b.declinedByLevel);
    printKeyValue("behavior.yielded", b.yielded);
    printKeyValue("behavior.last_rest", b.lastRestReasonName);
    printKeyValue("behavior.blocked_resting", b.blockedResting);
    serialOut().print(" ");
    printBuildIdentity(serialOut(), "node");
    serialOut().println();
}

// One line on the fit between the network signal and the scalar detector
// (detection/HopGapContract.h). status=fail: the train can never be one
// occurrence; status=warn: only where the room tail bridges the gaps (the
// shipped 10 ms gap against release debounce 10 is warn, step 5h item 1).
void Node::printHopGapCheck() {
    const NodeSignalComposer::Values& hop = _composer.values();
    if (!hop.hopEnabled) {
        return;
    }
    uint32_t hz[8];
    detection::HopTrainShape shape;
    shape.steps = static_cast<unsigned long>(_composer.hopFrequencies(bandGrid(), hz, sizeof(hz) / sizeof(hz[0])));
    shape.pulseMs = hop.hopPulseMs;
    shape.gapMs = hop.hopGapMs;
    const detection::ScalarTransientConfig& sc = _activeDetectionProfile.scalarTransient;
    const detection::HopGapCheck c = detection::checkHopGap(shape, sc);
    serialOut().print("RB hop_check status=");
    serialOut().print(!c.hardOk() ? "fail" : (!c.softOk() ? "warn" : "ok"));
    printKeyValue("train_ms", c.trainMs);
    printKeyValue("gap_ms", shape.gapMs);
    printKeyValue("release_debounce_ms", sc.releaseDebounceMs);
    printKeyValue("max_gap_ms", sc.maxGapMs);
    printKeyValue("duration_ms", sc.minTransientDurationMs);
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
    printKeyValue("history_needed_ms", c.historyNeededMs);
    printKeyValue("history_ms", c.historySpanMs);
    printKeyValue("early_window_ms", c.earlyWindowMs);
    serialOut().print(" issues=");
    if (c.hardOk() && c.softOk()) {
        serialOut().println("none");
        return;
    }
    const char* sep = "";
    if (!c.historyFits) {
        serialOut().print("history_too_short");
        sep = ",";
    }
    if (!c.shapeFits) {
        serialOut().print(sep);
        serialOut().print("shape_window_too_long");
        sep = ",";
    }
    if (!c.windowCompleteAtRelease) {
        serialOut().print(sep);
        serialOut().print("window_after_inspection");
        sep = ",";
    }
    if (!c.shapeLevelInDomain) {
        // The shape rule's level (the release threshold) is not a FrequencyTarget score (review 12.0 L4).
        serialOut().print(sep);
        serialOut().print("shape_level_out_of_domain");
        sep = ",";
    }
    if (!c.classLimitsOrdered) {
        // A medium limit above its strong limit is ignored (deep review I-M-B).
        serialOut().print(sep);
        serialOut().print("class_limits_unordered");
    }
    serialOut().println();
}

void Node::printRbSummary() const {
    const float avgStrength = _counters.pending > 0
        ? (static_cast<float>(_counters.strengthSumScaled) / 100.0f) / static_cast<float>(_counters.pending)
        : 0.0f;
    const float avgDuration = _counters.pending > 0
        ? static_cast<float>(_counters.durationSumMs) / static_cast<float>(_counters.pending)
        : 0.0f;
    const AudioSignalStats& stats = _audioSignal.stats();

    serialOut().print("RB summary: pending=");
    serialOut().print(_counters.pending);
    printKeyValue("accepted", _counters.patternAccepted);
    printKeyValue("validPatterns", _counters.validPatterns);
    printKeyValue("chirpStarted", _counters.chirpStarted);
    printKeyValue("overflowPending", _counters.overflowPending);
    printKeyValue("avg_strength", avgStrength, 1);
    printKeyValue("avg_duration", avgDuration, 1);
    serialOut().print("ms");
    printKeyValue("profile", profileName());
    printBuildIdentity(serialOut(), "node");
    serialOut().println();
    serialOut().print("RB baseline state=");
    serialOut().println(_baseline.stateName());
    serialOut().print("RB log mode=");
    serialOut().println(rbLogModeName());

    printRbBehaviorSummary();
    printRbSignalSummary();
}

void Node::printRbBehaviorSummary() const {
    const BehaviorStatus b = _behavior.status(timebase::nowMs());  // step 11i N6
    serialOut().print("RB behavior idle=");
    serialOut().print(b.config.idleEnabled ? 1 : 0);
    printKeyValue("wait", b.config.waitAfterHeardMs);
    printKeyValue("refractory", b.config.refractoryAfterEmitMs);
    printKeyValue("suppressSelfChirp", b.config.behaviorSuppressSelfChirpMs);
    printKeyValue("detectionSuppressTail", b.config.detectionSuppressTailMsOwnEmit);
    printKeyValue("idleTimeout", b.config.idleTimeoutMs);
    printKeyValue("idleTimeoutVariation", b.config.idleTimeVariationMs);
    printKeyValue("idleBlockedHeard", b.config.idleBlockedAfterHeardMs);
    printKeyValue("idleBlockedOwnEmit", b.config.idleBlockedAfterOwnEmitMs);
    printKeyValue("idleEnabled", b.config.idleEnabled ? 1 : 0);
    printKeyValue("lastDecision", b.lastDecisionName);
    printKeyValue("lastBlock", b.lastBlockReasonName);
    printKeyValue("lastPattern", b.lastVerdictTypeName);
    printKeyValue("lastHeardMs", b.lastHeardMs);
    printKeyValue("lastEmitMs", b.lastEmitMs);
    printKeyValue("waitUntilMs", b.waitUntilMs);
    printKeyValue("refractoryUntilMs", b.refractoryUntilMs);
    printKeyValue("ownEmitDetectionSuppressUntilMs", b.ownEmitSuppressUntilMs);
    printKeyValue("outputBusy", b.outputBusy ? 1 : 0);
    printKeyValue("patternsReceived", b.patternsReceived);
    printKeyValue("ignoredInvalid", b.patternsIgnoredInvalid);
    printKeyValue("blockedOutputBusy", b.blockedOutputBusy);
    printKeyValue("blockedWaiting", b.blockedWaiting);
    printKeyValue("blockedRefractory", b.blockedRefractory);
    printKeyValue("blockedSelfSuppressed", b.blockedSelfSuppressed);
    printKeyValue("wouldEmit", b.wouldEmitCount);
    printKeyValue("emitted", b.emittedCount);
    serialOut().println();
}

void Node::printRbSignalSummary() const {
    serialOut().print("RB occurrence baseline=");
    serialOut().print(_audioSignal.baseline(), 1);
    printKeyValue("mag", _audioSignal.signalMagnitude());
    printKeyValue("smooth", _audioSignal.smoothedSignalMagnitude());
    serialOut().println();
}
