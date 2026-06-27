#pragma once

#include <stddef.h>
#include <stdint.h>
#include "config.h"

namespace VectorTableRam {
/** Полная таблица векторов Cortex-M: 16 системных + периферийные слоты. */
inline constexpr size_t kVectorTableWordCount = 16u + PHL::IRQ::kNvicLastIRQn + 1u;
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

/** Прямая запись слота [0 .. PHL::IRQ::kVectorTableWordCount) (слот 0 — начальный SP). */
void SetWord(size_t index, uint32_t value) noexcept;

} // namespace VectorTableRam
