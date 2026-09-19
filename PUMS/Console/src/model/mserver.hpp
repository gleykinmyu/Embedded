/**
 * @file mserver.hpp
 * @brief Сервер сегмента проекта: smcp::Server + SessionConsole bank + DriveMechBank.
 */

#pragma once

#include "model/drive_mech.hpp"
#include "smcp/GroupConsole/group.hpp"
#include "smcp/Server/server.hpp"
#include "smcp/transport/ilink.hpp"
#include "smcp/transport/session.hpp"

class MServer : public smcp::Server<smcp::kMechCount, smcp::msg::kMaxConsoles> {
public:
    using Base = smcp::Server<smcp::kMechCount, smcp::msg::kMaxConsoles>;
    using Base::kSessionCount;

    explicit MServer(smcp::ILink& link, smcp::Node::ClockFn clock) noexcept
        : Base(link, clock)
        , _sessionBank(*this)
        , _mechs(*this)
    {}

    [[nodiscard]] DriveMech& mech(uint8_t id) noexcept { return _mechs[id]; }
    [[nodiscard]] const DriveMech& mech(uint8_t id) const noexcept { return _mechs[id]; }

protected:
    [[nodiscard]] smcp::msg::ErrorCode acceptSelect(uint8_t console_id,
                                                    smcp::Selection selected) const noexcept override
    {
        /* Сегмент после запроса: чужие Selected ∪ предлагаемая маска этой консоли. */
        smcp::Selection merged = selected;
        for (uint8_t mid = 0; mid < smcp::kMechCount; ++mid) {
            const DriveMech& m = _mechs[mid];
            if (m.isSelected() && !m.isSelectedBy(console_id)) {
                merged.add(mid);
            }
        }
        if (merged.count() > DriveMech::kMaxSelected) {
            return smcp::msg::ErrorCode::SelectLimit;
        }
        return smcp::msg::ErrorCode::Ok;
    }

private:
    /* После Base registry: SessionConsole регистрируется в Node::sessions(). */
    smcp::SessionBank<kSessionCount, smcp::SessionConsole> _sessionBank;
    DriveMechBank<smcp::kMechCount> _mechs;
};
