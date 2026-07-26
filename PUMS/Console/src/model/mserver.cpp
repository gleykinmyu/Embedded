/**
 * @file mserver.cpp
 * @brief MServer: Select / Telemetry поверх очередей Packet.
 */

#include "model/mserver.hpp"

#include <variant>

MServer::MServer(uint8_t server_id) noexcept : _server_id(server_id)
{
    for (std::size_t i = 0; i < smcp::kMechCount; ++i) {
        _mechs[i] = DriveMech(static_cast<uint8_t>(i), DriveMech::Type::Rope);
    }
}

DriveMech& MServer::mech(uint8_t mech_id) noexcept
{
    if (mech_id >= smcp::kMechCount) {
        return _mechs[0];
    }
    return _mechs[mech_id];
}

const DriveMech& MServer::mech(uint8_t mech_id) const noexcept
{
    if (mech_id >= smcp::kMechCount) {
        return _mechs[0];
    }
    return _mechs[mech_id];
}

void MServer::poll() noexcept
{
    smcp::msg::Packet pkt;
    while (_rx.pop(pkt)) {
        handlePacket(pkt);
    }
}

void MServer::handlePacket(const smcp::msg::Packet& pkt) noexcept
{
    if (!pkt.hdr.isAddressedTo(_server_id)) {
        return;
    }

    if (const auto* sel = std::get_if<smcp::msg::Select>(&pkt.body)) {
        handleSelect(pkt.hdr, *sel);
    }
}

void MServer::handleSelect(const smcp::msg::Header& hdr, const smcp::msg::Select& body) noexcept
{
    if (!smcp::msg::isConsoleId(hdr.src_id)) {
        return;
    }

    if (body.action == smcp::msg::Select::Action::Set) {
        /* Полная маска владения src на этом сегменте. */
        for (uint8_t id = 0; id < smcp::kMechCount; ++id) {
            DriveMech& m = _mechs[id];
            bool changed = false;

            if (body.selection.contains(id)) {
                changed = m.select(hdr.src_id);
            } else if (m.isSelectedBy(hdr.src_id)) {
                changed = m.select(smcp::kSelectOwnerNone);
            }

            if (changed) {
                (void)pushTelemetry(id);
            }
        }
        return;
    }

    const bool select = (body.action == smcp::msg::Select::Action::Select);
    if (!select && body.action != smcp::msg::Select::Action::Deselect) {
        return;
    }

    for (uint8_t id = 0; id < smcp::kMechCount; ++id) {
        if (!body.selection.contains(id)) {
            continue;
        }

        DriveMech& m = _mechs[id];
        bool changed = false;

        if (select) {
            changed = m.select(hdr.src_id);
        } else if (m.isSelectedBy(hdr.src_id)) {
            changed = m.select(smcp::kSelectOwnerNone);
        }

        if (changed) {
            (void)pushTelemetry(id);
        }
    }
}

bool MServer::pushTelemetry(uint8_t mech_id) noexcept
{
    if (mech_id >= smcp::kMechCount) {
        return false;
    }

    const DriveMech& m = _mechs[mech_id];

    smcp::msg::Telemetry tel{};
    tel.mech_id = mech_id;
    tel.select_owner_id = m.select_owner_id();
    tel.position_mm = m.position();
    tel.status = m.status();

    smcp::msg::Packet pkt{};
    pkt.hdr.prio = smcp::msg::defaultPrio(smcp::msg::MsgId::Telemetry);
    pkt.hdr.src_id = _server_id;
    pkt.hdr.dst_id = smcp::msg::kBroadcastId;
    pkt.hdr.msg_id = smcp::msg::MsgId::Telemetry;
    pkt.hdr.pkt_id = nextPktId();
    pkt.body = tel;

    return _tx.push(pkt);
}
