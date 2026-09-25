#include "digital_pin.hpp"
#include <util/delay.h>

namespace {

/** 24 реле: PF0..PF7, PK0..PK7, PC0..PC7. Ноль включает реле. */
DigitalPin<Port::F, 0> f0;
DigitalPin<Port::F, 1> f1;
DigitalPin<Port::F, 2> f2;
DigitalPin<Port::F, 3> f3;
DigitalPin<Port::F, 4> f4;
DigitalPin<Port::F, 5> f5;
DigitalPin<Port::F, 6> f6;
DigitalPin<Port::F, 7> f7;
DigitalPin<Port::K, 0> k0;
DigitalPin<Port::K, 1> k1;
DigitalPin<Port::K, 2> k2;
DigitalPin<Port::K, 3> k3;
DigitalPin<Port::K, 4> k4;
DigitalPin<Port::K, 5> k5;
DigitalPin<Port::K, 6> k6;
DigitalPin<Port::K, 7> k7;
DigitalPin<Port::C, 0> c0;
DigitalPin<Port::C, 1> c1;
DigitalPin<Port::C, 2> c2;
DigitalPin<Port::C, 3> c3;
DigitalPin<Port::C, 4> c4;
DigitalPin<Port::C, 5> c5;
DigitalPin<Port::C, 6> c6;
DigitalPin<Port::C, 7> c7;

BIF::IDigitalPin* const relays[] = {
    &f0, &f1, &f2, &f3, &f4, &f5, &f6, &f7, &k0, &k1, &k2, &k3,
    &k4, &k5, &k6, &k7, &c0, &c1, &c2, &c3, &c4, &c5, &c6, &c7,
};

constexpr uint8_t kRelayCount = sizeof(relays) / sizeof(relays[0]);

void relaysInit()
{
    for (BIF::IDigitalPin* pin : relays) {
        pin->Set();
        pin->Init(BIF::PinMode::Output);
    }
}

void relaysApply(uint8_t index)
{
    for (uint8_t i = 0; i < kRelayCount; ++i) {
        if (i == index)
            relays[i]->Clear();
        else
            relays[i]->Set();
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
        if (step == kRelayCount)
            step = 0;
    }
}
