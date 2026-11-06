/**
 * @file mserver.hpp
 * @brief Сервер сегмента проекта: segment::Server + Session bank + оси.
 */

#pragma once

#include "smcp/show/group.hpp"
#include "segment/server.hpp"
#include "smcp/ilink.hpp"
#include "smcp/session.hpp"

/** Leaf-ось: setTarget / resetFault — привод (пока stub). */
class MAxis : public smcp::segment::server::Axis {
public:
    using smcp::segment::server::Axis::Axis;

    [[nodiscard]] bool setTarget(const smcp::axis::MotionTarget& target) noexcept override
    {
        (void)target;
        /* TODO: привод — Moving / лимиты. */
        return true;
    }

    [[nodiscard]] bool resetFault() noexcept override { return true; }
};

class MServer : public smcp::segment::Server<24, smcp::segment::kMaxConsoles> {
public:
    using Base = smcp::segment::Server<24, smcp::segment::kMaxConsoles>;
    using Base::kSessionCount;
    using Base::kMechCount;
    static_assert(kMechCount <= smcp::kMechCount);

    /** Тестовый PIN-хэш сервиса (пока не NVM). */
    static constexpr uint32_t kServicePinHash = 0xA5A5A5A5u;

    explicit MServer(smcp::ILink& link, smcp::Node::ClockFn clock) noexcept
        : Base(link, clock)
        , _sessionBank(*this)
        , _axes(*this)
    {
        setServicePinHash(kServicePinHash);
    }

    [[nodiscard]] MAxis& axis(uint8_t id) noexcept { return _axes[id]; }
    [[nodiscard]] const MAxis& axis(uint8_t id) const noexcept { return _axes[id]; }

protected:
    [[nodiscard]] smcp::SessionSlots& sessions() noexcept override { return _sessionBank; }

    [[nodiscard]] smcp::segment::vError acceptSelect(uint8_t console_id,
                                                     smcp::segment::Selection selected) const noexcept override
    {
        /* TEST: оси 11…14 по одной можно, вместе — зона SelectLimit. */
        smcp::segment::Selection combo;
        combo.add(7u);
        combo.add(8u);
        combo.add(9u);
        if ((selected & combo).count() > 1u) {
            return smcp::segment::Error::SelectLimit;
        }

        /* Сегмент после запроса: чужие Selected ∪ предлагаемая маска этой консоли. */
        smcp::segment::Selection merged = selected;
        for (uint8_t mid = 0; mid < kMechCount; ++mid) {
            const MAxis& a = _axes[mid];
            if (a.isSelected() && !a.isSelectedBy(console_id)) {
                merged.add(mid);
            }
        }
        if (merged.count() > smcp::segment::server::Axis::kMaxSelected) {
            return smcp::segment::Error::SelectLimit;
        }
        return smcp::session::Error::None;
    }

private:
    smcp::SessionBank<kSessionCount, smcp::segment::server::Session> _sessionBank;
    smcp::segment::server::AxisBank<MAxis, kMechCount> _axes;
};
