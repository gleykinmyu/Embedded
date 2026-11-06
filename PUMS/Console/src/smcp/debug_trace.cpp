/**
 * @file debug_trace.cpp
 * @brief SMCP_TRACE_LINK: дамп Packet на ILink.
 */

#include "smcp/debug.hpp"

#if defined(SMCP_TRACE_LINK)

#include "segment/message.hpp"

namespace smcp {
namespace {

[[nodiscard]] char actionCh(segment::Action a) noexcept
{
    switch (a) {
    case segment::Action::Add: return '+';
    case segment::Action::Remove: return '-';
    case segment::Action::Set: return '=';
    case segment::Action::Last: break;
    }
    return '?';
}

void printBody(const Packet& pkt) noexcept
{
#if defined(SMCP_TRACE_SHORT)
    if (pkt.msg.id == session::Nack::kId) {
        session::Nack nack{};
        if (nack.unpack(pkt.msg)) {
            std::printf("Nk %s %08lX",
                segment::vError::from_raw(nack.error.raw()).cstr(),
                static_cast<unsigned long>(nack.detail));
            return;
        }
    }
    if (pkt.msg.id == session::Fault::kId) {
        session::Fault fault{};
        if (fault.unpack(pkt.msg)) {
            char name[session::Fault::kDetailMax + 1u]{};
            for (uint8_t i = 0; i < fault.detail_len; ++i) {
                name[i] = static_cast<char>(fault.detail[i]);
            }
            if (fault.detail_len > 0u) {
                std::printf("Ft %s \"%s\"", session::Fault::cstr(fault.error), name);
            } else {
                std::printf("Ft %s", session::Fault::cstr(fault.error));
            }
            return;
        }
    }
    if (pkt.msg.id == session::Ack::kId) {
        std::printf("Ack");
        return;
    }
    if (pkt.msg.id == session::Heartbeat::kId) {
        std::printf("HB");
        return;
    }
    if (pkt.msg.id == segment::Select::kId) {
        segment::Select sel{};
        if (sel.unpack(pkt.msg)) {
            std::printf("S%c %lX", actionCh(sel.action),
                static_cast<unsigned long>(sel.selection.raw()));
            return;
        }
    }
    if (pkt.msg.id == segment::Block::kId) {
        segment::Block blk{};
        if (blk.unpack(pkt.msg)) {
            std::printf("B%c %lX", actionCh(blk.action),
                static_cast<unsigned long>(blk.selection.raw()));
            return;
        }
    }
    if (pkt.msg.id == segment::GetTelemetry::kId) {
        segment::GetTelemetry gt{};
        if (gt.unpack(pkt.msg)) {
            std::printf("GT %lX", static_cast<unsigned long>(gt.selection.raw()));
            return;
        }
    }
    if (pkt.msg.id == segment::SetTarget::kId) {
        segment::SetTarget st{};
        if (st.unpack(pkt.msg)) {
            if (st.all_selected) {
                std::printf("Tg * %c %ld/%u",
                    st.target.mode == axis::MoveMode::Relative ? 'R' : 'A',
                    static_cast<long>(st.target.target),
                    static_cast<unsigned>(st.target.speed));
            } else {
                std::printf("Tg %u %c %ld/%u",
                    static_cast<unsigned>(st.axis_id),
                    st.target.mode == axis::MoveMode::Relative ? 'R' : 'A',
                    static_cast<long>(st.target.target),
                    static_cast<unsigned>(st.target.speed));
            }
            return;
        }
    }
    if (pkt.msg.id == segment::ResetFault::kId) {
        segment::ResetFault rf{};
        if (rf.unpack(pkt.msg)) {
            std::printf("RF %lX", static_cast<unsigned long>(rf.selection.raw()));
            return;
        }
    }
    if (pkt.msg.id == segment::Telemetry::kId) {
        segment::Telemetry tel{};
        if (tel.unpack(pkt.msg)) {
            std::printf("Te %u h=%u %ld %02X",
                static_cast<unsigned>(tel.axis_id),
                static_cast<unsigned>(tel.holder_id),
                static_cast<long>(tel.position),
                static_cast<unsigned>(tel.status.raw()));
            return;
        }
    }
    std::printf("%s", segment::cstr(pkt.msg.id));
#else
    if (pkt.msg.id == session::Nack::kId) {
        session::Nack nack{};
        if (nack.unpack(pkt.msg)) {
            std::printf("Nack error=%s detail=0x%08lX",
                segment::vError::from_raw(nack.error.raw()).cstr(),
                static_cast<unsigned long>(nack.detail));
            return;
        }
    }
    if (pkt.msg.id == session::Fault::kId) {
        session::Fault fault{};
        if (fault.unpack(pkt.msg)) {
            char name[session::Fault::kDetailMax + 1u]{};
            for (uint8_t i = 0; i < fault.detail_len; ++i) {
                name[i] = static_cast<char>(fault.detail[i]);
            }
            if (fault.detail_len > 0u) {
                std::printf("Fault error=%s name=\"%s\"",
                    session::Fault::cstr(fault.error), name);
            } else {
                std::printf("Fault error=%s", session::Fault::cstr(fault.error));
            }
            return;
        }
    }
    if (pkt.msg.id == session::Ack::kId) {
        std::printf("Ack");
        return;
    }
    if (pkt.msg.id == session::Heartbeat::kId) {
        std::printf("Heartbeat");
        return;
    }
    if (pkt.msg.id == segment::Select::kId) {
        segment::Select sel{};
        if (sel.unpack(pkt.msg)) {
            std::printf("Select %s mask=0x%08lX",
                segment::cstr(sel.action),
                static_cast<unsigned long>(sel.selection.raw()));
            return;
        }
    }
    if (pkt.msg.id == segment::Block::kId) {
        segment::Block blk{};
        if (blk.unpack(pkt.msg)) {
            std::printf("Block %s mask=0x%08lX",
                segment::cstr(blk.action),
                static_cast<unsigned long>(blk.selection.raw()));
            return;
        }
    }
    if (pkt.msg.id == segment::GetTelemetry::kId) {
        segment::GetTelemetry gt{};
        if (gt.unpack(pkt.msg)) {
            std::printf("GetTelemetry mask=0x%08lX",
                static_cast<unsigned long>(gt.selection.raw()));
            return;
        }
    }
    if (pkt.msg.id == segment::SetTarget::kId) {
        segment::SetTarget st{};
        if (st.unpack(pkt.msg)) {
            if (st.all_selected) {
                std::printf("SetTarget all %s target=%ld speed=%u",
                    cstr(st.target.mode),
                    static_cast<long>(st.target.target),
                    static_cast<unsigned>(st.target.speed));
            } else {
                std::printf("SetTarget mech=%u %s target=%ld speed=%u",
                    static_cast<unsigned>(st.axis_id),
                    cstr(st.target.mode),
                    static_cast<long>(st.target.target),
                    static_cast<unsigned>(st.target.speed));
            }
            return;
        }
    }
    if (pkt.msg.id == segment::ResetFault::kId) {
        segment::ResetFault rf{};
        if (rf.unpack(pkt.msg)) {
            std::printf("ResetFault mask=0x%08lX",
                static_cast<unsigned long>(rf.selection.raw()));
            return;
        }
    }
    if (pkt.msg.id == segment::Telemetry::kId) {
        segment::Telemetry tel{};
        if (tel.unpack(pkt.msg)) {
            std::printf("Telemetry mech=%u holder=%u pos=%ld st=0x%02X",
                static_cast<unsigned>(tel.axis_id),
                static_cast<unsigned>(tel.holder_id),
                static_cast<long>(tel.position),
                static_cast<unsigned>(tel.status.raw()));
            return;
        }
    }
    std::printf("%s", segment::cstr(pkt.msg.id));
#endif
}

} // namespace

void smcpTracePacket(const char* dir, uint8_t /*node_id*/, const Packet& pkt) noexcept
{
#if !defined(SMCP_TRACE_HB)
    if (pkt.msg.id == session::Heartbeat::kId) {
        return;
    }
#endif

#if defined(SMCP_TRACE_SHORT)
    std::printf("%c %u>%u #%u ",
        (dir != nullptr && dir[0] == 'T') ? 'T' : 'R',
        static_cast<unsigned>(pkt.src_id),
        static_cast<unsigned>(pkt.dst_id),
        static_cast<unsigned>(pkt.txn_id));
#else
    std::printf("[SMCP] %s %u>%u #%u ",
        dir,
        static_cast<unsigned>(pkt.src_id),
        static_cast<unsigned>(pkt.dst_id),
        static_cast<unsigned>(pkt.txn_id));
#endif
    printBody(pkt);
    std::printf("\n");
}

} // namespace smcp

#endif // SMCP_TRACE_LINK
