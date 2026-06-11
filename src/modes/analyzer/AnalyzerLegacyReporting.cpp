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



