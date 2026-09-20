/**
 * @file debug_trace.cpp
 * @brief SMCP_TRACE_LINK: дамп Packet / Message на ILink.
 */

#include "smcp/debug.hpp"

#if defined(SMCP_TRACE_LINK)

#include <variant>

#include "smcp/transport/message.hpp"

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

void printBody(const msg::Message& body) noexcept
{
#if defined(SMCP_TRACE_SHORT)
    if (const auto* nack = std::get_if<msg::Nack>(&body)) {
        std::printf("Nk %s %u", msg::cstrS(nack->code),
            static_cast<unsigned>(nack->detail));
        return;
    }
    if (std::holds_alternative<msg::Ack>(body)) {
        std::printf("Ack");
        return;
    }
    if (std::holds_alternative<msg::Heartbeat>(body)) {
        std::printf("HB");
        return;
    }
    if (const auto* sel = std::get_if<msg::Select>(&body)) {
        std::printf("S%c %lX", actionCh(sel->action),
            static_cast<unsigned long>(sel->selection.raw()));
        return;
    }
    if (const auto* blk = std::get_if<msg::Block>(&body)) {
        std::printf("B%c %lX", actionCh(blk->action),
            static_cast<unsigned long>(blk->selection.raw()));
        return;
    }
    if (const auto* gt = std::get_if<msg::GetTelemetry>(&body)) {
        std::printf("GT %lX", static_cast<unsigned long>(gt->selection.raw()));
        return;
    }
    if (const auto* st = std::get_if<msg::SetTarget>(&body)) {
        std::printf("Tg %u %ld/%u",
            static_cast<unsigned>(st->mech_id),
            static_cast<long>(st->target.target_mm),
            static_cast<unsigned>(st->target.speed_mm_s));
        return;
    }
    if (const auto* tel = std::get_if<msg::Telemetry>(&body)) {
        std::printf("Te %u h=%u %ld %02X",
            static_cast<unsigned>(tel->mech_id),
            static_cast<unsigned>(tel->holder_id),
            static_cast<long>(tel->position_mm),
            static_cast<unsigned>(tel->status.raw()));
        return;
    }
    std::printf("?");
#else
    if (const auto* nack = std::get_if<msg::Nack>(&body)) {
        std::printf("Nack code=%s detail=%u", msg::cstr(nack->code),
            static_cast<unsigned>(nack->detail));
        return;
    }
    if (std::holds_alternative<msg::Ack>(body)) {
        std::printf("Ack");
        return;
    }
    if (std::holds_alternative<msg::Heartbeat>(body)) {
        std::printf("Heartbeat");
        return;
    }
    if (const auto* sel = std::get_if<msg::Select>(&body)) {
        std::printf("Select %s mask=0x%08lX",
            msg::cstr(sel->action),
            static_cast<unsigned long>(sel->selection.raw()));
        return;
    }
    if (const auto* blk = std::get_if<msg::Block>(&body)) {
        std::printf("Block %s mask=0x%08lX",
            msg::cstr(blk->action),
            static_cast<unsigned long>(blk->selection.raw()));
        return;
    }
    if (const auto* gt = std::get_if<msg::GetTelemetry>(&body)) {
        std::printf("GetTelemetry mask=0x%08lX",
            static_cast<unsigned long>(gt->selection.raw()));
        return;
    }
    if (const auto* st = std::get_if<msg::SetTarget>(&body)) {
        std::printf("SetTarget mech=%u target=%ld speed=%u",
            static_cast<unsigned>(st->mech_id),
            static_cast<long>(st->target.target_mm),
            static_cast<unsigned>(st->target.speed_mm_s));
        return;
    }
    if (const auto* tel = std::get_if<msg::Telemetry>(&body)) {
        std::printf("Telemetry mech=%u holder=%u pos=%ld st=0x%02X",
            static_cast<unsigned>(tel->mech_id),
            static_cast<unsigned>(tel->holder_id),
            static_cast<long>(tel->position_mm),
            static_cast<unsigned>(tel->status.raw()));
        return;
    }
    std::printf("?");
#endif
}

} // namespace

void smcpTracePacket(const char* dir, uint8_t /*node_id*/, const msg::Packet& pkt) noexcept
{
#if !defined(SMCP_TRACE_HB)
    if (std::holds_alternative<msg::Heartbeat>(pkt.body)) {
        return;
    }
#endif

#if defined(SMCP_TRACE_SHORT)
    std::printf("%c %u>%u #%u ",
        (dir != nullptr && dir[0] == 'T') ? 'T' : 'R',
        static_cast<unsigned>(pkt.hdr.src_id),
        static_cast<unsigned>(pkt.hdr.dst_id),
        static_cast<unsigned>(pkt.pkt_id));
#else
    std::printf("[SMCP] %s %u>%u #%u ",
        dir,
        static_cast<unsigned>(pkt.hdr.src_id),
        static_cast<unsigned>(pkt.hdr.dst_id),
        static_cast<unsigned>(pkt.pkt_id));
#endif
    printBody(pkt.body);
    std::printf("\n");
}

} // namespace smcp

#endif // SMCP_TRACE_LINK
