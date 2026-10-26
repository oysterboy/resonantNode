/*
Host test for the shared PARAM command handler (app/ParamCommands, step 11e
item 9): the grammar every mode answers, what a line writes and what it tells
the caller to apply, with and without a store. Output goes to stdout as on
the board; the assertions are on the fields and the result. Runs in
native-test-behavior; includes the sources it tests directly, like
test_param_registry.
*/
#include <unity.h>

#include <map>
#include <string>

#include "../../lib/ParamRegistry/src/ParamRegistry.cpp"
#include "../../lib/ParamRegistry/src/ParamRegistry.h"
#include "../../src/app/ParamBindings.h"
#include "../../src/app/ParamCommands.cpp"
#include "../../src/app/ParamIds.h"

namespace {

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
        entries[entry.id] = entry;
        return true;
    }
    bool clear() override {
        entries.clear();
        ++clears;
        return true;
    }
    std::map<uint16_t, param::StoredParam> entries;
    int clears = 0;
};

detection::ScalarTransientConfig scalar;
BehaviorGateConfig behavior;

param::ParamRegistry makeRegistry() {
    param::ParamRegistry r;
    r.setGroupNamer(param::moduleNameForGroup);
    param::bindScalarTransientParams(r, scalar);
    param::bindBehaviorParams(r, behavior);
    r.captureDefaults();
    return r;
}

param::ParamCommandResult run(const param::ParamRegistry& r, const char* line, param::ParamStore* store = nullptr) {
    return param::handleParamCommand(line, r, store, Serial);
}

}  // namespace

void setUp() {
    scalar = detection::ScalarTransientConfig();
    behavior = BehaviorGateConfig();
}
void tearDown() {}

void test_bindings_register_every_field_once() {
    param::ParamRegistry r = makeRegistry();
    TEST_ASSERT_EQUAL_UINT32(13 + 13, r.count());
    TEST_ASSERT_NOT_NULL(r.find("detection.scalar_onset_threshold"));
    TEST_ASSERT_NOT_NULL(r.find("detection.scalar_max_gap_ms"));
    TEST_ASSERT_NOT_NULL(r.find("behavior.rest_ms"));
    TEST_ASSERT_NULL(r.find("detection.scalar_observed_stream"));  // the profile's, not a param
    TEST_ASSERT_EQUAL_STRING("detection", r.groupName(r.find("detection.scalar_cooldown_ms")->group));
    TEST_ASSERT_EQUAL_STRING("behavior", r.groupName(r.find("behavior.idle_enabled")->group));
}

void test_set_writes_the_field_and_names_the_group() {
    param::ParamRegistry r = makeRegistry();
    const param::ParamCommandResult result = run(r, "PARAM SET detection.scalar_onset_threshold 2000");
    TEST_ASSERT_EQUAL_INT(static_cast<int>(param::ParamCommandEffect::Group), static_cast<int>(result.effect));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(param::ModuleId::Detection), result.group);
    TEST_ASSERT_EQUAL_FLOAT(2000.0f, scalar.onsetDetectionThreshold);

    const param::ParamCommandResult b = run(r, "param set behavior.wait_after_heard_ms 250");
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(param::ModuleId::Behavior), b.group);
    TEST_ASSERT_EQUAL_UINT32(250, behavior.waitAfterHeardMs);
}

void test_refused_set_changes_nothing() {
    param::ParamRegistry r = makeRegistry();
    const uint32_t before = scalar.cooldownAfterOnsetMs;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(param::ParamCommandEffect::None),
                          static_cast<int>(run(r, "PARAM SET detection.scalar_cooldown_ms 99999").effect));  // out of range
    TEST_ASSERT_EQUAL_INT(static_cast<int>(param::ParamCommandEffect::None),
                          static_cast<int>(run(r, "PARAM SET detection.scalar_cooldown_ms 12x").effect));  // parse error
    TEST_ASSERT_EQUAL_INT(static_cast<int>(param::ParamCommandEffect::None),
                          static_cast<int>(run(r, "PARAM SET detection.scalar_cooldown_ms 10 junk").effect));  // two values
    TEST_ASSERT_EQUAL_INT(static_cast<int>(param::ParamCommandEffect::None),
                          static_cast<int>(run(r, "PARAM SET detection.nothing 1").effect));
    TEST_ASSERT_EQUAL_UINT32(before, scalar.cooldownAfterOnsetMs);
    // The Analyzer's old key=value form and a bare verb write nothing either.
    TEST_ASSERT_EQUAL_INT(static_cast<int>(param::ParamCommandEffect::None),
                          static_cast<int>(run(r, "PARAM scalar_onset_threshold=2000").effect));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(param::ParamCommandEffect::None), static_cast<int>(run(r, "PARAM").effect));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(param::ParamCommandEffect::None), static_cast<int>(run(r, "PARAM STATUS").effect));
}

void test_reset_without_a_store_restores_the_defaults() {
    param::ParamRegistry r = makeRegistry();
    run(r, "PARAM SET detection.scalar_release_threshold 500");
    run(r, "PARAM SET behavior.rest_ms 1");
    const param::ParamCommandResult result = run(r, "PARAM RESET");
    TEST_ASSERT_EQUAL_INT(static_cast<int>(param::ParamCommandEffect::AllGroups), static_cast<int>(result.effect));
    TEST_ASSERT_EQUAL_FLOAT(detection::ScalarTransientConfig().onsetReleaseThreshold, scalar.onsetReleaseThreshold);
    TEST_ASSERT_EQUAL_UINT32(BehaviorGateConfig().restMs, behavior.restMs);
}

void test_save_and_load_need_a_store() {
    param::ParamRegistry r = makeRegistry();
    TEST_ASSERT_EQUAL_INT(static_cast<int>(param::ParamCommandEffect::None), static_cast<int>(run(r, "PARAM SAVE").effect));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(param::ParamCommandEffect::None), static_cast<int>(run(r, "PARAM LOAD").effect));

    MemoryStore store;
    run(r, "PARAM SET detection.scalar_min_peak_strength 123", &store);
    run(r, "PARAM SAVE", &store);
    TEST_ASSERT_EQUAL_UINT32(26, store.entries.size());
    scalar.minTransientPeakStrength = 1.0f;  // drift the field; LOAD puts the stored value back
    const param::ParamCommandResult loaded = run(r, "PARAM LOAD", &store);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(param::ParamCommandEffect::AllGroups), static_cast<int>(loaded.effect));
    TEST_ASSERT_EQUAL_FLOAT(123.0f, scalar.minTransientPeakStrength);
    // RESET clears the store too.
    run(r, "PARAM RESET", &store);
    TEST_ASSERT_EQUAL_INT(1, store.clears);
    TEST_ASSERT_EQUAL_UINT32(0, store.entries.size());
    TEST_ASSERT_EQUAL_FLOAT(detection::ScalarTransientConfig().minTransientPeakStrength, scalar.minTransientPeakStrength);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_bindings_register_every_field_once);
    RUN_TEST(test_set_writes_the_field_and_names_the_group);
    RUN_TEST(test_refused_set_changes_nothing);
    RUN_TEST(test_reset_without_a_store_restores_the_defaults);
    RUN_TEST(test_save_and_load_need_a_store);
    return UNITY_END();
}
