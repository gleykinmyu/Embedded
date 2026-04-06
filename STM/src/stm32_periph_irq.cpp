// Векторы PHL (USART, SPI, I2C, CAN, SDIO): в отдельном TU в src/, чтобы они всегда
// попадали в линковку. В libboard.a отдельный .o с IRQ часто не извлекается линкером
// (символ уже «закрыт» weak из startup → Default_Handler).
#include "core/irq.h"

extern "C" {

#define CALL_HANDLER(NAME, METHOD) \
    constexpr uint8_t idx = static_cast<uint8_t>(PHL::Index::NAME); \
    if (IRQ::Registry[idx]) { \
        static_cast<IRQ::IHandler<PHL::ID::NAME>*>(IRQ::Registry[idx])->METHOD(); \
    }

#define M_GEN_ISR_UART(NAME, T_, A_, ORIG) void ORIG##_IRQHandler() { CALL_HANDLER(NAME, IrqHandler) }
#define M_GEN_ISR_SPI(NAME, T_, A_, ORIG)  void ORIG##_IRQHandler() { CALL_HANDLER(NAME, IrqHandler) }
#define M_GEN_ISR_SDIO(NAME, T_, A_, ORIG) void ORIG##_IRQHandler() { CALL_HANDLER(NAME, IrqHandler) }

#define M_GEN_ISR_I2C(NAME, T_, A_, ORIG) \
    void ORIG##_EV_IRQHandler() { CALL_HANDLER(NAME, EventIrqHandler) } \
    void ORIG##_ER_IRQHandler() { CALL_HANDLER(NAME, ErrorIrqHandler) }

#define M_GEN_ISR_CAN(NAME, T_, A_, ORIG) \
    void ORIG##_TX_IRQHandler()  { CALL_HANDLER(NAME, TxIrqHandler)  } \
    void ORIG##_RX0_IRQHandler() { CALL_HANDLER(NAME, Rx0IrqHandler) } \
    void ORIG##_RX1_IRQHandler() { CALL_HANDLER(NAME, Rx1IrqHandler) } \
    void ORIG##_SCE_IRQHandler() { CALL_HANDLER(NAME, SceIrqHandler) }

#define M_DISPATCH_ISR(NAME, T_, A_, ORIG) M_GEN_ISR_##T_(NAME, T_, A_, ORIG)

PHL_PERIPH_LIST(M_DISPATCH_ISR)

#undef CALL_HANDLER
#undef M_GEN_ISR_UART
#undef M_GEN_ISR_SPI
#undef M_GEN_ISR_I2C
#undef M_GEN_ISR_CAN
#undef M_GEN_ISR_SDIO
#undef M_DISPATCH_ISR

} // extern "C"
