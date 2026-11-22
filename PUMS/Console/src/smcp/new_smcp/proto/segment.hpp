/**
 * @file new_smcp/proto/segment.hpp
 * @brief Northbound demux (Select…Telemetry). Без split Console/Server.
 *
 * Исходящие команды — через Reply (RequestJob) снаружи; здесь только RX → sink.
 */

#pragma once

#include "comm/old/container.hpp"
#include "smcp/new_smcp/transport.hpp"

namespace smcp::proto {

/** Приём northbound PDU (тело ещё в pkt.payload; pack/unpack PDU — позже / снаружи). */
struct ISegmentSink {
    virtual void onSegment(const Frame& pkt) noexcept = 0;

protected:
    ~ISegmentSink() = default;
};

class Segment : public comm::Proto<Frame> {
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

    [[nodiscard]] bool onPacket(const Frame& pkt, MISC::time_t) noexcept override
    {
        if (!isSegmentMsg(pkt.id)) {
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
    [[nodiscard]] bool peekTx(Frame&, MISC::time_t) noexcept override { return false; }

private:
    ISegmentSink* _sink = nullptr;
};

} // namespace smcp::proto
