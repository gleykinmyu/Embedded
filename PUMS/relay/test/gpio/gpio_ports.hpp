#ifndef GPIO_PORTS_HPP
#define GPIO_PORTS_HPP

/**
 * Список портов и ножек с линией. Подключается после PinIrq,
 * когда шаблон Port уже объявлен. Файл ветки тот же, что в gpio_target.hpp.
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
#elif defined(CONFIG_IDF_TARGET_ESP32)
#include "esp32.hpp"
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
#include "esp32s3.hpp"
#else
#error GPIO: камень не задан компилятором
#endif

#endif
