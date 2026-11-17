/**
 * @file new_smcp/proto/segment.hpp
 * @brief Northbound demux (Select…Telemetry). Без split Console/Server.
 *
 * Исходящие команды — через Reply (RequestJob) снаружи; здесь только RX → sink.
 */

#pragma once

#include "comm/container.hpp"
#include "smcp/new_smcp/packet.hpp"
#include "smcp/new_smcp/message.hpp"

namespace smcp::proto {

/** Приём northbound PDU (тело ещё в pkt.msg; pack/unpack PDU — позже / снаружи). */
struct ISegmentSink {
    virtual void onSegment(const Packet& pkt) noexcept = 0;

protected:
    ~ISegmentSink() = default;
};

class Segment : public comm::Proto<Packet> {
public:
    explicit Segment(SessionT& session, ISegmentSink* sink = nullptr) noexcept
        : Proto(session)
        , _sink(sink)
    {}

    void setSink(ISegmentSink* sink) noexcept { _sink = sink; }

    [[nodiscard]] static bool isSegmentMsg(MsgId id) noexcept
    {
        switch (id) {
        case MsgId::Select:
        case MsgId::Block:
        case MsgId::SetTarget:
        case MsgId::GetTelemetry:
        case MsgId::ResetFault:
        case MsgId::ServiceAccess:
        case MsgId::Telemetry:
            return true;
        default:
            return false;
        }
    }

    [[nodiscard]] bool onPacket(const Packet& pkt, MISC::time_t) noexcept override
    {
        if (!isSegmentMsg(pkt.msg.id)) {
            return false;
        }
        if (_sink != nullptr) {
            _sink->onSegment(pkt);
        }
        return true;
    }

    void tick(MISC::time_t) noexcept override {}
    void abort() noexcept override {}

protected:
    [[nodiscard]] bool peekTx(Packet&, MISC::time_t) noexcept override { return false; }

private:
    ISegmentSink* _sink = nullptr;
};

} // namespace smcp::proto
