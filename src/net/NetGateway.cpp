// Moved from ResonantNodeNet.cpp's file-scope state (step 11i N3); the lines it prints are unchanged.
#include "NetGateway.h"

#include <string.h>
#include <new>

#include "../app/SerialOut.h"

NetGateway::~NetGateway() {
    delete[] _join;
}

void NetGateway::begin() {
    _openReply = Reply();
    _openReplyActive = false;
    if (_join != nullptr) {
        for (size_t i = 0; i < kJoinSlots; ++i) {
            _join[i].name[0] = '\0';
            _join[i].seq = 0;
            _join[i].joiner.reset();
        }
    }
    _joinNext = 0;
}

void NetGateway::Reply::send(bool last) {
    net->sendReply(mac, seq, part++, last, name, held, heldLen);
    heldLen = 0;
}

void NetGateway::Reply::tee(void* ctx, const uint8_t* data, size_t len) {
    Reply& r = *static_cast<Reply*>(ctx);
    for (size_t i = 0; i < len; ++i) {
        const char ch = static_cast<char>(data[i]);
        if (ch == '\r') {
            continue;
        }
        if (r.heldLen == sizeof(r.held)) {
            r.send(false);
        }
        r.held[r.heldLen++] = ch == '\n' ? net::kLineEnd : ch;
    }
}

void NetGateway::openReply(const uint8_t mac[6], uint16_t seq, const char* name) {
    // A new command ends a reply still open (its output stops here).
    closeOpenReply();
    _openReply = Reply();
    _openReply.net = &_net;
    memcpy(_openReply.mac, mac, 6);
    _openReply.seq = seq;
    _openReply.name = name;
    serialOut().setTee(&Reply::tee, &_openReply);
}

void NetGateway::endReply() {
    serialOut().setTee(nullptr, nullptr);
    _openReply.send(true);
}

void NetGateway::closeOpenReply() {
    if (!_openReplyActive) {
        return;
    }
    endReply();
    _openReplyActive = false;
}

NetGateway::ReplyScope::ReplyScope(NetGateway& gateway, const uint8_t mac[6], uint16_t seq, const char* name)
    : _gateway(gateway) {
    _gateway.openReply(mac, seq, name);
}

NetGateway::ReplyScope::~ReplyScope() {
    if (_keepOpen) {
        _gateway._openReplyActive = true;  // the self-test reports over the next seconds
        return;
    }
    _gateway.endReply();
}

NetGateway::JoinSlot* NetGateway::joinSlotFor(const char* name, uint16_t seq) {
    if (_join == nullptr) {
        _join = new (std::nothrow) JoinSlot[kJoinSlots];
        if (_join == nullptr) {
            return nullptr;
        }
    }
    for (size_t i = 0; i < kJoinSlots; ++i) {
        JoinSlot& s = _join[i];
        if (strcmp(s.name, name) == 0) {
            if (s.seq != seq) {
                s.seq = seq;
                s.joiner.reset();
                s.nextPart = 0;
                s.missing = 0;
            }
            return &s;
        }
    }
    JoinSlot& s = _join[_joinNext];
    _joinNext = (_joinNext + 1) % kJoinSlots;
    strncpy(s.name, name, net::kTargetLen);
    s.name[net::kTargetLen] = '\0';
    s.seq = seq;
    s.joiner.reset();
    s.nextPart = 0;
    s.missing = 0;
    return &s;
}

void NetGateway::handleReplyFrame(const net::Frame& f) {
    JoinSlot* slot = joinSlotFor(f.target, f.seq);
    if (slot == nullptr) {
        return;
    }
    const unsigned part = static_cast<unsigned>(f.part);
    if (part != slot->nextPart) {
        // Lost frames (a full queue or outbox, a failed send): the line in progress is cut there, marked, rather
        // than spliced onto the next frame's text into one plausible wrong line (deep review N-M-B).
        slot->missing += part > slot->nextPart ? part - slot->nextPart : 1U;
        slot->joiner.flush([&](const char* line, size_t n) {
            serialOut().print("NET reply from=");
            serialOut().print(f.target);
            serialOut().print(" seq=");
            serialOut().print(f.seq);
            serialOut().print(" part=");
            serialOut().print(slot->nextPart > 0 ? slot->nextPart - 1U : 0U);
            serialOut().print(" gap=1 ");
            serialOut().write(reinterpret_cast<const uint8_t*>(line), n);
            serialOut().println();
        });
    }
    slot->nextPart = part + 1U;
    slot->joiner.feed(f.text, f.textLen, f.last, [&](const char* line, size_t n) {
        serialOut().print("NET reply from=");
        serialOut().print(f.target);
        serialOut().print(" seq=");
        serialOut().print(f.seq);
        serialOut().print(" part=");
        serialOut().print(f.part);
        serialOut().print(' ');
        serialOut().write(reinterpret_cast<const uint8_t*>(line), n);
        serialOut().println();
    });
    if (f.last) {
        serialOut().print("NET reply from=");
        serialOut().print(f.target);
        serialOut().print(" seq=");
        serialOut().print(f.seq);
        serialOut().print(" done parts=");
        serialOut().print(part + 1U);
        if (slot->missing > 0) {
            serialOut().print(" missing=");
            serialOut().print(slot->missing);
        }
        serialOut().println();
    }
}
