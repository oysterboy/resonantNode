/*
The detector report printers on the host (critique D4.3): the two lines the Analyzer prints per SEQ trial
from the DetectorReport, SEQ_SOURCE_CORE (printDetectorReportGenericLine) and SEQ_SOURCE_SPEC
(printDetectorDetailLine -> printScalarTransientDetailLine). The bench tools read them generically:
benchlib.line_type() keys a line by its prefix plus a space, parse_run_log() splits it with
KV = (\S+?)=(\S*), and seqcmp diffs the key set of each line type between two runs. Pinned here: one line
per call, prefix first, every token after it a k=v pair with no space in the value, the keys in this
order, and a few values in the formatting the logs carry (floats to 1 or 2 places, flags as 0/1, a null
reason as "none"). A new or renamed key is a deliberate change: update the list here and say so in the
commit, since seqcmp will flag it against older bench runs.

Output goes through serialOut(), which is stdout on the host (HOST_REPLAY); the test points fd 1 at a
temp file around each call. Runs in native-test-behavior.
*/
#include <unity.h>

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <algorithm>
#include <string>
#include <vector>

#include "../../src/detection/detectors/DetectorNames.cpp"
#include "../../src/modes/analyzer/report/DetectorReportPrinter.cpp"
#include "../../src/modes/analyzer/report/ScalarTransientPrinter.cpp"

using namespace detection;

namespace {

// What fn() writes to stdout.
template <typename Fn>
std::string capture(Fn fn) {
    fflush(stdout);
    FILE* tmp = tmpfile();
    TEST_ASSERT_NOT_NULL_MESSAGE(tmp, "tmpfile");
    const int saved = dup(1);
    dup2(fileno(tmp), 1);
    fn();
    fflush(stdout);
    dup2(saved, 1);
    close(saved);
    std::string out;
    rewind(tmp);
    char buf[512];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), tmp)) > 0) {
        out.append(buf, n);
    }
    fclose(tmp);
    return out;
}

std::vector<std::string> tokens(const std::string& line) {
    std::vector<std::string> out;
    size_t i = 0;
    while (i < line.size()) {
        const size_t j = line.find(' ', i);
        const size_t end = j == std::string::npos ? line.size() : j;
        if (end > i) {
            out.push_back(line.substr(i, end - i));
        }
        i = end + 1;
    }
    return out;
}

// The line must be "<prefix> k=v k=v ...\n"; returns the keys and fills values.
std::vector<std::string> keysOf(const std::string& out, const char* prefix, std::vector<std::string>& values) {
    TEST_ASSERT_TRUE_MESSAGE(!out.empty() && out.back() == '\n', "one line, newline-terminated");
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, static_cast<int>(std::count(out.begin(), out.end(), '\n')), "one line");
    const std::string line = out.substr(0, out.size() - 1);
    TEST_ASSERT_EQUAL_INT(0, static_cast<int>(line.find(std::string(prefix) + " ")));
    const std::vector<std::string> t = tokens(line);
    std::vector<std::string> keys;
    for (size_t i = 1; i < t.size(); ++i) {
        const size_t eq = t[i].find('=');
        TEST_ASSERT_TRUE_MESSAGE(eq != std::string::npos && eq > 0, t[i].c_str());
        keys.push_back(t[i].substr(0, eq));
        values.push_back(t[i].substr(eq + 1));
    }
    return keys;
}

std::string valueOf(const std::vector<std::string>& keys, const std::vector<std::string>& values, const char* key) {
    for (size_t i = 0; i < keys.size(); ++i) {
        if (keys[i] == key) {
            return values[i];
        }
    }
    return "<missing>";
}

void assertKeys(const char* const* expected, size_t n, const std::vector<std::string>& keys) {
    TEST_ASSERT_EQUAL_INT_MESSAGE(static_cast<int>(n), static_cast<int>(keys.size()), "key count");
    for (size_t i = 0; i < n; ++i) {
        TEST_ASSERT_EQUAL_STRING(expected[i], keys[i].c_str());
    }
}

const char* const kCoreKeys[] = {
    "trial", "detector", "source.selection", "source.occurrence_id", "source.candidate_id", "source.report_matched",
    "window.start_ms", "window.end_ms", "accepted.present", "accepted.start_ms", "accepted.peak_ms",
    "accepted.end_ms", "accepted.duration_ms", "accepted.strength", "accepted.confidence", "accepted.peak",
    "accepted.mean", "accepted.rms", "accepted.coverage_attack_ms", "accepted.coverage_release_ms",
    "accepted.sustained_ms", "accepted.island_count", "accepted.gap_count", "accepted.island_max_ms",
    "accepted.gap_max_ms", "reject.present", "reject.class", "reject.detector_reason", "reject.start_ms",
    "reject.peak_ms", "reject.end_ms", "reject.duration_ms", "reject.strength", "reject.confidence", "reject.peak",
    "reject.mean", "reject.rms", "candidate.mean_strength", "candidate.min_strength", "candidate.strength_count",
    "candidate.matched_strength_count", "reject.coverage_attack_ms", "reject.coverage_release_ms",
    "reject.sustained_ms", "reject.island_count", "reject.gap_count", "reject.island_max_ms", "reject.gap_max_ms",
    "carrier.quality_pass", "candidate.min_strength_pass", "threshold.min_duration_ms", "threshold.max_duration_ms",
    "aggregate.accepted_count", "aggregate.rejected_count",
};

const char* const kSpecKeys[] = {
    "detail.scalar.accepted.value", "detail.scalar.accepted.baseline", "detail.scalar.accepted.lift",
    "detail.scalar.accepted.normalized", "detail.scalar.reject.value", "detail.scalar.reject.baseline",
    "detail.scalar.reject.lift", "detail.scalar.reject.normalized", "detail.scalar.reject.opened",
    "detail.scalar.reject.crossed_onset", "detail.scalar.reject.crossed_release", "detail.scalar.threshold.onset",
    "detail.scalar.threshold.release", "detail.scalar.threshold.min_strength", "detail.scalar.aggregate.too_short",
    "detail.scalar.aggregate.too_long", "detail.scalar.aggregate.strength_too_low",
    "detail.scalar.aggregate.max_rejected_lift", "detail.scalar.aggregate.best_rejected_value",
    "detail.scalar.inspect.reject_reason", "detail.scalar.inspect.no_emit_reason",
    "detail.scalar.inspect.gate_reason", "detail.scalar.inspect.min_strength_pass",
    "detail.scalar.inspect.carrier_quality_required", "detail.scalar.inspect.carrier_coverage_passed",
    "detail.scalar.inspect.carrier_island_passed", "detail.scalar.inspect.carrier_gap_passed",
    "detail.scalar.inspect.opened", "detail.scalar.inspect.released", "detail.scalar.inspect.valid_release",
    "detail.scalar.inspect.emit_allowed", "detail.scalar.inspect.open_ms", "detail.scalar.inspect.peak_ms",
    "detail.scalar.inspect.release_ms", "detail.scalar.inspect.duration_ms", "detail.scalar.inspect.peak_strength",
};

DetectorReport acceptedReport() {
    DetectorReport r;
    r.detectorId = DetectorId::ScalarTransient;
    r.sourceSelection = "accepted";
    r.sourceOccurrenceId = 7;
    r.reportStartMs = 1000;
    r.reportEndMs = 1400;
    r.accepted.present = true;
    r.accepted.startMs = 1059;
    r.accepted.durationMs = 201;
    r.accepted.strength = 9083.0f;
    r.accepted.confidence = 0.857f;
    r.accepted.meanStrength = 4321.3f;  // no half-way value: the shim (printf) and Arduino's Print round those differently
    r.accepted.strengthCount = 12;
    r.scalar.accepted.normalized = 0.456f;
    r.scalar.inspect.matchedMeanPassed = true;
    r.scalar.inspect.peakStrength = 9083.0f;
    r.aggregates.acceptedCount = 1;
    return r;
}

DetectorReport rejectReport() {
    DetectorReport r;
    r.detectorId = DetectorId::ScalarTransient;
    r.selectedReject.present = true;
    r.selectedReject.rejectClass = DetectorRejectClass::Timing;
    r.selectedReject.detectorReason = "too_long";
    r.selectedReject.durationMs = 412;
    r.selectedReject.meanStrength = 1500.5f;
    r.scalar.inspect.carrierQualityRequired = true;
    r.scalar.inspect.carrierGapPassed = false;
    r.aggregates.rejectedCount = 3;
    return r;
}

}  // namespace

void setUp() {}
void tearDown() {}

void test_core_line_accepted() {
    const DetectorReport r = acceptedReport();
    const std::string out = capture([&] { printDetectorReportGenericLine("SEQ_SOURCE_CORE", 42, &r); });
    std::vector<std::string> v;
    const std::vector<std::string> k = keysOf(out, "SEQ_SOURCE_CORE", v);
    assertKeys(kCoreKeys, sizeof(kCoreKeys) / sizeof(kCoreKeys[0]), k);
    TEST_ASSERT_EQUAL_STRING("42", valueOf(k, v, "trial").c_str());
    TEST_ASSERT_EQUAL_STRING("scalar_transient", valueOf(k, v, "detector").c_str());
    TEST_ASSERT_EQUAL_STRING("accepted", valueOf(k, v, "source.selection").c_str());
    TEST_ASSERT_EQUAL_STRING("1", valueOf(k, v, "accepted.present").c_str());
    TEST_ASSERT_EQUAL_STRING("201", valueOf(k, v, "accepted.duration_ms").c_str());
    TEST_ASSERT_EQUAL_STRING("9083.0", valueOf(k, v, "accepted.strength").c_str());
    TEST_ASSERT_EQUAL_STRING("0.86", valueOf(k, v, "accepted.confidence").c_str());
    TEST_ASSERT_EQUAL_STRING("none", valueOf(k, v, "reject.class").c_str());
    TEST_ASSERT_EQUAL_STRING("none", valueOf(k, v, "reject.detector_reason").c_str());
    // candidate.* follow the accepted candidate when there is one.
    TEST_ASSERT_EQUAL_STRING("4321.3", valueOf(k, v, "candidate.mean_strength").c_str());
    TEST_ASSERT_EQUAL_STRING("12", valueOf(k, v, "candidate.strength_count").c_str());
    TEST_ASSERT_EQUAL_STRING("1", valueOf(k, v, "carrier.quality_pass").c_str());
    TEST_ASSERT_EQUAL_STRING("1", valueOf(k, v, "candidate.min_strength_pass").c_str());
}

void test_core_line_reject() {
    const DetectorReport r = rejectReport();
    const std::string out = capture([&] { printDetectorReportGenericLine("SEQ_SOURCE_CORE", 3, &r); });
    std::vector<std::string> v;
    const std::vector<std::string> k = keysOf(out, "SEQ_SOURCE_CORE", v);
    assertKeys(kCoreKeys, sizeof(kCoreKeys) / sizeof(kCoreKeys[0]), k);
    TEST_ASSERT_EQUAL_STRING("0", valueOf(k, v, "accepted.present").c_str());
    TEST_ASSERT_EQUAL_STRING("1", valueOf(k, v, "reject.present").c_str());
    TEST_ASSERT_EQUAL_STRING("timing", valueOf(k, v, "reject.class").c_str());
    TEST_ASSERT_EQUAL_STRING("too_long", valueOf(k, v, "reject.detector_reason").c_str());
    TEST_ASSERT_EQUAL_STRING("412", valueOf(k, v, "reject.duration_ms").c_str());
    // candidate.* follow the reject when nothing was accepted.
    TEST_ASSERT_EQUAL_STRING("1500.5", valueOf(k, v, "candidate.mean_strength").c_str());
    TEST_ASSERT_EQUAL_STRING("0", valueOf(k, v, "carrier.quality_pass").c_str());  // required, gap failed
    TEST_ASSERT_EQUAL_STRING("3", valueOf(k, v, "aggregate.rejected_count").c_str());
}

// No report (no trial source): the same keys with empty values, not a shorter line.
void test_core_line_without_report_keeps_its_keys() {
    const std::string out = capture([] { printDetectorReportGenericLine("SEQ_SOURCE_CORE", 9, nullptr); });
    std::vector<std::string> v;
    const std::vector<std::string> k = keysOf(out, "SEQ_SOURCE_CORE", v);
    assertKeys(kCoreKeys, sizeof(kCoreKeys) / sizeof(kCoreKeys[0]), k);
    TEST_ASSERT_EQUAL_STRING("unknown", valueOf(k, v, "detector").c_str());
    TEST_ASSERT_EQUAL_STRING("none", valueOf(k, v, "source.selection").c_str());
}

void test_spec_line_scalar() {
    const DetectorReport r = rejectReport();
    const std::string out = capture([&] { printDetectorDetailLine("SEQ_SOURCE_SPEC", &r); });
    std::vector<std::string> v;
    const std::vector<std::string> k = keysOf(out, "SEQ_SOURCE_SPEC", v);
    assertKeys(kSpecKeys, sizeof(kSpecKeys) / sizeof(kSpecKeys[0]), k);
    // The selected reject's reason fills the three inspect reasons.
    TEST_ASSERT_EQUAL_STRING("too_long", valueOf(k, v, "detail.scalar.inspect.reject_reason").c_str());
    TEST_ASSERT_EQUAL_STRING("too_long", valueOf(k, v, "detail.scalar.inspect.gate_reason").c_str());
    TEST_ASSERT_EQUAL_STRING("1", valueOf(k, v, "detail.scalar.inspect.carrier_quality_required").c_str());
    TEST_ASSERT_EQUAL_STRING("0", valueOf(k, v, "detail.scalar.inspect.carrier_gap_passed").c_str());

    const DetectorReport a = acceptedReport();
    const std::string outA = capture([&] { printDetectorDetailLine("SEQ_SOURCE_SPEC", &a); });
    std::vector<std::string> va;
    const std::vector<std::string> ka = keysOf(outA, "SEQ_SOURCE_SPEC", va);
    TEST_ASSERT_EQUAL_STRING("none", valueOf(ka, va, "detail.scalar.inspect.reject_reason").c_str());
    TEST_ASSERT_EQUAL_STRING("0.46", valueOf(ka, va, "detail.scalar.accepted.normalized").c_str());
    TEST_ASSERT_EQUAL_STRING("9083.0", valueOf(ka, va, "detail.scalar.inspect.peak_strength").c_str());
}

// No report, or another detector family: no SEQ_SOURCE_SPEC line at all.
void test_spec_line_absent_without_scalar_report() {
    TEST_ASSERT_EQUAL_STRING("", capture([] { printDetectorDetailLine("SEQ_SOURCE_SPEC", nullptr); }).c_str());
    DetectorReport unknown;
    TEST_ASSERT_EQUAL_STRING("", capture([&] { printDetectorDetailLine("SEQ_SOURCE_SPEC", &unknown); }).c_str());
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_core_line_accepted);
    RUN_TEST(test_core_line_reject);
    RUN_TEST(test_core_line_without_report_keeps_its_keys);
    RUN_TEST(test_spec_line_scalar);
    RUN_TEST(test_spec_line_absent_without_scalar_report);
    return UNITY_END();
}
