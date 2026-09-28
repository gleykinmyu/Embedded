/**
 * @file debug.cpp
 * @brief Default SMCP_TRACE_LINK dump (envelope). App may override (strong).
 */

#include "smcp/debug.hpp"

#if defined(SMCP_TRACE_LINK)

#include "smcp/message.hpp"

#include <cstdio>

namespace smcp {

__attribute__((weak))
void smcpTracePacket(const char* dir, uint8_t /*node_id*/, const Packet& pkt) noexcept
{
#if !defined(SMCP_TRACE_HB)
    if (pkt.msg.id == msg::Heartbeat::kId) {
        return;
    }
#endif

    std::printf(SMCP_PICK(
            "%c %u>%u #%u %s dlc=%u\n",
            "[SMCP] %s %u>%u #%u id=%s dlc=%u\n"),
        SMCP_PICK((dir != nullptr && dir[0] == 'T') ? 'T' : 'R', dir),
        static_cast<unsigned>(pkt.src_id),
        static_cast<unsigned>(pkt.dst_id),
        static_cast<unsigned>(pkt.pkt_id),
        SMCP_PICK(msg::cstrMsgS(pkt.msg.id), msg::cstrMsg(pkt.msg.id)),
        static_cast<unsigned>(pkt.msg.dlc));
}

} // namespace smcp

#endif // SMCP_TRACE_LINK
