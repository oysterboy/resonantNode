/*
Whispernet's gate rule (WhisperGate, ECHO-002) and the FIFO to the I2S writer (SampleFifo) on the host: a phrase asks
once, an answered one opens from its start (at most the delay back), a failing verdict closes it and a passing one
reopens from the closing, a phrase once too loud stays shut, a new phrase or the end closes. The FIFO keeps order,
drops when full, counts underruns. Runs in native-test-behavior.
*/
#include <unity.h>

#include "../../src/audio/SampleFifo.h"
#include "../../src/behavior/WhisperGate.h"

using Event = WhisperGate::Event;

void setUp() {}
void tearDown() {}

// The first pass asks; answered, the gate opens from the phrase's start.
void test_ask_then_open_from_the_start() {
    WhisperGate g;
    g.setDelayMs(1000);
    WhisperGate::Step s = g.onVerdict(5000, true, false, 5300);
    TEST_ASSERT_EQUAL(Event::Ask, s.event);
    s = g.decide(true);
    TEST_ASSERT_EQUAL(Event::Open, s.event);
    TEST_ASSERT_EQUAL_UINT32(5000, s.fromMs);
    TEST_ASSERT_TRUE(g.open());
    TEST_ASSERT_EQUAL(Event::None, g.onVerdict(5000, true, false, 5400).event);  // stays open
}

// Not answered: the phrase stays shut, and does not ask again.
void test_not_answered_stays_shut() {
    WhisperGate g;
    TEST_ASSERT_EQUAL(Event::Ask, g.onVerdict(100, true, false, 400).event);
    TEST_ASSERT_EQUAL(Event::None, g.decide(false).event);
    TEST_ASSERT_EQUAL(Event::None, g.onVerdict(100, true, false, 500).event);
    TEST_ASSERT_FALSE(g.open());
}

// A phrase longer than the line: the gate passes only what is still unplayed.
void test_open_at_most_the_delay_back() {
    WhisperGate g;
    g.setDelayMs(400);
    g.onVerdict(1000, false, false, 1300);
    g.onVerdict(1000, true, false, 3000);
    const WhisperGate::Step s = g.decide(true);
    TEST_ASSERT_EQUAL(Event::Open, s.event);
    TEST_ASSERT_EQUAL_UINT32(2600, s.fromMs);
}

// Fail closes; a later pass in the answered phrase reopens from the closing.
void test_fail_closes_and_pass_reopens() {
    WhisperGate g;
    g.onVerdict(0, true, false, 300);
    g.decide(true);
    WhisperGate::Step s = g.onVerdict(0, false, false, 600);
    TEST_ASSERT_EQUAL(Event::Close, s.event);
    TEST_ASSERT_EQUAL_UINT32(600, s.fromMs);
    s = g.onVerdict(0, true, false, 900);
    TEST_ASSERT_EQUAL(Event::Open, s.event);
    TEST_ASSERT_EQUAL_UINT32(600, s.fromMs);
}

// Too loud once: shut to the phrase's end, open again only for the next phrase.
void test_too_loud_latches_to_the_end() {
    WhisperGate g;
    g.onVerdict(0, true, false, 300);
    g.decide(true);
    TEST_ASSERT_EQUAL(Event::Close, g.onVerdict(0, false, true, 400).event);
    TEST_ASSERT_EQUAL(Event::None, g.onVerdict(0, true, false, 500).event);
    TEST_ASSERT_EQUAL(Event::None, g.onPhraseEnd(800).event);
    TEST_ASSERT_EQUAL(Event::Ask, g.onVerdict(2000, true, false, 2300).event);
}

// The phrase's end closes an open gate; a new phrase closes the old one's.
void test_end_and_new_phrase_close() {
    WhisperGate g;
    g.onVerdict(0, true, false, 300);
    g.decide(true);
    WhisperGate::Step s = g.onPhraseEnd(1500);
    TEST_ASSERT_EQUAL(Event::Close, s.event);
    TEST_ASSERT_EQUAL_UINT32(1500, s.fromMs);
    g.onVerdict(2000, true, false, 2300);
    g.decide(true);
    s = g.onVerdict(3000, true, false, 3300);  // the detector moved on without an end
    TEST_ASSERT_EQUAL(Event::Close, s.event);
}

void test_fifo_order_drop_underrun() {
    SampleFifo f;
    TEST_ASSERT_TRUE(f.begin(4));
    for (int16_t i = 1; i <= 5; ++i) {
        f.push(i);
    }
    TEST_ASSERT_EQUAL_UINT32(4, f.available());
    TEST_ASSERT_EQUAL_UINT32(1, f.dropped());
    int16_t out[6] = {};
    TEST_ASSERT_EQUAL_UINT32(4, f.pop(out, 6));
    TEST_ASSERT_EQUAL_INT16(1, out[0]);
    TEST_ASSERT_EQUAL_INT16(4, out[3]);
    TEST_ASSERT_EQUAL_UINT32(1, f.underruns());
    f.push(7);  // wraps
    TEST_ASSERT_EQUAL_UINT32(1, f.pop(out, 1));
    TEST_ASSERT_EQUAL_INT16(7, out[0]);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_ask_then_open_from_the_start);
    RUN_TEST(test_not_answered_stays_shut);
    RUN_TEST(test_open_at_most_the_delay_back);
    RUN_TEST(test_fail_closes_and_pass_reopens);
    RUN_TEST(test_too_loud_latches_to_the_end);
    RUN_TEST(test_end_and_new_phrase_close);
    RUN_TEST(test_fifo_order_drop_underrun);
    return UNITY_END();
}
