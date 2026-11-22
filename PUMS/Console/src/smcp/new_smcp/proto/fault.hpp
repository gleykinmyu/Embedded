/**
 * @file new_smcp/proto/fault.hpp
 * @brief Fault — broadcast announce (dst=FF). RX зовёт Node::onFault.
 */

#pragma once

#include "comm/old/container.hpp"
#include "smcp/new_smcp/node.hpp"
#include "smcp/new_smcp/pdu.hpp"

namespace smcp::proto {

class FaultProto : public comm::Proto<Frame> {
public:
    explicit FaultProto(Node& node) noexcept
        : Proto(node)
        , _node(node)
    {}

    /** Поставить Fault::Code::IdConflict в очередь на TX. */
    void raiseIdConflict(const char* name) noexcept
    {
        _pending = true;
        _pdu = {};
        _pdu.reason = Fault::Code::IdConflict;
        _pdu.detail_len = 0;
        if (name != nullptr) {
            for (; _pdu.detail_len < Fault::kDetailMax && name[_pdu.detail_len] != '\0';
                 ++_pdu.detail_len) {
                _pdu.detail[_pdu.detail_len] = static_cast<uint8_t>(name[_pdu.detail_len]);
            }
        }
    }

    [[nodiscard]] bool onPacket(const Frame& pkt, MISC::time_t) noexcept override
    {
        SMCP_IF_MSG(Fault) {
            _node.onFault(pkt.src, body);
            return true;
        }
        return false;
    }

    void tick(MISC::time_t) noexcept override {}
    void abort() noexcept override
    {
        _pending = false;
        _inFlight = false;
    }

protected:
    [[nodiscard]] bool peekTx(Frame& out, MISC::time_t) noexcept override
    {
        if (!_pending || _inFlight) {
            return false;
        }
        out = {};
        out.dst = addr_t::kBroadcast;
        out.txn = 0;
        _pdu.pack(out);
        _inFlight = true;
        _pending = false;
        return true;
    }

    void onTxDone(const Frame&, bool) noexcept override { _inFlight = false; }

private:
    Node& _node;
    Fault _pdu{};
    bool _pending = false;
    bool _inFlight = false;
};

} // namespace smcp::proto
