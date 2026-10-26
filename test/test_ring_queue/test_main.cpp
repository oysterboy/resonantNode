/*
RingQueue on the host (step 11e item 4; critique D4.3, the radio outbox): the FIFO NodeNet's outbox,
DetectionRuntime's queues and Behavior's own-emit history sit on. NodeNet.cpp itself only builds for the
ESP32 (ESP-NOW, FreeRTOS queue), so its pacing (kTxSpacingMs, reply parts kReplySpacingMs apart) is not
reachable here; what the outbox relies on is: order kept across the wrap, emplace() refusing when full (the
caller counts txOutboxFull) without touching what is queued, and the front slot staying readable after
popFront() until the next emplace() (NodeNet::service sends from it after popping). Runs in
native-test-behavior.
*/
#include <unity.h>

#include "../../src/app/RingQueue.h"

void setUp() {}
void tearDown() {}

void test_fifo_order_across_the_wrap() {
    RingQueue<int, 4> q;
    TEST_ASSERT_TRUE(q.empty());
    for (int round = 0; round < 3; ++round) {  // the read index walks past the end of the storage
        for (int i = 0; i < 3; ++i) {
            TEST_ASSERT_TRUE(q.push(round * 10 + i));
        }
        for (int i = 0; i < 3; ++i) {
            int v = -1;
            TEST_ASSERT_TRUE(q.pop(v));
            TEST_ASSERT_EQUAL_INT(round * 10 + i, v);
        }
    }
    int v = 0;
    TEST_ASSERT_FALSE(q.pop(v));
}

void test_full_queue_refuses_and_keeps_its_elements() {
    RingQueue<int, 3> q;
    TEST_ASSERT_TRUE(q.push(1));
    TEST_ASSERT_TRUE(q.push(2));
    int* slot = q.emplace();
    TEST_ASSERT_NOT_NULL(slot);
    *slot = 3;
    TEST_ASSERT_TRUE(q.full());
    TEST_ASSERT_NULL(q.emplace());  // NodeNet::enqueue counts txOutboxFull here
    TEST_ASSERT_FALSE(q.push(4));
    TEST_ASSERT_EQUAL_UINT32(3, q.count());
    TEST_ASSERT_EQUAL_INT(1, q.at(0));
    TEST_ASSERT_EQUAL_INT(2, q.at(1));
    TEST_ASSERT_EQUAL_INT(3, q.at(2));
}

// NodeNet::service: `Outgoing& o = front(); popFront(); ... encode(o.frame)`.
void test_front_slot_survives_pop_until_next_emplace() {
    RingQueue<int, 2> q;
    q.push(7);
    q.push(8);
    int& front = q.front();
    q.popFront();
    TEST_ASSERT_EQUAL_INT(7, front);  // still there
    TEST_ASSERT_EQUAL_INT(8, q.front());
    *q.emplace() = 9;  // reuses the popped slot
    TEST_ASSERT_EQUAL_INT(9, front);
}

void test_push_overwrite_drops_the_oldest() {
    RingQueue<int, 3> q;
    for (int i = 1; i <= 5; ++i) {
        q.pushOverwrite(i);
    }
    TEST_ASSERT_EQUAL_UINT32(3, q.count());
    TEST_ASSERT_EQUAL_INT(3, q.at(0));
    TEST_ASSERT_EQUAL_INT(5, q.at(2));
    q.clear();
    TEST_ASSERT_TRUE(q.empty());
    q.popFront();  // nothing happens when empty
    TEST_ASSERT_TRUE(q.empty());
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_fifo_order_across_the_wrap);
    RUN_TEST(test_full_queue_refuses_and_keeps_its_elements);
    RUN_TEST(test_front_slot_survives_pop_until_next_emplace);
    RUN_TEST(test_push_overwrite_drops_the_oldest);
    return UNITY_END();
}
