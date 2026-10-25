/*
NetFrame on the host (PAR-013, step 11a): encode/decode round trip, rejects,
target matching, the remote command allowlist and the repeat filter. Runs in
native-test-behavior.
*/
#include <unity.h>

#include "../../src/net/NetFrame.h"

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
    TEST_ASSERT_TRUE(net::remoteCommandAllowed("PARAM GET behavior.rest_ms"));
    TEST_ASSERT_TRUE(net::remoteCommandAllowed("PARAM RESET"));
    TEST_ASSERT_FALSE(net::remoteCommandAllowed("PARAM DUMP"));   // 38 lines per node: USB only
    TEST_ASSERT_FALSE(net::remoteCommandAllowed("PARAM LIST"));
    TEST_ASSERT_FALSE(net::remoteCommandAllowed("PARAM SAVEX"));
    TEST_ASSERT_TRUE(net::remoteCommandAllowed("NET PING"));
    TEST_ASSERT_FALSE(net::remoteCommandAllowed("RB CALL"));
    TEST_ASSERT_FALSE(net::remoteCommandAllowed("SELFTEST"));
    TEST_ASSERT_FALSE(net::remoteCommandAllowed("PARAMSET x"));
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

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_command_round_trip);
    RUN_TEST(test_reply_part_and_long_text_is_cut_to_one_frame);
    RUN_TEST(test_decode_rejects_foreign_or_broken_frames);
    RUN_TEST(test_targets_and_allowlist);
    RUN_TEST(test_seq_filter_drops_repeats_per_sender);
    return UNITY_END();
}
