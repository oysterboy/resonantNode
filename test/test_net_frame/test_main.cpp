/*
NetFrame on the host (PAR-013, step 11a): encode/decode round trip, rejects,
target matching, the remote command allowlist and the repeat filter. The filter
across a sender's reboot (critique D1.3 / D4.3): it remembers only the last seq
per sender, so a sender that restarts its count drops exactly the first command
of its new life whose seq equals the last of the old one; NodeNet::begin() seeds
the count from esp_random() so that this is a 1-in-65535 event, not "one command
per boot". Runs in native-test-behavior.
*/
#include <unity.h>

#include "../../src/net/NetFrame.h"
#include "../../src/net/RemoteCommandPolicy.h"

void setUp() {}
void tearDown() {}

void test_command_round_trip() {
    net::Frame f;
    f.type = net::FrameType::Command;
    f.netId = 0x52;
    f.seq = 0xBEEF;
    net::setTarget(f, "*");
    const char* text = "PARAM SET behavior.rest_ms 6000";
    net::setText(f, text, strlen(text));
    uint8_t buf[net::kMaxFrameBytes];
    const size_t len = net::encode(f, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_UINT32(net::kHeaderBytes + strlen(text), len);
    net::Frame g;
    TEST_ASSERT_TRUE(net::decode(buf, len, g));
    TEST_ASSERT_EQUAL(net::FrameType::Command, g.type);
    TEST_ASSERT_EQUAL_UINT8(0x52, g.netId);
    TEST_ASSERT_EQUAL_UINT16(0xBEEF, g.seq);
    TEST_ASSERT_EQUAL_STRING("*", g.target);
    TEST_ASSERT_EQUAL_STRING(text, g.text);
    TEST_ASSERT_TRUE(g.last);
}

void test_reply_part_and_long_text_is_cut_to_one_frame() {
    net::Frame f;
    f.type = net::FrameType::Reply;
    f.part = 3;
    f.last = false;
    net::setTarget(f, "E3-long-name");  // cut to 8
    char text[400];
    memset(text, 'x', sizeof(text));
    TEST_ASSERT_EQUAL_UINT32(net::kMaxText, net::setText(f, text, sizeof(text)));
    uint8_t buf[net::kMaxFrameBytes];
    const size_t len = net::encode(f, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_UINT32(net::kMaxFrameBytes, len);
    net::Frame g;
    TEST_ASSERT_TRUE(net::decode(buf, len, g));
    TEST_ASSERT_EQUAL_UINT8(3, g.part);
    TEST_ASSERT_FALSE(g.last);
    TEST_ASSERT_EQUAL_STRING("E3-long-", g.target);
    TEST_ASSERT_EQUAL_UINT32(net::kMaxText, g.textLen);
}

void test_decode_rejects_foreign_or_broken_frames() {
    net::Frame f;
    net::setTarget(f, "E1");
    net::setText(f, "PARAM DUMP", 10);
    uint8_t buf[net::kMaxFrameBytes];
    const size_t len = net::encode(f, buf, sizeof(buf));
    net::Frame g;
    TEST_ASSERT_FALSE(net::decode(buf, net::kHeaderBytes - 1, g));  // short
    buf[0] = 'X';
    TEST_ASSERT_FALSE(net::decode(buf, len, g));  // magic
    buf[0] = 'R';
    buf[2] = 9;
    TEST_ASSERT_FALSE(net::decode(buf, len, g));  // version
    buf[2] = net::kVersion;
    buf[3] = 7;
    TEST_ASSERT_FALSE(net::decode(buf, len, g));  // type
    buf[3] = 1;
    buf[net::kHeaderBytes + 2] = '\n';
    TEST_ASSERT_FALSE(net::decode(buf, len, g));  // a second line hidden in the text
}

void test_targets_and_allowlist() {
    TEST_ASSERT_TRUE(net::targetMatches("*", "E2"));
    TEST_ASSERT_TRUE(net::targetMatches("e2", "E2"));
    TEST_ASSERT_FALSE(net::targetMatches("E3", "E2"));
    TEST_ASSERT_FALSE(net::targetMatches("", "E2"));
    TEST_ASSERT_TRUE(net::remoteCommandAllowed("PARAM SET output.silent 1"));
    TEST_ASSERT_TRUE(net::remoteCommandAllowed("param save"));
    TEST_ASSERT_TRUE(net::remoteCommandAllowed("PARAM DUMP"));  // paced by the sender since 2026-10-25
    TEST_ASSERT_TRUE(net::remoteCommandAllowed("RB STATUS"));
    TEST_ASSERT_TRUE(net::remoteCommandAllowed("RB CALL"));
    TEST_ASSERT_TRUE(net::remoteCommandAllowed("SELFTEST"));
    TEST_ASSERT_TRUE(net::remoteCommandAllowed("NET PING"));
    TEST_ASSERT_FALSE(net::remoteCommandAllowed("NET * PARAM SET x 1"));  // the radio does not address itself
    TEST_ASSERT_FALSE(net::remoteCommandAllowed("NET STATUS"));
    TEST_ASSERT_FALSE(net::remoteCommandAllowed("PARAMSET x"));
    TEST_ASSERT_FALSE(net::remoteCommandAllowed("SELFTESTX"));
    TEST_ASSERT_TRUE(net::remoteCommandNeedsOneTarget("SELFTEST chirp n=5"));
    TEST_ASSERT_FALSE(net::remoteCommandNeedsOneTarget("RB STATUS"));
}

void test_seq_filter_drops_repeats_per_sender() {
    net::SeqFilter filter;
    const uint8_t a[6] = {1, 2, 3, 4, 5, 6};
    const uint8_t b[6] = {6, 5, 4, 3, 2, 1};
    TEST_ASSERT_TRUE(filter.accept(a, 10));
    TEST_ASSERT_FALSE(filter.accept(a, 10));  // the repeat of one broadcast
    TEST_ASSERT_TRUE(filter.accept(b, 10));   // another sender, same number
    TEST_ASSERT_TRUE(filter.accept(a, 11));
    TEST_ASSERT_FALSE(filter.accept(a, 11));
}

// A sender reboots and counts from 1 again (NodeNet before D1.3): its first command
// is dropped when the old life ended on seq 1, then the count runs on. Only the last
// seq is remembered, so a reboot after more than one command loses nothing.
void test_seq_filter_across_a_sender_restart_at_seq_1() {
    net::SeqFilter filter;
    const uint8_t gw[6] = {0x24, 0x6F, 0x28, 1, 2, 3};
    // Old life: one command, broadcast kCommandRepeats (3) times.
    TEST_ASSERT_TRUE(filter.accept(gw, 1));
    TEST_ASSERT_FALSE(filter.accept(gw, 1));
    TEST_ASSERT_FALSE(filter.accept(gw, 1));
    // Reboot, the first command of the new life has seq 1 again: every copy dropped.
    TEST_ASSERT_FALSE(filter.accept(gw, 1));
    TEST_ASSERT_FALSE(filter.accept(gw, 1));
    TEST_ASSERT_FALSE(filter.accept(gw, 1));
    TEST_ASSERT_TRUE(filter.accept(gw, 2));  // the next one runs

    net::SeqFilter f2;
    for (uint16_t s = 1; s <= 5; ++s) {
        TEST_ASSERT_TRUE(f2.accept(gw, s));
    }
    // Reboot after five: 1..5 of the new life all run (each differs from the one before).
    for (uint16_t s = 1; s <= 5; ++s) {
        TEST_ASSERT_TRUE(f2.accept(gw, s));
        TEST_ASSERT_FALSE(f2.accept(gw, s));
    }
}

// What the random start buys: the new life's first command runs for every start
// except the one equal to the old life's last seq.
void test_seq_filter_random_start_runs_the_first_command() {
    const uint8_t gw[6] = {0x24, 0x6F, 0x28, 1, 2, 3};
    const uint16_t lastOfOldLife = 0x3A41;
    uint32_t dropped = 0;
    for (uint32_t start = 1; start <= 0xFFFF; ++start) {  // NodeNet never uses seq 0
        net::SeqFilter filter;
        filter.accept(gw, lastOfOldLife);
        if (!filter.accept(gw, static_cast<uint16_t>(start))) {
            ++dropped;
            TEST_ASSERT_EQUAL_UINT16(lastOfOldLife, start);
        }
    }
    TEST_ASSERT_EQUAL_UINT32(1, dropped);
}

// kSenders slots, oldest replaced: after eight other senders a repeat is no longer
// recognized and runs twice. Five nodes plus a gateway stay well inside.
void test_seq_filter_forgets_a_sender_after_ksenders_others() {
    net::SeqFilter filter;
    const uint8_t a[6] = {0xA, 0, 0, 0, 0, 1};
    TEST_ASSERT_TRUE(filter.accept(a, 77));
    for (uint8_t i = 0; i < net::SeqFilter::kSenders - 1; ++i) {
        const uint8_t other[6] = {0xB, 0, 0, 0, 0, i};
        TEST_ASSERT_TRUE(filter.accept(other, 1));
    }
    TEST_ASSERT_FALSE(filter.accept(a, 77));  // seven others: still remembered
    const uint8_t eighth[6] = {0xC, 0, 0, 0, 0, 0};
    TEST_ASSERT_TRUE(filter.accept(eighth, 1));
    TEST_ASSERT_TRUE(filter.accept(a, 77));  // its slot was reused
}

// The gateway joins a reply stream: line ends inside frames, a long line across frames.
void test_line_joiner_rebuilds_lines_across_frames() {
    net::LineJoiner<64> j;
    char got[4][65];
    int n = 0;
    auto emit = [&](const char* line, size_t len) {
        memcpy(got[n], line, len);
        got[n][len] = '\0';
        ++n;
    };
    const char f1[] = "first\x1F"
                      "second-sta";
    const char f2[] = "rts-here\x1F"
                      "third";
    j.feed(f1, sizeof(f1) - 1, false, emit);
    TEST_ASSERT_EQUAL_INT(1, n);
    j.feed(f2, sizeof(f2) - 1, true, emit);
    TEST_ASSERT_EQUAL_INT(3, n);
    TEST_ASSERT_EQUAL_STRING("first", got[0]);
    TEST_ASSERT_EQUAL_STRING("second-starts-here", got[1]);
    TEST_ASSERT_EQUAL_STRING("third", got[2]);
}

// Deep review N-M-B: after a lost frame the gateway flushes the partial line instead of gluing it to the next one.
void test_line_joiner_flush_keeps_a_partial_line_apart() {
    net::LineJoiner<64> j;
    char got[4][65];
    int n = 0;
    auto emit = [&](const char* line, size_t len) {
        memcpy(got[n], line, len);
        got[n][len] = ' ';
        ++n;
    };
    const char f1[] = "value=1 then-cut";
    j.feed(f1, sizeof(f1) - 1, false, emit);
    j.flush(emit);  // the next frame was lost
    const char f3[] = "PARAM path=x";
    j.feed(f3, sizeof(f3) - 1, true, emit);
    TEST_ASSERT_EQUAL_INT(2, n);
    TEST_ASSERT_EQUAL_STRING("value=1 then-cut", got[0]);
    TEST_ASSERT_EQUAL_STRING("PARAM path=x", got[1]);
    j.flush(emit);  // nothing pending: no line
    TEST_ASSERT_EQUAL_INT(2, n);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_command_round_trip);
    RUN_TEST(test_reply_part_and_long_text_is_cut_to_one_frame);
    RUN_TEST(test_decode_rejects_foreign_or_broken_frames);
    RUN_TEST(test_targets_and_allowlist);
    RUN_TEST(test_seq_filter_drops_repeats_per_sender);
    RUN_TEST(test_seq_filter_across_a_sender_restart_at_seq_1);
    RUN_TEST(test_seq_filter_random_start_runs_the_first_command);
    RUN_TEST(test_seq_filter_forgets_a_sender_after_ksenders_others);
    RUN_TEST(test_line_joiner_rebuilds_lines_across_frames);
    RUN_TEST(test_line_joiner_flush_keeps_a_partial_line_apart);
    return UNITY_END();
}
