/**
 * @file debug_trace.cpp
 * @brief SMCP_TRACE_LINK: дамп Packet на ILink.
 */

#include "smcp/debug.hpp"

#if defined(SMCP_TRACE_LINK)

#include "smcp/Console/console_message.hpp"

namespace smcp {
namespace {

[[nodiscard]] char actionCh(msg::Action a) noexcept
{
    switch (a) {
    case msg::Action::Add: return '+';
    case msg::Action::Remove: return '-';
    case msg::Action::Set: return '=';
    }
    return '?';
}

void printBody(const msg::Packet& pkt) noexcept
{
#if defined(SMCP_TRACE_SHORT)
    if (pkt.msg.id == msg::Nack::kId) {
        msg::Nack nack{};
        if (nack.unpack(pkt.msg)) {
            std::printf("Nk %s %u", msg::cstrS(nack.code),
                static_cast<unsigned>(nack.detail));
            return;
        }
    }
    if (pkt.msg.id == msg::Ack::kId) {
        std::printf("Ack");
        return;
    }
    if (pkt.msg.id == msg::Heartbeat::kId) {
        std::printf("HB");
        return;
    }
    if (pkt.msg.id == msg::Select::kId) {
        msg::Select sel{};
        if (sel.unpack(pkt.msg)) {
            std::printf("S%c %lX", actionCh(sel.action),
                static_cast<unsigned long>(sel.selection.raw()));
            return;
        }
    }
    if (pkt.msg.id == msg::Block::kId) {
        msg::Block blk{};
        if (blk.unpack(pkt.msg)) {
            std::printf("B%c %lX", actionCh(blk.action),
                static_cast<unsigned long>(blk.selection.raw()));
            return;
        }
    }
    if (pkt.msg.id == msg::GetTelemetry::kId) {
        msg::GetTelemetry gt{};
        if (gt.unpack(pkt.msg)) {
            std::printf("GT %lX", static_cast<unsigned long>(gt.selection.raw()));
            return;
        }
    }
    if (pkt.msg.id == msg::SetTarget::kId) {
        msg::SetTarget st{};
        if (st.unpack(pkt.msg)) {
            std::printf("Tg %u %ld/%u",
                static_cast<unsigned>(st.mech_id),
                static_cast<long>(st.target.target_mm),
                static_cast<unsigned>(st.target.speed_mm_s));
            return;
        }
    }
    if (pkt.msg.id == msg::Telemetry::kId) {
        msg::Telemetry tel{};
        if (tel.unpack(pkt.msg)) {
            std::printf("Te %u h=%u %ld %02X",
                static_cast<unsigned>(tel.mech_id),
                static_cast<unsigned>(tel.holder_id),
                static_cast<long>(tel.position_mm),
                static_cast<unsigned>(tel.status.raw()));
            return;
        }
    }
    std::printf("%s", msg::cstrPduS(pkt.msg.id));
#else
    if (pkt.msg.id == msg::Nack::kId) {
        msg::Nack nack{};
        if (nack.unpack(pkt.msg)) {
            std::printf("Nack code=%s detail=%u", msg::cstr(nack.code),
                static_cast<unsigned>(nack.detail));
            return;
        }
    }
    if (pkt.msg.id == msg::Ack::kId) {
        std::printf("Ack");
        return;
    }
    if (pkt.msg.id == msg::Heartbeat::kId) {
        std::printf("Heartbeat");
        return;
    }
    if (pkt.msg.id == msg::Select::kId) {
        msg::Select sel{};
        if (sel.unpack(pkt.msg)) {
            std::printf("Select %s mask=0x%08lX",
                msg::cstr(sel.action),
                static_cast<unsigned long>(sel.selection.raw()));
            return;
        }
    }
    if (pkt.msg.id == msg::Block::kId) {
        msg::Block blk{};
        if (blk.unpack(pkt.msg)) {
            std::printf("Block %s mask=0x%08lX",
                msg::cstr(blk.action),
                static_cast<unsigned long>(blk.selection.raw()));
            return;
        }
    }
    if (pkt.msg.id == msg::GetTelemetry::kId) {
        msg::GetTelemetry gt{};
        if (gt.unpack(pkt.msg)) {
            std::printf("GetTelemetry mask=0x%08lX",
                static_cast<unsigned long>(gt.selection.raw()));
            return;
        }
    }
    if (pkt.msg.id == msg::SetTarget::kId) {
        msg::SetTarget st{};
        if (st.unpack(pkt.msg)) {
            std::printf("SetTarget mech=%u target=%ld speed=%u",
                static_cast<unsigned>(st.mech_id),
                static_cast<long>(st.target.target_mm),
                static_cast<unsigned>(st.target.speed_mm_s));
            return;
        }
    }
    if (pkt.msg.id == msg::Telemetry::kId) {
        msg::Telemetry tel{};
        if (tel.unpack(pkt.msg)) {
            std::printf("Telemetry mech=%u holder=%u pos=%ld st=0x%02X",
                static_cast<unsigned>(tel.mech_id),
                static_cast<unsigned>(tel.holder_id),
                static_cast<long>(tel.position_mm),
                static_cast<unsigned>(tel.status.raw()));
            return;
        }
    }
    std::printf("%s", msg::cstrPdu(pkt.msg.id));
#endif
}

} // namespace

void smcpTracePacket(const char* dir, uint8_t /*node_id*/, const msg::Packet& pkt) noexcept
{
#if !defined(SMCP_TRACE_HB)
    if (pkt.msg.id == msg::Heartbeat::kId) {
        return;
    }
#endif

#if defined(SMCP_TRACE_SHORT)
    std::printf("%c %u>%u #%u ",
        (dir != nullptr && dir[0] == 'T') ? 'T' : 'R',
        static_cast<unsigned>(pkt.src_id),
        static_cast<unsigned>(pkt.dst_id),
        static_cast<unsigned>(pkt.pkt_id));
#else
    std::printf("[SMCP] %s %u>%u #%u ",
        dir,
        static_cast<unsigned>(pkt.src_id),
        static_cast<unsigned>(pkt.dst_id),
        static_cast<unsigned>(pkt.pkt_id));
#endif
    printBody(pkt);
    std::printf("\n");
}

} // namespace smcp

#endif // SMCP_TRACE_LINK
