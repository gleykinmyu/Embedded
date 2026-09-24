#include "periph.hpp"

namespace {

struct Spi {};
struct Uart {};

Spi spi;
Uart uart;

GPIO::W25Q<Spi> flash{spi, GPIO::PortD::pin<4>};
GPIO::W25Q<Spi> flash_on_irq_pin{spi, GPIO::PortD::pin<2>};
GPIO::SerialPort<Uart> serial{uart};
GPIO::Button button{GPIO::PortD::pin<0>};

template <class A, class B>
struct SameType {
    static constexpr bool value = false;
};
template <class A>
struct SameType<A, A> {
    static constexpr bool value = true;
};

static_assert(SameType<decltype(GPIO::PortD::pin<0>), const GPIO::PinIrq>::value,
              "PD0 — INT0");
static_assert(SameType<decltype(GPIO::PortD::pin<3>), const GPIO::PinIrq>::value,
              "PD3 — INT3");
static_assert(SameType<decltype(GPIO::PortD::pin<4>), const GPIO::Pin>::value,
              "PD4 — без линии");
static_assert(SameType<decltype(GPIO::PortD::pin<7>), const GPIO::Pin>::value,
              "PD7 — без линии");
static_assert(__is_base_of(GPIO::Pin, GPIO::PinIrq), "PinIrq — это Pin");

} // namespace

void gpio_design_demo()
{
    serial.bindPins(GPIO::PortD::pin<3>, GPIO::PortD::pin<4>);
    flash.select();
    flash.deselect();
    (void)button.attach(nullptr);
}
