#include "FrequencyMatchPrinter.h"

#include <Arduino.h>
#include "../../../app/SerialOut.h"

// Frequency-match detector-specific rendering implementation.
namespace detection::frequency {

void printFrequencyMatchDetailLine(const char* prefix, const detection::DetectorReport* report) {
    if (report == nullptr || report->detectorId != detection::DetectorId::FrequencyMatch) {
        return;
    }

    const auto& frequency = report->frequency;
    serialOut().print(prefix);
    serialOut().print(" detail.frequency.accepted.score=");
    serialOut().print(frequency.accepted.score, 2);
    serialOut().print(" detail.frequency.accepted.contrast=");
    serialOut().print(frequency.accepted.contrast, 2);
    serialOut().print(" detail.frequency.reject.score=");
    serialOut().print(frequency.selectedReject.score, 2);
    serialOut().print(" detail.frequency.reject.contrast=");
    serialOut().print(frequency.selectedReject.contrast, 2);
    serialOut().print(" detail.frequency.threshold.score_min=");
    serialOut().print(frequency.thresholds.scoreThreshold, 2);
    serialOut().print(" detail.frequency.threshold.contrast_min=");
    serialOut().print(frequency.thresholds.contrastThreshold, 2);
    serialOut().print(" detail.frequency.aggregate.score_ok=");
    serialOut().print(frequency.aggregates.scoreOkCount);
    serialOut().print(" detail.frequency.aggregate.contrast_ok=");
    serialOut().print(frequency.aggregates.contrastOkCount);
    serialOut().print(" detail.frequency.aggregate.both_ok=");
    serialOut().print(frequency.aggregates.bothOkCount);
    serialOut().print(" detail.frequency.aggregate.match=");
    serialOut().print(frequency.aggregates.matchCount);
    serialOut().print(" detail.frequency.inspect.reject_reason=");
    serialOut().print(frequency.inspect.rejectReason != nullptr ? frequency.inspect.rejectReason : "none");
    serialOut().print(" detail.frequency.inspect.no_emit_reason=");
    serialOut().print(frequency.inspect.noEmitReason != nullptr ? frequency.inspect.noEmitReason : "none");
    serialOut().print(" detail.frequency.inspect.gate_reason=");
    serialOut().print(frequency.inspect.gateReason != nullptr ? frequency.inspect.gateReason : "none");
    serialOut().print(" detail.frequency.inspect.pending_state=");
    serialOut().print(frequency.inspect.pendingState != nullptr ? frequency.inspect.pendingState : "none");
    serialOut().print(" detail.frequency.inspect.ready_ok=");
    serialOut().print(frequency.inspect.readyOk ? 1 : 0);
    serialOut().print(" detail.frequency.inspect.gate_open=");
    serialOut().print(frequency.inspect.gateOpen ? 1 : 0);
    serialOut().print(" detail.frequency.inspect.opened=");
    serialOut().print(frequency.inspect.opened ? 1 : 0);
    serialOut().print(" detail.frequency.inspect.released=");
    serialOut().print(frequency.inspect.released ? 1 : 0);
    serialOut().print(" detail.frequency.inspect.emitted=");
    serialOut().print(frequency.inspect.emitted ? 1 : 0);
    serialOut().print(" detail.frequency.inspect.valid_release=");
    serialOut().print(frequency.inspect.validRelease ? 1 : 0);
    serialOut().print(" detail.frequency.inspect.emit_allowed=");
    serialOut().print(frequency.inspect.emitAllowed ? 1 : 0);
    serialOut().print(" detail.frequency.inspect.open_ms=");
    serialOut().print(frequency.inspect.openMs);
    serialOut().print(" detail.frequency.inspect.peak_ms=");
    serialOut().print(frequency.inspect.peakMs);
    serialOut().print(" detail.frequency.inspect.release_ms=");
    serialOut().print(frequency.inspect.releaseMs);
    serialOut().print(" detail.frequency.inspect.duration_ms=");
    serialOut().println(frequency.inspect.durationMs);
}

} // namespace detection::frequency
