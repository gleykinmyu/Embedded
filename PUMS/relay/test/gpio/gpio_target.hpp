#ifndef GPIO_TARGET_HPP
#define GPIO_TARGET_HPP

/**
 * Один камень на сборку. Макрос уже задал компилятор.
 * Имя файла ветки — общее для неё: avr, stm32f4xx, esp32, esp32s3.
 */
#if defined(__AVR__)
#include "avr.hpp"
#elif defined(STM32F405xx) || defined(STM32F415xx) || defined(STM32F407xx) || defined(STM32F417xx) \
   || defined(STM32F427xx) || defined(STM32F437xx) || defined(STM32F429xx) || defined(STM32F439xx) \
   || defined(STM32F401xC) || defined(STM32F401xE) || defined(STM32F410Tx) || defined(STM32F410Cx) \
   || defined(STM32F410Rx) || defined(STM32F411xE) || defined(STM32F446xx) || defined(STM32F469xx) \
   || defined(STM32F479xx) || defined(STM32F412Cx) || defined(STM32F412Rx) || defined(STM32F412Vx) \
   || defined(STM32F412Zx) || defined(STM32F413xx) || defined(STM32F423xx)
#include "stm32f4xx.hpp"
#define GPIO_STM32F4 1
#elif defined(CONFIG_IDF_TARGET_ESP32)
#include "esp32.hpp"
#define GPIO_ESP 1
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
#include "esp32s3.hpp"
#define GPIO_ESP 1
#else
#error GPIO: камень не задан компилятором
#endif

#endif
