#pragma once

#include <stddef.h>
#include <stdint.h>
#include "config.h"

namespace VectorTableRam {
/** Системные исключения Cortex-M (слоты 0…15, SysTick = 15). */
inline constexpr size_t kSystemVectorCount = 16u;
/** Полная таблица: 16 системных + периферийные (0…FPU_IRQn). */
inline constexpr size_t kVectorTableWordCount =
    kSystemVectorCount + PHL::IRQ::kPeripheralIrqCount;

static_assert(kVectorTableWordCount
                  == kSystemVectorCount + static_cast<size_t>(FPU_IRQn) + 1u,
              "vector table size must be 16 + (FPU_IRQn + 1)");
#if defined(STM32F407xx)
static_assert(kVectorTableWordCount == 98u, "STM32F407 vector table is 98 words");
#endif

/**
 * Копирует g_pfnVectors из Flash в RAM, выставляет SCB->VTOR.
 * Включает системные векторы (слот 15 — SysTick, PendSV и т.д.): отдельно
 * регистрировать SysTick не нужно — только вызывать до HAL_Init() / SysTick.
 */
void Init() noexcept;

/** Подмена вектора периферийного IRQ (IRQn >= 0), handler — void(void), например PHL member_route. */
void SetIrqHandler(IRQn_Type irqn, void (*handler)()) noexcept;

/** Вернуть слот IRQ к копии из g_pfnVectors (после Init — как во Flash-таблице). */
void RestoreDefaultIrqHandler(IRQn_Type irqn) noexcept;

/** Прямая запись слота [0 .. kVectorTableWordCount) (слот 0 — начальный SP). */
void SetWord(size_t index, uint32_t value) noexcept;

} // namespace VectorTableRam
