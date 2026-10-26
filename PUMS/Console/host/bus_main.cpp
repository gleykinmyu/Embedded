/**
 * @file bus_main.cpp
 * @brief TCP CAN hub + live ARP/link table (redraw in-place).
 *
 *   ./smcp_bus [--port 19740]
 */

#include <arpa/inet.h>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "wire_frame.hpp"

namespace {

constexpr uint16_t kDefaultPort = 19740u;
constexpr int kMaxClients = 64;
constexpr uint8_t kMsgHeartbeat = 0x30u;
constexpr uint8_t kBroadcastId = 0xFFu;
/** Нет кадров дольше — строка уходит из Nodes / Links. */
constexpr double kStaleSec = 20.0;

using Steady = std::chrono::steady_clock;

struct Client {
    int fd = -1;
    uint8_t partial[smcp::host::kWireFrameSize]{};
    std::size_t partial_n = 0;
};

struct NodeInfo {
    uint64_t tx = 0; /**< as src */
    uint64_t rx = 0; /**< as dst (unicast) */
    Steady::time_point last{};
};

struct LinkInfo {
    uint64_t frames = 0;
    uint64_t hb = 0;
    uint8_t last_msg = 0; /**< last non-HB if any */
    bool saw_data = false;
    Steady::time_point last{};
};

struct SmcpHdr {
    uint8_t msg_id = 0;
    uint8_t dst = 0;
    uint8_t src = 0;
    uint8_t txn = 0;
};

bool setNonBlocking(int fd)
{
    const int flags = ::fcntl(fd, F_GETFL, 0);
    return flags >= 0 && ::fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
}

void closeClient(Client& c)
{
    if (c.fd >= 0) {
        ::close(c.fd);
        c.fd = -1;
    }
    c.partial_n = 0;
}

bool writeAll(int fd, const uint8_t* data, std::size_t n)
{
    std::size_t sent = 0;
    while (sent < n) {
        const ssize_t w = ::send(fd, data + sent, n - sent, 0);
        if (w < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
            }
            return false;
        }
        sent += static_cast<std::size_t>(w);
    }
    return true;
}

void broadcast(std::vector<Client>& clients, std::size_t from, const uint8_t* wire)
{
    for (std::size_t i = 0; i < clients.size(); ++i) {
        if (i == from || clients[i].fd < 0) {
            continue;
        }
        if (!writeAll(clients[i].fd, wire, smcp::host::kWireFrameSize)) {
            closeClient(clients[i]);
        }
    }
}

bool decodeHdr(const uint8_t wire[smcp::host::kWireFrameSize], SmcpHdr& out)
{
    BIF::CAN::Frame frame{};
    if (!smcp::host::unpackWireFrame(wire, frame)) {
        return false;
    }
    const uint32_t id = frame.id.get();
    out.txn = static_cast<uint8_t>(id & 0x3Fu);
    out.src = static_cast<uint8_t>((id >> 6) & 0xFFu);
    out.dst = static_cast<uint8_t>((id >> 14) & 0xFFu);
    out.msg_id = static_cast<uint8_t>((id >> 22) & 0x7Fu);
    return true;
}

const char* msgName(uint8_t id)
{
    switch (id) {
    case 0x01: return "Ack";
    case 0x02: return "Nack";
    case 0x03: return "Fault";
    case 0x30: return "HB";
    case 0x31: return "Get";
    case 0x32: return "Set";
    case 0x33: return "DBegin";
    case 0x34: return "Data";
    default: return nullptr;
    }
}

void formatMsg(uint8_t id, char* buf, std::size_t n)
{
    if (const char* name = msgName(id)) {
        std::snprintf(buf, n, "%-6s", name);
    } else {
        std::snprintf(buf, n, "0x%02X  ", static_cast<unsigned>(id));
    }
}

double ageSec(Steady::time_point t, Steady::time_point now)
{
    return std::chrono::duration<double>(now - t).count();
}

std::size_t countClients(const std::vector<Client>& clients)
{
    std::size_t n = 0;
    for (const Client& c : clients) {
        if (c.fd >= 0) {
            ++n;
        }
    }
    return n;
}

void noteFrame(std::map<uint8_t, NodeInfo>& nodes,
               std::map<std::pair<uint8_t, uint8_t>, LinkInfo>& links,
               const SmcpHdr& h, Steady::time_point now)
{
    NodeInfo& sn = nodes[h.src];
    sn.tx += 1;
    sn.last = now;

    if (h.dst != kBroadcastId) {
        NodeInfo& dn = nodes[h.dst];
        dn.rx += 1;
        dn.last = now;
    }

    const auto key = std::make_pair(h.src, h.dst);
    LinkInfo& L = links[key];
    L.frames += 1;
    L.last = now;
    if (h.msg_id == kMsgHeartbeat) {
        L.hb += 1;
    } else {
        L.last_msg = h.msg_id;
        L.saw_data = true;
    }
}

void pruneStale(std::map<uint8_t, NodeInfo>& nodes,
                std::map<std::pair<uint8_t, uint8_t>, LinkInfo>& links,
                Steady::time_point now)
{
    for (auto it = links.begin(); it != links.end();) {
        if (ageSec(it->second.last, now) > kStaleSec) {
            it = links.erase(it);
        } else {
            ++it;
        }
    }
    for (auto it = nodes.begin(); it != nodes.end();) {
        if (ageSec(it->second.last, now) > kStaleSec) {
            it = nodes.erase(it);
        } else {
            ++it;
        }
    }
}

void drawDashboard(uint16_t port, const std::vector<Client>& clients,
                   const std::map<uint8_t, NodeInfo>& nodes,
                   const std::map<std::pair<uint8_t, uint8_t>, LinkInfo>& links,
                   uint64_t total_frames)
{
    const auto now = Steady::now();
    /* Clear screen + home — таблица на одном месте. */
    std::fputs("\033[2J\033[H", stdout);

    std::printf("SMCP bus  127.0.0.1:%u   TCP clients: %zu   frames: %llu\n",
        static_cast<unsigned>(port),
        countClients(clients),
        static_cast<unsigned long long>(total_frames));
    std::printf("(live table; stale > %.0fs removed; Ctrl+C quit)\n\n", kStaleSec);

    std::puts("Nodes");
    std::puts("  id     age     tx       rx");
    std::puts("  -----  ------  -------  -------");
    if (nodes.empty()) {
        std::puts("  (none yet)");
    } else {
        for (const auto& kv : nodes) {
            std::printf("  0x%02X   %5.1fs  %-7llu  %-7llu\n",
                static_cast<unsigned>(kv.first),
                ageSec(kv.second.last, now),
                static_cast<unsigned long long>(kv.second.tx),
                static_cast<unsigned long long>(kv.second.rx));
        }
    }

    std::puts("\nLinks  (src → dst)");
    std::puts("  from   to     last     data    HB      age");
    std::puts("  -----  -----  -------  ------  ------  ------");
    if (links.empty()) {
        std::puts("  (none yet)");
    } else {
        for (const auto& kv : links) {
            const uint8_t src = kv.first.first;
            const uint8_t dst = kv.first.second;
            const LinkInfo& L = kv.second;
            char msg[16]{};
            if (L.saw_data) {
                formatMsg(L.last_msg, msg, sizeof(msg));
            } else {
                std::snprintf(msg, sizeof(msg), "%-6s", "HB");
            }
            if (dst == kBroadcastId) {
                std::printf("  0x%02X → FF    %-7s  %-6llu  %-6llu  %5.1fs\n",
                    static_cast<unsigned>(src), msg,
                    static_cast<unsigned long long>(L.frames - L.hb),
                    static_cast<unsigned long long>(L.hb),
                    ageSec(L.last, now));
            } else {
                std::printf("  0x%02X → 0x%02X  %-7s  %-6llu  %-6llu  %5.1fs\n",
                    static_cast<unsigned>(src),
                    static_cast<unsigned>(dst), msg,
                    static_cast<unsigned long long>(L.frames - L.hb),
                    static_cast<unsigned long long>(L.hb),
                    ageSec(L.last, now));
            }
        }
    }

    std::fflush(stdout);
}

} // namespace

int main(int argc, char** argv)
{
    uint16_t port = kDefaultPort;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            port = static_cast<uint16_t>(std::stoul(argv[++i]));
        } else if (std::strcmp(argv[i], "-h") == 0 || std::strcmp(argv[i], "--help") == 0) {
            std::fprintf(stderr, "Usage: %s [--port N]\n", argv[0]);
            return 0;
        } else {
            std::fprintf(stderr, "Usage: %s [--port N]\n", argv[0]);
            return 64;
        }
    }

    std::setvbuf(stdout, nullptr, _IOFBF, 4096);

    const int listen_fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        std::perror("socket");
        return 1;
    }
    const int one = 1;
    (void)::setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    if (!setNonBlocking(listen_fd)) {
        std::perror("fcntl");
        return 1;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (::bind(listen_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        std::perror("bind");
        return 1;
    }
    if (::listen(listen_fd, 16) != 0) {
        std::perror("listen");
        return 1;
    }

    std::vector<Client> clients;
    clients.reserve(static_cast<std::size_t>(kMaxClients));
    std::map<uint8_t, NodeInfo> nodes;
    std::map<std::pair<uint8_t, uint8_t>, LinkInfo> links;
    uint64_t total_frames = 0;
    bool dirty = true;
    auto last_draw = Steady::now() - std::chrono::seconds(1);

    drawDashboard(port, clients, nodes, links, total_frames);

    for (;;) {
        std::vector<pollfd> pfds;
        pfds.push_back(pollfd{listen_fd, POLLIN, 0});
        for (Client& c : clients) {
            if (c.fd >= 0) {
                pfds.push_back(pollfd{c.fd, POLLIN, 0});
            }
        }

        const int pr = ::poll(pfds.data(), static_cast<nfds_t>(pfds.size()), 200);
        if (pr < 0) {
            if (errno == EINTR) {
                continue;
            }
            std::perror("poll");
            break;
        }

        if (pfds[0].revents & POLLIN) {
            for (;;) {
                const int fd = ::accept(listen_fd, nullptr, nullptr);
                if (fd < 0) {
                    break;
                }
                if (!setNonBlocking(fd)) {
                    ::close(fd);
                    continue;
                }
                (void)::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));

                std::size_t slot = clients.size();
                for (std::size_t i = 0; i < clients.size(); ++i) {
                    if (clients[i].fd < 0) {
                        slot = i;
                        break;
                    }
                }
                if (slot == clients.size()) {
                    if (static_cast<int>(clients.size()) >= kMaxClients) {
                        ::close(fd);
                        continue;
                    }
                    clients.push_back(Client{});
                }
                clients[slot] = Client{};
                clients[slot].fd = fd;
                dirty = true;
            }
        }

        std::size_t pfd_i = 1;
        for (std::size_t ci = 0; ci < clients.size(); ++ci) {
            if (clients[ci].fd < 0) {
                continue;
            }
            const short rev = pfds[pfd_i++].revents;
            if (rev == 0) {
                continue;
            }
            if (rev & (POLLERR | POLLHUP | POLLNVAL)) {
                closeClient(clients[ci]);
                dirty = true;
                continue;
            }
            if (!(rev & POLLIN)) {
                continue;
            }

            uint8_t chunk[256]{};
            const ssize_t n = ::recv(clients[ci].fd, chunk, sizeof(chunk), 0);
            if (n <= 0) {
                closeClient(clients[ci]);
                dirty = true;
                continue;
            }

            Client& c = clients[ci];
            std::size_t off = 0;
            while (off < static_cast<std::size_t>(n)) {
                const std::size_t need = smcp::host::kWireFrameSize - c.partial_n;
                const std::size_t take = (static_cast<std::size_t>(n) - off) < need
                    ? (static_cast<std::size_t>(n) - off)
                    : need;
                std::memcpy(c.partial + c.partial_n, chunk + off, take);
                c.partial_n += take;
                off += take;
                if (c.partial_n == smcp::host::kWireFrameSize) {
                    SmcpHdr hdr{};
                    if (decodeHdr(c.partial, hdr)) {
                        noteFrame(nodes, links, hdr, Steady::now());
                        ++total_frames;
                        dirty = true;
                    }
                    broadcast(clients, ci, c.partial);
                    c.partial_n = 0;
                }
            }
        }

        const auto now = Steady::now();
        if (dirty && now - last_draw >= std::chrono::milliseconds(200)) {
            pruneStale(nodes, links, now);
            drawDashboard(port, clients, nodes, links, total_frames);
            last_draw = now;
            dirty = false;
        } else if (now - last_draw >= std::chrono::milliseconds(500)) {
            pruneStale(nodes, links, now);
            drawDashboard(port, clients, nodes, links, total_frames);
            last_draw = now;
            dirty = false;
        }
    }

    ::close(listen_fd);
    return 0;
}
