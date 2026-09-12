/**
 * @file drive_mech.hpp
 * @brief Конкретный IMech сегмента: register в smcp::IServer.
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <utility>

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

    void select(uint8_t console_id) noexcept override;
    void block(bool blocked) noexcept override;
    void setTarget(const smcp::MotionTarget& target) noexcept override;
    void resetFault() noexcept override;

private:
    Type _type = Type::Rope;
    static uint8_t s_selectedCount;
};

template <uint8_t N>
class DriveMechBank {
public:
    explicit DriveMechBank(smcp::IServer& owner) noexcept
        : DriveMechBank(owner, std::make_index_sequence<N>{})
    {}

    [[nodiscard]] DriveMech& operator[](uint8_t i) noexcept { return _items[i]; }
    [[nodiscard]] const DriveMech& operator[](uint8_t i) const noexcept { return _items[i]; }

private:
    template <std::size_t... I>
    DriveMechBank(smcp::IServer& owner, std::index_sequence<I...>) noexcept
        : _items{DriveMech{owner, static_cast<uint8_t>(I)}...}
    {}

    DriveMech _items[N];
};
