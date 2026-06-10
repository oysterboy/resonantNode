#include "AnalyzerApp.h"

#include <Arduino.h>
#include <math.h>
#include <string.h>

#include "../../TimingUtils.h"
#include "../../detection/DetectionDerivedValues.h"
#include "../../detection/DetectionNames.h"
#include "../../detection/patterns/PatternNames.h"
#include "AnalyzerHealthHelpers.h"

namespace {

struct AnalyzerFieldDescriptor {
    const char* namespaceName;
    const char* fieldName;
};

void printFieldLabel(const AnalyzerFieldDescriptor& descriptor) {
    if (descriptor.namespaceName != nullptr && descriptor.namespaceName[0] != '\0') {
        Serial.print(descriptor.namespaceName);
        Serial.print('.');
    }
    Serial.print(descriptor.fieldName != nullptr ? descriptor.fieldName : "unknown");
    Serial.print('=');
}

void printField(const AnalyzerFieldDescriptor& descriptor, const char* value) {
    printFieldLabel(descriptor);
    Serial.print(value != nullptr ? value : "none");
}

void printField(const AnalyzerFieldDescriptor& descriptor, bool value) {
    printFieldLabel(descriptor);
    Serial.print(value ? 1 : 0);
}

void printField(const AnalyzerFieldDescriptor& descriptor, unsigned long value) {
    printFieldLabel(descriptor);
    Serial.print(value);
}

void printField(const AnalyzerFieldDescriptor& descriptor, long value) {
    printFieldLabel(descriptor);
    Serial.print(value);
}

void printField(const AnalyzerFieldDescriptor& descriptor, float value, uint8_t precision = 1) {
    printFieldLabel(descriptor);
    Serial.print(value, precision);
}

constexpr AnalyzerFieldDescriptor kSourceKindField{nullptr, "source_kind"};
constexpr AnalyzerFieldDescriptor kStreamKindField{nullptr, "stream_kind"};
constexpr AnalyzerFieldDescriptor kOccurrenceStateField{nullptr, "occurrence_state"};
constexpr AnalyzerFieldDescriptor kEmittedField{nullptr, "emitted"};
constexpr AnalyzerFieldDescriptor kAcceptedField{nullptr, "accepted"};
constexpr AnalyzerFieldDescriptor kReasonField{nullptr, "analyzer_miss_reason"};
constexpr AnalyzerFieldDescriptor kSourceStateField{nullptr, "state"};
constexpr AnalyzerFieldDescriptor kSourceScopeField{nullptr, "scope"};
constexpr AnalyzerFieldDescriptor kSourceLastCandidatePresentField{nullptr, "last_candidate_present"};
constexpr AnalyzerFieldDescriptor kSourceLastCandidatePeakMsField{nullptr, "last_candidate_peak_ms"};
constexpr AnalyzerFieldDescriptor kSourceLastCandidateDurationMsField{nullptr, "last_candidate_duration_ms"};
constexpr AnalyzerFieldDescriptor kSourceLastCandidateSampleCountField{nullptr, "last_candidate_sample_count"};
constexpr AnalyzerFieldDescriptor kSourceLastCandidateReasonField{nullptr, "last_candidate_reason"};
constexpr AnalyzerFieldDescriptor kSourceLastCandidateGateReasonField{nullptr, "last_candidate_gate_reason"};
constexpr AnalyzerFieldDescriptor kSourceLastCandidateScopeField{nullptr, "last_candidate_scope"};
constexpr AnalyzerFieldDescriptor kSourceCandidateCountField{nullptr, "candidate_count"};
constexpr AnalyzerFieldDescriptor kSourceRejectCountField{nullptr, "reject_count"};
constexpr AnalyzerFieldDescriptor kSourceAcceptedCountField{nullptr, "accepted_count"};
constexpr AnalyzerFieldDescriptor kSourceBestDurationMsField{nullptr, "best_dur_ms"};
constexpr AnalyzerFieldDescriptor kSourceSecondBestDurationMsField{nullptr, "second_dur_ms"};
constexpr AnalyzerFieldDescriptor kSourceBestOpenMsField{nullptr, "best_open_ms"};
constexpr AnalyzerFieldDescriptor kSourceBestPeakMsField{nullptr, "best_peak_ms"};
constexpr AnalyzerFieldDescriptor kSourceBestLastMatchMsField{nullptr, "best_last_match_ms"};
constexpr AnalyzerFieldDescriptor kSourceBestCloseMsField{nullptr, "best_close_ms"};
constexpr AnalyzerFieldDescriptor kSourceBestPeakScoreField{nullptr, "best_peak_score"};
constexpr AnalyzerFieldDescriptor kSourceBestPeakContrastField{nullptr, "best_peak_contrast"};
constexpr AnalyzerFieldDescriptor kSourceBestRejectReasonField{nullptr, "best_reject_reason"};
constexpr AnalyzerFieldDescriptor kSourceBestGateReasonField{nullptr, "best_gate_reason"};
constexpr AnalyzerFieldDescriptor kSourceScoreTooLowFramesField{nullptr, "score_too_low_frames"};
constexpr AnalyzerFieldDescriptor kSourceContrastTooLowFramesField{nullptr, "contrast_too_low_frames"};
constexpr AnalyzerFieldDescriptor kSourceScoreAndContrastTooLowFramesField{nullptr, "score_and_contrast_too_low_frames"};
constexpr AnalyzerFieldDescriptor kSourceMaxPrimaryField{nullptr, "max_primary"};
constexpr AnalyzerFieldDescriptor kSourceMaxPrimaryMsField{nullptr, "max_primary_ms"};
constexpr AnalyzerFieldDescriptor kSourceMaxSecondaryField{nullptr, "max_secondary"};
constexpr AnalyzerFieldDescriptor kSourceMaxSecondaryMsField{nullptr, "max_secondary_ms"};
constexpr AnalyzerFieldDescriptor kSourceTotalMatchMsField{nullptr, "total_match_ms"};
constexpr AnalyzerFieldDescriptor kSourceTotalGapMsField{nullptr, "total_gap_ms"};
constexpr AnalyzerFieldDescriptor kSourceMaxGapMsField{nullptr, "max_gap_ms"};
constexpr AnalyzerFieldDescriptor kSourceIslandCountField{nullptr, "island_count"};
constexpr AnalyzerFieldDescriptor kRejectCountField{nullptr, "reject_count"};
constexpr AnalyzerFieldDescriptor kSupportTargetField{nullptr, "support_target"};
constexpr AnalyzerFieldDescriptor kSupportField{nullptr, "support"};
constexpr AnalyzerFieldDescriptor kEvidenceFreqScoreField{"evidence.freq", "score"};
constexpr AnalyzerFieldDescriptor kEvidenceFreqContrastField{"evidence.freq", "contrast"};
constexpr AnalyzerFieldDescriptor kEvidenceScalarModeField{"evidence.scalar", "mode"};
constexpr AnalyzerFieldDescriptor kEvidenceScalarClassificationField{"evidence.scalar", "classification"};
constexpr AnalyzerFieldDescriptor kEvidenceScalarPeakField{"evidence.scalar", "peak"};
constexpr AnalyzerFieldDescriptor kEvidenceScalarMeanField{"evidence.scalar", "mean"};
constexpr AnalyzerFieldDescriptor kEvidenceScalarLastField{"evidence.scalar", "last"};
constexpr AnalyzerFieldDescriptor kEvidenceScalarBaselineField{"evidence.scalar", "baseline"};
constexpr AnalyzerFieldDescriptor kEvidenceScalarLiftField{"evidence.scalar", "lift"};
constexpr AnalyzerFieldDescriptor kEvidenceScalarSustainedMsField{"evidence.scalar", "sustained_ms"};
constexpr AnalyzerFieldDescriptor kEvidenceScalarSustainedCountField{"evidence.scalar", "sustained_count"};
constexpr AnalyzerFieldDescriptor kEvidenceScalarSustainedThresholdField{"evidence.scalar", "sustained_threshold"};
constexpr AnalyzerFieldDescriptor kInspectorModeField{"inspector", "mode"};
constexpr AnalyzerFieldDescriptor kInspectorStreamField{"inspector", "stream"};
constexpr AnalyzerFieldDescriptor kInspectorClassificationField{"inspector", "classification"};
constexpr AnalyzerFieldDescriptor kInspectorPeakField{"inspector", "peak"};
constexpr AnalyzerFieldDescriptor kInspectorMeanField{"inspector", "mean"};
constexpr AnalyzerFieldDescriptor kInspectorLastField{"inspector", "last"};
constexpr AnalyzerFieldDescriptor kInspectorCountField{"inspector", "count"};
constexpr AnalyzerFieldDescriptor kInspectorCenteredField{"inspector", "centered"};
constexpr AnalyzerFieldDescriptor kInspectorBaselineField{"inspector", "baseline"};
constexpr AnalyzerFieldDescriptor kInspectorLiftField{"inspector", "lift"};
constexpr AnalyzerFieldDescriptor kInspectorSustainedField{"inspector", "sustained"};
constexpr AnalyzerFieldDescriptor kInspectorSustainedMsField{"inspector", "sustained_ms"};
constexpr AnalyzerFieldDescriptor kInspectorSustainedCountField{"inspector", "sustained_count"};
constexpr AnalyzerFieldDescriptor kInspectorSustainedThresholdField{"inspector", "sustained_threshold"};
constexpr AnalyzerFieldDescriptor kInspectorStrengthField{"inspector", "strength"};
constexpr AnalyzerFieldDescriptor kInspectorSupportBasisField{"inspector", "support_basis"};
constexpr AnalyzerFieldDescriptor kInspectorNoteField{"inspector", "note"};
constexpr AnalyzerFieldDescriptor kInspectorAnchorField{"inspector", "anchor"};
constexpr AnalyzerFieldDescriptor kSourceFreqPeakScoreField{"source.freq", "peak_score"};
constexpr AnalyzerFieldDescriptor kSourceFreqPeakContrastField{"source.freq", "peak_contrast"};
constexpr AnalyzerFieldDescriptor kSourceScalarPeakStrengthField{"source.scalar", "peak_strength"};
constexpr AnalyzerFieldDescriptor kSourceFreqLiveReasonField{"source.freq", "live_reason"};
constexpr AnalyzerFieldDescriptor kSourceFreqLiveWouldField{"source.freq", "live_would"};
constexpr AnalyzerFieldDescriptor kSourceFreqLiveReadyField{"source.freq", "live_ready"};
constexpr AnalyzerFieldDescriptor kSourceFreqLiveGateField{"source.freq", "live_gate"};
constexpr AnalyzerFieldDescriptor kSourceFreqLivePresentField{"source.freq", "live_present"};
constexpr AnalyzerFieldDescriptor kSourceFreqLiveValidField{"source.freq", "live_valid"};
constexpr AnalyzerFieldDescriptor kSourceFreqLiveMatchField{"source.freq", "live_match"};
constexpr AnalyzerFieldDescriptor kSourceFreqLiveStateField{"source.freq", "live_state"};
constexpr AnalyzerFieldDescriptor kSourceWindowStartField{"source.freq", "window_start_ms"};
constexpr AnalyzerFieldDescriptor kSourceWindowEndField{"source.freq", "window_end_ms"};
constexpr AnalyzerFieldDescriptor kSourceDiagFirstFrameField{"source.freq", "diag_first_frame_ms"};
constexpr AnalyzerFieldDescriptor kSourceDiagLastFrameField{"source.freq", "diag_last_frame_ms"};
constexpr AnalyzerFieldDescriptor kSourceExpectedWindowField{"source.freq", "expected_window_ms"};
constexpr AnalyzerFieldDescriptor kSourceExpectedFrameCountField{"source.freq", "expected_frame_count_estimate"};
constexpr AnalyzerFieldDescriptor kSourceDiagFrameCountOkField{"source.freq", "diag_frame_count_ok"};
constexpr AnalyzerFieldDescriptor kSourceFramesField{"source.freq", "frames"};
constexpr AnalyzerFieldDescriptor kSourceValidFramesField{"source.freq", "valid_frames"};
constexpr AnalyzerFieldDescriptor kSourceFreshUpdatesField{"source.freq", "fresh_updates"};
constexpr AnalyzerFieldDescriptor kSourceScoreOkFramesField{"source.freq", "score_ok_frames"};
constexpr AnalyzerFieldDescriptor kSourceContrastOkFramesField{"source.freq", "contrast_ok_frames"};
constexpr AnalyzerFieldDescriptor kSourceBothOkFramesField{"source.freq", "both_ok_frames"};
constexpr AnalyzerFieldDescriptor kSourceMatchFramesField{"source.freq", "match_frames"};
constexpr AnalyzerFieldDescriptor kSourceRejectFramesField{"source.freq", "reject_frames"};
constexpr AnalyzerFieldDescriptor kSourceReleaseScoreOkFramesField{"source.freq", "release_score_ok_frames"};
constexpr AnalyzerFieldDescriptor kSourceReleaseContrastOkFramesField{"source.freq", "release_contrast_ok_frames"};
constexpr AnalyzerFieldDescriptor kSourceReleaseBothOkFramesField{"source.freq", "release_both_ok_frames"};
constexpr AnalyzerFieldDescriptor kSourceReleaseScoreTooLowFramesField{"source.freq", "release_score_too_low_frames"};
constexpr AnalyzerFieldDescriptor kSourceReleaseContrastTooLowFramesField{"source.freq", "release_contrast_too_low_frames"};
constexpr AnalyzerFieldDescriptor kSourceReleaseScoreAndContrastTooLowFramesField{"source.freq", "release_score_and_contrast_too_low_frames"};
constexpr AnalyzerFieldDescriptor kSourceReleaseNoEvidenceFramesField{"source.freq", "release_no_evidence_frames"};
constexpr AnalyzerFieldDescriptor kSourceDiagLongestMatchStreakFramesField{"source.freq", "diag_longest_match_streak_frames"};
constexpr AnalyzerFieldDescriptor kSourceDiagLongestMatchStreakMsField{"source.freq", "diag_longest_match_streak_ms"};
constexpr AnalyzerFieldDescriptor kSourceCloseCauseField{"source.freq", "close_cause"};
constexpr AnalyzerFieldDescriptor kSourceSumScoreField{"source.freq", "sum_score"};
constexpr AnalyzerFieldDescriptor kSourceSumContrastField{"source.freq", "sum_contrast"};
constexpr AnalyzerFieldDescriptor kSourceMeanScoreField{"source.freq", "mean_score"};
constexpr AnalyzerFieldDescriptor kSourceMeanContrastField{"source.freq", "mean_contrast"};
constexpr AnalyzerFieldDescriptor kSourceScoreThresholdField{"source.freq", "score_threshold"};
constexpr AnalyzerFieldDescriptor kSourceContrastThresholdField{"source.freq", "contrast_threshold"};
constexpr AnalyzerFieldDescriptor kSourceMaxScoreField{"source.freq", "max_score"};
constexpr AnalyzerFieldDescriptor kSourceMaxScoreMsField{"source.freq", "max_score_ms"};
constexpr AnalyzerFieldDescriptor kSourceMaxContrastField{"source.freq", "max_contrast"};
constexpr AnalyzerFieldDescriptor kSourceMaxContrastMsField{"source.freq", "max_contrast_ms"};
constexpr AnalyzerFieldDescriptor kSourceAcceptedDtMsField{"source.freq", "accepted_dt_ms"};
constexpr AnalyzerFieldDescriptor kSourceAcceptedDurationMsField{"source.freq", "accepted_duration_ms"};
constexpr AnalyzerFieldDescriptor kSourceAcceptedStrengthField{"source.freq", "accepted_strength"};
constexpr AnalyzerFieldDescriptor kSourceAcceptedScoreField{"source.freq", "accepted_score"};
constexpr AnalyzerFieldDescriptor kSourceAcceptedContrastField{"source.freq", "accepted_contrast"};
constexpr AnalyzerFieldDescriptor kSourceCandidateActiveAtTrialStartField{"source.freq", "candidate_active_at_trial_start"};
constexpr AnalyzerFieldDescriptor kSourceCandidateFirstMsField{"source.freq", "candidate_first_ms"};
constexpr AnalyzerFieldDescriptor kSourceCandidateLastMatchMsField{"source.freq", "candidate_last_match_ms"};
constexpr AnalyzerFieldDescriptor kSourceCandidateHoldMsField{"source.freq", "candidate_hold_ms"};
constexpr AnalyzerFieldDescriptor kSourceRefractoryRemainingMsField{"source.freq", "refractory_remaining_ms"};
constexpr AnalyzerFieldDescriptor kSourceOpenedThisTrialField{"source.freq", "opened_this_trial"};
constexpr AnalyzerFieldDescriptor kSourceClosedThisTrialField{"source.freq", "closed_this_trial"};
constexpr AnalyzerFieldDescriptor kSourceEmittedThisTrialField{"source.freq", "emitted_this_trial"};
constexpr AnalyzerFieldDescriptor kSourceRejectedThisTrialField{"source.freq", "rejected_this_trial"};
constexpr AnalyzerFieldDescriptor kSourceFreshReleaseOkUpdatesField{"source.freq", "fresh_release_ok_updates"};
constexpr AnalyzerFieldDescriptor kSourceHeldReleaseOkUpdatesField{"source.freq", "held_release_ok_updates"};
constexpr AnalyzerFieldDescriptor kSourceSelectedAcceptPresentField{"source.freq", "selected_accept_present"};
constexpr AnalyzerFieldDescriptor kSourceSelectedAcceptDurationMsField{"source.freq", "selected_accept_duration_ms"};
constexpr AnalyzerFieldDescriptor kSourceSelectedRejectPeakMsField{"source.freq", "selected_reject_peak_ms"};
constexpr AnalyzerFieldDescriptor kSourceSelectedRejectDurationMsField{"source.freq", "selected_reject_duration_ms"};
constexpr AnalyzerFieldDescriptor kSourceSelectedRejectSampleCountField{"source.freq", "selected_reject_sample_count"};
constexpr AnalyzerFieldDescriptor kSourceAcceptedCandidateIdField{"source.freq", "accepted_candidate_id"};
constexpr AnalyzerFieldDescriptor kSourceSelectedRejectCandidateIdField{"source.freq", "selected_reject_candidate_id"};
constexpr AnalyzerFieldDescriptor kSourceLastCandidateIdField{"source.freq", "last_candidate_id"};
constexpr AnalyzerFieldDescriptor kSourceLifecycleCandidateIdField{"source.freq", "lifecycle_candidate_id"};
constexpr AnalyzerFieldDescriptor kSourceDurationUsedForDecisionMsField{"source.freq", "duration_used_for_decision_ms"};
constexpr AnalyzerFieldDescriptor kSourceDurationPrintedMsField{"source.freq", "duration_printed_ms"};
constexpr AnalyzerFieldDescriptor kSourceMinDurationUsedMsField{"source.freq", "min_duration_used_ms"};
constexpr AnalyzerFieldDescriptor kSourceMinDurationReportedMsField{"source.freq", "min_duration_reported_ms"};
constexpr AnalyzerFieldDescriptor kSourceDurationOkField{"source.freq", "duration_ok"};
constexpr AnalyzerFieldDescriptor kSourceAnalyzerMissReasonField{"source.freq", "analyzer_miss_reason"};
constexpr AnalyzerFieldDescriptor kSourceSelectedRejectReasonField{"source.freq", "selected_reject_reason"};
constexpr AnalyzerFieldDescriptor kSourceSelectedRejectGateReasonField{"source.freq", "selected_reject_gate_reason"};
constexpr AnalyzerFieldDescriptor kSourceSourceLastRejectReasonField{"source.freq", "source_last_reject_reason"};
constexpr AnalyzerFieldDescriptor kSourceFmOpenMsField{"source.freq", "fm_open_ms"};
constexpr AnalyzerFieldDescriptor kSourceFmPeakMsField{"source.freq", "fm_peak_ms"};
constexpr AnalyzerFieldDescriptor kSourceFmReleaseMsField{"source.freq", "fm_release_ms"};
constexpr AnalyzerFieldDescriptor kSourceFmDurationMsField{"source.freq", "fm_duration_ms"};
constexpr AnalyzerFieldDescriptor kSourceFmMinDurationMsField{"source.freq", "fm_min_duration_ms"};
constexpr AnalyzerFieldDescriptor kSourceFmMaxDurationMsField{"source.freq", "fm_max_duration_ms"};
constexpr AnalyzerFieldDescriptor kSourceFmDurationOkField{"source.freq", "fm_duration_ok"};
constexpr AnalyzerFieldDescriptor kSourceFmOpenedField{"source.freq", "fm_opened"};
constexpr AnalyzerFieldDescriptor kSourceFmReleasedField{"source.freq", "fm_released"};
constexpr AnalyzerFieldDescriptor kSourceFmEmittedField{"source.freq", "fm_emitted"};
constexpr AnalyzerFieldDescriptor kSourceFmValidReleaseField{"source.freq", "fm_valid_release"};
constexpr AnalyzerFieldDescriptor kSourceFmEmitAllowedField{"source.freq", "fm_emit_allowed"};
constexpr AnalyzerFieldDescriptor kSourceFmCloseCauseField{"source.freq", "fm_close_cause"};
constexpr AnalyzerFieldDescriptor kSourceDiagDurationInconsistentField{"source.freq", "diag_duration_inconsistent"};
constexpr AnalyzerFieldDescriptor kSourceFreqEvidenceClassField{"source.freq", "freq_evidence_class"};
constexpr AnalyzerFieldDescriptor kSourceFreqHistoryScoreRecordsField{"source.freq", "history_score_records"};
constexpr AnalyzerFieldDescriptor kSourceFreqHistoryContrastRecordsField{"source.freq", "history_contrast_records"};
constexpr AnalyzerFieldDescriptor kSourceTraceSourceOccurrenceEmittedField{"source.freq", "trace_source_occurrence_emitted"};
constexpr AnalyzerFieldDescriptor kSourceTraceRuntimeEvidenceSeenField{"source.freq", "trace_runtime_evidence_seen"};
constexpr AnalyzerFieldDescriptor kSourceTraceRuntimeOccurrenceReceivedField{"source.freq", "trace_runtime_occurrence_received"};
constexpr AnalyzerFieldDescriptor kSourceTraceAnalyzerSeenField{"source.freq", "trace_analyzer_seen"};
constexpr AnalyzerFieldDescriptor kSourceDetectionGateBlockedField{"source.freq", "detection_gate_blocked"};
constexpr AnalyzerFieldDescriptor kSourceDetectionGateReasonField{"source.freq", "detection_gate_reason"};
constexpr AnalyzerFieldDescriptor kSourceNearMissField{"source.freq", "near_miss"};
constexpr AnalyzerFieldDescriptor kSourceNearMissReasonField{"source.freq", "near_miss_reason"};
constexpr AnalyzerFieldDescriptor kSourceDiagInconsistentField{"source.freq", "diag_inconsistent"};
constexpr AnalyzerFieldDescriptor kSourceCurrentTrialIdField{"source.freq", "current_trial_id"};
constexpr AnalyzerFieldDescriptor kSourceAcceptedTrialIdField{"source.freq", "accepted_trial_id"};
constexpr AnalyzerFieldDescriptor kSourceAcceptedSourceField{"source.freq", "accepted_source"};
constexpr AnalyzerFieldDescriptor kSourceAnalyzerResultField{"source.freq", "analyzer_result"};
constexpr AnalyzerFieldDescriptor kSourceAnalyzerReasonField{"source.freq", "analyzer_reason"};
constexpr AnalyzerFieldDescriptor kSourceLiveFreqReasonField{"source.freq", "live_freq_reason"};
constexpr AnalyzerFieldDescriptor kSourceLiveFreqWouldField{"source.freq", "live_freq_would"};
constexpr AnalyzerFieldDescriptor kSourceLiveFreqReadyField{"source.freq", "live_freq_ready"};
constexpr AnalyzerFieldDescriptor kSourceLiveFreqGateField{"source.freq", "live_freq_gate"};
constexpr AnalyzerFieldDescriptor kSourceLiveFreqPresentField{"source.freq", "live_freq_present"};
constexpr AnalyzerFieldDescriptor kSourceLiveFreqValidField{"source.freq", "live_freq_valid"};
constexpr AnalyzerFieldDescriptor kSourceLiveFreqMatchField{"source.freq", "live_freq_match"};
constexpr AnalyzerFieldDescriptor kSourceLiveFreqStateField{"source.freq", "live_freq_state"};
constexpr AnalyzerFieldDescriptor kSourceAmpCenteredField{"source.amp", "centered"};
constexpr AnalyzerFieldDescriptor kSourceAmpLevelField{"source.amp", "level"};
constexpr AnalyzerFieldDescriptor kSourceAmpBaselineField{"source.amp", "baseline"};
constexpr AnalyzerFieldDescriptor kSourceAmpLiftField{"source.amp", "lift"};
constexpr AnalyzerFieldDescriptor kSourceAmpPeakField{"source.amp", "peak"};
constexpr AnalyzerFieldDescriptor kSourceAmpMeanField{"source.amp", "mean"};
constexpr AnalyzerFieldDescriptor kSourceAmpPeakTimeField{"source.amp.peak", "timeMs"};
constexpr AnalyzerFieldDescriptor kSourceOnsetRejectField{"source.amp", "onset_reject"};
constexpr AnalyzerFieldDescriptor kSourceTransientRejectField{"source.amp", "transient_reject"};
constexpr AnalyzerFieldDescriptor kSourceTransientRejectDurField{"source.amp", "transient_reject_dur"};
constexpr AnalyzerFieldDescriptor kSourceTransientRejectStrengthField{"source.amp", "transient_reject_strength"};
constexpr AnalyzerFieldDescriptor kScalarStateField{nullptr, "state"};
constexpr AnalyzerFieldDescriptor kScalarAcceptedSourceField{nullptr, "accepted_source"};
constexpr AnalyzerFieldDescriptor kScalarAcceptedTrialIdField{nullptr, "accepted_trial_id"};
constexpr AnalyzerFieldDescriptor kScalarAcceptedStartMsField{nullptr, "accepted_start_ms"};
constexpr AnalyzerFieldDescriptor kScalarAcceptedPeakMsField{nullptr, "accepted_peak_ms"};
constexpr AnalyzerFieldDescriptor kScalarAcceptedReleaseMsField{nullptr, "accepted_release_ms"};
constexpr AnalyzerFieldDescriptor kScalarAcceptedDtMsField{nullptr, "accepted_dt_ms"};
constexpr AnalyzerFieldDescriptor kScalarAcceptedDurMsField{nullptr, "accepted_dur_ms"};
constexpr AnalyzerFieldDescriptor kScalarAcceptedStrengthField{nullptr, "accepted_strength"};
constexpr AnalyzerFieldDescriptor kScalarAcceptedScoreField{nullptr, "accepted_score"};
constexpr AnalyzerFieldDescriptor kScalarAcceptedContrastField{nullptr, "accepted_contrast"};
constexpr AnalyzerFieldDescriptor kScalarBestPeakMsField{nullptr, "best_peak_ms"};
constexpr AnalyzerFieldDescriptor kScalarBestDurMsField{nullptr, "best_dur_ms"};
constexpr AnalyzerFieldDescriptor kScalarBestStrengthField{nullptr, "best_strength"};
constexpr AnalyzerFieldDescriptor kScalarWindowStartField{nullptr, "window_start_ms"};
constexpr AnalyzerFieldDescriptor kScalarWindowEndField{nullptr, "window_end_ms"};
constexpr AnalyzerFieldDescriptor kScalarExpectedWindowField{nullptr, "expected_window_ms"};
constexpr AnalyzerFieldDescriptor kScalarExpectedFrameCountField{nullptr, "expected_frame_count_estimate"};
constexpr AnalyzerFieldDescriptor kScalarDiagFrameCountOkField{nullptr, "diag_frame_count_ok"};
constexpr AnalyzerFieldDescriptor kScalarRejectReasonField{nullptr, "scalar_reject_reason"};
constexpr AnalyzerFieldDescriptor kScalarNoEmitReasonField{nullptr, "scalar_no_emit_reason"};
constexpr AnalyzerFieldDescriptor kScalarGateReasonField{nullptr, "scalar_gate_reason"};
constexpr AnalyzerFieldDescriptor kScalarOpenedField{nullptr, "scalar_opened"};
constexpr AnalyzerFieldDescriptor kScalarReleasedField{nullptr, "scalar_released"};
constexpr AnalyzerFieldDescriptor kScalarEmittedField{nullptr, "scalar_emitted"};
constexpr AnalyzerFieldDescriptor kScalarValidReleaseField{nullptr, "scalar_valid_release"};
constexpr AnalyzerFieldDescriptor kScalarEmitAllowedField{nullptr, "scalar_emit_allowed"};
constexpr AnalyzerFieldDescriptor kScalarOpenMsField{nullptr, "scalar_open_ms"};
constexpr AnalyzerFieldDescriptor kScalarPeakMsField{nullptr, "scalar_peak_ms"};
constexpr AnalyzerFieldDescriptor kScalarReleaseMsField{nullptr, "scalar_release_ms"};
constexpr AnalyzerFieldDescriptor kScalarDurationMsField{nullptr, "scalar_duration_ms"};
constexpr AnalyzerFieldDescriptor kScalarMinDurationMsField{nullptr, "scalar_min_duration_ms"};
constexpr AnalyzerFieldDescriptor kScalarMaxDurationMsField{nullptr, "scalar_max_duration_ms"};
constexpr AnalyzerFieldDescriptor kScalarTraceSourceOccurrenceEmittedField{nullptr, "trace_source_occurrence_emitted"};
constexpr AnalyzerFieldDescriptor kScalarTraceRuntimeEvidenceSeenField{nullptr, "trace_runtime_evidence_seen"};
constexpr AnalyzerFieldDescriptor kScalarTraceRuntimeOccurrenceReceivedField{nullptr, "trace_runtime_occurrence_received"};
constexpr AnalyzerFieldDescriptor kScalarTraceAnalyzerSeenField{nullptr, "trace_analyzer_seen"};
constexpr AnalyzerFieldDescriptor kScalarDetectionGateBlockedField{nullptr, "detection_gate_blocked"};
constexpr AnalyzerFieldDescriptor kScalarDetectionGateReasonField{nullptr, "detection_gate_reason"};
constexpr AnalyzerFieldDescriptor kScalarDiagInconsistentField{nullptr, "diag_inconsistent"};
constexpr AnalyzerFieldDescriptor kScalarCurrentTrialIdField{nullptr, "current_trial_id"};
constexpr AnalyzerFieldDescriptor kScalarLiveScalarReasonField{nullptr, "live_scalar_reason"};
constexpr AnalyzerFieldDescriptor kScalarLiveScalarWouldField{nullptr, "live_scalar_would"};
constexpr AnalyzerFieldDescriptor kScalarLiveScalarReadyField{nullptr, "live_scalar_ready"};
constexpr AnalyzerFieldDescriptor kScalarLiveScalarGateField{nullptr, "live_scalar_gate"};
constexpr AnalyzerFieldDescriptor kScalarLiveScalarPresentField{nullptr, "live_scalar_present"};
constexpr AnalyzerFieldDescriptor kScalarLiveScalarValidField{nullptr, "live_scalar_valid"};
constexpr AnalyzerFieldDescriptor kScalarLiveScalarMatchField{nullptr, "live_scalar_match"};
constexpr AnalyzerFieldDescriptor kScalarLiveScalarStateField{nullptr, "live_scalar_state"};
constexpr AnalyzerFieldDescriptor kScalarAmpCenteredField{nullptr, "amp_centered"};
constexpr AnalyzerFieldDescriptor kScalarAmpLevelField{nullptr, "amp_level"};
constexpr AnalyzerFieldDescriptor kScalarAmpBaselineField{nullptr, "amp_baseline"};
constexpr AnalyzerFieldDescriptor kScalarAmpLiftField{nullptr, "amp_lift"};
constexpr AnalyzerFieldDescriptor kScalarOnsetRejectField{nullptr, "onset_reject"};
constexpr AnalyzerFieldDescriptor kScalarTransientRejectField{nullptr, "transient_reject"};
constexpr AnalyzerFieldDescriptor kScalarTransientRejectDurField{nullptr, "transient_reject_dur"};
constexpr AnalyzerFieldDescriptor kScalarTransientRejectStrengthField{nullptr, "transient_reject_strength"};

const char* legacySourceCandidateScopeName(unsigned long peakMs, unsigned long windowStartMs, unsigned long windowEndMs) {
    if (peakMs == 0UL || windowEndMs < windowStartMs) {
        return "unknown";
    }
    if (peakMs < windowStartMs) {
        return "before_window";
    }
    if (peakMs > windowEndMs) {
        return "after_window";
    }
    return "in_window";
}

unsigned long sourceGapCount(const AnalyzerSourceCandidateSummary& summary) {
    return summary.islandCount > 0 ? summary.islandCount - 1UL : 0UL;
}

bool legacySourceFragmented(const AnalyzerSourceCandidateSummary& summary) {
    return sourceGapCount(summary) > 0 || summary.totalGapMs > 0;
}

void legacyPrintCompactGapFields(const AnalyzerSourceCandidateSummary& summary, bool alwaysPrintIslands = false) {
    const unsigned long gapCount = sourceGapCount(summary);
    if (gapCount > 0 || summary.totalGapMs > 0 || summary.maxGapMs > 0 || summary.candidateCount > 1) {
        Serial.print(" gap_count=");
        Serial.print(gapCount);
        Serial.print(" max_gap_ms=");
        Serial.print(summary.maxGapMs);
    }
    if (alwaysPrintIslands || summary.islandCount > 1) {
        Serial.print(" islands=");
        Serial.print(summary.islandCount > 0 ? summary.islandCount : 1UL);
    }
    if (legacySourceFragmented(summary)) {
        Serial.print(" fragmented=1");
    }
}

void legacyPrintSourceRejectSummaryLine(
    const char* label,
    unsigned long trialId,
    const AnalyzerSourceCandidateSummary& summary,
    unsigned long windowStartMs,
    unsigned long windowEndMs
) {
    const bool hasRejects = summary.present;
    const char* reason = hasRejects && summary.bestRejectReason != nullptr && summary.bestRejectReason[0] != '\0'
        ? summary.bestRejectReason
        : "none";
    const char* gateReason = hasRejects && summary.bestGateReason != nullptr && summary.bestGateReason[0] != '\0'
        ? summary.bestGateReason
        : "None";
    const char* scope = hasRejects
        ? legacySourceCandidateScopeName(summary.bestPeakMs, windowStartMs, windowEndMs)
        : "unknown";

    Serial.println(label);
    Serial.print("  ");
    printField(kSourceRejectCountField, summary.rejectCount);
    if (!hasRejects) {
        Serial.println();
        return;
    }
    Serial.print(' ');
    printField(kSourceTotalMatchMsField, summary.totalMatchMs);
    Serial.println();
    Serial.print("  ");
    printField(kSourceTotalGapMsField, summary.totalGapMs);
    Serial.print(' ');
    printField(kSourceMaxGapMsField, summary.maxGapMs);
    Serial.println();
    Serial.print("  ");
    printField(kSourceBestDurationMsField, summary.bestDurationMs);
    Serial.print(' ');
    printField(kSourceSecondBestDurationMsField, summary.secondBestDurationMs);
    Serial.print(' ');
    printField(kSourceBestPeakScoreField, summary.bestPeakPrimary, 1);
    Serial.print(' ');
    printField(kSourceBestPeakContrastField, summary.bestPeakSecondary, 2);
    Serial.println();
    Serial.print("  ");
    printField(kSourceBestOpenMsField, summary.bestOpenMs);
    Serial.print(' ');
    printField(kSourceBestPeakMsField, summary.bestPeakMs);
    Serial.print(' ');
    printField(kSourceBestLastMatchMsField, summary.bestLastMatchMs);
    Serial.print(' ');
    printField(kSourceBestCloseMsField, summary.bestCloseMs);
    Serial.println();
    Serial.print("  ");
    printField(kSourceBestRejectReasonField, reason);
    Serial.print(' ');
    printField(kSourceBestGateReasonField, gateReason);
    Serial.print(' ');
    printField(kSourceCloseCauseField, summary.closeCause != nullptr ? summary.closeCause : "none");
    Serial.println();
    Serial.print("  ");
    printField(kSourceMaxPrimaryField, summary.maxPeakPrimary, 1);
    Serial.print(' ');
    printField(kSourceMaxPrimaryMsField, summary.maxPeakPrimaryMs);
    Serial.print(' ');
    printField(kSourceMaxSecondaryField, summary.maxPeakSecondary, 2);
    Serial.print(' ');
    printField(kSourceMaxSecondaryMsField, summary.maxPeakSecondaryMs);
    Serial.println();
    Serial.print("  ");
    printField(kSourceScopeField, scope);
    Serial.println();
}

void legacyPrintCompactFrequencySourceSummary(
    const AnalyzerReport& report,
    const AnalyzerSourceStageReport& source,
    const AnalyzerFrequencyDiagnostic& frequencySource
) {
    const auto& summary = source.sourceSummary;
    const bool accepted = source.acceptedPresent;
    const float score = accepted ? frequencySource.acceptedScore : frequencySource.maxScore;
    const float contrast = accepted ? frequencySource.acceptedContrast : frequencySource.maxContrast;
    const unsigned long durationMs = accepted ? frequencySource.acceptedDurationMs : summary.bestDurationMs;
    const char* closeCause = frequencySource.fmCloseCause != nullptr ? frequencySource.fmCloseCause : "none";

    Serial.print("SEQ_SOURCE t=");
    Serial.print(report.context.trial);
    Serial.print(" state=");
    Serial.print(accepted ? "accepted" : "none");
    Serial.print(" src=freq");
    Serial.print(" score=");
    Serial.print(score, 1);
    Serial.print(" contrast=");
    Serial.print(contrast, 2);
    Serial.print(" dur=");
    Serial.print(durationMs);
    Serial.print("ms");
    Serial.print(" cand=");
    Serial.print(summary.candidateCount);
    Serial.print(" rejects=");
    Serial.print(summary.rejectCount);
    if (accepted) {
        legacyPrintCompactGapFields(summary, true);
    }
    Serial.print(" close=");
    Serial.print(closeCause);
    Serial.println();
}

void legacyPrintCompactFrequencySourceExtras(
    const AnalyzerReport& report,
    const AnalyzerSourceStageReport& source,
    const AnalyzerFrequencyDiagnostic& frequencySource
) {
    const auto& summary = source.sourceSummary;
    const unsigned long candidateId = frequencySource.acceptedCandidateId > 0
        ? frequencySource.acceptedCandidateId
        : frequencySource.lifecycleCandidateId;
    const unsigned long holdMs = frequencySource.candidateLastMatchMs >= frequencySource.fmOpenMs
        ? frequencySource.candidateLastMatchMs - frequencySource.fmOpenMs
        : 0UL;

    Serial.print("SEQ_SOURCE_CAND t=");
    Serial.print(report.context.trial);
    Serial.print(" id=");
    Serial.print(candidateId);
    Serial.print(" open=");
    Serial.print(frequencySource.fmOpenMs);
    Serial.print(" peak=");
    Serial.print(frequencySource.fmPeakMs);
    Serial.print(" release=");
    Serial.print(frequencySource.fmReleaseMs);
    Serial.print(" hold=");
    Serial.print(holdMs);
    Serial.print(" dur=");
    Serial.print(frequencySource.fmDurationMs);
    Serial.print(" min=");
    Serial.print(frequencySource.fmMinDurationUsedMs);
    Serial.print(" duration_ok=");
    Serial.print(frequencySource.fmDurationOk ? 1 : 0);
    Serial.print(" emitted=");
    Serial.println(frequencySource.fmEmitted ? 1 : 0);

    if (source.acceptedPresent || summary.totalGapMs > 0 || summary.islandCount > 1) {
        Serial.print("SEQ_SOURCE_GAPS t=");
        Serial.print(report.context.trial);
        Serial.print(" accepted_id=");
        Serial.print(frequencySource.acceptedCandidateId);
        Serial.print(" islands=");
        Serial.print(summary.islandCount > 0 ? summary.islandCount : 1UL);
        Serial.print(" gap_count=");
        Serial.print(sourceGapCount(summary));
        Serial.print(" total_gap_ms=");
        Serial.print(summary.totalGapMs);
        Serial.print(" max_gap_ms=");
        Serial.print(summary.maxGapMs);
        Serial.print(" longest_match_ms=");
        Serial.print(frequencySource.diagLongestMatchStreakMs);
        Serial.print(" coverage_ms=");
        Serial.println(summary.totalMatchMs);
    }

    if (summary.rejectCount > 0) {
        Serial.print("SEQ_SOURCE_REJECT t=");
        Serial.print(report.context.trial);
        Serial.print(" rejects=");
        Serial.print(summary.rejectCount);
        Serial.print(" best_dur=");
        Serial.print(summary.bestDurationMs);
        Serial.print(" second_dur=");
        Serial.print(summary.secondBestDurationMs);
        Serial.print(" best_score=");
        Serial.print(summary.bestPeakPrimary, 1);
        Serial.print(" best_reason=");
        Serial.print(summary.bestRejectReason != nullptr ? summary.bestRejectReason : "none");
        Serial.print(" best_gap=");
        Serial.println(summary.maxGapMs);
    }
}

void legacyPrintCompactScalarSourceSummary(
    const AnalyzerReport& report,
    const AnalyzerSourceStageReport& source,
    const AnalyzerScalarDiagnostic& scalarSource
) {
    const auto& summary = source.sourceSummary;
    const bool accepted = source.acceptedPresent;

    Serial.print("SEQ_SOURCE t=");
    Serial.print(report.context.trial);
    Serial.print(" state=");
    Serial.print(accepted ? "accepted" : "none");
    Serial.print(" src=scalar");
    Serial.print(" score=");
    Serial.print(accepted ? scalarSource.acceptedScore : summary.bestPeakPrimary, 1);
    Serial.print(" contrast=0.00");
    Serial.print(" dur=");
    Serial.print(accepted ? scalarSource.acceptedDurationMs : summary.bestDurationMs);
    Serial.print("ms");
    Serial.print(" cand=");
    Serial.print(summary.candidateCount);
    Serial.print(" rejects=");
    Serial.print(summary.rejectCount);
    if (accepted) {
        legacyPrintCompactGapFields(summary, true);
    }
    Serial.print(" close=");
    Serial.print(summary.closeCause != nullptr ? summary.closeCause : "none");
    Serial.println();
}

void legacyPrintCompactScalarSourceExtras(
    const AnalyzerReport& report,
    const AnalyzerSourceStageReport& source,
    const AnalyzerScalarDiagnostic& scalarSource
) {
    const auto& summary = source.sourceSummary;
    const auto& lastCandidate = source.lastCandidate;

    Serial.print("SEQ_SOURCE_CAND t=");
    Serial.print(report.context.trial);
    Serial.print(" id=0");
    Serial.print(" open=");
    Serial.print(scalarSource.scalarOpenMs);
    Serial.print(" peak=");
    Serial.print(scalarSource.scalarPeakMs);
    Serial.print(" release=");
    Serial.print(scalarSource.scalarReleaseMs);
    Serial.print(" hold=");
    Serial.print(scalarSource.scalarReleaseMs >= scalarSource.scalarOpenMs ? scalarSource.scalarReleaseMs - scalarSource.scalarOpenMs : 0UL);
    Serial.print(" dur=");
    Serial.print(scalarSource.scalarDurationMs);
    Serial.print(" min=");
    Serial.print(scalarSource.scalarMinDurationMs);
    Serial.print(" duration_ok=");
    Serial.print(scalarSource.scalarValidRelease ? 1 : 0);
    Serial.print(" emitted=");
    Serial.println(scalarSource.sourceOccurrenceEmitted ? 1 : 0);

    if (source.acceptedPresent || summary.totalGapMs > 0 || summary.islandCount > 1) {
        Serial.print("SEQ_SOURCE_GAPS t=");
        Serial.print(report.context.trial);
        Serial.print(" accepted_id=0");
        Serial.print(" islands=");
        Serial.print(summary.islandCount > 0 ? summary.islandCount : 1UL);
        Serial.print(" gap_count=");
        Serial.print(sourceGapCount(summary));
        Serial.print(" total_gap_ms=");
        Serial.print(summary.totalGapMs);
        Serial.print(" max_gap_ms=");
        Serial.print(summary.maxGapMs);
        Serial.print(" longest_match_ms=");
        Serial.print(lastCandidate.durationMs);
        Serial.print(" coverage_ms=");
        Serial.println(summary.totalMatchMs);
    }

    if (summary.rejectCount > 0) {
        Serial.print("SEQ_SOURCE_REJECT t=");
        Serial.print(report.context.trial);
        Serial.print(" rejects=");
        Serial.print(summary.rejectCount);
        Serial.print(" best_dur=");
        Serial.print(summary.bestDurationMs);
        Serial.print(" second_dur=");
        Serial.print(summary.secondBestDurationMs);
        Serial.print(" best_score=");
        Serial.print(summary.bestPeakPrimary, 1);
        Serial.print(" best_reason=");
        Serial.print(summary.bestRejectReason != nullptr ? summary.bestRejectReason : "none");
        Serial.print(" best_gap=");
        Serial.println(summary.maxGapMs);
    }
}

void legacyPrintSequenceSourcePreamble(
    bool scalarProfile,
    const AnalyzerSourceStageReport& source,
    unsigned long currentTrialId,
    bool acceptedPresent,
    const char* acceptedSource,
    long acceptedDtMs,
    unsigned long acceptedDurationMs,
    float acceptedStrength,
    float acceptedScore,
    float acceptedContrast,
    unsigned long windowStartMs,
    unsigned long windowEndMs
) {
    Serial.println("SEQ_SOURCE");
    Serial.print("state=");
    Serial.print(acceptedPresent ? "accepted" : "none");
    if (acceptedPresent) {
        Serial.print(' ');
        if (scalarProfile) {
            printField(kScalarAcceptedSourceField, acceptedSource != nullptr ? acceptedSource : "none");
        } else {
            printField(kSourceAcceptedSourceField, acceptedSource != nullptr ? acceptedSource : "none");
        }
        Serial.println();
        Serial.print(' ');
        if (scalarProfile) {
            printField(kScalarAcceptedDtMsField, acceptedDtMs);
        } else {
            printField(kSourceAcceptedDtMsField, acceptedDtMs);
        }
        Serial.print(' ');
        if (scalarProfile) {
            printField(kScalarAcceptedDurMsField, acceptedDurationMs);
        } else {
            printField(kSourceAcceptedDurationMsField, acceptedDurationMs);
        }
        Serial.println();
        Serial.print(' ');
        if (scalarProfile) {
            printField(kScalarAcceptedStrengthField, acceptedStrength, 1);
        } else {
            printField(kSourceAcceptedStrengthField, acceptedStrength, 1);
        }
        Serial.println();
        Serial.print(' ');
        if (scalarProfile) {
            printField(kScalarAcceptedScoreField, acceptedScore, 1);
            Serial.print(' ');
            printField(kScalarAcceptedContrastField, acceptedContrast, 2);
        } else {
            printField(kSourceAcceptedScoreField, acceptedScore, 1);
            Serial.print(' ');
            printField(kSourceAcceptedContrastField, acceptedContrast, 2);
        }
    }
    Serial.println();

    legacyPrintSourceRejectSummaryLine(
        "SEQ_SOURCE_REJECTS",
        currentTrialId,
        source.sourceSummary,
        windowStartMs,
        windowEndMs
    );
}

void legacyPrintSequenceSourceLifecycleDetail(
    const AnalyzerReport& report,
    const AnalyzerSourceStageReport& source,
    const AnalyzerFrequencyDiagnostic& frequencySource
) {
    const FrequencyMatchDetector* detector = report.frequencyDetector;
    const unsigned long candidateOpenMs = detector != nullptr ? detector->candidateOpenMs : 0UL;
    const unsigned long candidateLastMatchedMs = detector != nullptr ? detector->candidateLastMatchedMs : 0UL;
    const unsigned long refractoryRemainingMs = detector != nullptr && detector->candidateRefractoryUntilMs > report.context.timestampMs
        ? detector->candidateRefractoryUntilMs - report.context.timestampMs
        : 0UL;

    Serial.print("SEQ_SOURCE_LIFECYCLE");
    Serial.print(' ');
    printField(kSourceCandidateActiveAtTrialStartField, source.activeAtTrialStart);
    Serial.print(' ');
    printField(kSourceCandidateFirstMsField, candidateOpenMs);
    Serial.print(' ');
    printField(kSourceCandidateLastMatchMsField, candidateLastMatchedMs);
    Serial.print(' ');
    printField(kSourceCandidateHoldMsField,
        candidateLastMatchedMs >= candidateOpenMs
            ? candidateLastMatchedMs - candidateOpenMs
            : 0UL);
    Serial.print(' ');
    printField(kSourceAcceptedCandidateIdField, report.frequency.acceptedCandidateId);
    Serial.print(' ');
    printField(kSourceSelectedRejectCandidateIdField, report.frequency.selectedRejectCandidateId);
    Serial.print(' ');
    printField(kSourceLastCandidateIdField, report.frequency.lastCandidateId);
    Serial.print(' ');
    printField(kSourceLifecycleCandidateIdField, report.frequency.lifecycleCandidateId);
    Serial.print(' ');
    printField(kSourceRefractoryRemainingMsField, refractoryRemainingMs);
    Serial.print(' ');
    printField(kSourceOpenedThisTrialField, source.openedThisTrial);
    Serial.print(' ');
    printField(kSourceClosedThisTrialField, source.closedThisTrial);
    Serial.print(' ');
    printField(kSourceEmittedThisTrialField, source.emittedThisTrial);
    Serial.print(' ');
    printField(kSourceRejectedThisTrialField, source.rejectedThisTrial);
    Serial.print(' ');
    printField(kSourceFreshReleaseOkUpdatesField, report.frequency.releaseScoreOkFrames);
    Serial.print(' ');
    printField(kSourceHeldReleaseOkUpdatesField, report.frequency.heldFrames);
    Serial.println();
}

void legacyPrintFrequencyMatchSourceDetail(
    const AnalyzerReport& report,
    const AnalyzerSourceStageReport& source,
    const AnalyzerFrequencyDiagnostic& frequencySource,
    unsigned int detailLevel,
    bool compactSourceDiag
) {
    const auto& frequencySourceSummary = source.sourceSummary;
    const auto& frequencyLastCandidate = source.lastCandidate;
    const bool lastCandidatePresent = frequencyLastCandidate.present;

    if (detailLevel >= 1U && lastCandidatePresent) {
        Serial.print("SEQ_SOURCE_LAST_CANDIDATE");
        Serial.print(' ');
        printField(kSourceLastCandidatePresentField, lastCandidatePresent);
        Serial.print(' ');
        printField(kSourceLastCandidatePeakMsField, frequencyLastCandidate.peakMs);
        Serial.print(' ');
        printField(kSourceLastCandidateDurationMsField, frequencyLastCandidate.durationMs);
        Serial.print(' ');
        printField(kSourceLastCandidateSampleCountField, frequencyLastCandidate.sampleCount);
        Serial.print(' ');
        printField(kSourceFreqPeakScoreField, frequencyLastCandidate.peakPrimary, 1);
        Serial.print(' ');
        printField(kSourceFreqPeakContrastField, frequencyLastCandidate.peakSecondary, 2);
        Serial.print(' ');
        printField(kSourceLastCandidateReasonField, frequencyLastCandidate.reason != nullptr ? frequencyLastCandidate.reason : "none");
        Serial.print(' ');
        printField(kSourceLastCandidateGateReasonField, frequencyLastCandidate.gateReason != nullptr ? frequencyLastCandidate.gateReason : "none");
        Serial.print(' ');
        printField(kSourceScopeField, frequencyLastCandidate.scope != nullptr ? frequencyLastCandidate.scope : "unknown");
        Serial.print(' ');
        printField(kSourceAcceptedCandidateIdField, frequencySource.acceptedCandidateId);
        Serial.print(' ');
        printField(kSourceSelectedRejectCandidateIdField, frequencySource.selectedRejectCandidateId);
        Serial.print(' ');
        printField(kSourceLastCandidateIdField, frequencySource.lastCandidateId);
        Serial.print(' ');
        printField(kSourceLifecycleCandidateIdField, frequencySource.lifecycleCandidateId);
        Serial.print(' ');
        printField(kSourceFmOpenMsField, frequencySource.fmOpenMs);
        Serial.print(' ');
        printField(kSourceFmPeakMsField, frequencySource.fmPeakMs);
        Serial.print(' ');
        printField(kSourceFmReleaseMsField, frequencySource.fmReleaseMs);
        Serial.print(' ');
        printField(kSourceFmDurationMsField, frequencySource.fmDurationMs);
        Serial.print(' ');
        printField(kSourceFmMinDurationMsField, frequencySource.fmMinDurationMs);
        Serial.print(' ');
        printField(kSourceFmMaxDurationMsField, frequencySource.fmMaxDurationMs);
        Serial.print(' ');
        printField(kSourceFmDurationOkField, frequencySource.fmDurationOk);
        Serial.print(' ');
        printField(kSourceFmOpenedField, frequencySource.fmOpened);
        Serial.print(' ');
        printField(kSourceFmReleasedField, frequencySource.fmReleased);
        Serial.print(' ');
        printField(kSourceFmEmittedField, frequencySource.fmEmitted);
        Serial.print(' ');
        printField(kSourceFmValidReleaseField, frequencySource.fmValidRelease);
        Serial.print(' ');
        printField(kSourceFmEmitAllowedField, frequencySource.fmEmitAllowed);
        Serial.print(' ');
        printField(kSourceFmCloseCauseField, frequencySource.fmCloseCause != nullptr ? frequencySource.fmCloseCause : "none");
        Serial.print(' ');
        printField(kSourceDiagDurationInconsistentField, frequencySource.fmDurationInconsistent);
        Serial.println();
    }

    Serial.print("SEQ_SOURCE_DIAG");
    Serial.print(' ');
    printField(kSourceCurrentTrialIdField, frequencySource.currentTrialId);
    Serial.print(' ');
    printField(AnalyzerFieldDescriptor{"source.freq", "window_start_ms"}, frequencySource.windowStartMs);
    Serial.print(' ');
    printField(AnalyzerFieldDescriptor{"source.freq", "window_end_ms"}, frequencySource.windowEndMs);
    Serial.print(' ');
    printField(AnalyzerFieldDescriptor{"source.freq", "window_center_ms"},
        frequencySource.windowEndMs >= frequencySource.windowStartMs
            ? frequencySource.windowStartMs + ((frequencySource.windowEndMs - frequencySource.windowStartMs) / 2UL)
            : 0UL);
    Serial.print(' ');
    printField(kSourceDiagFirstFrameField, frequencySource.diagFirstFrameMs);
    Serial.print(' ');
    printField(kSourceDiagLastFrameField, frequencySource.diagLastFrameMs);
    Serial.print(' ');
    printField(kSourceExpectedWindowField, frequencySource.expectedWindowMs);
    Serial.print(' ');
    printField(kSourceExpectedFrameCountField, frequencySource.expectedFrameCountEstimate);
    Serial.print(' ');
    printField(AnalyzerFieldDescriptor{"source.freq", "window_ms"}, frequencySource.windowMs);
    Serial.print(' ');
    printField(AnalyzerFieldDescriptor{"source.freq", "update_step_ms"}, frequencySource.updateStepMs);
    Serial.print(' ');
    printField(AnalyzerFieldDescriptor{"source.freq", "overlap_ratio"}, frequencySource.overlapRatio, 3);
    Serial.print(' ');
    printField(AnalyzerFieldDescriptor{"source.freq", "bucket_count"}, frequencySource.bucketCount);
    Serial.print(' ');
    printField(AnalyzerFieldDescriptor{"source.freq", "value_count"}, frequencySource.valueCount);
    Serial.print(' ');
    printField(AnalyzerFieldDescriptor{"source.freq", "span_ms"}, frequencySource.spanMs);
    Serial.print(' ');
    printField(AnalyzerFieldDescriptor{"source.freq", "latest_value_age_ms"}, frequencySource.latestValueAgeMs);
    Serial.print(' ');
    printField(AnalyzerFieldDescriptor{"source.freq.band", "target_power_mean"}, frequencySource.targetPowerMean, 1);
    Serial.print(' ');
    printField(AnalyzerFieldDescriptor{"source.freq.band", "lower_power_mean"}, frequencySource.lowerPowerMean, 1);
    Serial.print(' ');
    printField(AnalyzerFieldDescriptor{"source.freq.band", "upper_power_mean"}, frequencySource.upperPowerMean, 1);
    Serial.print(' ');
    printField(AnalyzerFieldDescriptor{"source.freq.band", "neighbor_power_mean"}, frequencySource.neighborPowerMean, 1);
    Serial.print(' ');
    printField(AnalyzerFieldDescriptor{"source.freq.band", "neighbor_power_max_mean"}, frequencySource.neighborPowerMaxMean, 1);
    Serial.print(' ');
    printField(AnalyzerFieldDescriptor{"source.freq.band", "target_power_max"}, frequencySource.targetPowerMax, 1);
    Serial.print(' ');
    printField(AnalyzerFieldDescriptor{"source.freq.band", "lower_power_max"}, frequencySource.lowerPowerMax, 1);
    Serial.print(' ');
    printField(AnalyzerFieldDescriptor{"source.freq.band", "upper_power_max"}, frequencySource.upperPowerMax, 1);
    Serial.print(' ');
    printField(AnalyzerFieldDescriptor{"source.freq.band", "neighbor_power_mean_max"}, frequencySource.neighborPowerMeanMax, 1);
    Serial.print(' ');
    printField(AnalyzerFieldDescriptor{"source.freq.band", "neighbor_power_max_max"}, frequencySource.neighborPowerMaxMax, 1);
    Serial.print(' ');
    printField(AnalyzerFieldDescriptor{"source.freq.band", "target_power_max_ms"}, frequencySource.targetPowerMaxMs);
    Serial.print(' ');
    printField(AnalyzerFieldDescriptor{"source.freq.band", "lower_power_max_ms"}, frequencySource.lowerPowerMaxMs);
    Serial.print(' ');
    printField(AnalyzerFieldDescriptor{"source.freq.band", "upper_power_max_ms"}, frequencySource.upperPowerMaxMs);
    Serial.print(' ');
    printField(AnalyzerFieldDescriptor{"source.freq.band", "lower_score_mean"}, frequencySource.lowerScoreMean, 1);
    Serial.print(' ');
    printField(AnalyzerFieldDescriptor{"source.freq.band", "upper_score_mean"}, frequencySource.upperScoreMean, 1);
    Serial.print(' ');
    printField(AnalyzerFieldDescriptor{"source.freq.band", "lower_score_max"}, frequencySource.lowerScoreMax, 1);
    Serial.print(' ');
    printField(AnalyzerFieldDescriptor{"source.freq.band", "upper_score_max"}, frequencySource.upperScoreMax, 1);
    Serial.print(' ');
    printField(AnalyzerFieldDescriptor{"source.freq.band", "lower_score_max_ms"}, frequencySource.lowerScoreMaxMs);
    Serial.print(' ');
    printField(AnalyzerFieldDescriptor{"source.freq.band", "upper_score_max_ms"}, frequencySource.upperScoreMaxMs);

    if (compactSourceDiag) {
        Serial.print(' ');
        printField(kSourceDiagFrameCountOkField, frequencySource.diagFrameCountOk);
        Serial.print(' ');
        printField(kSourceFramesField, frequencySource.frames);
        Serial.print(' ');
        printField(kSourceValidFramesField, frequencySource.validFrames);
        Serial.print(' ');
        printField(kSourceAmpPeakField, frequencySource.ampPeak, 1);
        Serial.print(' ');
        printField(kSourceAmpMeanField, frequencySource.ampMean, 1);
        Serial.print(' ');
        printField(kSourceAmpPeakTimeField,
            frequencySource.ampPeakMs >= frequencySource.windowStartMs
                ? frequencySource.ampPeakMs - frequencySource.windowStartMs
                : 0UL);
        Serial.print(' ');
        printField(kSourceFreqHistoryScoreRecordsField, frequencySource.historyScoreRecords);
        Serial.print(' ');
        printField(kSourceFreqHistoryContrastRecordsField, frequencySource.historyContrastRecords);
        Serial.print(' ');
        printField(AnalyzerFieldDescriptor{"source.freq", "fresh_update_count"}, frequencySource.freshUpdateCount);
        Serial.print(' ');
        printField(AnalyzerFieldDescriptor{"source.freq", "held_update_count"}, frequencySource.heldUpdateCount);
        Serial.print(' ');
        printField(AnalyzerFieldDescriptor{"source.freq", "matched_update_count"}, frequencySource.matchedUpdateCount);
        Serial.print(' ');
        printField(AnalyzerFieldDescriptor{"source.freq", "candidate_duration_ms"}, frequencySource.candidateDurationMs);
        Serial.print(' ');
        printField(AnalyzerFieldDescriptor{"source.freq", "matched_span_ms"}, frequencySource.matchedSpanMs);
        Serial.print(' ');
        printField(AnalyzerFieldDescriptor{"source.freq", "matched_coverage_ms"}, frequencySource.matchedCoverageMs);
        Serial.print(' ');
        printField(AnalyzerFieldDescriptor{"source.freq", "max_gap_ms"}, frequencySource.sourceSummary.maxGapMs);
        Serial.print(' ');
        printField(AnalyzerFieldDescriptor{"source.freq", "fresh_coverage_ratio"}, frequencySource.freshCoverageRatio, 3);
        Serial.print(' ');
        printField(kSourceDurationUsedForDecisionMsField, frequencySource.fmDurationUsedMs);
        Serial.print(' ');
        printField(kSourceDurationPrintedMsField, frequencySource.fmDurationPrintedMs);
        Serial.print(' ');
        printField(kSourceMinDurationUsedMsField, frequencySource.fmMinDurationUsedMs);
        Serial.print(' ');
        printField(kSourceMinDurationReportedMsField, frequencySource.fmMinDurationReportedMs);
        Serial.print(' ');
        printField(kSourceDurationOkField, frequencySource.fmDurationOk);
        Serial.print(' ');
        printField(kSourceScoreTooLowFramesField, frequencySourceSummary.scoreTooLowFrames);
        Serial.print(' ');
        printField(kSourceContrastTooLowFramesField, frequencySourceSummary.contrastTooLowFrames);
        Serial.print(' ');
        printField(kSourceScoreAndContrastTooLowFramesField, frequencySourceSummary.scoreAndContrastTooLowFrames);
        Serial.print(' ');
        printField(kSourceScoreOkFramesField, frequencySource.scoreOkUpdates);
        Serial.print(' ');
        printField(kSourceContrastOkFramesField, frequencySource.contrastOkUpdates);
        Serial.print(' ');
        printField(kSourceBothOkFramesField, frequencySource.bothOkUpdates);
        Serial.print(' ');
        printField(kSourceMaxScoreField, frequencySource.maxScore, 1);
        Serial.print(' ');
        printField(kSourceMaxScoreMsField, frequencySource.maxScoreMs);
        Serial.print(' ');
        printField(kSourceMaxContrastField, frequencySource.maxContrast, 2);
        Serial.print(' ');
        printField(kSourceMaxContrastMsField, frequencySource.maxContrastMs);
        Serial.print(' ');
        printField(kSourceMeanScoreField, frequencySource.meanScore, 1);
        Serial.print(' ');
        printField(kSourceMeanContrastField, frequencySource.meanContrast, 2);
        Serial.print(' ');
        printField(kSourceScoreThresholdField, frequencySource.scoreThreshold, 1);
        Serial.print(' ');
        printField(kSourceContrastThresholdField, frequencySource.contrastThreshold, 2);
        Serial.print(' ');
        printField(kSourceFreqEvidenceClassField, frequencySource.freqEvidenceClass != nullptr ? frequencySource.freqEvidenceClass : "none");
        Serial.print(' ');
        printField(kSourceFmCloseCauseField, frequencySource.fmCloseCause != nullptr ? frequencySource.fmCloseCause : "none");
        Serial.print(' ');
        printField(kSourceDiagInconsistentField, frequencySource.inconsistent);
        Serial.println();
        return;
    }

    Serial.print(' ');
    printField(kSourceDiagFrameCountOkField, report.frequency.diagFrameCountOk);
    Serial.print(' ');
    printField(kSourceFramesField, report.frequency.frames);
    Serial.print(' ');
    printField(kSourceValidFramesField, report.frequency.validFrames);
    Serial.print(' ');
    printField(kSourceAmpPeakField, report.frequency.ampPeak, 1);
    Serial.print(' ');
    printField(kSourceAmpMeanField, report.frequency.ampMean, 1);
    Serial.print(' ');
    printField(kSourceAmpPeakTimeField,
        report.frequency.ampPeakMs >= report.frequency.windowStartMs
            ? report.frequency.ampPeakMs - report.frequency.windowStartMs
            : 0UL);
    Serial.print(' ');
    printField(kSourceFreqHistoryScoreRecordsField, report.frequency.historyScoreRecords);
    Serial.print(' ');
    printField(kSourceFreqHistoryContrastRecordsField, report.frequency.historyContrastRecords);
    Serial.print(' ');
    printField(AnalyzerFieldDescriptor{"source.freq", "window_ms"}, report.frequency.windowMs);
    Serial.print(' ');
    printField(AnalyzerFieldDescriptor{"source.freq", "update_step_ms"}, report.frequency.updateStepMs);
    Serial.print(' ');
    printField(AnalyzerFieldDescriptor{"source.freq", "overlap_ratio"}, report.frequency.overlapRatio, 3);
    Serial.print(' ');
    printField(AnalyzerFieldDescriptor{"source.freq", "bucket_count"}, report.frequency.bucketCount);
    Serial.print(' ');
    printField(AnalyzerFieldDescriptor{"source.freq", "value_count"}, report.frequency.valueCount);
    Serial.print(' ');
    printField(AnalyzerFieldDescriptor{"source.freq", "span_ms"}, report.frequency.spanMs);
    Serial.print(' ');
    printField(AnalyzerFieldDescriptor{"source.freq", "latest_value_age_ms"}, report.frequency.latestValueAgeMs);
    Serial.print(' ');
    printField(AnalyzerFieldDescriptor{"source.freq", "fresh_update_count"}, report.frequency.freshUpdateCount);
    Serial.print(' ');
    printField(AnalyzerFieldDescriptor{"source.freq", "held_update_count"}, report.frequency.heldUpdateCount);
    Serial.print(' ');
    printField(AnalyzerFieldDescriptor{"source.freq", "matched_update_count"}, report.frequency.matchedUpdateCount);
    Serial.print(' ');
    printField(AnalyzerFieldDescriptor{"source.freq", "candidate_duration_ms"}, report.frequency.candidateDurationMs);
    Serial.print(' ');
    printField(AnalyzerFieldDescriptor{"source.freq", "matched_span_ms"}, report.frequency.matchedSpanMs);
    Serial.print(' ');
    printField(AnalyzerFieldDescriptor{"source.freq", "matched_coverage_ms"}, report.frequency.matchedCoverageMs);
    Serial.print(' ');
    printField(AnalyzerFieldDescriptor{"source.freq", "max_gap_ms"}, report.frequency.sourceSummary.maxGapMs);
    Serial.print(' ');
    printField(AnalyzerFieldDescriptor{"source.freq", "fresh_coverage_ratio"}, report.frequency.freshCoverageRatio, 3);
    Serial.print(' ');
    printField(kSourceDurationUsedForDecisionMsField, report.frequency.fmDurationUsedMs);
    Serial.print(' ');
    printField(kSourceDurationPrintedMsField, report.frequency.fmDurationPrintedMs);
    Serial.print(' ');
    printField(kSourceMinDurationUsedMsField, report.frequency.fmMinDurationUsedMs);
    Serial.print(' ');
    printField(kSourceMinDurationReportedMsField, report.frequency.fmMinDurationReportedMs);
    Serial.print(' ');
    printField(kSourceDurationOkField, report.frequency.fmDurationOk);
    Serial.print(' ');
    printField(kSourceScoreTooLowFramesField, frequencySourceSummary.scoreTooLowFrames);
    Serial.print(' ');
    printField(kSourceContrastTooLowFramesField, frequencySourceSummary.contrastTooLowFrames);
    Serial.print(' ');
    printField(kSourceScoreAndContrastTooLowFramesField, frequencySourceSummary.scoreAndContrastTooLowFrames);
    Serial.print(' ');
    printField(kSourceScoreOkFramesField, report.frequency.scoreOkUpdates);
    Serial.print(' ');
    printField(kSourceContrastOkFramesField, report.frequency.contrastOkUpdates);
    Serial.print(' ');
    printField(kSourceBothOkFramesField, report.frequency.bothOkUpdates);
    Serial.print(' ');
    printField(kSourceMatchFramesField, report.frequency.matchFrames);
    Serial.print(' ');
    printField(kSourceRejectFramesField, report.frequency.rejectFrames);
    Serial.print(' ');
    printField(kSourceReleaseScoreOkFramesField, report.frequency.releaseScoreOkFrames);
    Serial.print(' ');
    printField(kSourceReleaseContrastOkFramesField, report.frequency.releaseContrastOkFrames);
    Serial.print(' ');
    printField(kSourceReleaseBothOkFramesField, report.frequency.releaseBothOkFrames);
    Serial.print(' ');
    printField(kSourceReleaseScoreTooLowFramesField, report.frequency.releaseScoreTooLowFrames);
    Serial.print(' ');
    printField(kSourceReleaseContrastTooLowFramesField, report.frequency.releaseContrastTooLowFrames);
    Serial.print(' ');
    printField(kSourceReleaseScoreAndContrastTooLowFramesField, report.frequency.releaseScoreAndContrastTooLowFrames);
    Serial.print(' ');
    printField(kSourceReleaseNoEvidenceFramesField, report.frequency.releaseNoEvidenceFrames);
    Serial.print(' ');
    printField(kSourceDiagLongestMatchStreakFramesField, report.frequency.diagLongestMatchStreakFrames);
    Serial.print(' ');
    printField(kSourceDiagLongestMatchStreakMsField, report.frequency.diagLongestMatchStreakMs);
    Serial.print(' ');
    printField(kSourceSumScoreField, report.frequency.sumScore, 1);
    Serial.print(' ');
    printField(kSourceSumContrastField, report.frequency.sumContrast, 2);
    Serial.print(' ');
    printField(kSourceAnalyzerMissReasonField, report.frequency.analyzerMissReason != nullptr ? report.frequency.analyzerMissReason : "none");
    Serial.print(' ');
    printField(kSourceSourceLastRejectReasonField, report.frequency.sourceLastRejectReason != nullptr ? report.frequency.sourceLastRejectReason : "none");
    Serial.print(' ');
    printField(kSourceSelectedRejectReasonField, report.frequency.selectedRejectReason != nullptr ? report.frequency.selectedRejectReason : "none");
    Serial.print(' ');
    printField(kSourceSelectedRejectGateReasonField, report.frequency.selectedRejectGateReason != nullptr ? report.frequency.selectedRejectGateReason : "none");
    Serial.print(' ');
    printField(kSourceFreqEvidenceClassField, report.source.frequencyMatch.freqEvidenceClass != nullptr ? report.source.frequencyMatch.freqEvidenceClass : "none");
    Serial.print(' ');
    printField(kSourceFmCloseCauseField, report.frequency.fmCloseCause != nullptr ? report.frequency.fmCloseCause : "none");
    Serial.print(' ');
    printField(kSourceDiagDurationInconsistentField, report.frequency.fmDurationInconsistent);
    Serial.print(' ');
    printField(kSourceNearMissField, report.frequency.nearMiss);
    Serial.print(' ');
    printField(kSourceNearMissReasonField, report.frequency.nearMissReason != nullptr ? report.frequency.nearMissReason : "none");
    Serial.print(' ');
    printField(kSourceDiagInconsistentField, report.frequency.inconsistent);
    if (detailLevel >= 2U && report.source.frequencyMatch.freqEvidenceClass != nullptr && strcmp(report.source.frequencyMatch.freqEvidenceClass, "strong_no_occurrence") == 0) {
        Serial.print(' ');
        printField(kSourceTraceSourceOccurrenceEmittedField, report.frequency.sourceOccurrenceEmitted);
        Serial.print(' ');
        printField(kSourceTraceRuntimeEvidenceSeenField, report.frequency.runtimeEvidenceSeen);
        Serial.print(' ');
        printField(kSourceTraceRuntimeOccurrenceReceivedField, report.frequency.runtimeOccurrenceReceived);
        Serial.print(' ');
        printField(kSourceTraceAnalyzerSeenField, report.frequency.analyzerSeenOccurrence);
        Serial.print(' ');
        printField(kSourceDetectionGateBlockedField, report.frequency.detectionGateBlocked);
        Serial.print(' ');
        printField(kSourceDetectionGateReasonField, report.frequency.detectionGateReason != nullptr ? report.frequency.detectionGateReason : "none");
    }
    Serial.println();

    if (detailLevel < 2U) {
        return;
    }

    Serial.print("SEQ_SOURCE_TRACE");
    Serial.print(' ');
    printField(kSourceCurrentTrialIdField, frequencySource.currentTrialId);
    if (report.frequency.inconsistent) {
        Serial.print(' ');
        printField(kSourceAcceptedTrialIdField, frequencySource.acceptedTrialId);
        Serial.print(' ');
        printField(kSourceAcceptedSourceField, frequencySource.acceptedSource != nullptr ? frequencySource.acceptedSource : "none");
        Serial.print(' ');
        printField(kSourceAnalyzerResultField, analyzerResultName(report.classification.result));
        Serial.print(' ');
        printField(kSourceAnalyzerReasonField, analyzerReasonName(report.classification.reason));
    }
    Serial.print(' ');
    printField(kSourceLiveFreqReasonField, frequencySource.liveFreqReason != nullptr ? frequencySource.liveFreqReason : "none");
    Serial.print(' ');
    printField(kSourceLiveFreqWouldField, frequencySource.liveFreqWould != nullptr ? frequencySource.liveFreqWould : "none");
    Serial.print(' ');
    printField(kSourceLiveFreqReadyField, frequencySource.liveFreqReady);
    Serial.print(' ');
    printField(kSourceLiveFreqGateField, frequencySource.liveFreqGate);
    Serial.print(' ');
    printField(kSourceLiveFreqPresentField, frequencySource.liveFreqPresent);
    Serial.print(' ');
    printField(kSourceLiveFreqValidField, frequencySource.liveFreqValid);
    Serial.print(' ');
    printField(kSourceLiveFreqMatchField, frequencySource.liveFreqMatch);
    Serial.print(' ');
    printField(kSourceLiveFreqStateField, frequencySource.liveFreqState != nullptr ? frequencySource.liveFreqState : "none");
    Serial.print(' ');
    printField(kSourceAmpCenteredField, report.profileDetail.scalarObservation.peak, 1);
    Serial.print(' ');
    printField(kSourceAmpLevelField, report.profileDetail.ampLevel, 1);
    Serial.print(' ');
    printField(kSourceAmpBaselineField, report.profileDetail.ampBase, 1);
    Serial.print(' ');
    printField(kSourceAmpLiftField, report.profileDetail.ampLift, 1);
    Serial.print(' ');
    printField(kSourceOnsetRejectField, detection::scalarInspectionNoteName(report.profileDetail.scalarObservation.note));
    Serial.print(' ');
    printField(kSourceTransientRejectField, report.debug.mainRejectReason != nullptr ? report.debug.mainRejectReason : "none");
    Serial.print(' ');
    printField(kSourceTransientRejectDurField, report.occurrences.primaryDurationMs);
    Serial.print(' ');
    printField(kSourceTransientRejectStrengthField, report.occurrences.primaryStrength, 1);
    Serial.println();
}

void legacyPrintScalarTransientSourceDetail(
    const AnalyzerReport& report,
    const AnalyzerSourceStageReport& source,
    const AnalyzerScalarDiagnostic& scalarSource,
    unsigned int detailLevel
) {
    const auto& scalarLastCandidate = source.lastCandidate;
    const bool scalarLastCandidatePresent = scalarLastCandidate.present;

    if (detailLevel >= 1U) {
        if (scalarLastCandidatePresent) {
            Serial.print("SEQ_SOURCE_LAST_CANDIDATE");
            Serial.print(' ');
            printField(kSourceLastCandidatePresentField, scalarLastCandidatePresent);
            Serial.print(' ');
            printField(kSourceLastCandidatePeakMsField, scalarLastCandidate.peakMs);
            Serial.print(' ');
            printField(kSourceLastCandidateDurationMsField, scalarLastCandidate.durationMs);
            Serial.print(' ');
            printField(kSourceLastCandidateSampleCountField, scalarLastCandidate.sampleCount);
            Serial.print(' ');
            printField(kSourceScalarPeakStrengthField, scalarLastCandidate.peakPrimary, 1);
            Serial.print(' ');
            printField(kSourceLastCandidateReasonField, scalarLastCandidate.reason != nullptr ? scalarLastCandidate.reason : "none");
            Serial.print(' ');
            printField(kSourceLastCandidateGateReasonField, scalarLastCandidate.gateReason != nullptr ? scalarLastCandidate.gateReason : "none");
            Serial.print(' ');
            printField(kSourceScopeField, scalarLastCandidate.scope != nullptr ? scalarLastCandidate.scope : "unknown");
            Serial.print(' ');
            printField(kScalarOpenMsField, scalarSource.scalarOpenMs);
            Serial.print(' ');
            printField(kScalarPeakMsField, scalarSource.scalarPeakMs);
            Serial.print(' ');
            printField(kScalarReleaseMsField, scalarSource.scalarReleaseMs);
            Serial.print(' ');
            printField(kScalarDurationMsField, scalarSource.scalarDurationMs);
            Serial.print(' ');
            printField(kScalarMinDurationMsField, scalarSource.scalarMinDurationMs);
            Serial.print(' ');
            printField(kScalarMaxDurationMsField, scalarSource.scalarMaxDurationMs);
            Serial.print(' ');
            printField(kScalarOpenedField, scalarSource.scalarOpened);
            Serial.print(' ');
            printField(kScalarReleasedField, scalarSource.scalarReleased);
            Serial.print(' ');
            printField(kScalarEmittedField, scalarSource.sourceOccurrenceEmitted);
            Serial.print(' ');
            printField(kScalarValidReleaseField, scalarSource.scalarValidRelease);
            Serial.print(' ');
            printField(kScalarEmitAllowedField, scalarSource.scalarEmitAllowed);
            Serial.println();
        }

        Serial.print("SEQ_SOURCE_DIAG");
        Serial.print(' ');
        printField(kScalarCurrentTrialIdField, scalarSource.currentTrialId);
        Serial.print(' ');
        printField(kScalarAcceptedTrialIdField, scalarSource.acceptedTrialId);
        Serial.print(' ');
        printField(kScalarAcceptedSourceField, scalarSource.acceptedSource != nullptr ? scalarSource.acceptedSource : "none");
        Serial.print(' ');
        printField(kScalarAcceptedDtMsField, scalarSource.acceptedDtMs);
        Serial.print(' ');
        printField(kScalarAcceptedDurMsField, scalarSource.acceptedDurationMs);
        Serial.print(' ');
        printField(kScalarAcceptedStrengthField, scalarSource.acceptedStrength, 1);
        Serial.print(' ');
        printField(kScalarAcceptedScoreField, scalarSource.acceptedScore, 1);
        Serial.print(' ');
        printField(kScalarAcceptedContrastField, scalarSource.acceptedContrast, 2);
        Serial.print(' ');
        printField(kScalarWindowStartField, scalarSource.windowStartMs);
        Serial.print(' ');
        printField(kScalarWindowEndField, scalarSource.windowEndMs);
        Serial.print(' ');
        printField(kScalarExpectedWindowField, scalarSource.expectedWindowMs);
        Serial.print(' ');
        printField(kScalarExpectedFrameCountField, scalarSource.expectedFrameCountEstimate);
        Serial.print(' ');
        printField(kScalarDiagFrameCountOkField, scalarSource.diagFrameCountOk);
        Serial.print(' ');
        printField(kScalarRejectReasonField, scalarSource.scalarRejectReason != nullptr ? scalarSource.scalarRejectReason : "unknown");
        Serial.print(' ');
        printField(kScalarNoEmitReasonField, scalarSource.scalarNoEmitReason != nullptr ? scalarSource.scalarNoEmitReason : "none");
        Serial.print(' ');
        printField(kScalarGateReasonField, scalarSource.scalarGateReason != nullptr ? scalarSource.scalarGateReason : "none");
        Serial.print(' ');
        printField(kScalarEmittedField, scalarSource.sourceOccurrenceEmitted);
        Serial.print(' ');
        printField(kScalarValidReleaseField, scalarSource.scalarValidRelease);
        Serial.print(' ');
        printField(kScalarEmitAllowedField, scalarSource.scalarEmitAllowed);
        Serial.print(' ');
        printField(kScalarOpenMsField, scalarSource.scalarOpenMs);
        Serial.print(' ');
        printField(kScalarPeakMsField, scalarSource.scalarPeakMs);
        Serial.print(' ');
        printField(kScalarReleaseMsField, scalarSource.scalarReleaseMs);
        Serial.print(' ');
        printField(kScalarDurationMsField, scalarSource.scalarDurationMs);
        Serial.print(' ');
        printField(kScalarMinDurationMsField, scalarSource.scalarMinDurationMs);
        Serial.print(' ');
        printField(kScalarMaxDurationMsField, scalarSource.scalarMaxDurationMs);
        Serial.print(' ');
        printField(kScalarTraceSourceOccurrenceEmittedField, scalarSource.sourceOccurrenceEmitted);
        Serial.print(' ');
        printField(kScalarTraceRuntimeEvidenceSeenField, scalarSource.runtimeEvidenceSeen);
        Serial.print(' ');
        printField(kScalarTraceRuntimeOccurrenceReceivedField, scalarSource.runtimeOccurrenceReceived);
        Serial.print(' ');
        printField(kScalarTraceAnalyzerSeenField, scalarSource.analyzerSeenOccurrence);
        Serial.print(' ');
        printField(kScalarDetectionGateBlockedField, scalarSource.detectionGateBlocked);
        Serial.print(' ');
        printField(kScalarDetectionGateReasonField, scalarSource.detectionGateReason != nullptr ? scalarSource.detectionGateReason : "none");
        Serial.print(' ');
        printField(kScalarDiagInconsistentField, scalarSource.inconsistent);
        Serial.println();
    }

    if (detailLevel < 2U) {
        return;
    }

    Serial.print(' ');
    printField(kScalarCurrentTrialIdField, scalarSource.currentTrialId);
    Serial.print(' ');
    printField(kScalarLiveScalarReasonField, scalarSource.liveScalarReason != nullptr ? scalarSource.liveScalarReason : "none");
    Serial.print(' ');
    printField(kScalarLiveScalarWouldField, scalarSource.liveScalarWould != nullptr ? scalarSource.liveScalarWould : "none");
    Serial.print(' ');
    printField(kScalarLiveScalarReadyField, scalarSource.liveScalarReady);
    Serial.print(' ');
    printField(kScalarLiveScalarGateField, scalarSource.liveScalarGate);
    Serial.print(' ');
    printField(kScalarLiveScalarPresentField, scalarSource.liveScalarPresent);
    Serial.print(' ');
    printField(kScalarLiveScalarValidField, scalarSource.liveScalarValid);
    Serial.print(' ');
    printField(kScalarLiveScalarMatchField, scalarSource.liveScalarMatch);
    Serial.print(' ');
    printField(kScalarLiveScalarStateField, scalarSource.liveScalarState != nullptr ? scalarSource.liveScalarState : "none");
    Serial.print(' ');
    printField(kScalarAmpCenteredField, report.profileDetail.ampCenteredMagnitude, 1);
    Serial.print(' ');
    printField(kScalarAmpLevelField, report.profileDetail.ampLevel, 1);
    Serial.print(' ');
    printField(kScalarAmpBaselineField, report.profileDetail.ampBase, 1);
    Serial.print(' ');
    printField(kScalarAmpLiftField, report.profileDetail.ampLift, 1);
    Serial.print(' ');
    printField(kScalarOnsetRejectField, scalarSource.scalarNoEmitReason != nullptr ? scalarSource.scalarNoEmitReason : "none");
    Serial.print(' ');
    printField(kScalarTransientRejectField, scalarSource.scalarRejectReason != nullptr ? scalarSource.scalarRejectReason : "none");
    Serial.print(' ');
    printField(kScalarTransientRejectDurField, scalarSource.scalarDurationMs);
    Serial.print(' ');
    printField(kScalarTransientRejectStrengthField, report.profileDetail.scalarObservation.classificationValue, 1);
    Serial.println();
}

const char* legacyAnalyzerProfileDetailNamespace(detection::DetectionProfileKind profileKind) {
    switch (profileKind) {
        case detection::DetectionProfileKind::Amp:
            return "amp";
        case detection::DetectionProfileKind::ChirpExperimental:
            return "chirp_experimental";
        case detection::DetectionProfileKind::ScalarFreqExperimental:
            return "scalar_freq_experimental";
        case detection::DetectionProfileKind::TonalPulse:
        default:
            return "tonal_pulse";
    }
}

const char* legacyAnalyzerProfileDetailSummary(detection::DetectionProfileKind profileKind) {
    switch (profileKind) {
        case detection::DetectionProfileKind::Amp:
            return "amp scalar profile view";
        case detection::DetectionProfileKind::ChirpExperimental:
            return "chirp_experimental profile view";
        case detection::DetectionProfileKind::ScalarFreqExperimental:
            return "scalar_freq_experimental experimental profile view";
        case detection::DetectionProfileKind::TonalPulse:
        default:
            return "generic tonal pulse profile view";
    }
}

const char* legacyOccurrenceSourceName(detection::OccurrenceSource source) {
    switch (source) {
        case detection::OccurrenceSource::Amp:
            return "amp";
        case detection::OccurrenceSource::Frequency:
            return "frequency";
        case detection::OccurrenceSource::Broadband:
            return "broadband";
        case detection::OccurrenceSource::None:
        default:
            return "none";
    }
}

const char* legacyOccurrenceRejectReasonName(detection::OccurrenceRejectReason reason) {
    switch (reason) {
        case detection::OccurrenceRejectReason::None:
            return "none";
        case detection::OccurrenceRejectReason::TooShort:
            return "too_short";
        case detection::OccurrenceRejectReason::TooLong:
            return "too_long";
        case detection::OccurrenceRejectReason::TooWeak:
            return "too_weak";
        case detection::OccurrenceRejectReason::BelowThreshold:
            return "below_threshold";
        case detection::OccurrenceRejectReason::Cooldown:
            return "cooldown";
        case detection::OccurrenceRejectReason::MissingFrequencyEvidence:
            return "missing_frequency_evidence";
        case detection::OccurrenceRejectReason::MissingAmpSupport:
            return "missing_amp_support";
        case detection::OccurrenceRejectReason::InvalidTiming:
            return "invalid_timing";
        case detection::OccurrenceRejectReason::UnsupportedKind:
            return "unsupported_kind";
        case detection::OccurrenceRejectReason::Unknown:
        default:
            return "unknown";
    }
}

const char* legacyStrengthClassName(detection::StrengthClass value) {
    switch (value) {
        case detection::StrengthClass::None:
            return "none";
        case detection::StrengthClass::Weak:
            return "weak";
        case detection::StrengthClass::Medium:
            return "medium";
        case detection::StrengthClass::Strong:
            return "strong";
        case detection::StrengthClass::Unknown:
        default:
            return "unknown";
    }
}

void legacyPrintScalarObservation(const detection::ScalarInspectionObservation& observation) {
    Serial.print(" scalar.inspect");
    Serial.print(" ");
    printField(kInspectorStreamField, detection::featureStreamName(observation.stream));
    Serial.print(" ");
    printField(kInspectorModeField, detection::scalarInspectionModeName(observation.mode));
    Serial.print(" ");
    printField(kInspectorClassificationField, observation.classificationValue, 1);
    Serial.print(" ");
    printField(kInspectorCountField, static_cast<unsigned long>(observation.sampleCount));
    Serial.print(" ");
    printField(kInspectorSupportBasisField, detection::scalarInspectionBasisName(observation.supportBasis));
    Serial.print(" ");
    printField(kInspectorNoteField, detection::scalarInspectionNoteName(observation.note));
    Serial.print(" ");
    printField(kInspectorAnchorField, detection::scalarInspectionAnchorName(observation.anchor));
    Serial.print(" ");
    Serial.print("pre_floor_window_start_ms=");
    Serial.print(observation.preFloorWindowStartMs);
    Serial.print(" ");
    Serial.print("pre_floor_window_end_ms=");
    Serial.print(observation.preFloorWindowEndMs);
    Serial.print(" ");
    Serial.print("window_ms=");
    Serial.print(observation.windowMs);
    Serial.print(" ");
    Serial.print("bucket_count=");
    Serial.print(static_cast<unsigned long>(observation.bucketCount));
    Serial.print(" ");
    Serial.print("value_count=");
    Serial.print(static_cast<unsigned long>(observation.valueCount));
    Serial.print(" ");
    Serial.print("span_ms=");
    Serial.print(static_cast<unsigned long>(observation.spanMs));
    Serial.print(" ");
    Serial.print("coverage_ratio=");
    Serial.print(observation.coverageRatio, 3);
    Serial.print(" ");
    Serial.print("latest_value_age_ms=");
    Serial.print(static_cast<unsigned long>(observation.latestValueAgeMs));
    Serial.print(" ");
    Serial.print("pre_floor_anchor=");
    Serial.print(detection::scalarInspectionAnchorName(observation.preFloorAnchor));
    Serial.print(" ");
    Serial.print("pre_floor_note=");
    Serial.print(detection::scalarInspectionNoteName(observation.preFloorNote));
    Serial.print(" ");
    Serial.print("pre_floor_window_ms=");
    Serial.print(observation.preFloorWindowMs);
    Serial.print(" ");
    Serial.print("pre_floor_value_count=");
    Serial.print(static_cast<unsigned long>(observation.preFloorValueCount));
    Serial.print(" ");
    Serial.print("pre_floor_bucket_count=");
    Serial.print(static_cast<unsigned long>(observation.preFloorBucketCount));
    Serial.print(" ");
    Serial.print("pre_floor_span_ms=");
    Serial.print(static_cast<unsigned long>(observation.preFloorSpanMs));
    Serial.print(" ");
    Serial.print("pre_floor_coverage_ratio=");
    Serial.print(observation.preFloorCoverageRatio, 3);
    Serial.print(" ");
    Serial.print("pre_floor_latest_value_age_ms=");
    Serial.print(static_cast<unsigned long>(observation.preFloorLatestValueAgeMs));
    Serial.print(" ");
    Serial.print("pre_floor_median=");
    Serial.print(observation.preFloorMedian, 1);
    Serial.print(" ");
    Serial.print("pre_floor_p75=");
    Serial.print(observation.preFloorP75, 1);
    Serial.print(" ");
    Serial.print("pre_floor_rms=");
    Serial.print(observation.preFloorRms, 1);
    Serial.print(" ");
    Serial.print("pre_floor_trimmed_mean=");
    Serial.print(observation.preFloorTrimmedMean, 1);
    Serial.print(" ");
    Serial.print("lift_p75=");
    Serial.print(detection::scalarInspectionLiftP75(observation), 1);
    Serial.print(" ");
    Serial.print("lift_rms=");
    Serial.print(detection::scalarInspectionLiftRms(observation), 1);
    Serial.print(" ");
    Serial.print("lift_trimmed_mean=");
    Serial.print(detection::scalarInspectionLiftTrimmedMean(observation), 1);
    Serial.print(" ");
    printField(kInspectorPeakField, observation.peak, 1);
    Serial.print(" ");
    printField(kInspectorMeanField, observation.mean, 1);
    Serial.print(" ");
    Serial.print("rms=");
    Serial.print(observation.rms, 1);
    Serial.print(" ");
    Serial.print("median=");
    Serial.print(observation.median, 1);
    Serial.print(" ");
    Serial.print("p75=");
    Serial.print(observation.p75, 1);
    Serial.print(" ");
    Serial.print("p90=");
    Serial.print(observation.p90, 1);
    Serial.print(" ");
    Serial.print("trimmed_mean=");
    Serial.print(observation.trimmedMean, 1);
    Serial.print(" ");
    printField(kInspectorLastField, observation.last, 1);
    Serial.print(" ");
    printField(kInspectorBaselineField, observation.mean, 1);
    Serial.print(" ");
    printField(kInspectorLiftField, detection::scalarInspectionLift(observation), 1);
    Serial.print(" ");
    printField(kInspectorSustainedCountField, static_cast<unsigned long>(observation.sustainedCount));
    Serial.print("@");
    printField(kInspectorSustainedThresholdField, observation.sustainedThreshold, 1);
    Serial.print(" ");
    printField(kInspectorStrengthField, legacyStrengthClassName(observation.strength));
    Serial.print(" ");
}

void legacyPrintInspectionScalarDetails(const AnalyzerReport& report) {
    if (!report.profileDetail.scalarObservation.available) {
        Serial.print(" scalar.inspect stream=unknown mode=unknown value=unknown");
        return;
    }

    legacyPrintScalarObservation(report.profileDetail.scalarObservation);
}

const char* legacySequenceTrialDurationClass(long durMs) {
    if (durMs < 0) {
        return "invalid";
    }
    if (durMs < 80L) {
        return "short";
    }
    if (durMs <= 180L) {
        return "clean";
    }
    if (durMs <= 240L) {
        return "smeared";
    }
    return "too_long";
}

size_t legacyAnalyzerReasonIndex(AnalyzerReason value) {
    return static_cast<size_t>(value);
}

const char* legacySequenceDiagModeName(AnalyzerApp::SequenceDiagMode mode) {
    switch (mode) {
        case AnalyzerApp::SequenceDiagMode::Miss:
            return "miss";
        case AnalyzerApp::SequenceDiagMode::Trial:
            return "trial";
        case AnalyzerApp::SequenceDiagMode::Off:
        default:
            return "off";
    }
}

const char* legacySequenceFaultClassNameFromMiss(
    const AnalyzerReport& report,
    const char* rawHealthClass,
    bool audioPresent,
    bool rawHealthWarningOnly,
    bool timingBacklog
) {
    const bool targetBandStrong = report.frequency.maxScore >= (report.frequency.scoreThreshold * 0.75f) ||
                                  report.frequency.maxContrast >= (report.frequency.contrastThreshold * 0.75f) ||
                                  report.frequency.scoreOkUpdates > 0 ||
                                  report.frequency.contrastOkUpdates > 0 ||
                                  report.frequency.bothOkUpdates > 0;
    const bool targetBandPresent = targetBandStrong ||
                                   report.frequency.matchFrames > 0 ||
                                   report.frequency.fmOpened ||
                                   report.frequency.fmReleased ||
                                   report.frequency.fmEmitted;
    const bool repeatLikeRaw = strcmp(rawHealthClass, "repeated") == 0 ||
                               strcmp(rawHealthClass, "flatline") == 0 ||
                               strcmp(rawHealthClass, "dc_stuck") == 0;

    if (!audioPresent) {
        return "NO_AUDIO_EVENT";
    }
    if (report.frequency.fmDurationInconsistent) {
        return "DURATION_REJECT_INCONSISTENT";
    }
    if (report.frequency.fmOpened && report.frequency.fmReleased && !report.frequency.fmEmitted) {
        return targetBandStrong ? "TARGET_PRESENT_DURATION_REJECT" : "DETECTOR_LIFECYCLE_REJECT";
    }
    if (repeatLikeRaw && targetBandPresent && !report.frequency.fmEmitted) {
        return "INPUT_REPEATED_TARGET_PRESENT_NO_OCCURRENCE";
    }
    if (repeatLikeRaw && !targetBandPresent) {
        return "INPUT_REPEATED_NO_TARGET";
    }
    if (strcmp(rawHealthClass, "clipped") == 0 || strcmp(rawHealthClass, "low_information") == 0) {
        return "INPUT_SAMPLE_BAD";
    }
    if (rawHealthWarningOnly) {
        return "AUDIO_REPEAT_WARNING";
    }
    if (report.frequency.freshUpdateCount == 0 && report.frequency.heldUpdateCount > 0) {
        return "FRESHNESS_HELD_EVIDENCE_SUSPECT";
    }
    if (timingBacklog && !targetBandPresent) {
        return "TIMING_BACKLOG";
    }
    if (!targetBandPresent) {
        return "NO_TARGET_BAND_EVIDENCE";
    }
    if (report.frequency.freshFrames == 0 && report.frequency.heldFrames > 0) {
        return "FRESHNESS_HELD_EVIDENCE_SUSPECT";
    }
    if (targetBandStrong && report.frequency.fmOpened && report.frequency.fmReleased && !report.frequency.fmEmitted) {
        return "TARGET_PRESENT_NO_OCCURRENCE";
    }
    if (targetBandStrong && (report.frequency.fmOpened || report.frequency.fmReleased) && !report.frequency.fmEmitted) {
        return "DETECTOR_LIFECYCLE_REJECT";
    }
    if (timingBacklog) {
        return "TIMING_BACKLOG";
    }
    return "UNKNOWN";
}

unsigned int sequenceDetailLevel(const AnalyzerApp::SeqOutputConfig& outputConfig) {
    if (outputConfig.mode == AnalyzerApp::SeqOutputMode::Explain) {
        return 2U;
    }
    return outputConfig.verbosity >= 2U ? 2U : outputConfig.verbosity >= 1U ? 1U : 0U;
}

const char* legacyRejectStageName(const AnalyzerReport& report) {
    switch (report.classification.reason) {
        case AnalyzerReason::MissingPipelineResult:
        case AnalyzerReason::NoOccurrence:
            return "source";
        case AnalyzerReason::OccurrenceSeenButRejected:
        case AnalyzerReason::InspectionFailed:
            return "inspect";
        case AnalyzerReason::PatternCandidateRejected:
        case AnalyzerReason::MultipleValidPatterns:
        case AnalyzerReason::MultipleCompetingPatterns:
        case AnalyzerReason::DuplicatePatternAfterPrimary:
            return "pattern";
        case AnalyzerReason::InvalidAudio:
        case AnalyzerReason::FieldTooDense:
        case AnalyzerReason::UnexpectedValidPatternWithoutTrigger:
        case AnalyzerReason::ValidPatternBeforeWindow:
        case AnalyzerReason::ValidPatternAfterWindow:
        case AnalyzerReason::Unknown:
        case AnalyzerReason::None:
        default:
            return "unknown";
    }
}

const char* legacyAnalyzerEvidenceTargetName(detection::EvidenceTarget value) {
    switch (value) {
        case detection::EvidenceTarget::AmpStrength:
            return "AmpStrength";
        case detection::EvidenceTarget::FrequencyScoreStrength:
            return "FrequencyScoreStrength";
        case detection::EvidenceTarget::FrequencyContrastQuality:
            return "FrequencyContrastQuality";
        case detection::EvidenceTarget::TargetBandStrength:
            return "TargetBandStrength";
        case detection::EvidenceTarget::None:
        default:
            return "None";
    }
}

bool legacySequenceDiagnosticsShouldPrint(AnalyzerApp::SequenceDiagMode mode, AnalyzerResult result) {
    if (mode == AnalyzerApp::SequenceDiagMode::Trial) {
        return true;
    }
    if (mode != AnalyzerApp::SequenceDiagMode::Miss) {
        return false;
    }

    switch (result) {
        case AnalyzerResult::Miss:
        case AnalyzerResult::Late:
        case AnalyzerResult::Duplicate:
        case AnalyzerResult::Rejected:
        case AnalyzerResult::Ambiguous:
        case AnalyzerResult::TooDense:
        case AnalyzerResult::InvalidAudio:
            return true;
        case AnalyzerResult::Expected:
        case AnalyzerResult::Early:
        case AnalyzerResult::Unknown:
        default:
            return false;
    }
}

} // namespace




void AnalyzerApp::legacyPrintSequenceDiagnostics(const AnalyzerReport& report) const {
    if (_valMode) {
        return;
    }
    if (!shouldPrintSequenceSource(report)) {
        return;
    }
    const unsigned int detailLevel = sequenceDetailLevel(_sequenceTest.outputConfig);
    if (report.profileDetail.emitter != nullptr && strcmp(report.profileDetail.emitter, "ScalarTransientSource") == 0) {
        legacyPrintSequenceScalarDiagnostics(report);
        return;
    }
    const auto& source = report.source;
    const auto& frequencySource = source.frequencyMatch;
    if (detailLevel == 0U) {
        legacyPrintCompactFrequencySourceSummary(report, source, frequencySource);
        return;
    }
    if (detailLevel == 1U) {
        legacyPrintCompactFrequencySourceSummary(report, source, frequencySource);
        legacyPrintCompactFrequencySourceExtras(report, source, frequencySource);
        return;
    }
    legacyPrintSequenceSourcePreamble(
        false,
        source,
        frequencySource.currentTrialId,
        source.acceptedPresent,
        frequencySource.acceptedSource,
        frequencySource.acceptedDtMs,
        frequencySource.acceptedDurationMs,
        frequencySource.acceptedStrength,
        frequencySource.acceptedScore,
        frequencySource.acceptedContrast,
        frequencySource.windowStartMs,
        frequencySource.windowEndMs
    );
    legacyPrintSequenceSourceLifecycleDetail(report, source, frequencySource);
    legacyPrintFrequencyMatchSourceDetail(report, source, frequencySource, detailLevel, false);
}

void AnalyzerApp::legacyPrintSequenceScalarDiagnostics(const AnalyzerReport& report) const {
    if (_valMode) {
        return;
    }
    if (!shouldPrintSequenceSource(report)) {
        return;
    }

    const unsigned int detailLevel = sequenceDetailLevel(_sequenceTest.outputConfig);
    const auto& source = report.source;
    const auto& scalarSource = source.scalarTransient;
    if (detailLevel == 0U) {
        legacyPrintCompactScalarSourceSummary(report, source, scalarSource);
        return;
    }
    if (detailLevel == 1U) {
        legacyPrintCompactScalarSourceSummary(report, source, scalarSource);
        legacyPrintCompactScalarSourceExtras(report, source, scalarSource);
        return;
    }

    legacyPrintSequenceSourcePreamble(
        true,
        source,
        scalarSource.currentTrialId,
        source.acceptedPresent,
        scalarSource.acceptedSource,
        scalarSource.acceptedDtMs,
        scalarSource.acceptedDurationMs,
        scalarSource.acceptedStrength,
        scalarSource.acceptedScore,
        scalarSource.acceptedContrast,
        scalarSource.windowStartMs,
        scalarSource.windowEndMs
    );
    legacyPrintScalarTransientSourceDetail(report, source, scalarSource, detailLevel);
}


void AnalyzerApp::legacyPrintBaseSummary() const {
    const unsigned long samples = _baseSession.samples;
    const unsigned long rawAvg = samples > 0 ? _baseSession.rawSum / samples : 0;
    const float deltaAvg = samples > 0 ? _baseSession.deltaSum / static_cast<float>(samples) : 0.0f;
    const float baselineAvg = samples > 0 ? _baseSession.baselineSum / static_cast<float>(samples) : 0.0f;
    const float baselineDrift = _baseSession.baselineMax - _baseSession.baselineMin;
    const int rawSwing = _baseSession.rawMax - _baseSession.rawMin;
    const float deltaSwing = _baseSession.deltaMax - _baseSession.deltaMin;
    const float deltaPeak = _baseSession.deltaMax >= 0.0f ? _baseSession.deltaMax : -_baseSession.deltaMax;
    const float deltaFloor = _baseSession.deltaMin <= 0.0f ? -_baseSession.deltaMin : _baseSession.deltaMin;
    const float deltaQuietPeak = deltaPeak > deltaFloor ? deltaPeak : deltaFloor;

    Serial.print("BASE done: samples=");
    Serial.print(samples);
    if (_baseSession.ignoredRawSamples > 0) {
        Serial.print(" ignored_raw=");
        Serial.print(_baseSession.ignoredRawSamples);
    }
    Serial.print(" rawSample_avg=");
    Serial.print(rawAvg);
    Serial.print(" rawSample_min=");
    Serial.print(_baseSession.rawMin);
    Serial.print(" rawSample_max=");
    Serial.print(_baseSession.rawMax);
    Serial.print(" rawSample_swing=");
    Serial.print(rawSwing);
    Serial.print(" centeredSample_avg=");
    Serial.print(deltaAvg, 1);
    Serial.print(" centeredSample_min=");
    Serial.print(_baseSession.deltaMin, 1);
    Serial.print(" centeredSample_max=");
    Serial.print(_baseSession.deltaMax, 1);
    Serial.print(" centeredSample_swing=");
    Serial.print(deltaSwing, 1);
    Serial.print(" baseline_avg=");
    Serial.print(baselineAvg, 1);
    Serial.print(" baseline_min=");
    Serial.print(_baseSession.baselineMin, 1);
    Serial.print(" baseline_max=");
    Serial.print(_baseSession.baselineMax, 1);
    Serial.print(" baseline_drift=");
    Serial.println(baselineDrift, 1);
    Serial.print("BASE quiet: quiet_rawSample_min=");
    Serial.print(_baseSession.rawMin);
    Serial.print(" quiet_rawSample_max=");
    Serial.print(_baseSession.rawMax);
    Serial.print(" quiet_rawSample_swing=");
    Serial.print(rawSwing);
    Serial.print(" quiet_centeredSample_min=");
    Serial.print(_baseSession.deltaMin, 1);
    Serial.print(" quiet_centeredSample_max=");
    Serial.print(_baseSession.deltaMax, 1);
    Serial.print(" quiet_centeredSample_swing=");
    Serial.print(deltaSwing, 1);
    Serial.print(" quiet_centeredSample_peak=");
    Serial.println(deltaQuietPeak, 1);
    legacyPrintBaseHints();
    printAudioSourceSummary();
    printOccurrenceSummary();
}

void AnalyzerApp::legacyPrintBaseHints() const {
    const float quietDeltaPeak = _baseSession.deltaMax >= 0.0f ? _baseSession.deltaMax : -_baseSession.deltaMax;
    const float quietDeltaFloor = _baseSession.deltaMin <= 0.0f ? -_baseSession.deltaMin : _baseSession.deltaMin;
    const float quietNoisePeak = quietDeltaPeak > quietDeltaFloor ? quietDeltaPeak : quietDeltaFloor;
    const unsigned long suggestedMinStrength = static_cast<unsigned long>(quietNoisePeak) + 6;
    const unsigned long suggestedAttack = static_cast<unsigned long>(quietNoisePeak) + 10;
    const unsigned long suggestedRelease = suggestedAttack > 6 ? suggestedAttack - 6 : suggestedAttack;

    Serial.print("BASE hints: quiet_rawSample_peak=");
    Serial.print(_baseSession.rawMax);
    Serial.print(" quiet_centeredSample_max=");
    Serial.print(_baseSession.deltaMax, 1);
    Serial.print(" quiet_centeredSample_peak=");
    Serial.print(quietNoisePeak, 1);
    Serial.print(" suggested_minStrength=");
    Serial.print(suggestedMinStrength);
    Serial.print(" suggested_attack=");
    Serial.print(suggestedAttack);
    Serial.print(" suggested_release=");
    Serial.println(suggestedRelease);
}

void AnalyzerApp::legacyPrintCaptureSummary() const {
    const unsigned long completed = _captureSession.completed;
    const float avgRawSwing = completed > 0 ? static_cast<float>(_captureSession.totalRawSwing) / static_cast<float>(completed) : 0.0f;
    const float avgDeltaSwing = completed > 0 ? _captureSession.totalDeltaSwing / static_cast<float>(completed) : 0.0f;
    const unsigned long quietRawAvg = _captureSession.quietRawSamples > 0 ? _captureSession.quietRawSum / _captureSession.quietRawSamples : 0;
    const float quietDeltaAvg = _captureSession.quietDeltaSamples > 0 ? _captureSession.quietDeltaSum / static_cast<float>(_captureSession.quietDeltaSamples) : 0.0f;

    Serial.print("CAP done: tries=");
    Serial.print(_captureSession.totalTrials);
    Serial.print(" completed=");
    Serial.print(completed);
    Serial.print(" avg_rawSample_swing=");
    Serial.print(avgRawSwing, 1);
    Serial.print(" avg_centeredSample_swing=");
    Serial.print(avgDeltaSwing, 1);
    Serial.print(" best_rawSample_swing=");
    Serial.print(_captureSession.bestRawSwing);
    Serial.print(" best_centeredSample_swing=");
    Serial.println(_captureSession.bestDeltaSwing, 1);
    Serial.print("CAP quiet: rawSample_avg=");
    Serial.print(quietRawAvg);
    Serial.print(" rawSample_peak=");
    Serial.print(_captureSession.quietRawMax);
    Serial.print(" centeredSample_avg=");
    Serial.print(quietDeltaAvg, 1);
    Serial.print(" centeredSample_peak=");
    Serial.println(_captureSession.quietDeltaMax, 1);
    legacyPrintCaptureHints();
    printAudioSourceSummary();
    printOccurrenceSummary();
}

void AnalyzerApp::legacyPrintCaptureHints() const {
    const unsigned long quietRawAvg = _captureSession.quietRawSamples > 0 ? _captureSession.quietRawSum / _captureSession.quietRawSamples : 0;
    const float quietDeltaPeak = _captureSession.quietDeltaMax >= 0.0f ? _captureSession.quietDeltaMax : -_captureSession.quietDeltaMax;
    const float quietDeltaFloor = _captureSession.quietDeltaMin <= 0.0f ? -_captureSession.quietDeltaMin : _captureSession.quietDeltaMin;
    const float quietNoisePeak = quietDeltaPeak > quietDeltaFloor ? quietDeltaPeak : quietDeltaFloor;
    const unsigned long suggestedMinStrength = static_cast<unsigned long>(quietNoisePeak) + 6;
    const unsigned long suggestedAttack = static_cast<unsigned long>(quietNoisePeak) + 10;
    const unsigned long suggestedRelease = suggestedAttack > 6 ? suggestedAttack - 6 : suggestedAttack;

    Serial.print("CAP hints: suggested_minStrength=");
    Serial.print(suggestedMinStrength);
    Serial.print(" suggested_attack=");
    Serial.print(suggestedAttack);
    Serial.print(" suggested_release=");
    Serial.print(suggestedRelease);
    Serial.print(" quiet_rawSample_avg=");
    Serial.print(quietRawAvg);
    Serial.print(" quiet_centeredSample_peak=");
    Serial.println(quietNoisePeak, 1);
}

void AnalyzerApp::legacyPrintValueFrame(unsigned long now) const {
    if (_lastPrintMs != 0 && !timing::elapsedSince(now, _lastPrintMs, kPrintIntervalMs)) {
        return;
    }

    _lastPrintMs = now;
    Serial.print("rawSample:");
    Serial.print(_audioSignal.rawSignal());
    Serial.print('\t');
    Serial.print("baseline:");
    Serial.print(static_cast<int>(_audioSignal.baseline()));
    Serial.print('\t');
    Serial.print("centeredSample:");
    Serial.print(_audioSignal.centeredSignal());
    Serial.print('\t');
    Serial.print("signalLevel:");
    Serial.print(_audioSignal.signalMagnitude());
    Serial.print('\t');
    Serial.print("smoothedLevel:");
    Serial.print(static_cast<int>(_audioSignal.smoothedSignalMagnitude()));
    Serial.print('\t');
    Serial.print("onset:");
    Serial.print(now < _valOnsetLatchedUntilMs ? 1 : 0);
    Serial.print('\t');
    Serial.print("transient:");
    Serial.println(now < _valTransientLatchedUntilMs ? 1 : 0);
}

