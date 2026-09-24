#if defined(__AVR__)
#include "avr.inl"
#elif defined(GPIO_STM32F4)
#include "stm32f4xx.inl"
#elif defined(GPIO_ESP)
#include "esp.inl"
#else
#error GPIO: камень не задан компилятором
#endif
