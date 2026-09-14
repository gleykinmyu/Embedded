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

void printBody(const msg::Message& body) noexcept
{
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
}

} // namespace

void smcpTracePacket(const char* dir, uint8_t node_id, const msg::Packet& pkt) noexcept
{
#if !defined(SMCP_TRACE_HB)
    if (std::holds_alternative<msg::Heartbeat>(pkt.body)) {
        return;
    }
#endif

    std::printf("[SMCP] %s node=%u src=%u dst=%u pkt=%u ",
        dir,
        static_cast<unsigned>(node_id),
        static_cast<unsigned>(pkt.hdr.src_id),
        static_cast<unsigned>(pkt.hdr.dst_id),
        static_cast<unsigned>(pkt.pkt_id));
    printBody(pkt.body);
    std::printf("\n");
}

} // namespace smcp

#endif // SMCP_TRACE_LINK
