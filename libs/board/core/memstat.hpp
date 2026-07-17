#pragma once

#include <cstddef>
#include <cstdint>

/**
 * Оценка использования SRAM на Cortex-M (STM32 + GNU ld).
 *
 * boot() — в начале main(), до глубоких вызовов: заливает watermark стека.
 * HAL_SYSTICK_Callback — в memstat.cpp, раз в 1 ms: только min MSP (без _sbrk).
 * trackFreeMin() — из main loop; resetFreeMin() — после вывода (окно 1 с).
 * Поля minSp/minSpWindow/minGap — под CriticalSection (core/critical_section.hpp).
 *
 * Если нужен свой HAL_SYSTICK_Callback — не линкуйте memstat.cpp или вызывайте
 * memstat::trackStackMin() из своего обработчика.
 */
namespace memstat {

struct Snapshot {
    /** .data + .bss */
    std::size_t staticBytes = 0u;
    /** Байт от heap start до текущего break (_sbrk). */
    std::size_t heapUsed = 0u;
    /** Резерв линкера (_Min_Heap_Size). */
    std::size_t heapReserved = 0u;
    /** Текущее: _estack - MSP. */
    std::size_t stackUsedNow = 0u;
    /** max(watermark, _estack - min MSP с boot). */
    std::size_t stackPeak = 0u;
    /** Резерв линкера (_Min_Stack_Size). */
    std::size_t stackReserved = 0u;
    /** MSP - heap break; 0 если пересечение. */
    std::size_t gapBytes = 0u;
    /** _estack - _sdata (размер SRAM под приложение). */
    std::size_t ramTotal = 0u;
};

/** Watermark стека + сброс min MSP. Вызвать первой строкой main(). */
void boot() noexcept;

/** Обновить min MSP; вызывается из HAL_SYSTICK_Callback или вручную. */
void trackStackMin() noexcept;

/** Учесть текущий зазор heap↔stack; min за окно — freeMinBytes(). */
void trackFreeMin() noexcept;

/** Начать новое окно (после вывода на экран). */
void resetFreeMin() noexcept;

[[nodiscard]] Snapshot snapshot() noexcept;

/** Минимальный gapBytes с последнего resetFreeMin(). */
[[nodiscard]] std::size_t freeMinBytes() noexcept;

/** Короткая строка для StatusBar, напр. "min 92k". */
void formatFreeMin(char* buf, std::size_t cap) noexcept;

/** Текущий зазор, напр. "free 96k". */
void format(const Snapshot& snap, char* buf, std::size_t cap) noexcept;

[[nodiscard]] std::size_t stackPeakBytes() noexcept;
[[nodiscard]] std::size_t stackUsedNowBytes() noexcept;
[[nodiscard]] std::size_t heapUsedBytes() noexcept;
[[nodiscard]] std::size_t heapStackGapBytes() noexcept;
[[nodiscard]] std::size_t staticBytes() noexcept;

} // namespace memstat
