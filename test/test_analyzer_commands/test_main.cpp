/*
Host test for the Analyzer console's dispatch and token checks (step 11j item 6,
src/modes/analyzer/AnalyzerCommandParse.h). The handlers are AnalyzerApp members
that need the board (I2S, Serial2, the Emitter link), so the host reaches the pure
part: which handler a line goes to, and the on/off, band and hop checks RAW and SEQ
share. Runs in native-test-behavior.
*/
#include <unity.h>

#include "../../src/modes/analyzer/AnalyzerCommandParse.h"

using analyzer_commands::Group;
using analyzer_commands::groupOf;

void setUp() {}
void tearDown() {}

void test_each_group_is_reached_by_its_verb_in_any_case() {
    TEST_ASSERT_TRUE(groupOf("HELP") == Group::Help);
    TEST_ASSERT_TRUE(groupOf("help") == Group::Help);
    TEST_ASSERT_TRUE(groupOf("SELFTEST") == Group::SelfTest);
    TEST_ASSERT_TRUE(groupOf("selftest chirp n=5") == Group::SelfTest);
    TEST_ASSERT_TRUE(groupOf("OTA url=http://10.0.0.2:8070/a.bin") == Group::Ota);
    TEST_ASSERT_TRUE(groupOf("PARAM SET detection.scalar_onset 2000") == Group::Param);
    TEST_ASSERT_TRUE(groupOf("raw trigger f=3200 dur=100") == Group::Raw);
    TEST_ASSERT_TRUE(groupOf("SEQ start 10 period=2000") == Group::Seq);
    TEST_ASSERT_TRUE(groupOf("seq MODE quiet") == Group::Seq);
    TEST_ASSERT_TRUE(groupOf("EMIT CHIRP freq=3200 dur=100") == Group::Emit);
}

void test_help_is_the_whole_line_and_prefixes_are_plain() {
    TEST_ASSERT_TRUE(groupOf("HELP me") == Group::Unknown);  // HELP matches the whole line only
    TEST_ASSERT_TRUE(groupOf("SEQUENCE") == Group::Seq);     // plain prefix, as the Analyzer always matched
    TEST_ASSERT_TRUE(groupOf("PARAMS") == Group::Param);
    TEST_ASSERT_TRUE(groupOf("EMIT") == Group::Unknown);  // EMIT needs its space and a command
    TEST_ASSERT_TRUE(groupOf("EMITCHIRP") == Group::Unknown);
}

void test_lines_of_other_modes_are_unknown() {
    TEST_ASSERT_TRUE(groupOf("") == Group::Unknown);
    TEST_ASSERT_TRUE(groupOf("RB STATUS") == Group::Unknown);
    TEST_ASSERT_TRUE(groupOf("NET * PARAM DUMP") == Group::Unknown);
    TEST_ASSERT_TRUE(groupOf("CHIRP 3200 100") == Group::Unknown);
}

void test_on_off_tokens() {
    bool valid = false;
    TEST_ASSERT_TRUE(analyzer_commands::parseOnOff("on", &valid));
    TEST_ASSERT_TRUE(valid);
    TEST_ASSERT_TRUE(analyzer_commands::parseOnOff("TRUE", &valid));
    TEST_ASSERT_TRUE(valid);
    TEST_ASSERT_TRUE(analyzer_commands::parseOnOff("1", &valid));
    TEST_ASSERT_TRUE(valid);
    TEST_ASSERT_FALSE(analyzer_commands::parseOnOff("Off", &valid));
    TEST_ASSERT_TRUE(valid);
    TEST_ASSERT_FALSE(analyzer_commands::parseOnOff("0", &valid));
    TEST_ASSERT_TRUE(valid);
    TEST_ASSERT_FALSE(analyzer_commands::parseOnOff("false", &valid));
    TEST_ASSERT_TRUE(valid);

    analyzer_commands::parseOnOff("maybe", &valid);
    TEST_ASSERT_FALSE(valid);
    analyzer_commands::parseOnOff("", &valid);
    TEST_ASSERT_FALSE(valid);
    analyzer_commands::parseOnOff(nullptr, &valid);
    TEST_ASSERT_FALSE(valid);
    TEST_ASSERT_TRUE(analyzer_commands::parseOnOff("on", nullptr));  // valid is optional
}

void test_band_spec() {
    unsigned long lo = 7;
    unsigned long hi = 7;
    TEST_ASSERT_TRUE(analyzer_commands::parseBandSpec("2950-3950", 8000, lo, hi));
    TEST_ASSERT_EQUAL_UINT32(2950, lo);
    TEST_ASSERT_EQUAL_UINT32(3950, hi);
    TEST_ASSERT_TRUE(analyzer_commands::parseBandSpec("OFF", 8000, lo, hi));
    TEST_ASSERT_EQUAL_UINT32(0, lo);
    TEST_ASSERT_EQUAL_UINT32(0, hi);

    lo = 7;
    hi = 7;
    TEST_ASSERT_FALSE(analyzer_commands::parseBandSpec("3950-2950", 8000, lo, hi));  // high below low
    TEST_ASSERT_FALSE(analyzer_commands::parseBandSpec("0-3950", 8000, lo, hi));     // no low end
    TEST_ASSERT_FALSE(analyzer_commands::parseBandSpec("3000", 8000, lo, hi));       // no high end
    TEST_ASSERT_FALSE(analyzer_commands::parseBandSpec("3000-9000", 8000, lo, hi));  // above the limit
    TEST_ASSERT_FALSE(analyzer_commands::parseBandSpec("x", 8000, lo, hi));
    TEST_ASSERT_EQUAL_UINT32(7, lo);  // a refused spec leaves the band as it was
    TEST_ASSERT_EQUAL_UINT32(7, hi);
    TEST_ASSERT_TRUE(analyzer_commands::parseBandSpec("3000-3000", 8000, lo, hi));  // one bin is a band
}

void test_hop_list() {
    TEST_ASSERT_TRUE(analyzer_commands::validHopList("3000,3250,3500", 40));
    TEST_ASSERT_TRUE(analyzer_commands::validHopList("3200", 40));
    TEST_ASSERT_FALSE(analyzer_commands::validHopList("", 40));
    TEST_ASSERT_FALSE(analyzer_commands::validHopList("3000;3250", 40));
    TEST_ASSERT_FALSE(analyzer_commands::validHopList("3000, 3250", 40));
    TEST_ASSERT_TRUE(analyzer_commands::validHopList("123456789", 10));  // 9 + terminator fits 10
    TEST_ASSERT_FALSE(analyzer_commands::validHopList("1234567890", 10));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_each_group_is_reached_by_its_verb_in_any_case);
    RUN_TEST(test_help_is_the_whole_line_and_prefixes_are_plain);
    RUN_TEST(test_lines_of_other_modes_are_unknown);
    RUN_TEST(test_on_off_tokens);
    RUN_TEST(test_band_spec);
    RUN_TEST(test_hop_list);
    return UNITY_END();
}
