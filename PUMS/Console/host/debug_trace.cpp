/**
 * @file debug_trace.cpp
 * @brief SMCP_TRACE_LINK для host: на RX всегда сырой дамп (+ попытка расшифровки).
 */

#include "smcp/debug.hpp"

#if defined(SMCP_TRACE_LINK)

#include "nack_fmt.hpp"
#include "smcp/message.hpp"
#include "smcp/session/multi_byte.hpp"

#include <cstdio>

namespace smcp {

void smcpTracePacket(const char* dir, uint8_t /*node_id*/, const Packet& pkt) noexcept
{
#if !defined(SMCP_TRACE_HB)
    if (pkt.msg.id == msg::Heartbeat::kId) {
        return;
    }
#endif

    const bool is_rx = (dir != nullptr && dir[0] == 'R');
    const char* name = msg::cstr(pkt.msg.id);
    const bool known = (name != nullptr && name[0] != '?' && name[0] != '\0');

    std::printf("[SMCP] %s %u>%u #%u id=%s(0x%02X) dlc=%u",
        dir != nullptr ? dir : "?",
        static_cast<unsigned>(pkt.src_id),
        static_cast<unsigned>(pkt.dst_id),
        static_cast<unsigned>(pkt.txn_id),
        known ? name : "?",
        static_cast<unsigned>(pkt.msg.id),
        static_cast<unsigned>(pkt.msg.dlc));

    /* Голый payload всегда на RX; на TX — тоже, если есть data. */
    if (is_rx || pkt.msg.dlc > 0u) {
        std::fputs(" raw=", stdout);
        if (pkt.msg.dlc == 0u) {
            std::fputs("-", stdout);
        } else {
            for (uint8_t i = 0; i < pkt.msg.dlc && i < Message::kMaxDlc; ++i) {
                std::printf("%02X", static_cast<unsigned>(pkt.msg.data[i]));
            }
        }
    }

    if (is_rx) {
        if (pkt.msg.id == msg::Nack::kId) {
            msg::Nack nack{};
            if (nack.unpack(pkt.msg)) {
                char buf[160]{};
                host::formatNack(buf, sizeof(buf), 0, pkt.txn_id, nack);
                std::printf(" | %s", buf);
            } else {
                std::fputs(" | Nack(unpack fail)", stdout);
            }
        } else if (pkt.msg.id == msg::Ack::kId) {
            std::fputs(" | Ack", stdout);
        } else if (pkt.msg.id == msg::Fault::kId) {
            msg::Fault fault{};
            if (fault.unpack(pkt.msg)) {
                char name[msg::Fault::kDetailMax + 1u]{};
                for (uint8_t i = 0; i < fault.detail_len; ++i) {
                    name[i] = static_cast<char>(fault.detail[i]);
                }
                if (fault.detail_len > 0u) {
                    std::printf(" | Fault %s name=\"%s\"",
                        msg::Fault::cstr(fault.error), name);
                } else {
                    std::printf(" | Fault %s", msg::Fault::cstr(fault.error));
                }
            } else {
                std::fputs(" | Fault(unpack fail)", stdout);
            }
        } else if (pkt.msg.id == msg::Get::kId) {
            msg::Get g{};
            if (g.unpack(pkt.msg)) {
                std::printf(" | Get tag=0x%02X", static_cast<unsigned>(g.tag));
            }
        } else if (pkt.msg.id == msg::Set::kId) {
            msg::Set s{};
            if (s.unpack(pkt.msg)) {
                std::printf(" | Set tag=0x%02X len=%u",
                    static_cast<unsigned>(s.tag),
                    static_cast<unsigned>(s.len));
            }
        } else if (pkt.msg.id == msg::DataBegin::kId) {
            msg::DataBegin b{};
            if (b.unpack(pkt.msg)) {
                std::printf(" | DataBegin tag=0x%02X size=%u crc=0x%08X",
                    static_cast<unsigned>(b.tag),
                    static_cast<unsigned>(b.size),
                    static_cast<unsigned>(b.crc32));
            }
        } else if (pkt.msg.id == msg::Data::kId) {
            msg::Data d{};
            if (d.unpack(pkt.msg)) {
                std::printf(" | Data seq=%u len=%u",
                    static_cast<unsigned>(d.seq),
                    static_cast<unsigned>(d.len));
            }
        } else if (!known) {
            std::fputs(" | (unknown msg)", stdout);
        }
    }

    std::fputc('\n', stdout);
}

} // namespace smcp

#endif // SMCP_TRACE_LINK
