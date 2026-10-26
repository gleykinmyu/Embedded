/**
 * @file node_main.cpp
 * @brief Один процесс = один Node. Запуск без аргументов → REPL не завершается.
 *
 *   ./smcp_bus
 *   ./smcp_node              # id назначаешь вручную: id 0x01
 *   ./smcp_node --id 0x10    # опционально сразу
 */

#include <atomic>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <poll.h>
#include <readline/history.h>
#include <readline/readline.h>
#include <unistd.h>

#include "clock.hpp"
#include "error_log.hpp"
#include "sim_node.hpp"
#include "socket_can.hpp"
#include "smcp/can_link.hpp"
#include "smcp/message.hpp"

namespace {

void sleepMs(unsigned ms)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

bool parseId(const std::string& s, uint8_t& out)
{
    try {
        const unsigned long v = std::stoul(s, nullptr, 0);
        if (v == 0u || v > 0xFFu) {
            return false;
        }
        out = static_cast<uint8_t>(v);
        return true;
    } catch (...) {
        return false;
    }
}

/** Hex байты, либо один токен "text" / 'text' → ASCII (для send). */
bool parsePayload(const std::vector<std::string>& toks, std::size_t from, std::vector<uint8_t>& out)
{
    out.clear();
    if (from >= toks.size()) {
        return true;
    }

    /* ASCII: "hello" или 'hello' (один токен после кавычек в split — см. ниже) */
    if (toks.size() == from + 1u) {
        const std::string& t = toks[from];
        if (t.size() >= 2u
            && ((t.front() == '"' && t.back() == '"')
                || (t.front() == '\'' && t.back() == '\''))) {
            for (std::size_t i = 1; i + 1 < t.size(); ++i) {
                out.push_back(static_cast<uint8_t>(t[i]));
            }
            return true;
        }
        if (t.rfind("ascii:", 0) == 0) {
            for (std::size_t i = 6; i < t.size(); ++i) {
                out.push_back(static_cast<uint8_t>(t[i]));
            }
            return true;
        }
    }

    for (std::size_t i = from; i < toks.size(); ++i) {
        const std::string& t = toks[i];
        if (t.empty()) {
            return false;
        }
        try {
            if (t.size() > 2u && (t.size() % 2u) == 0u
                && t.find_first_not_of("0123456789abcdefABCDEF") == std::string::npos) {
                for (std::size_t p = 0; p + 1 < t.size(); p += 2) {
                    out.push_back(static_cast<uint8_t>(
                        std::stoul(t.substr(p, 2), nullptr, 16)));
                }
            } else {
                /* 0xNN или NN — только если токен целиком число */
                if (t.find_first_not_of("0123456789abcdefABCDEFxX") != std::string::npos) {
                    return false;
                }
                const unsigned long v = std::stoul(t, nullptr, 0);
                if (v > 0xFFu) {
                    return false;
                }
                out.push_back(static_cast<uint8_t>(v));
            }
        } catch (...) {
            return false;
        }
    }
    return true;
}

/** Токены по пробелам; "..." / '...' — один токен с кавычками. */
std::vector<std::string> split(const std::string& line)
{
    std::vector<std::string> toks;
    std::size_t i = 0;
    while (i < line.size()) {
        while (i < line.size() && std::isspace(static_cast<unsigned char>(line[i]))) {
            ++i;
        }
        if (i >= line.size()) {
            break;
        }
        if (line[i] == '"' || line[i] == '\'') {
            const char q = line[i];
            std::size_t j = i + 1;
            while (j < line.size() && line[j] != q) {
                ++j;
            }
            if (j < line.size()) {
                toks.push_back(line.substr(i, j - i + 1)); /* включая кавычки */
                i = j + 1;
            } else {
                toks.push_back(line.substr(i));
                break;
            }
        } else {
            std::size_t j = i;
            while (j < line.size() && !std::isspace(static_cast<unsigned char>(line[j]))) {
                ++j;
            }
            toks.push_back(line.substr(i, j - i));
            i = j;
        }
    }
    return toks;
}

bool stdinReady()
{
    pollfd pfd{};
    pfd.fd = STDIN_FILENO;
    pfd.events = POLLIN;
    return ::poll(&pfd, 1, 0) > 0;
}

void help()
{
    std::puts(
        "SMCP transport bench (no app API). One window = one Node.\n"
        "  id <hex>                     begin(): Listen → Ready\n"
        "  name [str]                   Fault IdConflict detail (≤7)\n"
        "  end                          leave bus (Node::end)\n"
        "  open|start <peer>            Session::start (master HB)\n"
        "  close|stop <peer>            Session::close(None)\n"
        "  set <peer> <tag> <hex…>      TaggedOps Set (inline / multi)\n"
        "  get <peer> <tag>             TaggedOps Get\n"
        "  send <dst> <msgid> [txn N] [hex…|\"ascii\"]   raw Node::send\n"
        "  ack <dst> <txn>              class B\n"
        "  nack <dst> <txn> <err> [detail]\n"
        "  fault [name]                 broadcast Fault IdConflict\n"
        "  mute [off|tx|rx|all]         drop TX / RX (HbLost)\n"
        "  policy [echo|reject|busy]    Get/Set Nack on this node\n"
        "  clear | status | list\n"
        "  wait <ms> | errors | errors clear\n"
        "  history | help | quit");
}

struct Ctx {
    smcp::host::SocketCan* can = nullptr;
    smcp::CanLink* link = nullptr;
    smcp::host::SimNode* node = nullptr;
    smcp::host::ErrorLog* errors = nullptr;
    bool on_bus = false; /**< begin() уже вызывали */
    std::vector<std::string> cmd_hist{};
};


void pump(Ctx& ctx, unsigned ms = 2)
{
    ctx.can->poll();
    if (ctx.on_bus) {
        ctx.node->update();
    }
    sleepMs(ms);
}

void pumpFor(Ctx& ctx, uint32_t duration_ms)
{
    const uint32_t t0 = smcp::host::clockMs();
    while (smcp::host::clockMs() - t0 < duration_ms) {
        pump(ctx, 2);
    }
}

bool busUp(Ctx& ctx)
{
    ctx.can->poll();
    return ctx.can->availableForWrite() > 0u;
}

void cmdStatus(Ctx& ctx)
{
    std::printf("bus=%s  id=%s  status=%s\n",
        busUp(ctx) ? "up" : "waiting",
        ctx.on_bus ? "set" : "none",
        ctx.on_bus ? smcp::Node::cstr(ctx.node->getStatus()) : "Idle");
    if (!ctx.on_bus) {
        std::puts("  (assign: id 0x01)");
        return;
    }
    std::printf("  node_id=0x%02X name=\"%s\" policy=%s mute=%s%s\n",
        static_cast<unsigned>(ctx.node->id()), ctx.node->name(),
        smcp::host::SimNode::policyName(ctx.node->rxPolicy()),
        ctx.can->muteTx() ? "tx" : "",
        ctx.can->muteRx() ? "rx" : (ctx.can->muteTx() ? "" : "off"));
    for (uint8_t s = 0; s < smcp::host::SimNode::kMaxSessions; ++s) {
        smcp::Session* sess = ctx.node->session(s);
        if (sess == nullptr || sess->getStatus() == smcp::Session::Status::Idle) {
            continue;
        }
        std::printf("  slot%u peer=0x%02X %s%s\n",
            static_cast<unsigned>(s),
            static_cast<unsigned>(sess->peerId()),
            smcp::Session::cstr(sess->getStatus()),
            sess->isMaster() ? " M" : "");
    }
}

bool cmdId(Ctx& ctx, uint8_t neu)
{
    if (!busUp(ctx)) {
        std::puts("error: bus not connected yet (start smcp_bus, keep this window open)");
        return false;
    }
    if (ctx.on_bus) {
        ctx.node->end();
    }
    ctx.can->setMute(false, false);
    ctx.node->setRxPolicy(smcp::host::RxPolicy::Echo);
    ctx.link->setNodeId(neu);
    ctx.node->begin(neu);
    {
        char def[smcp::Node::kNameMax + 1u]{};
        std::snprintf(def, sizeof(def), "n%02X", static_cast<unsigned>(neu));
        ctx.node->setName(def);
    }
    ctx.on_bus = true;
    pumpFor(ctx, 300);
    std::printf("ok: id=0x%02X name=\"%s\" status=%s — can open sessions\n",
        static_cast<unsigned>(neu),
        ctx.node->name(),
        smcp::Node::cstr(ctx.node->getStatus()));
    return true;
}

bool requireOnBus(Ctx& ctx)
{
    if (!ctx.on_bus) {
        std::puts("error: set id first —  id 0x01");
        return false;
    }
    if (!busUp(ctx)) {
        std::puts("error: bus lost — restart smcp_bus");
        return false;
    }
    return true;
}

bool cmdOpen(Ctx& ctx, uint8_t peer)
{
    if (!requireOnBus(ctx)) {
        return false;
    }
    if (!ctx.node->isReady()) {
        std::printf("error: not Ready (%s) — wait or clear\n",
            smcp::Node::cstr(ctx.node->getStatus()));
        return false;
    }
    smcp::host::SimSession* s = ctx.node->startPeer(peer);
    if (s == nullptr) {
        std::puts("error: no free session slot");
        return false;
    }
    std::printf("opening → 0x%02X …\n", peer);
    const uint32_t t0 = smcp::host::clockMs();
    while (smcp::host::clockMs() - t0 < 5000u) {
        pump(ctx, 2);
        if (s->isOpen()) {
            break;
        }
    }
    std::printf("%s: session %s\n",
        s->isOpen() ? "ok" : "FAIL",
        smcp::Session::cstr(s->getStatus()));
    return s->isOpen();
}

bool cmdClose(Ctx& ctx, uint8_t peer)
{
    if (!requireOnBus(ctx)) {
        return false;
    }
    smcp::host::SimSession* s = ctx.node->peer(peer);
    if (s == nullptr) {
        std::printf("error: no session to 0x%02X\n", peer);
        return false;
    }
    s->close(smcp::Node::Fault::None);
    pumpFor(ctx, 100);
    std::printf("ok: closed → 0x%02X\n", peer);
    return true;
}

bool cmdSet(Ctx& ctx, uint8_t peer, uint8_t tag, const std::vector<uint8_t>& data)
{
    if (!requireOnBus(ctx)) {
        return false;
    }
    smcp::host::SimSession* s = ctx.node->peer(peer);
    if (s == nullptr || !s->isOpen()) {
        std::puts("error: session not Open (open <peer>)");
        return false;
    }
    if (data.empty() || data.size() > smcp::host::kStoreCap) {
        std::puts("error: bad payload");
        return false;
    }
    std::vector<uint8_t> buf = data;
    smcp::IBuf ib{buf.data(), static_cast<uint16_t>(buf.size())};
    s->clearFlags();
    ctx.node->clearAckFlags();
    if (!s->set(tag, ib)) {
        std::puts("error: set() failed");
        return false;
    }
    pumpFor(ctx, 2000);
    if (!ctx.node->lastAck()) {
        std::puts(ctx.node->lastNack() ? "error: set Nack" : "error: set no Ack");
        return false;
    }
    std::puts("ok: set");
    return true;
}

bool cmdGet(Ctx& ctx, uint8_t peer, uint8_t tag)
{
    if (!requireOnBus(ctx)) {
        return false;
    }
    smcp::host::SimSession* s = ctx.node->peer(peer);
    if (s == nullptr || !s->isOpen()) {
        std::puts("error: session not Open");
        return false;
    }
    uint8_t reply[smcp::host::kStoreCap]{};
    smcp::IBuf ib{reply, smcp::host::kStoreCap};
    s->clearFlags();
    if (!s->get(tag, ib)) {
        std::puts("error: get() failed");
        return false;
    }
    pumpFor(ctx, 2000);
    if (!s->lastReplyOk()) {
        std::puts("error: no reply");
        return false;
    }
    std::printf("ok: len=%u data=", static_cast<unsigned>(s->lastReplyLen()));
    for (uint16_t i = 0; i < s->lastReplyLen(); ++i) {
        std::printf("%02X", s->lastReply()[i]);
    }
    std::printf("\n");
    return true;
}

bool cmdSend(Ctx& ctx, uint8_t dst, uint8_t msg_id, uint8_t txn,
             const std::vector<uint8_t>& data)
{
    if (!requireOnBus(ctx)) {
        return false;
    }
    smcp::Message m{};
    m.id = msg_id;
    if (!data.empty()) {
        if (data.size() > smcp::Message::kMaxDlc) {
            std::puts("error: payload > 8");
            return false;
        }
        m.setPayload(data.data(), static_cast<uint8_t>(data.size()));
    }
    ctx.node->send(m, dst, txn);
    pumpFor(ctx, 50);
    std::printf("ok: sent msg=0x%02X → 0x%02X txn=%u dlc=%u\n",
        msg_id, dst, txn, m.dlc);
    return true;
}

bool parseNackError(const std::string& s, uint8_t& out)
{
    if (s == "NotSupported" || s == "nosupport" || s == "reject") {
        out = static_cast<uint8_t>(smcp::msg::Nack::Error::NotSupported);
        return true;
    }
    if (s == "Busy" || s == "busy") {
        out = static_cast<uint8_t>(smcp::msg::Nack::Error::Busy);
        return true;
    }
    if (s == "Body" || s == "body") {
        out = static_cast<uint8_t>(smcp::msg::Nack::Error::Body);
        return true;
    }
    if (s == "Timeout" || s == "timeout") {
        out = smcp::msg::Nack::kTimeout;
        return true;
    }
    unsigned long v = 0;
    try {
        v = std::stoul(s, nullptr, 0);
    } catch (...) {
        return false;
    }
    if (v > 0xFFu) {
        return false;
    }
    out = static_cast<uint8_t>(v);
    return true;
}

bool dispatch(Ctx& ctx, const std::vector<std::string>& t)
{
    if (t.empty()) {
        return true;
    }
    const std::string& cmd = t[0];
    if (cmd == "help" || cmd == "?") {
        help();
        return true;
    }
    if (cmd == "history") {
        if (ctx.cmd_hist.empty()) {
            std::puts("(empty)");
        } else {
            for (std::size_t i = 0; i < ctx.cmd_hist.size(); ++i) {
                std::printf("  %3zu  %s\n", i + 1, ctx.cmd_hist[i].c_str());
            }
        }
        return true;
    }
    if (cmd == "quit" || cmd == "exit") {
        return false;
    }
    if (cmd == "status" || cmd == "list") {
        cmdStatus(ctx);
        return true;
    }
    if (cmd == "errors") {
        if (t.size() >= 2 && t[1] == "clear") {
            ctx.errors->clear();
            std::puts("ok");
        } else {
            ctx.errors->dump();
        }
        return true;
    }
    if (cmd == "clear") {
        if (!requireOnBus(ctx)) {
            return true;
        }
        ctx.node->clearError();
        std::printf("ok: %s\n", smcp::Node::cstr(ctx.node->getStatus()));
        return true;
    }
    if (cmd == "wait" && t.size() >= 2) {
        pumpFor(ctx, static_cast<uint32_t>(std::stoul(t[1])));
        return true;
    }
    if (cmd == "id" && t.size() >= 2) {
        uint8_t neu = 0;
        if (!parseId(t[1], neu)) {
            std::puts("error: bad id (use 0x01 … 0xFF)");
            return true;
        }
        cmdId(ctx, neu);
        return true;
    }
    if (cmd == "name") {
        if (!requireOnBus(ctx)) {
            return true;
        }
        if (t.size() < 2) {
            std::printf("name=\"%s\"\n", ctx.node->name());
            return true;
        }
        if (t[1].size() > smcp::Node::kNameMax) {
            std::printf("error: name longer than %u chars\n",
                static_cast<unsigned>(smcp::Node::kNameMax));
            return true;
        }
        ctx.node->setName(t[1].c_str());
        std::printf("ok: name=\"%s\"\n", ctx.node->name());
        return true;
    }
    if (cmd == "end") {
        if (ctx.on_bus) {
            ctx.node->end();
            ctx.on_bus = false;
        }
        ctx.can->setMute(false, false);
        std::puts("ok: Idle");
        return true;
    }
    if (cmd == "mute") {
        if (t.size() < 2) {
            std::printf("mute tx=%u rx=%u\n",
                ctx.can->muteTx() ? 1u : 0u,
                ctx.can->muteRx() ? 1u : 0u);
            return true;
        }
        if (t[1] == "off") {
            ctx.can->setMute(false, false);
        } else if (t[1] == "tx") {
            ctx.can->setMute(true, false);
        } else if (t[1] == "rx") {
            ctx.can->setMute(false, true);
        } else if (t[1] == "all") {
            ctx.can->setMute(true, true);
        } else {
            std::puts("error: mute off|tx|rx|all");
            return true;
        }
        std::printf("ok: mute tx=%u rx=%u\n",
            ctx.can->muteTx() ? 1u : 0u,
            ctx.can->muteRx() ? 1u : 0u);
        return true;
    }
    if (cmd == "policy") {
        if (t.size() < 2) {
            std::printf("policy=%s\n",
                smcp::host::SimNode::policyName(ctx.node->rxPolicy()));
            return true;
        }
        if (t[1] == "echo") {
            ctx.node->setRxPolicy(smcp::host::RxPolicy::Echo);
        } else if (t[1] == "reject") {
            ctx.node->setRxPolicy(smcp::host::RxPolicy::Reject);
        } else if (t[1] == "busy") {
            ctx.node->setRxPolicy(smcp::host::RxPolicy::Busy);
        } else {
            std::puts("error: policy echo|reject|busy");
            return true;
        }
        std::printf("ok: policy=%s\n",
            smcp::host::SimNode::policyName(ctx.node->rxPolicy()));
        return true;
    }
    if ((cmd == "open" || cmd == "start") && t.size() >= 2) {
        uint8_t peer = 0;
        if (!parseId(t[1], peer)) {
            std::puts("error: bad peer");
            return true;
        }
        cmdOpen(ctx, peer);
        return true;
    }
    if ((cmd == "close" || cmd == "stop") && t.size() >= 2) {
        uint8_t peer = 0;
        if (!parseId(t[1], peer)) {
            std::puts("error: bad peer");
            return true;
        }
        cmdClose(ctx, peer);
        return true;
    }
    if (cmd == "set" && t.size() >= 4) {
        uint8_t peer = 0, tag = 0;
        if (!parseId(t[1], peer) || !parseId(t[2], tag)) {
            std::puts("error: bad args");
            return true;
        }
        std::vector<uint8_t> data;
        if (!parsePayload(t, 3, data)) {
            std::puts("error: bad payload (hex bytes, or \"ascii\")");
            return true;
        }
        cmdSet(ctx, peer, tag, data);
        return true;
    }
    if (cmd == "get" && t.size() >= 3) {
        uint8_t peer = 0, tag = 0;
        if (!parseId(t[1], peer) || !parseId(t[2], tag)) {
            std::puts("error: bad args");
            return true;
        }
        cmdGet(ctx, peer, tag);
        return true;
    }
    if (cmd == "send" && t.size() >= 3) {
        uint8_t dst = 0, mid = 0;
        if (!parseId(t[1], dst) || !parseId(t[2], mid)) {
            std::puts("error: bad args");
            return true;
        }
        uint8_t txn = 0;
        std::size_t payload_at = 3;
        if (t.size() >= 5 && t[3] == "txn") {
            try {
                const unsigned long v = std::stoul(t[4], nullptr, 0);
                if (v > smcp::protocol::kTxnIdMax) {
                    std::puts("error: txn 0…63");
                    return true;
                }
                txn = static_cast<uint8_t>(v);
            } catch (...) {
                std::puts("error: bad txn");
                return true;
            }
            payload_at = 5;
        }
        std::vector<uint8_t> data;
        if (t.size() > payload_at && !parsePayload(t, payload_at, data)) {
            std::puts("error: bad payload — use hex (68 65 6C) or \"hello\"");
            return true;
        }
        if (data.size() > smcp::Message::kMaxDlc) {
            std::puts("error: payload > 8 bytes (CAN DLC)");
            return true;
        }
        cmdSend(ctx, dst, mid, txn, data);
        return true;
    }
    if (cmd == "ack" && t.size() >= 3) {
        uint8_t dst = 0;
        if (!parseId(t[1], dst)) {
            std::puts("error: bad dst");
            return true;
        }
        uint8_t txn = 0;
        try {
            const unsigned long v = std::stoul(t[2], nullptr, 0);
            if (v == 0u || v > smcp::protocol::kTxnIdMax) {
                std::puts("error: txn 1…63");
                return true;
            }
            txn = static_cast<uint8_t>(v);
        } catch (...) {
            std::puts("error: bad txn");
            return true;
        }
        smcp::Message m{};
        if (!smcp::msg::Ack{}.pack(m)) {
            std::puts("error: pack");
            return true;
        }
        cmdSend(ctx, dst, m.id, txn, {});
        return true;
    }
    if (cmd == "nack" && t.size() >= 4) {
        uint8_t dst = 0, err = 0;
        if (!parseId(t[1], dst) || !parseNackError(t[3], err)) {
            std::puts("error: nack <dst> <txn> <err> [detail]");
            return true;
        }
        uint8_t txn = 0;
        try {
            const unsigned long v = std::stoul(t[2], nullptr, 0);
            if (v == 0u || v > smcp::protocol::kTxnIdMax) {
                std::puts("error: txn 1…63");
                return true;
            }
            txn = static_cast<uint8_t>(v);
        } catch (...) {
            std::puts("error: bad txn");
            return true;
        }
        uint32_t detail = smcp::msg::Nack::kDetailNone;
        if (t.size() >= 5) {
            try {
                detail = static_cast<uint32_t>(std::stoul(t[4], nullptr, 0));
            } catch (...) {
                std::puts("error: bad detail");
                return true;
            }
        }
        smcp::msg::Nack body{};
        body.error = err;
        body.detail = detail;
        smcp::Message m{};
        if (!body.pack(m)) {
            std::puts("error: pack");
            return true;
        }
        std::vector<uint8_t> data(m.data, m.data + m.dlc);
        cmdSend(ctx, dst, m.id, txn, data);
        return true;
    }
    if (cmd == "fault") {
        if (!requireOnBus(ctx)) {
            return true;
        }
        smcp::msg::Fault body{};
        body.error = smcp::msg::Fault::kIdConflict;
        if (t.size() >= 2) {
            body.setDetailStr(t[1].c_str());
        } else {
            body.setDetailStr(ctx.node->name());
        }
        smcp::Message m{};
        if (!body.pack(m)) {
            std::puts("error: pack");
            return true;
        }
        std::vector<uint8_t> data(m.data, m.data + m.dlc);
        cmdSend(ctx, smcp::Packet::kBroadcastId, m.id, 0, data);
        return true;
    }
    std::puts("error: unknown (help)");
    return true;
}

void usage(const char* argv0)
{
    std::fprintf(stderr,
        "Usage: %s [--id <node_id>] [--port N]\n"
        "  No args: interactive REPL (assign id manually).\n"
        "  Needs smcp_bus on 127.0.0.1 (default port %u).\n",
        argv0, static_cast<unsigned>(smcp::host::SocketCan::kDefaultPort));
}

} // namespace

int main(int argc, char** argv)
{
    uint8_t boot_id = 0;
    uint16_t port = smcp::host::SocketCan::kDefaultPort;
    bool have_boot_id = false;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--id") == 0 && i + 1 < argc) {
            if (!parseId(argv[++i], boot_id)) {
                usage(argv[0]);
                return 64;
            }
            have_boot_id = true;
        } else if (std::strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            port = static_cast<uint16_t>(std::stoul(argv[++i]));
        } else if (std::strcmp(argv[i], "-h") == 0 || std::strcmp(argv[i], "--help") == 0) {
            usage(argv[0]);
            return 0;
        } else {
            usage(argv[0]);
            return 64;
        }
    }

    std::setvbuf(stdout, nullptr, _IOLBF, 0);
    std::setvbuf(stderr, nullptr, _IOLBF, 0);

    smcp::host::ErrorLog errors;
    smcp::host::SocketCan can;
    (void)can.configure(smcp::host::SocketCan::Role::Connect, port);
    if (!can.open(1'000'000)) {
        std::fprintf(stderr, "SocketCan open failed\n");
        return 1;
    }

    /* placeholder id until user runs `id` */
    smcp::CanLink link(can, 1u);
    smcp::host::SimNode node(link, smcp::host::clockMs, &errors);
    Ctx ctx{&can, &link, &node, &errors, false};

    std::printf("=== SMCP node → bus 127.0.0.1:%u ===\n", static_cast<unsigned>(port));
    std::puts("Window stays open. Assign id, then open sessions.");
    help();

    if (!busUp(ctx)) {
        std::puts("waiting for smcp_bus… (start Bus window; this one will connect)");
    }

    if (have_boot_id) {
        /* ждём bus, потом id */
        const uint32_t t0 = smcp::host::clockMs();
        while (!busUp(ctx) && smcp::host::clockMs() - t0 < 30000u) {
            pump(ctx, 50);
        }
        if (busUp(ctx)) {
            cmdId(ctx, boot_id);
        } else {
            std::puts("bus not up yet — type id <hex> after starting bus");
        }
    }

    const bool use_readline = isatty(STDIN_FILENO) != 0;
    if (use_readline) {
        using_history();
    }

    std::mutex ctx_mu;
    std::atomic<bool> pump_run{true};
    std::atomic<bool> bus_announced{false};
    std::thread pump_thread([&] {
        while (pump_run.load()) {
            {
                std::lock_guard<std::mutex> lock(ctx_mu);
                pump(ctx, 0);
                if (!bus_announced.load() && busUp(ctx)) {
                    bus_announced.store(true);
                    std::fputs("\nbus connected — type:  id 0x01\n", stdout);
                    std::fflush(stdout);
                }
            }
            sleepMs(5);
        }
    });

    std::string line;
    for (;;) {
        char prompt[32]{};
        {
            std::lock_guard<std::mutex> lock(ctx_mu);
            if (ctx.on_bus) {
                std::snprintf(prompt, sizeof(prompt), "n%02X> ",
                    static_cast<unsigned>(ctx.node->id()));
            } else {
                std::snprintf(prompt, sizeof(prompt), "node> ");
            }
        }

        if (use_readline) {
            char* raw = readline(prompt);
            if (raw == nullptr) {
                break;
            }
            line = raw;
            if (!line.empty()) {
                add_history(raw);
                ctx.cmd_hist.push_back(line);
                if (ctx.cmd_hist.size() > 200u) {
                    ctx.cmd_hist.erase(ctx.cmd_hist.begin());
                }
            }
            std::free(raw);
        } else {
            while (!stdinReady()) {
                sleepMs(5);
            }
            std::fputs(prompt, stdout);
            std::fflush(stdout);
            if (!std::getline(std::cin, line)) {
                std::puts("\n(no stdin — idle; Ctrl+C to stop)");
                for (;;) {
                    sleepMs(200);
                }
            }
            if (!line.empty()) {
                ctx.cmd_hist.push_back(line);
            }
        }

        bool cont = true;
        {
            std::lock_guard<std::mutex> lock(ctx_mu);
            cont = dispatch(ctx, split(line));
        }
        if (!cont) {
            break;
        }
    }

    pump_run.store(false);
    pump_thread.join();

    if (ctx.on_bus) {
        node.end();
    }
    can.close();
    return 0;
}
