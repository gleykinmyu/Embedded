#pragma once

/**
 * IDigitalPin на GPIO ESP32. Номер ноги — параметр шаблона.
 * Set поднимает выход, Clear опускает.
 * На выходе включается и вход: без этого gpio_get_level на ESP32 читает 0,
 * и Read/Toggle не видят уровень выхода.
 */
#include "idigital_pin.hpp"

#include "driver/gpio.h"

template <gpio_num_t Pin>
class DigitalPin : public BIF::IDigitalPin {
    static_assert(Pin >= GPIO_NUM_0 && Pin < GPIO_NUM_MAX, "пин вне GPIO_NUM_MAX");

public:
    void Init(BIF::PinMode mode, BIF::PinPull pull = BIF::PinPull::None) override
    {
        gpio_config_t cfg = {};
        cfg.pin_bit_mask = 1ULL << static_cast<uint32_t>(Pin);
        cfg.intr_type = GPIO_INTR_DISABLE;
        cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
        if (mode == BIF::PinMode::Input) {
            cfg.mode = GPIO_MODE_INPUT;
            cfg.pull_up_en = (pull == BIF::PinPull::Up) ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE;
        } else {
            cfg.mode = GPIO_MODE_INPUT_OUTPUT;
            cfg.pull_up_en = GPIO_PULLUP_DISABLE;
        }
        gpio_config(&cfg);
        if (mode == BIF::PinMode::Output)
            gpio_set_level(Pin, 0);
    }

    void Set() override { gpio_set_level(Pin, 1); }

    void Clear() override { gpio_set_level(Pin, 0); }

    void Toggle() override { gpio_set_level(Pin, Read() ? 0 : 1); }

    [[nodiscard]] bool Read() const override { return gpio_get_level(Pin) != 0; }
};
