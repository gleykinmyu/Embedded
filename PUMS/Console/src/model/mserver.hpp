/**
 * @file mserver.hpp
 * @brief Сервер сегмента проекта: smcp::Server + SessionBank + DriveMechBank.
 */

#pragma once

#include "ican.hpp"
#include "model/drive_mech.hpp"
#include "smcp/Console/group.hpp"
#include "smcp/Server/server.hpp"
#include "smcp/transport/session.hpp"

class MServer : public smcp::Server<smcp::kMechCount, smcp::msg::kMaxConsoles> {
public:
    using Base = smcp::Server<smcp::kMechCount, smcp::msg::kMaxConsoles>;
    using Base::kSessionCount;

    explicit MServer(BIF::CAN::ICAN& can,
                     uint8_t server_id = smcp::msg::kServerIdMin) noexcept
        : Base(can, server_id)
        , _sessionBank(*this)
        , _mechs(*this)
    {
        for (smcp::Session& s : _sessionBank) {
            s.start();
        }
    }

    [[nodiscard]] smcp::Session& sessionAt(uint8_t slot) noexcept { return _sessionBank[slot]; }
    [[nodiscard]] const smcp::Session& sessionAt(uint8_t slot) const noexcept
    {
        return _sessionBank[slot];
    }

    [[nodiscard]] DriveMech& mech(uint8_t id) noexcept { return _mechs[id]; }
    [[nodiscard]] const DriveMech& mech(uint8_t id) const noexcept { return _mechs[id]; }

protected:
    [[nodiscard]] smcp::msg::ErrorCode acceptSelect(uint8_t console_id,
                                                    smcp::Selection selected) const noexcept override
    {
        (void)console_id;
        if (selected.count() > DriveMech::kMaxSelected) {
            return smcp::msg::ErrorCode::SelectLimit;
        }
        return smcp::msg::ErrorCode::Ok;
    }

private:
    /* После Base registry: SessionBank регистрирует объекты в Node::sessions(). */
    smcp::SessionBank<kSessionCount> _sessionBank;
    DriveMechBank<smcp::kMechCount> _mechs;
};
