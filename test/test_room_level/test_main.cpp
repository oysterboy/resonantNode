/*
RoomLevel on the host (ECHO-002): a steady room is learned, a short loud phrase barely lifts it, a louder room is
followed within seconds, a quieter one within a second, and nothing is learned while held. Runs in
native-test-behavior.
*/
#include <unity.h>

#include "../../src/audio/RoomLevel.h"

void setUp() {}
void tearDown() {}

namespace {
// ms of blocks of 5 ms at `level`.
void feed(RoomLevel& r, float level, uint32_t ms, bool learning = true) {
    for (uint32_t t = 0; t < ms; t += 5) {
        r.update(level, 5, learning);
    }
}
}  // namespace

void test_a_steady_room_is_learned() {
    RoomLevel r;
    feed(r, 80.0f, 2000);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 80.0f, r.value());
}

void test_a_phrase_barely_lifts_it() {
    RoomLevel r;
    feed(r, 80.0f, 2000);
    feed(r, 600.0f, 1000);  // a whispered second
    TEST_ASSERT_TRUE(r.value() < 140.0f);
}

void test_a_louder_room_is_followed() {
    RoomLevel r;
    feed(r, 80.0f, 2000);
    feed(r, 200.0f, 30000);
    TEST_ASSERT_FLOAT_WITHIN(10.0f, 200.0f, r.value());
    feed(r, 80.0f, 2000);  // and back down fast
    TEST_ASSERT_FLOAT_WITHIN(5.0f, 80.0f, r.value());
}

void test_held_while_not_learning() {
    RoomLevel r;
    feed(r, 80.0f, 2000);
    feed(r, 900.0f, 5000, false);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 80.0f, r.value());
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_a_steady_room_is_learned);
    RUN_TEST(test_a_phrase_barely_lifts_it);
    RUN_TEST(test_a_louder_room_is_followed);
    RUN_TEST(test_held_while_not_learning);
    return UNITY_END();
}
