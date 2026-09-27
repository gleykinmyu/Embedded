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

#if defined(SMCP_TRACE_SHORT)
    std::printf("%c %u>%u #%u %s dlc=%u\n",
        (dir != nullptr && dir[0] == 'T') ? 'T' : 'R',
        static_cast<unsigned>(pkt.src_id),
        static_cast<unsigned>(pkt.dst_id),
        static_cast<unsigned>(pkt.pkt_id),
        msg::cstrMsgS(pkt.msg.id),
        static_cast<unsigned>(pkt.msg.dlc));
#else
    std::printf("[SMCP] %s %u>%u #%u id=%s dlc=%u\n",
        dir,
        static_cast<unsigned>(pkt.src_id),
        static_cast<unsigned>(pkt.dst_id),
        static_cast<unsigned>(pkt.pkt_id),
        msg::cstrMsg(pkt.msg.id),
        static_cast<unsigned>(pkt.msg.dlc));
#endif
}

} // namespace smcp

#endif // SMCP_TRACE_LINK
