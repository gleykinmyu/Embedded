/**
 * @file can_link.hpp
 * @brief ILink над BIF::CAN::ICAN: Frame ↔ smcp::Packet.
 */

#pragma once

#include <cstdint>

#include "ican.hpp"
#include "smcp/ilink.hpp"

namespace smcp {

class CanLink : public ILink {
public:
    explicit CanLink(BIF::CAN::ICAN& can, uint8_t node_id = 1u) noexcept;

    [[nodiscard]] BIF::CAN::ICAN& can() noexcept { return _can; }
    [[nodiscard]] const BIF::CAN::ICAN& can() const noexcept { return _can; }

    [[nodiscard]] bool isOpen() noexcept override;

protected:
    [[nodiscard]] bool write(const Packet& pkt) noexcept override;
    [[nodiscard]] bool read(Packet& out) noexcept override;

private:
    BIF::CAN::ICAN& _can;
};

} // namespace smcp
