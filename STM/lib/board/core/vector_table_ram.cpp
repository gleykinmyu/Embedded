#include <string.h>
#include <stm32f4xx.h>
#include "vector_table_ram.hpp"

extern "C" {
/* Таблица из startup_stm32f407xx.s (.isr_vector) */
extern uint32_t g_pfnVectors[];
}

namespace {

constexpr size_t kWords = VectorTableRam::kVectorTableWordCount;

/* VTOR: выравнивание не меньше размера таблицы, округлённого до степени двойки (для F407 — 512). */
alignas(512) uint32_t s_ram_vectors[kWords];

} // namespace

namespace VectorTableRam {

void Init() noexcept
{
    const bool irq_unlock = (__get_PRIMASK() == 0U);
    __disable_irq();
    memcpy(s_ram_vectors, g_pfnVectors, sizeof(s_ram_vectors));
    SCB->VTOR = reinterpret_cast<uint32_t>(s_ram_vectors);
    __DSB();
    __ISB();
    if (irq_unlock) {
        __enable_irq();
    }
}

void SetIrqHandler(IRQn_Type irqn, void (*handler)()) noexcept
{
    if (irqn < 0 || handler == nullptr) {
        return;
    }
    const size_t idx = 16u + static_cast<size_t>(irqn);
    if (idx >= kWords) {
        return;
    }
    const bool irq_unlock = (__get_PRIMASK() == 0U);
    __disable_irq();
    s_ram_vectors[idx] = reinterpret_cast<uint32_t>(handler);
    __DSB();
    __ISB();
    if (irq_unlock) {
        __enable_irq();
    }
}

void RestoreDefaultIrqHandler(IRQn_Type irqn) noexcept
{
    if (irqn < 0) {
        return;
    }
    const size_t idx = 16u + static_cast<size_t>(irqn);
    if (idx >= kWords) {
        return;
    }
    const bool irq_unlock = (__get_PRIMASK() == 0U);
    __disable_irq();
    s_ram_vectors[idx] = g_pfnVectors[idx];
    __DSB();
    __ISB();
    if (irq_unlock) {
        __enable_irq();
    }
}

void SetWord(size_t index, uint32_t value) noexcept
{
    if (index >= kWords) {
        return;
    }
    const bool irq_unlock = (__get_PRIMASK() == 0U);
    __disable_irq();
    s_ram_vectors[index] = value;
    __DSB();
    __ISB();
    if (irq_unlock) {
        __enable_irq();
    }
}

} // namespace VectorTableRam
