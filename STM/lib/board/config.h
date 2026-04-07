#pragma once
#include <stm32f4xx.h>
#include <stm32f4xx_hal.h>
//#include "Debug.h"

namespace PHL {
namespace IRQ {
/** Периферийные IRQ: индекс NVIC 0 .. kNvicUserIrqCount - 1 (= FPU_IRQn). */
inline constexpr size_t kNvicLastIRQn = static_cast<size_t>(FPU_IRQn) + 1u;

} }

typedef HAL_StatusTypeDef CHW_Status;
#define PHL_UART_DEBUG PHL::uart1