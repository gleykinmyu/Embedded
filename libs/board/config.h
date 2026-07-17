#pragma once
#include <stm32f4xx.h>
#include <stm32f4xx_hal.h>

namespace PHL {
namespace IRQ {
/**
 * Число периферийных IRQ: индексы NVIC 0 .. FPU_IRQn включительно
 * (на F407: FPU_IRQn == 81 → 82 слота).
 */
inline constexpr size_t kPeripheralIrqCount = static_cast<size_t>(FPU_IRQn) + 1u;
/** То же, что kPeripheralIrqCount (для PHL::IRQ::Vector static_assert). */
inline constexpr size_t kNvicLastIRQn = kPeripheralIrqCount;

} }

typedef HAL_StatusTypeDef CHW_Status;
#define PHL_UART_DEBUG PHL::uart1