#include "FrequencyMatchDetector.h"

#include <string.h>

#include "../../TimingUtils.h"

namespace {

const char* frequencyRejectReasonFromState(const FrequencyMatchDetector& detector) {
    if (detector.candidateEmitted) {
        return "none";
    }
    if (detector.candidateClosed) {
        return detector.noEmitReason[0] != '\0' ? detector.noEmitReason : "unknown";
    }
    return detector.gateReason[0] != '\0' ? detector.gateReason : "unknown";
}

detection::DetectorRejectClass frequencyRejectClassFromReason(const char* reason) {
    if (reason == nullptr || strcmp(reason, "none") == 0) {
        return detection::DetectorRejectClass::None;
    }
    if (strcmp(reason, "duration_too_short") == 0 || strcmp(reason, "duration_too_long") == 0) {
        return detection::DetectorRejectClass::Timing;
    }
    if (strcmp(reason, "refractory") == 0) {
        return detection::DetectorRejectClass::Cooldown;
    }
    if (strstr(reason, "score") != nullptr || strstr(reason, "contrast") != nullptr || strstr(reason, "frequency") != nullptr) {
        return detection::DetectorRejectClass::Threshold;
    }

    return detection::DetectorRejectClass::Unknown;
}

} // namespace

void FrequencyMatchDetector::resetState() {
    evidencePresent = false;
    liveFrequencyOnly = false;
    firstThresholdCrossingSeen = false;
    wouldProduceCandidate = false;
    candidateActive = false;
    candidateEmitted = false;
    candidateClosed = false;
    candidateRefractoryUntilMs = 0;
    firstThresholdCrossingMs = 0;
    firstThresholdCrossingSample = 0;
    candidateOpenMs = 0;
    candidateOpenSample = 0;
    candidatePeakMs = 0;
    candidatePeakSample = 0;
    candidateCloseMs = 0;
    candidateCloseSample = 0;
    candidateHoldUpdates = 0;
    candidateDurationMs = 0;
    candidateLastMatchedMs = 0;
    attackScoreThreshold = 0.0f;
    releaseScoreThreshold = 0.0f;
    attackContrastThreshold = 0.0f;
    releaseContrastThreshold = 0.0f;
    evidenceOk = false;
    attackScoreOk = false;
    attackContrastOk = false;
    attackOk = false;
    releaseScoreOk = false;
    releaseContrastOk = false;
    releaseOk = false;
    emitAllowed = false;
    validRelease = false;
    candidatePeakScore = 0.0f;
    candidatePeakContrast = 0.0f;
    candidatePeakSampleCount = 0;
    candidateLifecycleId = 0;
    currentCandidateId = 0;
    acceptedCandidateId = 0;
    selectedRejectCandidateId = 0;
    lastCandidateId = 0;
    candidateMinDurationMs = 0;
    candidateMaxDurationMs = 0;
    acceptedCount = 0;
    rejectedCount = 0;
    bestDurationMs = 0;
    bestOpenMs = 0;
    bestPeakMs = 0;
    bestLastMatchMs = 0;
    bestCloseMs = 0;
    bestPeakScore = 0.0f;
    bestPeakContrast = 0.0f;
    bestRejectReason = "none";
    bestGateReason = "none";
    memset(&bestEvidence, 0, sizeof(bestEvidence));
    memset(&candidateEvidence, 0, sizeof(candidateEvidence));
    memset(candidateState, 0, sizeof(candidateState));
    strncpy(candidateState, "none", sizeof(candidateState) - 1);
    candidateState[sizeof(candidateState) - 1] = '\0';
    memset(gateReason, 0, sizeof(gateReason));
    strncpy(gateReason, "none", sizeof(gateReason) - 1);
    gateReason[sizeof(gateReason) - 1] = '\0';
    memset(wouldCandidateReason, 0, sizeof(wouldCandidateReason));
    strncpy(wouldCandidateReason, "none", sizeof(wouldCandidateReason) - 1);
    wouldCandidateReason[sizeof(wouldCandidateReason) - 1] = '\0';
    memset(noEmitReason, 0, sizeof(noEmitReason));
    strncpy(noEmitReason, "none", sizeof(noEmitReason) - 1);
    noEmitReason[sizeof(noEmitReason) - 1] = '\0';
    memset(&frequencyCandidate, 0, sizeof(frequencyCandidate));
    _acceptedOccurrence = {};
    _acceptedDetail = {};
    _pendingOccurrencePresent = false;
    _pendingOccurrence = {};
    _lastEmittedOccurrenceCloseMs = 0;
    resetDiagnosticsSummary();
}

void FrequencyMatchDetector::resetRejectSummary() {
    acceptedCount = 0;
    rejectedCount = 0;
    bestDurationMs = 0;
    bestOpenMs = 0;
    bestPeakMs = 0;
    bestLastMatchMs = 0;
    bestCloseMs = 0;
    bestPeakScore = 0.0f;
    bestPeakContrast = 0.0f;
    bestRejectReason = "none";
    bestGateReason = "none";
    memset(&bestEvidence, 0, sizeof(bestEvidence));
    memset(&candidateEvidence, 0, sizeof(candidateEvidence));
    candidateLifecycleId = 0;
    currentCandidateId = 0;
    acceptedCandidateId = 0;
    selectedRejectCandidateId = 0;
    lastCandidateId = 0;
    candidateDurationInconsistent = false;
    _acceptedOccurrence = {};
    _acceptedDetail = {};
    _pendingOccurrencePresent = false;
    _pendingOccurrence = {};
    _lastEmittedOccurrenceCloseMs = 0;
}

void FrequencyMatchDetector::setDiagnosticsEnabled(bool enabled) {
    _diagnosticsEnabled = enabled;
    if (!enabled) {
        resetDiagnosticsSummary();
    }
}

void FrequencyMatchDetector::resetDiagnosticsSummary() {
    diagnosticsScoreOkCount = 0;
    diagnosticsContrastOkCount = 0;
    diagnosticsBothOkCount = 0;
    diagnosticsMatchedCount = 0;
}

void FrequencyMatchDetector::updateBestRejectedCandidate() {
    if (!candidateClosed || candidateEmitted) {
        return;
    }

    if (candidateDurationMs >= bestDurationMs) {
        bestDurationMs = candidateDurationMs;
        bestOpenMs = candidateOpenMs;
        bestPeakMs = candidatePeakMs;
        bestLastMatchMs = candidateLastMatchedMs;
        bestCloseMs = candidateCloseMs;
        bestPeakScore = candidatePeakScore;
        bestPeakContrast = candidatePeakContrast;
        bestRejectReason = noEmitReason[0] != '\0' ? noEmitReason : "unknown";
        bestGateReason = gateReason[0] != '\0' ? gateReason : "unknown";
    }
}

void FrequencyMatchDetector::recordRejectedCandidate() {
    ++rejectedCount;
    updateBestRejectedCandidate();
}

void FrequencyMatchDetector::capturePendingOccurrence(const AudioSamplePacket& audioSamplePacket) {
    _pendingOccurrence = frequencyCandidate;
    _pendingOccurrence.detectorId = detection::DetectorId::FrequencyMatch;
    _pendingOccurrence.occurrenceType = detection::OccurrenceType::Frequency;
    _pendingOccurrence.present = true;
    _pendingOccurrence.confidence = _pendingOccurrence.valid ? 1.0f : 0.0f;
    _pendingOccurrence.frequency.present = true;
    _pendingOccurrence.frequency.measurement = candidateEvidence;
    _pendingOccurrence.frequency.measurement.present = true;
    _pendingOccurrence.frequency.measurement.matched = frequencyCandidate.valid;
    _pendingOccurrence.frequency.measurement.observedAtMs = audioSamplePacket.timeMs;
    _pendingOccurrence.frequency.measurement.targetHz = candidateEvidence.targetHz;
    _pendingOccurrence.scalar.value = audioSamplePacket.audioMagnitudeValue;
    _pendingOccurrence.scalar.baseline = audioSamplePacket.baseline;
    _pendingOccurrence.scalar.lift = _pendingOccurrence.scalar.value - _pendingOccurrence.scalar.baseline;
    _pendingOccurrencePresent = _pendingOccurrence.valid;
    if (_pendingOccurrencePresent) {
        _acceptedOccurrence.present = true;
        _acceptedOccurrence.startMs = _pendingOccurrence.startMs;
        _acceptedOccurrence.peakMs = _pendingOccurrence.peakMs;
        _acceptedOccurrence.endMs = _pendingOccurrence.endMs;
        _acceptedOccurrence.durationMs = _pendingOccurrence.durationMs;
        _acceptedOccurrence.strength = _pendingOccurrence.strength;
        _acceptedOccurrence.confidence = _pendingOccurrence.confidence;
        _acceptedDetail.score = _pendingOccurrence.frequency.score;
        _acceptedDetail.contrast = _pendingOccurrence.frequency.contrast;
    }
}

void FrequencyMatchDetector::update(const detection::FrequencyBandMeasurementPacket& evidence,
                                    const AudioSamplePacket& audioSamplePacket,
                                    unsigned long now,
                                    uint64_t currentSample,
                                    const FrequencyMatchEvaluation::Values& tuning,
                                    unsigned long releaseDebounceMs,
                                    unsigned long cooldownAfterReleaseMs,
                                    unsigned long minDurationMs) {
    const auto gates = FrequencyMatchEvaluation::evaluate(evidence, tuning);

    evidencePresent = evidence.present;
    evidenceOk = gates.evidenceOk;

    attackScoreThreshold = tuning.attackScoreMin;
    releaseScoreThreshold = tuning.releaseScoreMin;
    attackContrastThreshold = tuning.attackContrastMin;
    releaseContrastThreshold = tuning.releaseContrastMin;

    attackScoreOk = gates.attackScoreOk;
    attackContrastOk = gates.attackContrastOk;
    attackOk = gates.attackOk;
    releaseScoreOk = gates.releaseScoreOk;
    releaseContrastOk = gates.releaseContrastOk;
    releaseOk = gates.releaseOk;

    emitAllowed = false;
    validRelease = false;
    gateReason[0] = '\0';
    wouldCandidateReason[0] = '\0';

    candidateMinDurationMs = minDurationMs;
    candidateMaxDurationMs = 0;

    frequencyCandidate.detectorId = detection::DetectorId::FrequencyMatch;
    frequencyCandidate.occurrenceType = detection::OccurrenceType::Frequency;
    frequencyCandidate.present = evidence.present;
    frequencyCandidate.valid = false;

    const auto closeCandidate = [&](unsigned long minDurationMs) {
        candidateActive = false;
        candidateClosed = true;
        candidateCloseMs = now;
        candidateCloseSample = currentSample;
        candidateDurationMs = candidateCloseMs >= candidateOpenMs
            ? candidateCloseMs - candidateOpenMs
            : 0UL;
        const bool durationOk = candidateDurationMs >= minDurationMs;
        const bool accepted = durationOk;
        candidateState[0] = '\0';
        candidateEmitted = accepted;
        validRelease = accepted;
        emitAllowed = accepted;
        candidateRefractoryUntilMs = now + cooldownAfterReleaseMs;
        strncpy(candidateState, accepted ? "closed" : "rejected", sizeof(candidateState) - 1);
        candidateState[sizeof(candidateState) - 1] = '\0';
        strncpy(noEmitReason, accepted ? "none" : "duration_too_short", sizeof(noEmitReason) - 1);
        noEmitReason[sizeof(noEmitReason) - 1] = '\0';
        lastCandidateId = currentCandidateId;
        if (accepted) {
            ++acceptedCount;
            acceptedCandidateId = currentCandidateId;
        } else {
            selectedRejectCandidateId = currentCandidateId;
        }
        currentCandidateId = 0;
        candidateDurationInconsistent = accepted != durationOk;
        frequencyCandidate.valid = accepted;
        frequencyCandidate.releaseMs = candidateCloseMs;
        frequencyCandidate.releaseSample = candidateCloseSample;
        frequencyCandidate.endMs = candidateCloseMs;
        frequencyCandidate.durationMs = candidateDurationMs;
        frequencyCandidate.confidence = accepted ? 1.0f : 0.0f;
        if (!accepted) {
            recordRejectedCandidate();
        }
    };

    if (evidence.present) {
        if (attackOk) {
            if (!firstThresholdCrossingSeen) {
                firstThresholdCrossingSeen = true;
                firstThresholdCrossingMs = now;
                firstThresholdCrossingSample = currentSample;
            }
        }

        if (!candidateActive) {
            if (attackOk) {
                if (timing::beforeDeadline(now, candidateRefractoryUntilMs)) {
                    strncpy(gateReason, "refractory", sizeof(gateReason) - 1);
                    gateReason[sizeof(gateReason) - 1] = '\0';
                    wouldProduceCandidate = false;
                    strncpy(wouldCandidateReason, "refractory", sizeof(wouldCandidateReason) - 1);
                    wouldCandidateReason[sizeof(wouldCandidateReason) - 1] = '\0';
                } else {
                    wouldProduceCandidate = true;
                    candidateActive = true;
                    candidateClosed = false;
                    candidateEmitted = false;
                    currentCandidateId = ++candidateLifecycleId;
                    lastCandidateId = currentCandidateId;
                    candidateOpenMs = now;
                    candidateOpenSample = currentSample;
                    candidatePeakMs = now;
                    candidatePeakSample = currentSample;
                    candidatePeakScore = evidence.targetBandScoreValue;
                    candidatePeakContrast = evidence.targetBandContrastValue;
                    candidatePeakSampleCount = 0;
                    candidateHoldUpdates = 1;
                    candidateDurationMs = 0;
                    candidateLastMatchedMs = now;
                    candidateEvidence = evidence;
                    frequencyCandidate.startMs = now;
                    frequencyCandidate.startSample = currentSample;
                    frequencyCandidate.peakMs = now;
                    frequencyCandidate.peakSample = currentSample;
                    frequencyCandidate.releaseMs = 0;
                    frequencyCandidate.releaseSample = 0;
                    frequencyCandidate.endMs = 0;
                    frequencyCandidate.durationMs = 0;
                    frequencyCandidate.strength = evidence.targetBandScoreValue;
                    frequencyCandidate.frequency.present = true;
                    frequencyCandidate.frequency.score = evidence.targetBandScoreValue;
                    frequencyCandidate.frequency.contrast = evidence.targetBandContrastValue;
                    frequencyCandidate.confidence = 0.0f;
                    strncpy(candidateState, "open", sizeof(candidateState) - 1);
                    candidateState[sizeof(candidateState) - 1] = '\0';
                }
            } else {
                wouldProduceCandidate = false;
                strncpy(wouldCandidateReason, FrequencyMatchEvaluation::reasonName(gates.attackReason), sizeof(wouldCandidateReason) - 1);
                wouldCandidateReason[sizeof(wouldCandidateReason) - 1] = '\0';
            }
        } else {
            if (releaseOk) {
                candidateLastMatchedMs = now;
                ++candidateHoldUpdates;
                candidateDurationMs = candidateLastMatchedMs >= candidateOpenMs
                    ? candidateLastMatchedMs - candidateOpenMs
                    : 0UL;
                if (evidence.targetBandScoreValue > candidatePeakScore
                    || (evidence.targetBandScoreValue == candidatePeakScore && evidence.targetBandContrastValue > candidatePeakContrast)) {
                    candidatePeakMs = now;
                    candidatePeakSample = currentSample;
                    candidatePeakScore = evidence.targetBandScoreValue;
                    candidatePeakContrast = evidence.targetBandContrastValue;
                    candidatePeakSampleCount = 0;
                    candidateEvidence = evidence;
                    frequencyCandidate.peakMs = now;
                    frequencyCandidate.peakSample = currentSample;
                    frequencyCandidate.strength = evidence.targetBandScoreValue;
                    frequencyCandidate.frequency.score = evidence.targetBandScoreValue;
                    frequencyCandidate.frequency.contrast = evidence.targetBandContrastValue;
                }
                frequencyCandidate.durationMs = candidateDurationMs;
                frequencyCandidate.valid = false;
            } else {
                if (candidateLastMatchedMs > 0 && timing::elapsedSince(now, candidateLastMatchedMs, releaseDebounceMs)) {
                    closeCandidate(minDurationMs);
                }
            }
        }
    } else {
        if (candidateActive && candidateLastMatchedMs > 0 && timing::elapsedSince(now, candidateLastMatchedMs, releaseDebounceMs)) {
            closeCandidate(minDurationMs);
        }
    }

    if (_diagnosticsEnabled) {
        const bool better = !bestEvidence.present
            || evidence.targetBandScoreValue > bestEvidence.targetBandScoreValue
            || (evidence.targetBandScoreValue == bestEvidence.targetBandScoreValue
                && evidence.targetBandContrastValue > bestEvidence.targetBandContrastValue);
        if (evidence.present && better) {
            bestEvidence = evidence;
        }

        const auto bestEval = FrequencyMatchEvaluation::evaluate(bestEvidence, tuning);
        attackScoreOk = bestEval.attackScoreOk;
        attackContrastOk = bestEval.attackContrastOk;
        attackOk = bestEval.attackOk;
        releaseScoreOk = bestEval.releaseScoreOk;
        releaseContrastOk = bestEval.releaseContrastOk;
        releaseOk = bestEval.releaseOk;
        evidenceOk = bestEvidence.present ? bestEvidence.present : evidence.present;

        const char* suppress = "none";
        if (!evidenceOk) {
            suppress = "live_window_not_ready";
        } else if (!bestEval.evidenceOk) {
            suppress = "no_frequency_evidence";
        } else if (!bestEval.attackScoreOk) {
            suppress = "freq_score_too_low";
        }
        strncpy(gateReason, suppress, sizeof(gateReason) - 1);
        gateReason[sizeof(gateReason) - 1] = '\0';

        const char* wouldCandidate = wouldProduceCandidate ? "matched" : suppress;
        strncpy(wouldCandidateReason, wouldCandidate, sizeof(wouldCandidateReason) - 1);
        wouldCandidateReason[sizeof(wouldCandidateReason) - 1] = '\0';

        if (evidence.present) {
            if (attackScoreOk) {
                ++diagnosticsScoreOkCount;
            }
            if (attackContrastOk) {
                ++diagnosticsContrastOkCount;
            }
            if (attackScoreOk && attackContrastOk) {
                ++diagnosticsBothOkCount;
            }
            if (attackOk) {
                ++diagnosticsMatchedCount;
            }
        }
    }

    if (candidateEmitted && candidateCloseMs != _lastEmittedOccurrenceCloseMs) {
        capturePendingOccurrence(audioSamplePacket);
        _lastEmittedOccurrenceCloseMs = candidateCloseMs;
    }
}

void FrequencyMatchDetector::buildReport(detection::DetectorReport& out, unsigned long nowMs) const {
    out = {};
    out.detectorId = detection::DetectorId::FrequencyMatch;
    out.accepted = _acceptedOccurrence;
    out.frequency.accepted = _acceptedDetail;
    out.thresholds.minDurationMs = candidateMinDurationMs;
    out.thresholds.maxDurationMs = candidateMaxDurationMs;
    out.aggregates.acceptedCount = acceptedCount;
    out.aggregates.rejectedCount = rejectedCount;

    const bool selectedRejectPresent =
        !out.accepted.present &&
        rejectedCount > 0 &&
        (bestOpenMs > 0 || bestPeakMs > 0 || bestCloseMs > 0 || bestDurationMs > 0 || bestPeakScore > 0.0f ||
         bestPeakContrast > 0.0f || (bestRejectReason != nullptr && strcmp(bestRejectReason, "none") != 0));
    if (selectedRejectPresent) {
        out.selectedReject.present = true;
        out.selectedReject.rejectClass = frequencyRejectClassFromReason(bestRejectReason);
        out.selectedReject.detectorReason = bestRejectReason;
        out.selectedReject.startMs = bestOpenMs;
        out.selectedReject.peakMs = bestPeakMs;
        out.selectedReject.endMs = bestCloseMs;
        out.selectedReject.durationMs = bestDurationMs;
        out.selectedReject.strength = bestPeakScore;
        out.selectedReject.confidence = 0.0f;
        out.frequency.selectedReject.score = bestPeakScore;
        out.frequency.selectedReject.contrast = bestPeakContrast;
    }

    out.frequency.thresholds.scoreThreshold = attackScoreThreshold;
    out.frequency.thresholds.contrastThreshold = attackContrastThreshold;
    out.frequency.aggregates.scoreOkCount = diagnosticsScoreOkCount;
    out.frequency.aggregates.contrastOkCount = diagnosticsContrastOkCount;
    out.frequency.aggregates.bothOkCount = diagnosticsBothOkCount;
    out.frequency.aggregates.matchCount = diagnosticsMatchedCount;
    out.frequency.inspect.rejectReason = frequencyRejectReasonFromState(*this);
    out.frequency.inspect.noEmitReason = noEmitReason;
    out.frequency.inspect.gateReason = gateReason;
    out.frequency.inspect.candidateState = candidateState;
    out.frequency.inspect.readyOk = evidenceOk;
    out.frequency.inspect.gateOpen = attackOk;
    out.frequency.inspect.opened = candidateActive || candidateClosed || candidateEmitted || candidateOpenMs > 0;
    out.frequency.inspect.released = candidateClosed || candidateCloseMs > 0;
    out.frequency.inspect.emitted = candidateEmitted;
    out.frequency.inspect.validRelease = validRelease;
    out.frequency.inspect.emitAllowed = emitAllowed;
    out.frequency.inspect.openMs = candidateOpenMs;
    out.frequency.inspect.peakMs = candidatePeakMs;
    out.frequency.inspect.releaseMs = candidateCloseMs;
    out.frequency.inspect.durationMs = candidateDurationMs;

    // Mirror the scalar report window precedence exactly:
    // accepted event first, then active/open lifecycle, then selected reject.
    if (out.accepted.present) {
        out.reportStartMs = out.accepted.startMs;
        out.reportEndMs = out.accepted.endMs;
    } else if (out.frequency.inspect.opened) {
        out.reportStartMs = out.frequency.inspect.openMs;
        out.reportEndMs = out.frequency.inspect.released ? out.frequency.inspect.releaseMs : nowMs;
    } else if (out.selectedReject.present) {
        out.reportStartMs = out.selectedReject.startMs;
        out.reportEndMs = out.selectedReject.endMs;
    }
}

bool FrequencyMatchDetector::popOccurrence(detection::Occurrence& out) {
    if (!_pendingOccurrencePresent) {
        return false;
    }

    out = _pendingOccurrence;
    _pendingOccurrencePresent = false;
    _pendingOccurrence = {};
    return true;
}

