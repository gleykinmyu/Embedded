#include "crash_dump.hpp"

#include <stm32f4xx.h>

namespace {

void enableBkpsram() noexcept
{
    RCC->APB1ENR |= RCC_APB1ENR_PWREN;
    (void)RCC->APB1ENR;
    PWR->CR |= PWR_CR_DBP;
    RCC->AHB1ENR |= RCC_AHB1ENR_BKPSRAMEN;
    (void)RCC->AHB1ENR;
}

[[nodiscard]] CrashDump::Record* slot() noexcept
{
    return reinterpret_cast<CrashDump::Record*>(BKPSRAM_BASE);
}

[[noreturn]] void captureAndReset(CrashDump::Kind kind, const uint32_t* stacked) noexcept
{
    __disable_irq();
    enableBkpsram();

    CrashDump::Record* r = slot();
    r->magic = CrashDump::kMagic;
    r->kind = kind;
    r->cfsr = SCB->CFSR;
    r->hfsr = SCB->HFSR;
    r->dfsr = SCB->DFSR;
    r->mmfar = SCB->MMFAR;
    r->bfar = SCB->BFAR;
    r->afsr = SCB->AFSR;
    r->r0 = stacked[0];
    r->r1 = stacked[1];
    r->r2 = stacked[2];
    r->r3 = stacked[3];
    r->r12 = stacked[4];
    r->lr = stacked[5];
    r->pc = stacked[6];
    r->xpsr = stacked[7];
    r->msp = __get_MSP();
    r->psp = __get_PSP();
    __DSB();
    __ISB();

    NVIC_SystemReset();
    for (;;) {
    }
}

} // namespace

namespace CrashDump {

void enableFaultTraps() noexcept
{
    SCB->SHCSR |= SCB_SHCSR_USGFAULTENA_Msk | SCB_SHCSR_BUSFAULTENA_Msk
        | SCB_SHCSR_MEMFAULTENA_Msk;
}

bool take(Record& out) noexcept
{
    enableBkpsram();
    Record* r = slot();
    if (r->magic != kMagic) {
        return false;
    }
    out = *r;
    r->magic = 0u;
    r->kind = Kind::None;
    __DSB();
    return true;
}

const char* kindName(Kind k) noexcept
{
    switch (k) {
    case Kind::HardFault:
        return "HardFault";
    case Kind::MemManage:
        return "MemManage";
    case Kind::BusFault:
        return "BusFault";
    case Kind::UsageFault:
        return "UsageFault";
    default:
        return "Unknown";
    }
}

} // namespace CrashDump

extern "C" {

void HardFault_Handler_C(uint32_t* stacked);
void MemManage_Handler_C(uint32_t* stacked);
void BusFault_Handler_C(uint32_t* stacked);
void UsageFault_Handler_C(uint32_t* stacked);

__attribute__((naked)) void HardFault_Handler(void)
{
    __asm volatile(
        "tst lr, #4\n"
        "ite eq\n"
        "mrseq r0, msp\n"
        "mrsne r0, psp\n"
        "b HardFault_Handler_C\n");
}

void HardFault_Handler_C(uint32_t* stacked)
{
    captureAndReset(CrashDump::Kind::HardFault, stacked);
}

__attribute__((naked)) void MemManage_Handler(void)
{
    __asm volatile(
        "tst lr, #4\n"
        "ite eq\n"
        "mrseq r0, msp\n"
        "mrsne r0, psp\n"
        "b MemManage_Handler_C\n");
}

void MemManage_Handler_C(uint32_t* stacked)
{
    captureAndReset(CrashDump::Kind::MemManage, stacked);
}

__attribute__((naked)) void BusFault_Handler(void)
{
    __asm volatile(
        "tst lr, #4\n"
        "ite eq\n"
        "mrseq r0, msp\n"
        "mrsne r0, psp\n"
        "b BusFault_Handler_C\n");
}

void BusFault_Handler_C(uint32_t* stacked)
{
    captureAndReset(CrashDump::Kind::BusFault, stacked);
}

__attribute__((naked)) void UsageFault_Handler(void)
{
    __asm volatile(
        "tst lr, #4\n"
        "ite eq\n"
        "mrseq r0, msp\n"
        "mrsne r0, psp\n"
        "b UsageFault_Handler_C\n");
}

void UsageFault_Handler_C(uint32_t* stacked)
{
    captureAndReset(CrashDump::Kind::UsageFault, stacked);
}

} // extern "C"
