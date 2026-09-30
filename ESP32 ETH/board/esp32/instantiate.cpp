// Явные инстансы: шаблоны пина и UART собираются вместе с приложением.
#include "digital_pin.hpp"
#include "serial.hpp"

template class DigitalPin<GPIO_NUM_2>;
template class Usart::Serial<UART_NUM_0>;
template class Usart::Serial<UART_NUM_1>;
template class Usart::Serial<UART_NUM_2>;
