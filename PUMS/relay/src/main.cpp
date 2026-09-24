#include "periph.hpp"
#include <util/delay.h>

namespace {

#define RELAYS(F) \
    F(GPIO::PortF::pin<0>) F(GPIO::PortF::pin<1>) F(GPIO::PortF::pin<2>) F(GPIO::PortF::pin<3>) \
    F(GPIO::PortF::pin<4>) F(GPIO::PortF::pin<5>) F(GPIO::PortF::pin<6>) F(GPIO::PortF::pin<7>) \
    F(GPIO::PortK::pin<0>) F(GPIO::PortK::pin<1>) F(GPIO::PortK::pin<2>) F(GPIO::PortK::pin<3>) \
    F(GPIO::PortK::pin<4>) F(GPIO::PortK::pin<5>) F(GPIO::PortK::pin<6>) F(GPIO::PortK::pin<7>) \
    F(GPIO::PortC::pin<0>) F(GPIO::PortC::pin<1>) F(GPIO::PortC::pin<2>) F(GPIO::PortC::pin<3>) \
    F(GPIO::PortC::pin<4>) F(GPIO::PortC::pin<5>) F(GPIO::PortC::pin<6>) F(GPIO::PortC::pin<7>)

void relaysInit()
{
#define INIT_ONE(pin) pin.Set(); pin.Init(GPIO::Mode::Output);
    RELAYS(INIT_ONE)
#undef INIT_ONE
}

void relaysOff()
{
#define SET_ONE(pin) pin.Set();
    RELAYS(SET_ONE)
#undef SET_ONE
}

/** Ноль включает реле. Шаг называет константный пин, без таблицы указателей. */
void relaysApply(uint8_t index)
{
    relaysOff();
    switch (index) {
    case 0: GPIO::PortF::pin<0>.Clear(); break;
    case 1: GPIO::PortF::pin<1>.Clear(); break;
    case 2: GPIO::PortF::pin<2>.Clear(); break;
    case 3: GPIO::PortF::pin<3>.Clear(); break;
    case 4: GPIO::PortF::pin<4>.Clear(); break;
    case 5: GPIO::PortF::pin<5>.Clear(); break;
    case 6: GPIO::PortF::pin<6>.Clear(); break;
    case 7: GPIO::PortF::pin<7>.Clear(); break;
    case 8: GPIO::PortK::pin<0>.Clear(); break;
    case 9: GPIO::PortK::pin<1>.Clear(); break;
    case 10: GPIO::PortK::pin<2>.Clear(); break;
    case 11: GPIO::PortK::pin<3>.Clear(); break;
    case 12: GPIO::PortK::pin<4>.Clear(); break;
    case 13: GPIO::PortK::pin<5>.Clear(); break;
    case 14: GPIO::PortK::pin<6>.Clear(); break;
    case 15: GPIO::PortK::pin<7>.Clear(); break;
    case 16: GPIO::PortC::pin<0>.Clear(); break;
    case 17: GPIO::PortC::pin<1>.Clear(); break;
    case 18: GPIO::PortC::pin<2>.Clear(); break;
    case 19: GPIO::PortC::pin<3>.Clear(); break;
    case 20: GPIO::PortC::pin<4>.Clear(); break;
    case 21: GPIO::PortC::pin<5>.Clear(); break;
    case 22: GPIO::PortC::pin<6>.Clear(); break;
    default: GPIO::PortC::pin<7>.Clear(); break;
    }
}

} // namespace

int main()
{
    relaysInit();

    uint8_t step = 0;
    for (;;) {
        relaysApply(step);
        _delay_ms(1000);
        step = static_cast<uint8_t>(step + 1u);
        if (step == 24u)
            step = 0;
    }
}
