/*
Host test for the shared console helpers (step 11e item 8): the LineReader every
serial port uses (app/SerialLine.h) and the number / token parsing in
app/TextParse.h. Runs in native-test-behavior.
*/
#include <unity.h>

#include <stdint.h>
#include <string.h>

#include "../../src/app/SerialLine.h"
#include "../../src/app/TextParse.h"

void setUp() {}
void tearDown() {}

namespace {

// Feeds a string; returns how many lines ended, the last one in `last`.
template <size_t N>
int feedAll(serial_line::LineReader<N>& reader, const char* text, const char*& last, bool& tooLong) {
    int lines = 0;
    for (const char* p = text; *p != '\0'; ++p) {
        if (reader.feed(*p)) {
            ++lines;
            last = reader.line();
            tooLong = reader.tooLong();
        }
    }
    return lines;
}

}  // namespace

void test_reader_ends_a_line_at_newline_and_drops_cr_and_junk() {
    serial_line::LineReader<31> reader;
    const char* last = nullptr;
    bool tooLong = true;
    TEST_ASSERT_EQUAL_INT(1, feedAll(reader, "\xff\x01MODE REMOTE\r\n", last, tooLong));
    TEST_ASSERT_EQUAL_STRING("MODE REMOTE", last);
    TEST_ASSERT_FALSE(tooLong);
}

void test_reader_refuses_a_long_line_whole_and_recovers() {
    serial_line::LineReader<7> reader;
    const char* last = nullptr;
    bool tooLong = false;
    TEST_ASSERT_EQUAL_INT(1, feedAll(reader, "12345678\n", last, tooLong));
    TEST_ASSERT_TRUE(tooLong);
    TEST_ASSERT_EQUAL_UINT(0, reader.length());
    // The next line is read normally; nothing of the long one leaks into it.
    TEST_ASSERT_EQUAL_INT(1, feedAll(reader, "ok\n", last, tooLong));
    TEST_ASSERT_FALSE(tooLong);
    TEST_ASSERT_EQUAL_STRING("ok", last);
}

void test_reader_fits_exactly_capacity() {
    serial_line::LineReader<7> reader;
    const char* last = nullptr;
    bool tooLong = true;
    TEST_ASSERT_EQUAL_INT(1, feedAll(reader, "1234567\n", last, tooLong));
    TEST_ASSERT_FALSE(tooLong);
    TEST_ASSERT_EQUAL_STRING("1234567", last);
}

void test_reader_reports_an_empty_line() {
    serial_line::LineReader<7> reader;
    const char* last = nullptr;
    bool tooLong = true;
    TEST_ASSERT_EQUAL_INT(2, feedAll(reader, "\n\n", last, tooLong));
    TEST_ASSERT_EQUAL_UINT(0, reader.length());
    TEST_ASSERT_FALSE(tooLong);
}

void test_parse_unsigned_takes_whole_numbers_only() {
    unsigned long v = 7;
    TEST_ASSERT_TRUE(text_parse::parseUnsigned("1200", v));
    TEST_ASSERT_EQUAL_UINT32(1200, v);
    TEST_ASSERT_FALSE(text_parse::parseUnsigned("12a", v));
    TEST_ASSERT_FALSE(text_parse::parseUnsigned("abc", v));
    TEST_ASSERT_FALSE(text_parse::parseUnsigned("-1", v));
    TEST_ASSERT_FALSE(text_parse::parseUnsigned("", v));
    TEST_ASSERT_FALSE(text_parse::parseUnsigned(nullptr, v));
    TEST_ASSERT_EQUAL_UINT32(1200, v);  // untouched by the refusals
    uint8_t small = 0;
    TEST_ASSERT_TRUE(text_parse::parseUnsigned("2", small));
    TEST_ASSERT_EQUAL_UINT(2, small);
}

void test_token_value_and_rest() {
    TEST_ASSERT_EQUAL_STRING("2950-3950", text_parse::tokenValue("band=2950-3950"));
    TEST_ASSERT_EQUAL_STRING("", text_parse::tokenValue("band="));
    TEST_ASSERT_EQUAL_STRING("", text_parse::tokenValue("start"));
    TEST_ASSERT_EQUAL_STRING("TonalPulseScalar", text_parse::rest("RB PROFILE   TonalPulseScalar", "RB PROFILE"));
    TEST_ASSERT_EQUAL_STRING("", text_parse::rest("RB PROFILE", "RB PROFILE"));
    TEST_ASSERT_EQUAL_STRING("CHIRP freq=3200", text_parse::rest("EMIT CHIRP freq=3200", "EMIT"));
}

void test_parse_finite_float_refuses_junk_and_nan() {
    float f = 1.5f;
    TEST_ASSERT_TRUE(text_parse::parseFiniteFloat("2000", f));
    TEST_ASSERT_EQUAL_FLOAT(2000.0f, f);
    TEST_ASSERT_FALSE(text_parse::parseFiniteFloat("nan", f));
    TEST_ASSERT_FALSE(text_parse::parseFiniteFloat("12x", f));
    TEST_ASSERT_EQUAL_FLOAT(2000.0f, f);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_reader_ends_a_line_at_newline_and_drops_cr_and_junk);
    RUN_TEST(test_reader_refuses_a_long_line_whole_and_recovers);
    RUN_TEST(test_reader_fits_exactly_capacity);
    RUN_TEST(test_reader_reports_an_empty_line);
    RUN_TEST(test_parse_unsigned_takes_whole_numbers_only);
    RUN_TEST(test_token_value_and_rest);
    RUN_TEST(test_parse_finite_float_refuses_junk_and_nan);
    return UNITY_END();
}
