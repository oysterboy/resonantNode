/*
lib/ParamRegistry on the host (step 10 item 3, PAR-016): registration rules and
the parse rules a PARAM SET goes through, and persistence (PAR-010/011):
save, load with stale and invalid entries, restore defaults. Runs in
native-test-behavior.
*/
#include <unity.h>

#include <map>

#include "../../lib/ParamRegistry/src/ParamRegistry.h"
#include "../../lib/ParamRegistry/src/ParamRegistry.cpp"

namespace {

enum class Group : uint8_t {
    A,
    B
};
enum class Id : uint16_t {
    U16,
    U32,
    F,
    Flag,
    Other
};

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

// The NVS stand-in: one entry per id.
class MemoryStore : public param::ParamStore {
public:
    bool read(uint16_t id, param::StoredParam& out) override {
        auto it = entries.find(id);
        if (it == entries.end()) {
            return false;
        }
        out = it->second;
        return true;
    }
    bool write(const param::StoredParam& entry) override {
        ++writes;
        entries[entry.id] = entry;
        return true;
    }
    bool clear() override {
        entries.clear();
        return true;
    }
    std::map<uint16_t, param::StoredParam> entries;
    int writes = 0;
};

void test_save_then_load_restores_values_over_defaults() {
    param::ParamRegistry r = makeRegistry();
    r.captureDefaults();
    MemoryStore store;
    set(r, "a.u32", "1234");
    set(r, "b.f", "2.25");
    set(r, "b.flag", "1");
    const param::ParamSaveReport saved = r.save(store);
    TEST_ASSERT_EQUAL_UINT32(4, saved.written);  // every registered param, also the unchanged u16
    TEST_ASSERT_EQUAL_UINT32(0, saved.failed);

    r.restoreDefaults();
    TEST_ASSERT_EQUAL_UINT32(1000, u32);
    TEST_ASSERT_EQUAL_FLOAT(1.5f, f);
    TEST_ASSERT_FALSE(flag);

    const param::ParamLoadReport loaded = r.load(store, Serial);
    TEST_ASSERT_EQUAL_UINT32(4, loaded.stored);
    TEST_ASSERT_EQUAL_UINT32(4, loaded.applied);
    TEST_ASSERT_EQUAL_UINT32(0, loaded.stale);
    TEST_ASSERT_EQUAL_UINT32(0, loaded.invalid);
    TEST_ASSERT_EQUAL_UINT32(1234, u32);
    TEST_ASSERT_EQUAL_FLOAT(2.25f, f);
    TEST_ASSERT_TRUE(flag);
    TEST_ASSERT_EQUAL_UINT16(10, u16);

    // A second save of the same values writes nothing.
    const int writes = store.writes;
    TEST_ASSERT_EQUAL_UINT32(4, r.save(store).unchanged);
    TEST_ASSERT_EQUAL_INT(writes, store.writes);
}

void test_stored_value_wins_over_a_changed_default_and_counts_stale() {
    MemoryStore store;
    {
        param::ParamRegistry r = makeRegistry();
        r.captureDefaults();
        set(r, "a.u32", "1234");
        r.save(store);
    }
    // New firmware: the default of a.u32 moved from 1000 to 2000.
    setUp();
    u32 = 2000;
    param::ParamRegistry r = makeRegistry();
    r.captureDefaults();
    const param::ParamLoadReport loaded = r.load(store, Serial);
    TEST_ASSERT_EQUAL_UINT32(1, loaded.stale);
    TEST_ASSERT_EQUAL_UINT32(1234, u32);  // stored wins
    // A saved value equal to its old default is stale too, and still wins.
    store.entries.clear();
    {
        setUp();
        param::ParamRegistry old = makeRegistry();
        old.captureDefaults();  // u32 default 1000
        old.save(store);
    }
    u32 = 2000;
    r.captureDefaults();
    TEST_ASSERT_EQUAL_UINT32(1, r.load(store, Serial).stale);
    TEST_ASSERT_EQUAL_UINT32(1000, u32);
}

void test_invalid_entries_keep_the_field() {
    param::ParamRegistry r = makeRegistry();
    r.captureDefaults();
    MemoryStore store;
    r.save(store);
    store.entries[static_cast<uint16_t>(Id::U16)].valueBits = 501;  // past the 500 max
    store.entries[static_cast<uint16_t>(Id::F)].type = static_cast<uint8_t>(param::ParamType::UInt32);
    store.entries[static_cast<uint16_t>(Id::Flag)].version = 99;
    store.entries[static_cast<uint16_t>(Id::U32)].valueBits = 7;  // valid: applied
    u16 = 42;
    const param::ParamLoadReport loaded = r.load(store, Serial);
    TEST_ASSERT_EQUAL_UINT32(4, loaded.stored);
    TEST_ASSERT_EQUAL_UINT32(3, loaded.invalid);
    TEST_ASSERT_EQUAL_UINT32(1, loaded.applied);
    TEST_ASSERT_EQUAL_UINT16(42, u16);
    TEST_ASSERT_EQUAL_UINT32(7, u32);
}

void test_float_nan_bits_are_invalid() {
    param::ParamRegistry r = makeRegistry();
    r.captureDefaults();
    MemoryStore store;
    r.save(store);
    const float nan = NAN;
    uint32_t bits = 0;
    memcpy(&bits, &nan, sizeof(bits));
    store.entries[static_cast<uint16_t>(Id::F)].valueBits = bits;
    TEST_ASSERT_EQUAL_UINT32(1, r.load(store, Serial).invalid);
    TEST_ASSERT_EQUAL_FLOAT(1.5f, f);
}

void test_missing_entries_keep_defaults_and_clear_empties_the_store() {
    param::ParamRegistry r = makeRegistry();
    r.captureDefaults();
    MemoryStore store;
    set(r, "a.u16", "99");
    TEST_ASSERT_FALSE(r.isDefault(*r.find("a.u16")));
    r.save(store);
    TEST_ASSERT_TRUE(store.clear());
    r.restoreDefaults();
    TEST_ASSERT_TRUE(r.isDefault(*r.find("a.u16")));
    const param::ParamLoadReport loaded = r.load(store, Serial);
    TEST_ASSERT_EQUAL_UINT32(0, loaded.stored);
    TEST_ASSERT_EQUAL_UINT16(10, u16);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_duplicate_path_and_duplicate_id_are_rejected);
    RUN_TEST(test_unsigned_rejects_sign_junk_and_overflow);
    RUN_TEST(test_uint32_range_is_exact_above_float_precision);
    RUN_TEST(test_float_and_bool_rules);
    RUN_TEST(test_save_then_load_restores_values_over_defaults);
    RUN_TEST(test_stored_value_wins_over_a_changed_default_and_counts_stale);
    RUN_TEST(test_invalid_entries_keep_the_field);
    RUN_TEST(test_float_nan_bits_are_invalid);
    RUN_TEST(test_missing_entries_keep_defaults_and_clear_empties_the_store);
    return UNITY_END();
}
