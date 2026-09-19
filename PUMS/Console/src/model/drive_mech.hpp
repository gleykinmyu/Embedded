/**
 * @file drive_mech.hpp
 * @brief Конкретный IMech сегмента: register в smcp::IServer.
 */

#pragma once

#include <cstdint>

#include "obj_bank.hpp"
#include "smcp/mech.hpp"
#include "smcp/Server/server.hpp"

/** IMech на сервере: select сразу, лимит Selected. */
class DriveMech : public smcp::IMech {
public:
    static constexpr uint8_t kMaxSelected = 3u;

    enum class Type : uint8_t {
        Rope,
        Chain,
    };

    DriveMech(smcp::IServer& owner, uint8_t id, Type type = Type::Rope) noexcept;

    [[nodiscard]] Type type() const noexcept { return _type; }

    bool select(uint8_t console_id) noexcept override;
    void block(bool blocked) noexcept override;
    void setTarget(const smcp::MotionTarget& target) noexcept override;
    void resetFault() noexcept override;

private:
    Type _type = Type::Rope;
    static uint8_t s_selectedCount;
};

/** Банк DriveMech[N]: ctor регистрирует каждый в IServer. */
template <uint8_t N>
using DriveMechBank = MISC::ObjBank<DriveMech, N>;
