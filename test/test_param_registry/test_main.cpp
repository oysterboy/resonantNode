/*
lib/ParamRegistry on the host (step 10 item 3, PAR-016): registration rules and
the parse rules a PARAM SET goes through. Runs in native-test-behavior.
*/
#include <unity.h>

#include "../../lib/ParamRegistry/src/ParamRegistry.h"
#include "../../lib/ParamRegistry/src/ParamRegistry.cpp"

namespace {

enum class Group : uint8_t { A, B };
enum class Id : uint16_t { U16, U32, F, Flag, Other };

const char* groupName(uint8_t g) {
    return g == 0 ? "a" : (g == 1 ? "b" : nullptr);
}

uint16_t u16 = 10;
uint32_t u32 = 1000;
float f = 1.5f;
bool flag = false;

param::ParamRegistry makeRegistry() {
    param::ParamRegistry r;
    r.setGroupNamer(groupName);
    TEST_ASSERT_TRUE(r.addUInt16(Id::U16, Group::A, "a.u16", &u16, 0, 500));
    TEST_ASSERT_TRUE(r.addUInt32(Id::U32, Group::A, "a.u32", &u32, 0, 4000000000UL));
    TEST_ASSERT_TRUE(r.addFloat(Id::F, Group::B, "b.f", &f, -1.0f, 10.0f));
    TEST_ASSERT_TRUE(r.addBool(Id::Flag, Group::B, "b.flag", &flag));
    return r;
}

param::ParamSetStatus set(const param::ParamRegistry& r, const char* path, const char* value) {
    const param::ParamBinding* b = r.find(path);
    TEST_ASSERT_NOT_NULL(b);
    return r.applyValue(*b, value);
}

}  // namespace

void setUp() {
    u16 = 10;
    u32 = 1000;
    f = 1.5f;
    flag = false;
}
void tearDown() {}

void test_duplicate_path_and_duplicate_id_are_rejected() {
    param::ParamRegistry r = makeRegistry();
    uint16_t other = 0;
    TEST_ASSERT_FALSE(r.addUInt16(Id::Other, Group::A, "A.U16", &other, 0, 1));  // same path, any case
    TEST_ASSERT_FALSE(r.addUInt16(Id::U16, Group::A, "a.other", &other, 0, 1));  // same id
    TEST_ASSERT_EQUAL_UINT32(4, r.count());
    TEST_ASSERT_EQUAL_STRING("b", r.groupName(r.find("b.f")->group));
}

void test_unsigned_rejects_sign_junk_and_overflow() {
    param::ParamRegistry r = makeRegistry();
    TEST_ASSERT_EQUAL(param::ParamSetStatus::ParseError, set(r, "a.u16", "-1"));  // used to wrap to ULONG_MAX
    TEST_ASSERT_EQUAL(param::ParamSetStatus::ParseError, set(r, "a.u16", "+5"));
    TEST_ASSERT_EQUAL(param::ParamSetStatus::ParseError, set(r, "a.u16", "12abc"));
    TEST_ASSERT_EQUAL(param::ParamSetStatus::OutOfRange, set(r, "a.u16", "501"));
    TEST_ASSERT_EQUAL(param::ParamSetStatus::OutOfRange, set(r, "a.u32", "99999999999999999999"));
    TEST_ASSERT_EQUAL_UINT16(10, u16);
    TEST_ASSERT_EQUAL(param::ParamSetStatus::Ok, set(r, "a.u16", " 500 "));
    TEST_ASSERT_EQUAL_UINT16(500, u16);
}

void test_uint32_range_is_exact_above_float_precision() {
    param::ParamRegistry r = makeRegistry();
    // 4,000,000,001 rounds to 4e9 as a float; the integer check still rejects it.
    TEST_ASSERT_EQUAL(param::ParamSetStatus::OutOfRange, set(r, "a.u32", "4000000001"));
    TEST_ASSERT_EQUAL(param::ParamSetStatus::Ok, set(r, "a.u32", "4000000000"));
    TEST_ASSERT_EQUAL_UINT32(4000000000UL, u32);
}

void test_float_and_bool_rules() {
    param::ParamRegistry r = makeRegistry();
    TEST_ASSERT_EQUAL(param::ParamSetStatus::ParseError, set(r, "b.f", "nan"));
    TEST_ASSERT_EQUAL(param::ParamSetStatus::ParseError, set(r, "b.f", "1.5x"));
    TEST_ASSERT_EQUAL(param::ParamSetStatus::OutOfRange, set(r, "b.f", "inf"));
    TEST_ASSERT_EQUAL(param::ParamSetStatus::Ok, set(r, "b.f", "-0.5"));
    TEST_ASSERT_EQUAL_FLOAT(-0.5f, f);
    TEST_ASSERT_EQUAL(param::ParamSetStatus::ParseError, set(r, "b.flag", "yes"));
    TEST_ASSERT_EQUAL(param::ParamSetStatus::Ok, set(r, "b.flag", "on"));
    TEST_ASSERT_TRUE(flag);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_duplicate_path_and_duplicate_id_are_rejected);
    RUN_TEST(test_unsigned_rejects_sign_junk_and_overflow);
    RUN_TEST(test_uint32_range_is_exact_above_float_precision);
    RUN_TEST(test_float_and_bool_rules);
    return UNITY_END();
}
