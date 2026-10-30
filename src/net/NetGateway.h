#pragma once

#include <stddef.h>
#include <stdint.h>

#include "NetFrame.h"
#include "NodeNet.h"

/*
NetGateway (step 11i N3, docs/refactors/archive/node-split.md; moved to src/net/ 2026-10-30 for the echo modes)

The text layer of the radio on the Node (PAR-013, step 11a): the reply of a
remote command run here (the console tee, cut into reply frames) and, on the
gateway node, the reassembly of other nodes' replies into whole printed lines.
Owns what used to be file-scope state in ResonantNodeNet.cpp; the Node keeps
the command handling (which runs its own command path) and NodeNet the
transport. Interim: VEKTOR replaces this layer, NodeNet stays
(docs/decisions/2026-10-25-vektor-replaces-net.md).
*/
class NetGateway {
public:
    explicit NetGateway(NodeNet& net)
        : _net(net) {}
    ~NetGateway();

    NetGateway(const NetGateway&) = delete;
    NetGateway& operator=(const NetGateway&) = delete;

    // Boot: no reply open, the join slots empty (kept allocated once they are).
    void begin();

    // The reply of one remote command, as a scoped guard: from construction
    // every printed line is teed into reply frames (a reply still open is
    // closed first); on destruction the tee ends and the last frame goes out,
    // unless keepOpen() handed the reply on to closeOpenReply().
    class ReplyScope {
    public:
        ReplyScope(NetGateway& gateway, const uint8_t mac[6], uint16_t seq, const char* name);
        ~ReplyScope();
        ReplyScope(const ReplyScope&) = delete;
        ReplyScope& operator=(const ReplyScope&) = delete;
        // A command that runs on after its call returns (SELFTEST) keeps its
        // reply open; the Node closes it with closeOpenReply() when done.
        void keepOpen() { _keepOpen = true; }

    private:
        NetGateway& _gateway;
        bool _keepOpen = false;
    };

    bool replyOpen() const { return _openReplyActive; }
    // Ends an open reply: the tee stops, the last frame goes out.
    void closeOpenReply();

    // Gateway side: one reply frame from another node, printed as whole
    // "NET reply from=..." lines, then "... done parts=N" on its last frame.
    void handleReplyFrame(const net::Frame& frame);

private:
    // The reply of one remote command is everything it prints, as a byte stream
    // cut into frames: a newline travels as net::kLineEnd, a long line (RB STATUS)
    // runs on into the next frame, and the console's 1 KB flushes of one line join
    // up again. A frame is held until the next one fills, so the last frame carries
    // the "last" flag. The gateway's LineJoiner prints whole lines.
    struct Reply {
        NodeNet* net = nullptr;
        uint8_t mac[6] = {};
        uint16_t seq = 0;
        const char* name = nullptr;
        uint8_t part = 0;
        char held[net::kMaxText];
        size_t heldLen = 0;

        void send(bool last);
        static void tee(void* ctx, const uint8_t* data, size_t len);
    };

    // The gateway's reassembly, one per replying node (by name).
    struct JoinSlot {
        char name[net::kTargetLen + 1] = {};
        uint16_t seq = 0;
        unsigned nextPart = 0;  // the part expected next; a jump means lost frames (deep review N-M-B)
        unsigned missing = 0;
        net::LineJoiner<2560> joiner;
    };
    static constexpr size_t kJoinSlots = 5;

    void openReply(const uint8_t mac[6], uint16_t seq, const char* name);
    void endReply();
    JoinSlot* joinSlotFor(const char* name, uint16_t seq);

    NodeNet& _net;
    Reply _openReply;
    bool _openReplyActive = false;
    // On the heap at the first reply (only a gateway needs them; static RAM is full).
    JoinSlot* _join = nullptr;
    size_t _joinNext = 0;
};
