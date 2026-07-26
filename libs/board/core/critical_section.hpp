/**
 * @file critical_section.hpp
 * @brief Критическая секция Cortex-M через PRIMASK.
 *
 * RAII: `CriticalSection cs;` — запрет IRQ до конца блока.
 * Для ISerial::lock/unlock: `saveAndDisable()` / `restore(primask)`.
 */

#pragma once

#include <cstdint>

#include <stm32f4xx.h>

class CriticalSection {
    uint32_t _primask;

public:
    CriticalSection() noexcept
        : _primask(__get_PRIMASK())
    {
        __disable_irq();
    }

    ~CriticalSection() noexcept { __set_PRIMASK(_primask); }

    CriticalSection(const CriticalSection&) = delete;
    CriticalSection& operator=(const CriticalSection&) = delete;

    /** Запретить IRQ, вернуть прежний PRIMASK (для ручного lock/unlock). */
    [[nodiscard]] static uint32_t saveAndDisable() noexcept
    {
        const uint32_t primask = __get_PRIMASK();
        __disable_irq();
        return primask;
    }

    /** Восстановить PRIMASK, сохранённый через saveAndDisable(). */
    static void restore(uint32_t primask) noexcept { __set_PRIMASK(primask); }
};
