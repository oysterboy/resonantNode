#include "FrequencyMatchPrinter.h"

#include <Arduino.h>

// Frequency-match detector-specific rendering implementation.
namespace detection::frequency {

void printFrequencyMatchDetailLine(const char* prefix, const detection::DetectorReport* report) {
    if (report == nullptr || report->detectorId != detection::DetectorId::FrequencyMatch) {
        return;
    }

    const auto& frequency = report->frequency;
    Serial.print(prefix);
    Serial.print(" detail.frequency.accepted.score=");
    Serial.print(frequency.accepted.score, 2);
    Serial.print(" detail.frequency.accepted.contrast=");
    Serial.print(frequency.accepted.contrast, 2);
    Serial.print(" detail.frequency.reject.score=");
    Serial.print(frequency.selectedReject.score, 2);
    Serial.print(" detail.frequency.reject.contrast=");
    Serial.print(frequency.selectedReject.contrast, 2);
    Serial.print(" detail.frequency.threshold.score_min=");
    Serial.print(frequency.thresholds.scoreThreshold, 2);
    Serial.print(" detail.frequency.threshold.contrast_min=");
    Serial.print(frequency.thresholds.contrastThreshold, 2);
    Serial.print(" detail.frequency.aggregate.score_ok=");
    Serial.print(frequency.aggregates.scoreOkCount);
    Serial.print(" detail.frequency.aggregate.contrast_ok=");
    Serial.print(frequency.aggregates.contrastOkCount);
    Serial.print(" detail.frequency.aggregate.both_ok=");
    Serial.print(frequency.aggregates.bothOkCount);
    Serial.print(" detail.frequency.aggregate.match=");
    Serial.print(frequency.aggregates.matchCount);
    Serial.print(" detail.frequency.inspect.reject_reason=");
    Serial.print(frequency.inspect.rejectReason != nullptr ? frequency.inspect.rejectReason : "none");
    Serial.print(" detail.frequency.inspect.no_emit_reason=");
    Serial.print(frequency.inspect.noEmitReason != nullptr ? frequency.inspect.noEmitReason : "none");
    Serial.print(" detail.frequency.inspect.gate_reason=");
    Serial.print(frequency.inspect.gateReason != nullptr ? frequency.inspect.gateReason : "none");
    Serial.print(" detail.frequency.inspect.pending_state=");
    Serial.print(frequency.inspect.pendingState != nullptr ? frequency.inspect.pendingState : "none");
    Serial.print(" detail.frequency.inspect.ready_ok=");
    Serial.print(frequency.inspect.readyOk ? 1 : 0);
    Serial.print(" detail.frequency.inspect.gate_open=");
    Serial.print(frequency.inspect.gateOpen ? 1 : 0);
    Serial.print(" detail.frequency.inspect.opened=");
    Serial.print(frequency.inspect.opened ? 1 : 0);
    Serial.print(" detail.frequency.inspect.released=");
    Serial.print(frequency.inspect.released ? 1 : 0);
    Serial.print(" detail.frequency.inspect.emitted=");
    Serial.print(frequency.inspect.emitted ? 1 : 0);
    Serial.print(" detail.frequency.inspect.valid_release=");
    Serial.print(frequency.inspect.validRelease ? 1 : 0);
    Serial.print(" detail.frequency.inspect.emit_allowed=");
    Serial.print(frequency.inspect.emitAllowed ? 1 : 0);
    Serial.print(" detail.frequency.inspect.open_ms=");
    Serial.print(frequency.inspect.openMs);
    Serial.print(" detail.frequency.inspect.peak_ms=");
    Serial.print(frequency.inspect.peakMs);
    Serial.print(" detail.frequency.inspect.release_ms=");
    Serial.print(frequency.inspect.releaseMs);
    Serial.print(" detail.frequency.inspect.duration_ms=");
    Serial.println(frequency.inspect.durationMs);
}

} // namespace detection::frequency
