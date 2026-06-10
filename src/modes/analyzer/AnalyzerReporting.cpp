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

const char* sourceCandidateScopeName(unsigned long peakMs, unsigned long windowStartMs, unsigned long windowEndMs) {
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

bool sourceFragmented(const AnalyzerSourceCandidateSummary& summary) {
    return sourceGapCount(summary) > 0 || summary.totalGapMs > 0;
}

void printCompactGapFields(const AnalyzerSourceCandidateSummary& summary, bool alwaysPrintIslands = false) {
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
    if (sourceFragmented(summary)) {
        Serial.print(" fragmented=1");
    }
}

void printSourceRejectSummaryLine(
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
        ? sourceCandidateScopeName(summary.bestPeakMs, windowStartMs, windowEndMs)
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

void printCompactFrequencySourceSummary(
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
        printCompactGapFields(summary, true);
    }
    Serial.print(" close=");
    Serial.print(closeCause);
    Serial.println();
}

void printCompactFrequencySourceExtras(
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

void printCompactScalarSourceSummary(
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
        printCompactGapFields(summary, true);
    }
    Serial.print(" close=");
    Serial.print(summary.closeCause != nullptr ? summary.closeCause : "none");
    Serial.println();
}

void printCompactScalarSourceExtras(
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

void printSequenceSourcePreamble(
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

    printSourceRejectSummaryLine(
        "SEQ_SOURCE_REJECTS",
        currentTrialId,
        source.sourceSummary,
        windowStartMs,
        windowEndMs
    );
}

void printSequenceSourceLifecycleDetail(
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

void printFrequencyMatchSourceDetail(
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

void printScalarTransientSourceDetail(
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

const char* analyzerProfileDetailNamespace(detection::DetectionProfileKind profileKind) {
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

const char* analyzerProfileDetailSummary(detection::DetectionProfileKind profileKind) {
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

const char* occurrenceSourceName(detection::OccurrenceSource source) {
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

const char* occurrenceRejectReasonName(detection::OccurrenceRejectReason reason) {
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

const char* strengthClassName(detection::StrengthClass value) {
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

void printScalarObservation(const detection::ScalarInspectionObservation& observation) {
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
    printField(kInspectorStrengthField, strengthClassName(observation.strength));
    Serial.print(" ");
}

void printInspectionScalarDetails(const AnalyzerReport& report) {
    if (!report.profileDetail.scalarObservation.available) {
        Serial.print(" scalar.inspect stream=unknown mode=unknown value=unknown");
        return;
    }

    printScalarObservation(report.profileDetail.scalarObservation);
}

const char* sequenceTrialDurationClass(long durMs) {
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

size_t analyzerReasonIndex(AnalyzerReason value) {
    return static_cast<size_t>(value);
}

const char* sequenceDiagModeName(AnalyzerApp::SequenceDiagMode mode) {
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

const char* sequenceFaultClassNameFromMiss(
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

const char* rejectStageName(const AnalyzerReport& report) {
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

const char* analyzerEvidenceTargetName(detection::EvidenceTarget value) {
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

bool sequenceDiagnosticsShouldPrint(AnalyzerApp::SequenceDiagMode mode, AnalyzerResult result) {
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

void AnalyzerApp::printSequenceTrialResult(const AnalyzerReport& report) const {
    if (_valMode) {
        return;
    }

    Serial.print("SEQ_TRIAL trial=");
    Serial.print(report.context.trial);
    Serial.print(" profile=");
    Serial.print(report.context.profile != nullptr ? report.context.profile : "unknown");
    Serial.print(" result=");
    Serial.print(analyzerResultName(report.classification.result));
    Serial.print(" reason=");
    Serial.print(analyzerReasonName(report.classification.reason));
    Serial.print(" dt=");
    if (report.classification.dtMs >= 0) {
        Serial.print(report.classification.dtMs);
        Serial.print("ms");
    } else {
        Serial.print("-1ms");
    }
    Serial.print(" confidence=");
    Serial.print(report.primaryPattern.confidence, 2);
    if (report.classification.result != AnalyzerResult::Expected &&
        report.classification.result != AnalyzerResult::Early &&
        report.classification.result != AnalyzerResult::Late) {
        Serial.print(" failed_at=");
        Serial.print(analyzerStageName(report.classification.primaryStage));
    }
    if (report.frequency.acceptedPresent) {
        Serial.print(" duration_ms=");
        Serial.print(report.frequency.acceptedDurationMs);
    }
    Serial.print(" candidate_count=");
    Serial.print(_sequenceTest.currentTrialDiagnostics.candidateCount);
    Serial.print(" duplicate_count=");
    Serial.print(_sequenceTest.currentTrialDiagnostics.duplicateCount);
    printCompactGapFields(report.source.sourceSummary);
    if (_sequenceTest.outputConfig.verbosity > 0U) {
        if (_sequenceTest.currentMissStreak > 0) {
            Serial.print(" miss_streak=");
            Serial.print(_sequenceTest.currentMissStreak);
        }
    }
    Serial.println();
}

void AnalyzerApp::printSequenceStreak(const AnalyzerReport& report) const {
    if (_valMode) {
        return;
    }
    if (!shouldPrintSequenceStreak(report)) {
        return;
    }

    const auto& diagnostics = _sequenceTest.currentTrialDiagnostics;
    const AnalyzerResult result = report.classification.result;
    const bool streakRelevant =
        result == AnalyzerResult::Miss ||
        result == AnalyzerResult::Late ||
        result == AnalyzerResult::Duplicate ||
        result == AnalyzerResult::Unexpected ||
        result == AnalyzerResult::Rejected ||
        result == AnalyzerResult::Ambiguous ||
        result == AnalyzerResult::TooDense ||
        result == AnalyzerResult::InvalidAudio ||
        diagnostics.duplicateCount > 0 ||
        diagnostics.candidateCount > 1 ||
        diagnostics.candidateOverflowCount > 0;
    if (!streakRelevant) {
        return;
    }

    const char* audioHealth = report.frequency.audioHealth != nullptr ? report.frequency.audioHealth : "unknown";
    const double rawFrameCount = diagnostics.rawFrames > 0 ? static_cast<double>(diagnostics.rawFrames) : 0.0;
    const double rawMean = rawFrameCount > 0.0 ? static_cast<double>(diagnostics.rawSum) / rawFrameCount : 0.0;
    const double rawMeanAbs = rawFrameCount > 0.0 ? static_cast<double>(diagnostics.rawAbsSum) / rawFrameCount : 0.0;
    const double rawMinAbs = diagnostics.rawMin < 0 ? -static_cast<double>(diagnostics.rawMin) : static_cast<double>(diagnostics.rawMin);
    const double rawMaxAbsValue = diagnostics.rawMax < 0 ? -static_cast<double>(diagnostics.rawMax) : static_cast<double>(diagnostics.rawMax);
    const double rawMaxAbs = rawMinAbs > rawMaxAbsValue ? rawMinAbs : rawMaxAbsValue;
    const double rawRange = diagnostics.rawFrames > 0 && diagnostics.rawMax >= diagnostics.rawMin
        ? static_cast<double>(diagnostics.rawMax - diagnostics.rawMin)
        : 0.0;
    const double rawSpreadEst = rawRange > 0.0 ? rawRange / 3.4641016151377544 : 0.0;
    const double rawZeroCrossRate = diagnostics.rawFrames > 1
        ? static_cast<double>(diagnostics.rawZeroCrossings) / static_cast<double>(diagnostics.rawFrames - 1U)
        : 0.0;
    const double rawSameValueRatio = diagnostics.rawFrames > 1
        ? static_cast<double>(diagnostics.rawSameValueCount) / static_cast<double>(diagnostics.rawFrames - 1U)
        : 0.0;
    const char* rawHealthClass = rawHealthClassNameFromCounters(
        diagnostics.audioRmsTooHighFrames,
        diagnostics.rawFrames,
        static_cast<unsigned long>(rawMaxAbs),
        static_cast<float>(rawMean),
        static_cast<float>(rawMeanAbs),
        diagnostics.rawMin,
        diagnostics.rawMax,
        static_cast<float>(rawSameValueRatio),
        static_cast<unsigned long>(diagnostics.rawSameValueMaxRun),
        static_cast<unsigned long>(diagnostics.rawBlockHashRepeatCount),
        diagnostics.audioFlatlineFrames,
        diagnostics.audioZeroishFrames,
        diagnostics.audioLargeJumpFrames,
        diagnostics.audioRms,
        diagnostics.audioRmsTooLowFrames,
        diagnostics.audioRmsTooHighFrames
    );
    const bool audioRepeatWarningOnly = strcmp(audioHealth, "flatline") == 0 && strcmp(rawHealthClass, "ok") == 0;
    const bool audioPresent = diagnostics.audioFrames > 0;
    const bool timingBacklog = _sequenceTest.maxProcessingLagMs > 250UL;
    const char* faultClass = result == AnalyzerResult::Miss
        ? sequenceFaultClassNameFromMiss(
            report,
            rawHealthClass,
            audioPresent,
            audioRepeatWarningOnly,
            timingBacklog
        )
        : "none";
    const char* emitStatus = [&]() -> const char* {
        if (!diagnostics.emitSeen) {
            return "missing";
        }
        if (!(diagnostics.emitStartSeen && diagnostics.emitDoneSeen)) {
            return "partial";
        }

        switch (result) {
            case AnalyzerResult::Expected:
                return "ok";
            case AnalyzerResult::Late:
                return "late";
            case AnalyzerResult::Unexpected:
                return "unexpected";
            case AnalyzerResult::Rejected:
                return "rejected";
            case AnalyzerResult::Ambiguous:
                return "ambiguous";
            case AnalyzerResult::TooDense:
                return "too_dense";
            case AnalyzerResult::InvalidAudio:
                return "invalid_audio";
            case AnalyzerResult::Duplicate:
                return "duplicate";
            case AnalyzerResult::Miss:
                return "missing";
            case AnalyzerResult::Early:
            case AnalyzerResult::Unknown:
            default:
                return "ok";
        }
    }();

    const char* streakType = "miss_like";
    if (result == AnalyzerResult::Duplicate || diagnostics.duplicateCount > 0) {
        streakType = "duplicate_burst";
    } else if (diagnostics.candidateOverflowCount > 0 || diagnostics.candidateCount > 1) {
        streakType = "candidate_fragmentation";
    } else if (result == AnalyzerResult::Miss) {
        streakType = _sequenceTest.currentMissStreak > 1 ? "miss_continue" : "miss_start";
    } else if (result == AnalyzerResult::Late) {
        streakType = "late";
    } else if (result == AnalyzerResult::Unexpected) {
        streakType = "unexpected";
    }

    Serial.print("SEQ_STREAK trial=");
    Serial.print(report.context.trial);
    Serial.print(" type=");
    Serial.print(streakType);
    Serial.print(" result=");
    Serial.print(analyzerResultName(result));
    Serial.print(" streak=");
    Serial.print(_sequenceTest.currentMissStreak);
    Serial.print(" reason=");
    Serial.print(analyzerReasonName(report.classification.reason));
    Serial.print(" fault_class=");
    Serial.print(faultClass != nullptr ? faultClass : "none");
    if (_sequenceTest.outputConfig.verbosity >= 1U) {
        Serial.print(" audio_present=");
        Serial.print(audioPresent ? 1 : 0);
        Serial.print(" raw_health_class=");
        Serial.print(rawHealthClass != nullptr ? rawHealthClass : "unknown");
        Serial.print(" emit_status=");
        Serial.print(emitStatus);
        Serial.print(" candidate_count=");
        Serial.print(diagnostics.candidateCount);
        Serial.print(" duplicate_count=");
        Serial.print(diagnostics.duplicateCount);
        Serial.print(" timing_lag_max_ms=");
        Serial.print(_sequenceTest.maxProcessingLagMs);
    }
    if (_sequenceTest.outputConfig.verbosity >= 2U) {
        Serial.print(" raw_min=");
        Serial.print(diagnostics.rawFrames > 0 ? diagnostics.rawMin : 0);
        Serial.print(" raw_max=");
        Serial.print(diagnostics.rawFrames > 0 ? diagnostics.rawMax : 0);
        Serial.print(" raw_range=");
        Serial.print(rawRange, 1);
        Serial.print(" raw_mean=");
        Serial.print(rawMean, 1);
        Serial.print(" raw_mean_abs=");
        Serial.print(rawMeanAbs, 1);
        Serial.print(" raw_spread_est=");
        Serial.print(rawSpreadEst, 1);
        Serial.print(" zero_cross_rate=");
        Serial.print(rawZeroCrossRate, 3);
        Serial.print(" same_value_ratio=");
        Serial.print(rawSameValueRatio, 3);
        Serial.print(" repeated_sample_max_run=");
        Serial.print(static_cast<unsigned int>(diagnostics.rawSameValueMaxRun));
        Serial.print(" block_hash_repeat_count=");
        Serial.print(static_cast<unsigned int>(diagnostics.rawBlockHashRepeatCount));
        Serial.print(" audio_flatline_frames=");
        Serial.print(diagnostics.audioFlatlineFrames);
        Serial.print(" audio_zeroish_frames=");
        Serial.print(diagnostics.audioZeroishFrames);
        Serial.print(" audio_large_jump_frames=");
        Serial.print(diagnostics.audioLargeJumpFrames);
        Serial.print(" audio_rms=");
        Serial.print(diagnostics.audioRms, 1);
        Serial.print(" freq_score_max=");
        Serial.print(report.frequency.maxScore, 1);
        Serial.print(" freq_contrast_max=");
        Serial.print(report.frequency.maxContrast, 2);
        Serial.print(" fresh_update_count=");
        Serial.print(report.frequency.freshUpdateCount);
        Serial.print(" held_update_count=");
        Serial.print(report.frequency.heldUpdateCount);
        Serial.print(" matched_update_count=");
        Serial.print(report.frequency.matchedUpdateCount);
        Serial.print(" latest_feature_age_ms=");
        Serial.print(report.frequency.latestValueAgeMs);
        const bool candidatePresent = report.frequency.lifecycleCandidateId > 0;
        if (candidatePresent) {
            Serial.print(" candidate_id=");
            Serial.print(report.frequency.lifecycleCandidateId);
            Serial.print(" candidate_open_ms=");
            Serial.print(report.frequency.fmOpenMs);
            Serial.print(" candidate_last_match_ms=");
            Serial.print(report.frequency.candidateLastMatchMs);
            Serial.print(" candidate_release_ms=");
            Serial.print(report.frequency.fmReleaseMs);
            Serial.print(" candidate_close_ms=");
            Serial.print(report.frequency.fmReleaseMs);
            Serial.print(" candidate_duration_ms=");
            Serial.print(report.frequency.fmDurationMs);
            Serial.print(" duration_used_for_decision_ms=");
            Serial.print(report.frequency.fmDurationUsedMs);
            Serial.print(" min_duration_used_ms=");
            Serial.print(report.frequency.fmMinDurationUsedMs);
            Serial.print(" duration_ok=");
            Serial.print(report.frequency.fmDurationOk ? 1 : 0);
            Serial.print(" release_ok=");
            Serial.print(report.frequency.fmValidRelease ? 1 : 0);
            Serial.print(" emit_allowed=");
            Serial.print(report.frequency.fmEmitAllowed ? 1 : 0);
            Serial.print(" candidate_reject_reason=");
            Serial.print(report.frequency.sourceLastRejectReason != nullptr ? report.frequency.sourceLastRejectReason : "");
            Serial.print(" candidate_no_emit_reason=");
            Serial.print(report.frequency.selectedRejectReason != nullptr ? report.frequency.selectedRejectReason : "");
            Serial.print(" diag_duration_inconsistent=");
            Serial.print(report.frequency.fmDurationInconsistent ? 1 : 0);
            Serial.print(" diag_printed_duration_inconsistent=");
            Serial.print(report.frequency.fmPrintedDurationInconsistent ? 1 : 0);
            Serial.print(" det_active_start=");
            Serial.print(report.frequency.fmOpenMs);
            Serial.print(" accepted_candidate_id=");
            Serial.print(report.frequency.acceptedCandidateId);
            Serial.print(" selected_reject_candidate_id=");
            Serial.print(report.frequency.selectedRejectCandidateId);
            Serial.print(" last_candidate_id=");
            Serial.print(report.frequency.lastCandidateId);
            Serial.print(" lifecycle_candidate_id=");
            Serial.print(report.frequency.lifecycleCandidateId);
            Serial.print(" opened_this_trial=");
            Serial.print(report.frequency.fmOpened ? 1 : 0);
            Serial.print(" closed_this_trial=");
            Serial.print(report.frequency.fmReleased ? 1 : 0);
            Serial.print(" emitted_this_trial=");
            Serial.print(report.frequency.fmEmitted ? 1 : 0);
        }
    }
    Serial.println();
}

void AnalyzerApp::printSequenceTrialHeader(unsigned long trialNumber) const {
    if (_valMode) {
        return;
    }

    Serial.println();
    Serial.print("#");
    Serial.print(trialNumber);
    Serial.print(" ----------------");
    Serial.println();
}

void AnalyzerApp::printSequenceInspect(const AnalyzerReport& report) const {
    if (_valMode) {
        return;
    }
    if (!shouldPrintSequenceInspect(report)) {
        return;
    }
    if (!report.occurrences.present) {
        return;
    }

    const detection::DetectionProfile& selectedProfile = detection::detectionProfileForKind(_sequenceTest.profileKind);
    const unsigned int inspectDetailLevel = sequenceDetailLevel(_sequenceTest.outputConfig);
    const size_t moduleCount = report.profileDetail.inspectionObservationCount < selectedProfile.inspectionPlan.count
        ? report.profileDetail.inspectionObservationCount
        : selectedProfile.inspectionPlan.count;

    for (size_t i = 0; i < moduleCount; ++i) {
        const auto& module = selectedProfile.inspectionPlan.modules[i];
        const auto& observation = report.profileDetail.inspectionObservations[i];
        const char* targetName = analyzerEvidenceTargetName(module.target);
        const char* streamName = detection::featureStreamName(module.scalar.stream);

        Serial.print("SEQ_INSPECT");
        Serial.print(" module=");
        Serial.print(static_cast<unsigned long>(i + 1U));
        Serial.print(" target=");
        Serial.print(targetName != nullptr ? targetName : "unknown");
        Serial.print(" stream=");
        Serial.print(streamName != nullptr ? streamName : "unknown");
        Serial.print(" available=");
        Serial.print(observation.available ? 1 : 0);
        Serial.print(" evidence=");
        if (module.target == detection::EvidenceTarget::FrequencyScoreStrength) {
            Serial.print("freq.score=");
            Serial.print(report.profileDetail.freqScore, 2);
        } else if (module.target == detection::EvidenceTarget::FrequencyContrastQuality) {
            Serial.print("freq.contrast=");
            Serial.print(report.profileDetail.freqContrast, 2);
        } else if (module.target == detection::EvidenceTarget::AmpStrength) {
            Serial.print("scalar.classification=");
            Serial.print(observation.classificationValue, 1);
        } else if (module.target == detection::EvidenceTarget::TargetBandStrength) {
            Serial.print("target_band.level=");
            Serial.print(report.profileDetail.ampLevel, 1);
        } else {
            Serial.print("unknown");
        }
        if (observation.available) {
            Serial.print(" strength=");
            Serial.print(strengthClassName(observation.strength));
            Serial.print(" note=");
            Serial.print(detection::scalarInspectionNoteName(observation.note));
        }
        Serial.println();

        if (inspectDetailLevel >= 2U) {
            Serial.print("SEQ_INSPECT_COMPARE");
            Serial.print(" module=");
            Serial.print(static_cast<unsigned long>(i + 1U));
            Serial.print(" target=");
            Serial.print(targetName != nullptr ? targetName : "unknown");
            Serial.print(" stream=");
            Serial.print(streamName != nullptr ? streamName : "unknown");
            Serial.print(" accepted=");
            Serial.print(observation.available ? 1 : 0);
            Serial.print(" note=");
            Serial.print(detection::scalarInspectionNoteName(observation.note));
            Serial.print(" mode=");
            Serial.print(detection::scalarInspectionModeName(observation.mode));
            Serial.print(" anchor=");
            Serial.print(detection::scalarInspectionAnchorName(observation.anchor));
            Serial.print(" coverage=");
            Serial.print(observation.coverageRatio, 3);
            Serial.print(" pre_floor_coverage=");
            Serial.print(observation.preFloorCoverageRatio, 3);
            Serial.print(" peak=");
            Serial.print(observation.peak, 1);
            Serial.print(" mean=");
            Serial.print(observation.mean, 1);
            Serial.print(" rms=");
            Serial.print(observation.rms, 1);
            Serial.print(" median=");
            Serial.print(observation.median, 1);
            Serial.print(" p75=");
            Serial.print(observation.p75, 1);
            Serial.print(" p90=");
            Serial.print(observation.p90, 1);
            Serial.print(" trimmed_mean=");
            Serial.print(observation.trimmedMean, 1);
            Serial.print(" lift_p75=");
            Serial.print(detection::scalarInspectionLiftP75(observation), 1);
            Serial.print(" lift_rms=");
            Serial.print(detection::scalarInspectionLiftRms(observation), 1);
            Serial.print(" lift_trimmed_mean=");
            Serial.println(detection::scalarInspectionLiftTrimmedMean(observation), 1);
        }
    }

    return;
}


void AnalyzerApp::printSequencePattern(const AnalyzerReport& report) const {
    if (_valMode) {
        return;
    }
    if (!shouldPrintSequencePattern(report)) {
        return;
    }

    const unsigned int detailLevel = sequenceDetailLevel(_sequenceTest.outputConfig);

    Serial.print("SEQ_PATTERN");
    Serial.print(" pattern=");
    Serial.print(report.primaryPattern.type != nullptr ? report.primaryPattern.type : "none");
    Serial.print(" accepted=");
    Serial.print(report.primaryPattern.accepted ? 1 : 0);
    Serial.print(" candidate_accepted=");
    Serial.print(report.primaryPattern.candidateAccepted ? 1 : 0);
    Serial.print(" matched=");
    Serial.print(report.primaryPattern.patternMatched ? 1 : 0);
    Serial.print(" support=");
    Serial.print(report.primaryPattern.supportMatched ? 1 : 0);
    Serial.print(" reason=");
    Serial.print(report.primaryPattern.reason != nullptr ? report.primaryPattern.reason : "none");
    Serial.print(" reject_reason=");
    Serial.print(report.primaryPattern.rejectReason != nullptr ? report.primaryPattern.rejectReason : "none");
    if (detailLevel >= 2U) {
        Serial.print(" confidence=");
        Serial.print(report.primaryPattern.confidence, 2);
        Serial.print(" dt=");
        if (report.primaryPattern.dtMs >= 0) {
            Serial.print(report.primaryPattern.dtMs);
            Serial.print("ms");
        } else {
            Serial.print("-1ms");
        }
        Serial.print(" involved_occurrences=");
        Serial.print(report.primaryPattern.involvedOccurrences);
    }
    Serial.println();
}

void AnalyzerApp::printSequenceExplain(const AnalyzerReport& report) const {
    if (_valMode) {
        return;
    }
    if (!shouldPrintSequenceExplain(report)) {
        return;
    }

    const auto printMs = [](long value) {
        if (value >= 0) {
            Serial.print(value);
            Serial.print("ms");
        } else {
            Serial.print("-1ms");
        }
    };
    const auto printUnsignedMs = [](unsigned long value) {
        Serial.print(value);
        Serial.print("ms");
    };
    const auto printModuleList = [](const char* modules) {
        if (modules == nullptr || modules[0] == '\0') {
            Serial.println("none");
            return;
        }
        const char* cursor = modules;
        while (*cursor != '\0') {
            const char* start = cursor;
            while (*cursor != '\0' && *cursor != ',') {
                ++cursor;
            }
            while (start < cursor && (*start == ' ' || *start == '\t')) {
                ++start;
            }
            const char* end = cursor;
            while (end > start && (*(end - 1) == ' ' || *(end - 1) == '\t')) {
                --end;
            }
            Serial.print("  - ");
            for (const char* p = start; p < end; ++p) {
                Serial.print(*p);
            }
            Serial.println();
            if (*cursor == ',') {
                ++cursor;
            }
        }
    };

    Serial.print("SEQ_DUMP #");
    Serial.println(report.context.trial);
    Serial.println("1 pattern:");
    Serial.print("A_result=");
    Serial.print(analyzerResultName(report.classification.result));
    Serial.print(" A_dt=");
    printMs(report.classification.dtMs);
    Serial.print(" confidence=");
    Serial.print(report.primaryPattern.confidence, 2);
    Serial.println();
    Serial.print("pattern=");
    Serial.print(report.primaryPattern.type != nullptr ? report.primaryPattern.type : "unknown");
    Serial.print(" A_reason=");
    Serial.print(analyzerReasonName(report.classification.reason));
    Serial.print(" valid=");
    Serial.print(report.primaryPattern.accepted ? 1 : 0);
    Serial.print(" candidate_accepted=");
    Serial.print(report.primaryPattern.candidateAccepted ? 1 : 0);
    Serial.print(" pattern_matched=");
    Serial.print(report.primaryPattern.patternMatched ? 1 : 0);
    Serial.print(" support_matched=");
    Serial.print(report.primaryPattern.supportMatched ? 1 : 0);
    Serial.print(" behavior_eligible=");
    Serial.println(report.primaryPattern.behaviorEligible ? 1 : 0);

    Serial.println("2 detector:");
    Serial.print("kind=");
    Serial.print(report.occurrences.kind != nullptr ? report.occurrences.kind : "none");
    Serial.print(" source=");
    Serial.print(report.occurrences.primarySource != nullptr ? report.occurrences.primarySource : "none");
    Serial.print(" detector=");
    Serial.print(report.occurrences.detectorKind != nullptr ? report.occurrences.detectorKind : "unknown");
    Serial.print(" present=");
    Serial.print(report.occurrences.present ? 1 : 0);
    Serial.print(" valid=");
    Serial.print(report.occurrences.valid ? 1 : 0);
    Serial.print(" start_ms=");
    printUnsignedMs(report.occurrences.startMs);
    Serial.print(" peak_ms=");
    printUnsignedMs(report.occurrences.peakMs);
    Serial.print(" release_ms=");
    printUnsignedMs(report.occurrences.releaseMs);
    Serial.print(" dt=");
    printMs(report.occurrences.primaryDtMs);
    Serial.print(" dur=");
    printUnsignedMs(report.occurrences.primaryDurationMs);
    Serial.print(" strength=");
    Serial.print(report.occurrences.strength, 1);
    Serial.print(" confidence=");
    Serial.print(report.occurrences.confidence, 2);
    Serial.print(" reject_reason=");
    Serial.println(report.occurrences.rejectReason != nullptr ? report.occurrences.rejectReason : "none");

    Serial.println("3 inspectors:");
    Serial.print("module_results: module_strength_class=");
    Serial.print(report.inspection.moduleStrengthClass != nullptr ? report.inspection.moduleStrengthClass : "unknown");
    Serial.print(" module_count=");
    Serial.print(report.profileDetail.inspectionModuleCount);
    printInspectionScalarDetails(report);
    Serial.print(" evidence=");
    Serial.print(report.inspection.primaryEvidence != nullptr ? report.inspection.primaryEvidence : "none");
    Serial.print(" inspected=");
    Serial.print(report.inspection.inspected);
    Serial.print(" accepted=");
    Serial.print(report.inspection.accepted);
    Serial.print(" rejected=");
    Serial.print(report.inspection.rejected);
    Serial.print(" main_reject=");
    Serial.println(report.inspection.mainRejectReason != nullptr ? report.inspection.mainRejectReason : "none");
    Serial.println("modules:");
    printModuleList(report.profileDetail.inspectionModules);

    Serial.println("4 field:");
    Serial.print("field_state=");
    Serial.print(report.field.state != nullptr ? report.field.state : "unknown");
    Serial.print(" field_rawActivity=");
    Serial.print(report.field.rawActivity, 2);
    Serial.print(" field_validPatternActivity=");
    Serial.print(report.field.validPatternActivity, 2);
    Serial.print(" field_recent_valid=");
    Serial.print(report.field.recentValidPatterns);
    Serial.print(" field_recent_rejects=");
    Serial.println(report.field.recentRejects);

    Serial.println("5 debug:");
    Serial.print("A_debug_occurrences=");
    Serial.print(report.debug.occurrences);
    Serial.print(" A_debug_inspected=");
    Serial.print(report.debug.inspected);
    Serial.print(" A_debug_patterns=");
    Serial.print(report.debug.patterns);
    Serial.print(" A_debug_rejects=");
    Serial.print(report.debug.rejects);
    Serial.print(" A_debug_duplicate_hits=");
    Serial.print(report.debug.duplicates);
    Serial.print(" A_debug_unexpected_hits=");
    Serial.print(report.debug.unexpected);
    Serial.print(" A_debug_pattern_result_queue_overflow=");
    Serial.print(report.debug.patternResultQueueOverflowCount);
    Serial.print(" A_debug_main_reject=");
    Serial.println(report.debug.mainRejectReason != nullptr ? report.debug.mainRejectReason : "none");

    if (_sequenceTest.outputConfig.verbosity > 0 || _sequenceTest.outputConfig.mode == SeqOutputMode::Explain) {
        Serial.println("custom_expected:");
        Serial.print("pattern=");
        Serial.print(report.expected.patternType != nullptr ? report.expected.patternType : "none");
        Serial.print(" window=");
        printUnsignedMs(report.expected.windowStartMs);
        Serial.print("-");
        printUnsignedMs(report.expected.windowEndMs);
        Serial.print(" target=");
        Serial.print(report.context.target != nullptr ? report.context.target : "none");
        Serial.print(" source=");
        Serial.print(report.expected.expectedSource != nullptr ? report.expected.expectedSource : "none");
        Serial.print(" trigger_ms=");
        printUnsignedMs(report.expected.triggerMs);
        Serial.println();

        Serial.println("custom_occurrence:");
        Serial.print("A_total=");
        Serial.print(report.occurrences.total);
        Serial.print(" A_accepted=");
        Serial.print(report.occurrences.accepted);
        Serial.print(" A_rejected=");
        Serial.print(report.occurrences.rejected);
        Serial.print(" primary_source=");
        Serial.print(report.occurrences.primarySource != nullptr ? report.occurrences.primarySource : "none");
        Serial.print(" primary_dt=");
        printMs(report.occurrences.primaryDtMs);
        Serial.print(" primary_dur=");
        printUnsignedMs(report.occurrences.primaryDurationMs);
        Serial.print(" primary_strength=");
        Serial.print(report.occurrences.primaryStrength, 1);
        Serial.print(" confidence=");
        Serial.print(report.occurrences.confidence, 2);
        Serial.print(" A_main_reject=");
        Serial.println(report.occurrences.mainRejectReason != nullptr ? report.occurrences.mainRejectReason : "none");

        Serial.println("custom_pattern:");
        Serial.print("valid=");
        Serial.print(report.primaryPattern.accepted ? 1 : 0);
        Serial.print(" dt=");
        printMs(report.primaryPattern.dtMs);
        Serial.print(" confidence=");
        Serial.print(report.primaryPattern.confidence, 2);
        Serial.println();
        Serial.print("pattern=");
        Serial.print(report.primaryPattern.type != nullptr ? report.primaryPattern.type : "unknown");
        Serial.print(" amp_strength=");
        Serial.print(report.primaryPattern.ampStrength != nullptr ? report.primaryPattern.ampStrength : "unknown");
        Serial.print(" reject_reason=");
        Serial.print(report.primaryPattern.rejectReason != nullptr ? report.primaryPattern.rejectReason : "none");
        Serial.print(" occurrences=");
        Serial.println(report.primaryPattern.involvedOccurrences);

        Serial.println("custom_profile:");
        Serial.print("ns=");
        Serial.print(report.profileDetail.namespaceName != nullptr ? report.profileDetail.namespaceName : "none");
        Serial.print(" summary=");
        Serial.print(report.profileDetail.summary != nullptr ? report.profileDetail.summary : "");
        Serial.print(" amp_strength=");
        Serial.print(report.profileDetail.ampStrength != nullptr ? report.profileDetail.ampStrength : "unknown");
        Serial.print(" freq_score=");
        Serial.print(report.profileDetail.freqScore, 1);
        Serial.print(" freq_contrast=");
        Serial.print(report.profileDetail.freqContrast, 2);
        Serial.print(" amp_centered=");
        Serial.print(report.profileDetail.ampCenteredMagnitude, 1);
        Serial.print(" amp_base=");
        Serial.print(report.profileDetail.ampBase, 1);
        Serial.print(" amp_lift=");
        Serial.println(report.profileDetail.ampLift, 1);
    }
}

void AnalyzerApp::printSequenceCandidateLogs(unsigned long trialNumber, const SequenceTest::TrialDiagnostics& diagnostics) const {
    if (_valMode) {
        return;
    }
    if (!_sequenceTest.outputConfig.diagnosticsEnabled) {
        return;
    }
    if (_sequenceTest.outputConfig.mode != SeqOutputMode::Explain) {
        return;
    }

    for (unsigned long i = 0; i < diagnostics.candidateCount; ++i) {
        const auto& candidate = diagnostics.candidates[i];
        Serial.print("SEQ_CAND trial=");
        Serial.print(trialNumber);
        Serial.print(" idx=");
        Serial.print(i + 1UL);
        Serial.print(" candidate_class=");
        Serial.print(sequenceCandidateClassName(candidate.candidateClass));
        Serial.print(" onset_ms=");
        Serial.print(candidate.candidateMs);
        Serial.print(" onset_dt_ms=");
        Serial.print(candidate.dtFromTriggerMs);
        Serial.print(" peak_ms=");
        Serial.print(candidate.peakMs);
        Serial.print(" dur=");
        Serial.print(candidate.durationMs);
        Serial.print(" end_dt_ms=");
        if (candidate.endDtMs >= 0) {
            Serial.print(candidate.endDtMs);
            Serial.print("ms");
        } else {
            Serial.print("-");
        }
        Serial.print(" strength=");
        Serial.print(candidate.strength, 1);
        Serial.print(" pattern_valid=");
        Serial.print(candidate.patternValid ? 1 : 0);
        Serial.print(" candidateAccepted=");
        Serial.print(candidate.candidateAccepted ? 1 : 0);
        Serial.print(" patternMatched=");
        Serial.print(candidate.patternMatched ? 1 : 0);
        Serial.print(" supportMatched=");
        Serial.print(candidate.supportMatched ? 1 : 0);
        Serial.print(" behaviorEligible=");
        Serial.print(candidate.behaviorEligible ? 1 : 0);
        Serial.print(" reject_reason=");
        Serial.print(detection::patternRejectReasonName(candidate.rejectReasonCode));
        Serial.print(" pattern_type=");
        Serial.print(detection::patternTypeName(candidate.patternType));
        Serial.print(" reason=");
        Serial.println(detection::patternReasonName(candidate.reasonCode));
    }

    if (diagnostics.candidateOverflowCount > 0) {
        Serial.print("SEQ_CAND_WARN trial=");
        Serial.print(trialNumber);
        Serial.print(" overflow=");
        Serial.println(diagnostics.candidateOverflowCount);
    }
}

void AnalyzerApp::printSequenceDiagnostics(const AnalyzerReport& report) const {
    if (_valMode) {
        return;
    }
    if (!shouldPrintSequenceSource(report)) {
        return;
    }
    const unsigned int detailLevel = sequenceDetailLevel(_sequenceTest.outputConfig);
    if (report.profileDetail.emitter != nullptr && strcmp(report.profileDetail.emitter, "ScalarTransientSource") == 0) {
        printSequenceScalarDiagnostics(report);
        return;
    }
    const auto& source = report.source;
    const auto& frequencySource = source.frequencyMatch;
    if (detailLevel == 0U) {
        printCompactFrequencySourceSummary(report, source, frequencySource);
        return;
    }
    if (detailLevel == 1U) {
        printCompactFrequencySourceSummary(report, source, frequencySource);
        printCompactFrequencySourceExtras(report, source, frequencySource);
        return;
    }
    printSequenceSourcePreamble(
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
    printSequenceSourceLifecycleDetail(report, source, frequencySource);
    printFrequencyMatchSourceDetail(report, source, frequencySource, detailLevel, false);
}

void AnalyzerApp::printSequenceScalarDiagnostics(const AnalyzerReport& report) const {
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
        printCompactScalarSourceSummary(report, source, scalarSource);
        return;
    }
    if (detailLevel == 1U) {
        printCompactScalarSourceSummary(report, source, scalarSource);
        printCompactScalarSourceExtras(report, source, scalarSource);
        return;
    }

    printSequenceSourcePreamble(
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
    printScalarTransientSourceDetail(report, source, scalarSource, detailLevel);
}

void AnalyzerApp::printDetectionParameters() const {
    if (_valMode) {
        return;
    }
    Serial.print("SEQ tuning freqScore=");
    Serial.print(_frequencyEvidenceTuning.attackScoreMin, 1);
    Serial.print(" freqReleaseScore=");
    Serial.print(_frequencyEvidenceTuning.releaseScoreMin, 1);
    Serial.print(" freqContrast=");
    Serial.print(_frequencyEvidenceTuning.attackContrastMin, 1);
    Serial.print(" freqReleaseContrast=");
    Serial.print(_frequencyEvidenceTuning.releaseContrastMin, 1);
    Serial.print(" transientDetector=fixed");
    Serial.println();

    const detection::DetectionProfile& selectedProfile = detection::detectionProfileForKind(_sequenceTest.profileKind);
    Serial.print("SEQ freqmatch:");
    Serial.print(" min_duration_ms=");
    Serial.print(selectedProfile.frequencyMatch.minDurationMs);
    Serial.print(" release_debounce_ms=");
    Serial.print(selectedProfile.frequencyMatch.releaseDebounceMs);
    Serial.print(" cooldown_ms=");
    Serial.print(selectedProfile.frequencyMatch.cooldownAfterReleaseMs);
    Serial.print(" attack_score_min=");
    Serial.print(selectedProfile.frequencyMatch.attackScoreMin, 1);
    Serial.print(" release_score_min=");
    Serial.print(selectedProfile.frequencyMatch.releaseScoreMin, 1);
    Serial.print(" attack_contrast_min=");
    Serial.print(selectedProfile.frequencyMatch.attackContrastMin, 1);
    Serial.print(" release_contrast_min=");
    Serial.println(selectedProfile.frequencyMatch.releaseContrastMin, 1);

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

    Serial.print("FREQBAND runtime:");
    Serial.print(" freq.window_samples=");
    Serial.print(windowSizeSamples);
    Serial.print(" freq.window_ms=");
    Serial.print(windowMs, 2);
    Serial.print(" freq.update_every_samples=");
    Serial.print(frequencyUpdateEverySamples);
    Serial.print(" freq.update_period_ms=");
    Serial.print(updateStepMs, 3);
    Serial.print(" freq.target_hz=");
    Serial.print(_freqBandStream.targetFrequencyHz());
    Serial.print(" freq.produced_fresh_packet=");
    Serial.print(_freqBandStream.producedFreshPacketOnLastObserve() ? 1 : 0);
    Serial.print(" freq.packet_age_samples=");
    Serial.print(ageSamples);
    Serial.print(" freq.packet_age_ms=");
    Serial.println(ageMs, 3);
}

void AnalyzerApp::printTransientAcceptedDebug(unsigned long now, float strength, unsigned long durationMs) const {
    if (_valMode) {
        return;
    }
    Serial.print("DET transient accepted t=");
    Serial.print(now);
    Serial.print(" dur=");
    Serial.print(durationMs);
    Serial.print(" strength=");
    Serial.println(strength, 1);
}

void AnalyzerApp::printTransientStatsDebug(unsigned long now) const {
    if (_valMode) {
        return;
    }
    const unsigned long elapsedMs = now - _sequenceTest.startedAtMs;
    const unsigned long expectedCount = 0;
    const unsigned long successRate = expectedCount > 0 ? ((_sequenceTest.hits * 100UL) / expectedCount) : 0;

    Serial.print("DET transient stats t=");
    Serial.print(now);
    Serial.print(" hits=");
    Serial.print(_sequenceTest.hits);
    Serial.print(" expected=");
    Serial.print(expectedCount);
    Serial.print(" success=");
    Serial.print(successRate);
    Serial.println("%");
}

void AnalyzerApp::printSequenceSummary() const {
    if (_valMode) {
        return;
    }
    AnalyzerSummary summary = {};
    summary.profileName = activeAnalyzerProfileName();
    summary.trials = _sequenceTest.totalTrials;
    const auto selectMaxReason = [](const unsigned long* counts, AnalyzerReason fallback) {
        unsigned int bestCount = 0;
        AnalyzerReason bestReason = fallback;
        for (size_t i = 0; i <= static_cast<size_t>(AnalyzerReason::Unknown); ++i) {
            if (counts[i] > bestCount) {
                bestCount = counts[i];
                bestReason = static_cast<AnalyzerReason>(i);
            }
        }
        return bestReason;
    };

    const detection::DetectionProfile& selectedProfile = detection::detectionProfileForKind(_sequenceTest.profileKind);
    summary.expected = _sequenceTest.expectedHits;
    summary.early = 0;
    summary.late = _sequenceTest.lateHits;
    summary.miss = _sequenceTest.misses;
    summary.duplicate = _sequenceTest.duplicates > 0 ? 1U : 0U;
    summary.unexpected = _sequenceTest.unexpected;
    summary.rejected = 0;
    summary.ambiguous = 0;
    summary.tooDense = 0;
    summary.invalidAudio = _sequenceTest.invalidAudio;
    summary.startupArtifacts = _sequenceTest.startupArtifacts;
    summary.avgDtMs = _sequenceTest.patternDtCount > 0
        ? static_cast<float>(_sequenceTest.totalPatternDtMs) / static_cast<float>(_sequenceTest.patternDtCount)
        : -1.0f;
    summary.avgDurationMs = _sequenceTest.patternDurationCount > 0
        ? static_cast<float>(_sequenceTest.totalPatternDurationMs) / static_cast<float>(_sequenceTest.patternDurationCount)
        : -1.0f;
    summary.completed = static_cast<unsigned int>(_sequenceTest.completedTrials > 0 ? _sequenceTest.completedTrials : _sequenceTest.currentTrial);
    summary.avgConfidence = summary.completed > 0
        ? _sequenceTest.totalPatternConfidence / static_cast<float>(summary.completed)
        : 0.0f;
    summary.duplicateRate = summary.completed > 0 ? static_cast<float>(summary.duplicate) / static_cast<float>(summary.completed) : 0.0f;
    summary.unexpectedRate = summary.completed > 0 ? static_cast<float>(summary.unexpected) / static_cast<float>(summary.completed) : 0.0f;
    summary.mainMissReason = selectMaxReason(_sequenceTest.missReasonCounts, AnalyzerReason::None);
    summary.mainRejectReason = selectMaxReason(_sequenceTest.rejectReasonCounts, AnalyzerReason::None);

    const long avgDtRounded = summary.avgDtMs >= 0.0f ? static_cast<long>(summary.avgDtMs + 0.5f) : -1L;
    const long avgDurRounded = summary.avgDurationMs >= 0.0f ? static_cast<long>(summary.avgDurationMs + 0.5f) : -1L;

    if (_sequenceTest.outputConfig.verbosity == 0U &&
        _sequenceTest.outputConfig.mode != SeqOutputMode::Explain) {
        Serial.print("SEQ_SUMMARY profile=");
        Serial.print(summary.profileName != nullptr ? summary.profileName : "unknown");
        Serial.print(" trials=");
        Serial.print(summary.trials);
        Serial.print(" expected=");
        Serial.print(summary.expected);
        Serial.print(" miss=");
        Serial.print(summary.miss);
        Serial.print(" duplicate=");
        Serial.print(_sequenceTest.duplicates);
        Serial.print(" fragmented=");
        Serial.print(_sequenceTest.fragmentedAccepted);
        Serial.print(" miss_streak_max=");
        Serial.print(_sequenceTest.longestMissStreak);
        Serial.print(" avg_dt=");
        if (avgDtRounded >= 0) {
            Serial.print(avgDtRounded);
            Serial.print("ms");
        } else {
            Serial.print("-1ms");
        }
        Serial.print(" avg_dur=");
        if (avgDurRounded >= 0) {
            Serial.print(avgDurRounded);
            Serial.print("ms");
        } else {
            Serial.print("-1ms");
        }
        Serial.println();
        return;
    }

    Serial.println();
    Serial.print("SEQ_SUMMARY counts: expected=");
    Serial.print(summary.expected);
    Serial.print(" early=");
    Serial.print(summary.early);
    Serial.print(" late=");
    Serial.print(summary.late);
    Serial.print(" miss=");
    Serial.print(summary.miss);
    Serial.print(" duplicate=");
    Serial.print(summary.duplicate);
    Serial.print(" unexpected=");
    Serial.print(summary.unexpected);
    Serial.print(" rejected=");
    Serial.print(summary.rejected);
    Serial.print(" ambiguous=");
    Serial.print(summary.ambiguous);
    Serial.print(" too_dense=");
    Serial.print(summary.tooDense);
    Serial.print(" invalid_audio=");
    Serial.print(summary.invalidAudio);
    Serial.print(" startup_artifacts=");
    Serial.println(summary.startupArtifacts);

    Serial.print("  metadata: profile=");
    Serial.print(summary.profileName != nullptr ? summary.profileName : "unknown");
    Serial.print(" source=");
    Serial.print(selectedProfile.occurrenceSource == detection::OccurrenceSourceKind::FrequencyMatch
        ? "FrequencyMatchSource"
        : "ScalarTransientSource");
    Serial.print(" detector=");
    Serial.print(selectedProfile.occurrenceSource == detection::OccurrenceSourceKind::FrequencyMatch
        ? "FrequencyMatchSource"
        : "ScalarTransientSource");
    Serial.print(" required_support_target=");
    Serial.print(supportTargetDisplayName(
        selectedProfile.patternRulesConfig.requiredSupportTarget,
        selectedProfile.patternRulesConfig.requireSupportForAcceptance
    ));
    Serial.print(" support_gate=");
    Serial.print(selectedProfile.patternRulesConfig.requireSupportForAcceptance ? "enabled" : "disabled");
    Serial.print(" diag_mode=");
    Serial.print(sequenceDiagModeName(_sequenceTest.diagMode));
    Serial.print(" trials=");
    Serial.print(summary.trials);
    Serial.print(" completed=");
    Serial.print(summary.completed);
    Serial.print(" avg_pattern_confidence=");
    Serial.print(summary.avgConfidence, 2);
    Serial.print(" avg_dt=");
    if (avgDtRounded >= 0) {
        Serial.print(avgDtRounded);
        Serial.print("ms");
    } else {
        Serial.print("-1ms");
    }
    Serial.print(" avg_dur=");
    if (avgDurRounded >= 0) {
        Serial.print(avgDurRounded);
        Serial.print("ms");
    } else {
        Serial.print("-1ms");
    }
    Serial.println();

    Serial.print("  details: A_rates: duplicate=");
    Serial.print(summary.duplicateRate, 2);
    Serial.print(" unexpected=");
    Serial.print(summary.unexpectedRate, 2);
    Serial.print(" main_miss_reason=");
    Serial.print(analyzerReasonName(summary.mainMissReason));
    Serial.print(" main_reject_reason=");
    Serial.println(analyzerReasonName(summary.mainRejectReason));

    Serial.print("  miss_reason_counts: none=");
    Serial.print(_sequenceTest.missReasonCounts[analyzerReasonIndex(AnalyzerReason::None)]);
    Serial.print(" result_expected_window=");
    Serial.print(_sequenceTest.missReasonCounts[analyzerReasonIndex(AnalyzerReason::ValidPatternInExpectedWindow)]);
    Serial.print(" result_before_window=");
    Serial.print(_sequenceTest.missReasonCounts[analyzerReasonIndex(AnalyzerReason::ValidPatternBeforeWindow)]);
    Serial.print(" result_after_window=");
    Serial.print(_sequenceTest.missReasonCounts[analyzerReasonIndex(AnalyzerReason::ValidPatternAfterWindow)]);
    Serial.print(" no_occurrence=");
    Serial.print(_sequenceTest.missReasonCounts[analyzerReasonIndex(AnalyzerReason::NoOccurrence)]);
    Serial.print(" occ_rejected=");
    Serial.print(_sequenceTest.missReasonCounts[analyzerReasonIndex(AnalyzerReason::OccurrenceSeenButRejected)]);
    Serial.print(" inspection_failed=");
    Serial.print(_sequenceTest.missReasonCounts[analyzerReasonIndex(AnalyzerReason::InspectionFailed)]);
    Serial.print(" pattern_rejected=");
    Serial.print(_sequenceTest.missReasonCounts[analyzerReasonIndex(AnalyzerReason::PatternCandidateRejected)]);
    Serial.println();

    Serial.print("  freq_evidence_class_counts: accepted=");
    Serial.print(_sequenceTest.freqEvidenceClassCounts[0]);
    Serial.print(" strong_no_occurrence=");
    Serial.print(_sequenceTest.freqEvidenceClassCounts[1]);
    Serial.print(" partial=");
    Serial.print(_sequenceTest.freqEvidenceClassCounts[2]);
    Serial.print(" weak=");
    Serial.print(_sequenceTest.freqEvidenceClassCounts[3]);
    Serial.print(" none=");
    Serial.println(_sequenceTest.freqEvidenceClassCounts[4]);

    Serial.print("  reject_reason_counts: dup_after_primary=");
    Serial.print(_sequenceTest.rejectReasonCounts[analyzerReasonIndex(AnalyzerReason::DuplicatePatternAfterPrimary)]);
    Serial.print(" unexpected_without_trigger=");
    Serial.print(_sequenceTest.rejectReasonCounts[analyzerReasonIndex(AnalyzerReason::UnexpectedValidPatternWithoutTrigger)]);
    Serial.print(" invalid_audio=");
    Serial.println(_sequenceTest.rejectReasonCounts[analyzerReasonIndex(AnalyzerReason::InvalidAudio)]);

    Serial.print("  longest_miss_streak=");
    Serial.print(_sequenceTest.longestMissStreak);
    Serial.print(" first_miss_trial=");
    Serial.println(_sequenceTest.firstMissTrial);

    if (_sequenceTest.outputConfig.verbosity > 0 || _sequenceTest.outputConfig.mode == SeqOutputMode::Explain) {
        Serial.print("  A_profile_summary: matched_expected=");
        Serial.print(_sequenceTest.patternMatchedExpected);
        Serial.print(" unmatched_expected=");
        Serial.print(_sequenceTest.patternUnmatchedExpected);
        Serial.print(" matched_duplicates=");
        Serial.print(_sequenceTest.patternMatchedDuplicates);
        Serial.print(" unmatched_duplicates=");
        Serial.print(_sequenceTest.patternUnmatchedDuplicates);
        Serial.print(" matched_unexpected=");
        Serial.print(_sequenceTest.patternMatchedUnexpected);
        Serial.print(" unmatched_unexpected=");
        Serial.print(_sequenceTest.patternUnmatchedUnexpected);
        Serial.print(" freq_reject_score=");
        Serial.print(_sequenceTest.freqRejectScore);
        Serial.print(" freq_reject_contrast=");
        Serial.print(_sequenceTest.freqRejectContrast);
        Serial.print(" freq_reject_both=");
        Serial.print(_sequenceTest.freqRejectBoth);
        Serial.print(" freq_reject_no_evidence=");
        Serial.print(_sequenceTest.freqRejectNoEvidence);
        Serial.println();
    }
    if (_sequenceTest.showDetails) {
        printDetectionParameters();
    }
    if (_sequenceTest.outputConfig.verbosity > 0U || _sequenceTest.outputConfig.mode == SeqOutputMode::Explain) {
        printAudioSourceSummary();
        printOccurrenceSummary();
    }
}

void AnalyzerApp::printSequenceFinalOutput() const {
    if (_valMode) {
        return;
    }
    printSequenceSummary();
    if (_sequenceTest.outputConfig.verbosity > 0U ||
        _sequenceTest.outputConfig.mode == SeqOutputMode::System ||
        _sequenceTest.outputConfig.mode == SeqOutputMode::Explain) {
        printAudioRunSummary();
    }
}

void AnalyzerApp::printSequenceStatus() const {
    if (_valMode) {
        return;
    }

    Serial.print("SEQ_STATUS mode=");
    Serial.print(sequenceOutputModeName(_sequenceTest.outputConfig.mode));
    Serial.print(" when=");
    Serial.print(sequenceOutputWhenName(_sequenceTest.outputConfig.when));
    Serial.print(" verbosity=");
    Serial.print(_sequenceTest.outputConfig.verbosity);
    Serial.print(" profile=");
    Serial.print(_sequenceTest.active ? activeAnalyzerProfileName() : detection::detectionProfileName(_seqOutputConfig.profileKind));
    Serial.print(" tries=");
    Serial.print(_seqOutputConfig.totalTrials);
    Serial.print(" diagnostics=");
    Serial.print(_seqOutputConfig.diagnosticsEnabled ? "on" : "off");
    Serial.print(" freqband=");
    Serial.print(_seqOutputConfig.frequencyBandEnabled ? "on" : "off");
    Serial.print(" freqUpdateEverySamples=");
    Serial.print(_seqOutputConfig.frequencyUpdateEverySamples);
    Serial.println();
}

void AnalyzerApp::printSignalCheck() const {
    if (_valMode) {
        return;
    }
    if (!_sequenceTest.outputConfig.diagnosticsEnabled) {
        return;
    }

    const auto& diagnostics = _sequenceTest.currentTrialDiagnostics;
    if (_sequenceTest.currentTrial == 0 && diagnostics.audioFrames == 0) {
        Serial.println("SIGNALCHECK state=idle");
        return;
    }

    const char* audioHealth = "ok";
    if (diagnostics.audioRmsTooHighFrames > 0) {
        audioHealth = "clipped";
    } else if (diagnostics.audioLargeJumpFrames > 0) {
        audioHealth = "glitchy";
    } else if (diagnostics.audioFlatlineFrames > 0) {
        audioHealth = "flatline";
    } else if (diagnostics.audioZeroishFrames > 0 || diagnostics.audioRmsTooLowFrames > 0) {
        audioHealth = "zeroish";
    }

    Serial.print("SIGNALCHECK");
    Serial.print(" trial=");
    Serial.print(_sequenceTest.currentTrial);
    Serial.print(" audio_health=");
    Serial.print(audioHealth);
    Serial.print(" zeroish_frames=");
    Serial.print(diagnostics.audioZeroishFrames);
    Serial.print(" flatline_frames=");
    Serial.print(diagnostics.audioFlatlineFrames);
    Serial.print(" large_jump_frames=");
    Serial.print(diagnostics.audioLargeJumpFrames);
    Serial.print(" rms_too_low_frames=");
    Serial.print(diagnostics.audioRmsTooLowFrames);
    Serial.print(" rms_too_high_frames=");
    Serial.print(diagnostics.audioRmsTooHighFrames);
    Serial.print(" max_abs_delta=");
    Serial.print(diagnostics.audioMaxAbsDelta);
    Serial.print(" rms=");
    Serial.print(diagnostics.audioRms, 1);
    const double rawFrameCount = diagnostics.rawFrames > 0 ? static_cast<double>(diagnostics.rawFrames) : 0.0;
    const double rawMean = rawFrameCount > 0.0 ? static_cast<double>(diagnostics.rawSum) / rawFrameCount : 0.0;
    const double rawMeanAbs = rawFrameCount > 0.0 ? static_cast<double>(diagnostics.rawAbsSum) / rawFrameCount : 0.0;
    const double rawMinAbs = diagnostics.rawMin < 0 ? -static_cast<double>(diagnostics.rawMin) : static_cast<double>(diagnostics.rawMin);
    const double rawMaxAbsValue = diagnostics.rawMax < 0 ? -static_cast<double>(diagnostics.rawMax) : static_cast<double>(diagnostics.rawMax);
    const double rawMaxAbs = rawMinAbs > rawMaxAbsValue ? rawMinAbs : rawMaxAbsValue;
    const double rawRange = diagnostics.rawFrames > 0 && diagnostics.rawMax >= diagnostics.rawMin
        ? static_cast<double>(diagnostics.rawMax - diagnostics.rawMin)
        : 0.0;
    // Range-based proxy keeps the trial footprint small; exact variance would need another accumulator.
    const double rawSpreadEst = rawRange > 0.0 ? rawRange / 3.4641016151377544 : 0.0;
    const double rawZeroCrossRate = diagnostics.rawFrames > 1
        ? static_cast<double>(diagnostics.rawZeroCrossings) / static_cast<double>(diagnostics.rawFrames - 1U)
        : 0.0;
    const double rawSameValueRatio = diagnostics.rawFrames > 1
        ? static_cast<double>(diagnostics.rawSameValueCount) / static_cast<double>(diagnostics.rawFrames - 1U)
        : 0.0;
    const char* rawHealthClass = rawHealthClassNameFromCounters(
        diagnostics.audioRmsTooHighFrames,
        diagnostics.rawFrames,
        static_cast<unsigned long>(rawMaxAbs),
        static_cast<float>(rawMean),
        static_cast<float>(rawMeanAbs),
        diagnostics.rawMin,
        diagnostics.rawMax,
        static_cast<float>(rawSameValueRatio),
        static_cast<unsigned long>(diagnostics.rawSameValueMaxRun),
        static_cast<unsigned long>(diagnostics.rawBlockHashRepeatCount),
        diagnostics.audioFlatlineFrames,
        diagnostics.audioZeroishFrames,
        diagnostics.audioLargeJumpFrames,
        diagnostics.audioRms,
        diagnostics.audioRmsTooLowFrames,
        diagnostics.audioRmsTooHighFrames
    );
    Serial.print(" raw_min=");
    Serial.print(diagnostics.rawFrames > 0 ? diagnostics.rawMin : 0);
    Serial.print(" raw_max=");
    Serial.print(diagnostics.rawFrames > 0 ? diagnostics.rawMax : 0);
    Serial.print(" raw_mean=");
    Serial.print(rawMean, 1);
    Serial.print(" raw_spread_est=");
    Serial.print(rawSpreadEst, 1);
    Serial.print(" raw_mean_abs=");
    Serial.print(rawMeanAbs, 1);
    Serial.print(" dc_offset=");
    Serial.print(rawMean, 1);
    Serial.print(" zero_cross_rate=");
    Serial.print(rawZeroCrossRate, 3);
    Serial.print(" same_value_ratio=");
    Serial.print(rawSameValueRatio, 3);
    Serial.print(" repeated_sample_max_run=");
    Serial.print(static_cast<unsigned int>(diagnostics.rawSameValueMaxRun));
    Serial.print(" block_hash_repeat_count=");
    Serial.print(static_cast<unsigned int>(diagnostics.rawBlockHashRepeatCount));
    Serial.print(" raw_health_class=");
    Serial.print(rawHealthClass != nullptr ? rawHealthClass : "unknown");
    Serial.println();
}

void AnalyzerApp::printBaseSummary() const {
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
    printBaseHints();
    printAudioSourceSummary();
    printOccurrenceSummary();
}

void AnalyzerApp::printBaseHints() const {
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

void AnalyzerApp::printCaptureSummary() const {
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
    printCaptureHints();
    printAudioSourceSummary();
    printOccurrenceSummary();
}

void AnalyzerApp::printCaptureHints() const {
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

void AnalyzerApp::printAudioSourceSummary() const {
    const AudioSourceStats& stats = _audioSource.stats();
    Serial.println("AUDIO summary:");
    Serial.print("reads=");
    Serial.print(stats.reads);
    Serial.print(" readBytes=");
    Serial.print(stats.readBytes);
    Serial.print(" zeroReads=");
    Serial.print(stats.zeroReads);
    Serial.print(" shortReads=");
    Serial.print(stats.shortReads);
    Serial.print(" maxReadBytes=");
    Serial.print(stats.maxReadBytes);
    Serial.print(" noSampleLoops=");
    Serial.print(stats.noSampleLoops);
    Serial.print(" readErrors=");
    Serial.print(stats.readErrors);
    Serial.print(" overflow=");
    Serial.print(stats.overflowCount);
    Serial.print(" droppedBlocks=");
    Serial.print(stats.droppedBlockCount);
    Serial.print(" totalSamples=");
    Serial.println(static_cast<unsigned long long>(stats.totalSamplesRead));
}

void AnalyzerApp::printAudioRunSummary() const {
    if (!_sequenceTest.active && _sequenceTest.completedTrials == 0) {
        return;
    }

    const unsigned long now = millis();
    const unsigned long sampleRateHz = _audioSource.sampleRateHz() > 0 ? _audioSource.sampleRateHz() : 16000UL;
    const unsigned long runElapsedMs = _sequenceTest.startedAtMs > 0
        ? (now >= _sequenceTest.startedAtMs ? now - _sequenceTest.startedAtMs : 0UL)
        : 0UL;
    const unsigned long expectedSamples = static_cast<unsigned long>(
        (static_cast<unsigned long long>(runElapsedMs) * static_cast<unsigned long long>(sampleRateHz)) / 1000ULL);
    const unsigned long processedSamples = _sequenceTest.samplesProcessed;
    const float processedRatio = expectedSamples > 0
        ? static_cast<float>(processedSamples) / static_cast<float>(expectedSamples)
        : 0.0f;
    const unsigned long avgAvailableBytes = _sequenceTest.availableBytesSamples > 0
        ? static_cast<unsigned long>(_sequenceTest.availableBytesSum / static_cast<uint64_t>(_sequenceTest.availableBytesSamples))
        : 0UL;

    Serial.println("AUDIO run:");
    Serial.print("run_elapsed_ms=");
    Serial.print(runElapsedMs);
    Serial.print(" expected_samples=");
    Serial.print(expectedSamples);
    Serial.print(" processed_samples=");
    Serial.print(processedSamples);
    Serial.print(" processed_ratio=");
    Serial.print(processedRatio, 3);
    Serial.print(" max_available_bytes=");
    Serial.print(_sequenceTest.maxAvailableBytes);
    Serial.print(" avg_available_bytes=");
    Serial.print(avgAvailableBytes);
    Serial.print(" max_block_age_ms=");
    Serial.print(_sequenceTest.maxBlockAgeMs);
    Serial.print(" max_update_loop_us=");
    Serial.print(_sequenceTest.maxUpdateLoopUs);
    Serial.print(" max_processing_lag_ms=");
    Serial.println(_sequenceTest.maxProcessingLagMs);

    Serial.print("FREQBAND config:");
    Serial.print(" freqband=");
    Serial.print(_seqOutputConfig.frequencyBandEnabled ? "on" : "off");
    Serial.print(" updateEverySamples=");
    Serial.println(_seqOutputConfig.frequencyUpdateEverySamples);

    const unsigned long freqObserveCalls = _freqBandStream.profileObserveCalls();
    const unsigned long freqComputeCalls = _freqBandStream.profileComputeCalls();
    const float avgObserveUs = freqObserveCalls > 0
        ? static_cast<float>(_freqBandStream.profileObserveTotalUs()) / static_cast<float>(freqObserveCalls)
        : 0.0f;
    const float avgComputeUs = freqComputeCalls > 0
        ? static_cast<float>(_freqBandStream.profileComputeTotalUs()) / static_cast<float>(freqComputeCalls)
        : 0.0f;
    const float avgEnergyUs = freqComputeCalls > 0
        ? static_cast<float>(_freqBandStream.profileEnergyTotalUs()) / static_cast<float>(freqComputeCalls)
        : 0.0f;
    const float avgGoertzelUs = freqComputeCalls > 0
        ? static_cast<float>(_freqBandStream.profileGoertzelTotalUs()) / static_cast<float>(freqComputeCalls)
        : 0.0f;

    Serial.print("FREQBAND profile:");
    Serial.print(" observe_calls=");
    Serial.print(freqObserveCalls);
    Serial.print(" avg_observe_us=");
    Serial.print(avgObserveUs, 2);
    Serial.print(" compute_calls=");
    Serial.print(freqComputeCalls);
    Serial.print(" avg_compute_us=");
    Serial.print(avgComputeUs, 2);
    Serial.print(" avg_energy_us=");
    Serial.print(avgEnergyUs, 2);
    Serial.print(" avg_goertzel_us=");
    Serial.println(avgGoertzelUs, 2);

    const unsigned long freqFreshObserveCalls = _freqBandStream.profileComputeCalls();
    const unsigned long freqHeldObserveCalls = _freqBandStream.profileObserveCalls() > _freqBandStream.profileComputeCalls()
        ? _freqBandStream.profileObserveCalls() - _freqBandStream.profileComputeCalls()
        : 0UL;
    const unsigned long freqAgeSamples = _freqBandStream.lastPacketAgeSamples();
    const unsigned long freqComputedAtSample = _freqBandStream.sampleCount() >= freqAgeSamples
        ? _freqBandStream.sampleCount() - freqAgeSamples
        : 0UL;
    unsigned long freqHistoryScoreRecords = 0;
    unsigned long freqHistoryContrastRecords = 0;
    freqHistoryScoreRecords = _detection.featureHistory().sampleCount(detection::FeatureStreamId::FrequencyScore);
    freqHistoryContrastRecords = _detection.featureHistory().sampleCount(detection::FeatureStreamId::FrequencyContrast);

    Serial.print("FREQBAND freshness:");
    Serial.print(" fresh_frames=");
    Serial.print(freqFreshObserveCalls);
    Serial.print(" held_frames=");
    Serial.print(freqHeldObserveCalls);
    Serial.print(" age_samples=");
    Serial.print(freqAgeSamples);
    Serial.print(" computed_at_sample=");
    Serial.print(freqComputedAtSample);
    Serial.print(" history_score_records=");
    Serial.print(freqHistoryScoreRecords);
    Serial.print(" history_contrast_records=");
    Serial.println(freqHistoryContrastRecords);
}

void AnalyzerApp::printOccurrenceSummary() const {
    const AudioSignalStats& stats = _audioSignal.stats();
    Serial.println("OCCURRENCE summary:");
    Serial.print("blocks=");
    Serial.print(stats.blocksProcessed);
    Serial.print(" samples=");
    Serial.print(static_cast<unsigned long long>(stats.samplesProcessed));
    Serial.print(" lastBlockStart=");
    Serial.print(static_cast<unsigned long long>(_audioSignal.lastBlockStartSample()));
    Serial.print(" lastBlockCount=");
    Serial.print(_audioSignal.lastBlockSampleCount());
    Serial.print(" lastBlockMicros=");
    Serial.println(_audioSignal.lastBlockApproxStartMicros());
}

void AnalyzerApp::printValueFrame(unsigned long now) const {
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

