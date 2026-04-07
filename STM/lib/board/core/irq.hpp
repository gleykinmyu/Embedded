#pragma once

/**
 * PHL::IRQ: Vector — NVIC + запись обработчика в RAM-таблицу векторов (VTOR).
 * Регистрация: SetIrqHandler(irq, &member_route<T>). Снятие: RestoreDefaultIrqHandler.
 * Нужен VectorTableRam::Init() до регистрации (см. STMboard / main).
 */
#include <cstddef>
#include <cstdint>
#include "phl_defs.hpp"
#include "vector_table_ram.hpp"


namespace PHL {
namespace IRQ {

    template <PHL::ID id, IRQn_Type irqn>
    class Vector {
    public:
        static constexpr IRQn_Type irq = irqn;

        static_assert(irqn >= 0, "only peripheral IRQn_Type");
        static_assert(static_cast<std::size_t>(irqn) < kNvicLastIRQn,
                      "IRQn out of NVIC range");

        void enable() const noexcept { NVIC_EnableIRQ(irq); }

        void disable() const noexcept { NVIC_DisableIRQ(irq); }

        void set_priority(std::uint32_t preemption_priority) const noexcept
        {
            NVIC_SetPriority(irq, preemption_priority);
        }

        template <typename T>
        void handler(T* self, void (T::*method)()) const noexcept
        {
            pmf_cell_<T>.self = self;
            pmf_cell_<T>.m = method;
            VectorTableRam::SetIrqHandler(irq, &member_route<T>);
        }

        void unregister_handler() const noexcept
        {
            VectorTableRam::RestoreDefaultIrqHandler(irq);
        }

    private:
        template <typename U>
        struct PmfCell {
            U* self = nullptr;
            void (U::*m)() = nullptr;
        };

        template <typename T>
        static inline PmfCell<T> pmf_cell_{};

        template <typename T>
        static void member_route() noexcept
        {
            auto& c = pmf_cell_<T>;
            (c.self->*c.m)();
        }
    };

} // namespace IRQ
} // namespace PHL
