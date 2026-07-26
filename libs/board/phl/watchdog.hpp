#pragma once

/**
 * PHL::WatchDog — независимый watchdog (IWDG) на LSI.
 *
 * После begin() остановить нельзя; нужно периодически kick().
 * В debug IWDG замораживается (__HAL_DBGMCU_FREEZE_IWDG), чтобы не сбрасывать при breakpoint.
 *
 * Диапазон @ LSI=32 кГц: ~1 мс … ~32768 мс.
 */

#include <cstdint>

#include <stm32f4xx_hal.h>

namespace PHL {

class WatchDog {
public:
#if defined(LSI_VALUE)
    static constexpr uint32_t kLsiHz = LSI_VALUE;
#else
    static constexpr uint32_t kLsiHz = 32000u;
#endif

    static constexpr uint32_t kReloadMax = 0x0FFFu; ///< IWDG_RLR
    static constexpr uint32_t kMinTimeoutMs = 1u;
    /** (kReloadMax+1) * 256 / LSI * 1000 ≈ 32768 мс при 32 кГц. */
    static constexpr uint32_t kMaxTimeoutMs =
        static_cast<uint32_t>((static_cast<uint64_t>(kReloadMax) + 1u) * 256u * 1000u / kLsiHz);

    /**
     * Подобрать prescaler/reload под @a timeout_ms и запустить IWDG.
     * @return false, если timeout вне диапазона или HAL_IWDG_Init неудачен.
     */
    [[nodiscard]] bool begin(uint32_t timeout_ms) noexcept
    {
        uint32_t prescaler = 0u;
        uint32_t reload = 0u;
        if (!calcConfig(timeout_ms, prescaler, reload)) {
            return false;
        }

        /* Не сбрасывать MCU, пока ядро на breakpoint. */
        __HAL_DBGMCU_FREEZE_IWDG();

        _hiwdg.Instance = IWDG;
        _hiwdg.Init.Prescaler = prescaler;
        _hiwdg.Init.Reload = reload;

        if (HAL_IWDG_Init(&_hiwdg) != HAL_OK) {
            _enabled = false;
            _timeoutMs = 0u;
            return false;
        }

        _enabled = true;
        _timeoutMs = timeout_ms;
        _prescaler = prescaler;
        _reload = reload;
        return true;
    }

    /** Перезагрузить счётчик (кормить собаку). */
    void kick() noexcept
    {
        if (_enabled) {
            (void)HAL_IWDG_Refresh(&_hiwdg);
        }
    }

    [[nodiscard]] bool isEnabled() const noexcept { return _enabled; }

    /** Запрошенный timeout при begin (мс). */
    [[nodiscard]] uint32_t timeoutMs() const noexcept { return _timeoutMs; }

    [[nodiscard]] uint32_t prescaler() const noexcept { return _prescaler; }
    [[nodiscard]] uint32_t reload() const noexcept { return _reload; }

    /** Последний сброс был из‑за IWDG? */
    [[nodiscard]] static bool causedReset() noexcept
    {
        return __HAL_RCC_GET_FLAG(RCC_FLAG_IWDGRST) != 0u;
    }

    /** Сбросить флаги RCC CSR (IWDG/WWDG/PIN/… reset). */
    static void clearResetFlags() noexcept
    {
        __HAL_RCC_CLEAR_RESET_FLAGS();
    }

    /**
     * Подобрать PR/RLR под timeout_ms.
     * timeout ≈ reload * prescaler_div * 1000 / LSI  (reload в 1…0xFFF).
     */
    [[nodiscard]] static bool calcConfig(uint32_t timeout_ms,
                                         uint32_t& outPrescaler,
                                         uint32_t& outReload) noexcept
    {
        if (timeout_ms < kMinTimeoutMs || timeout_ms > kMaxTimeoutMs) {
            return false;
        }

        static constexpr struct {
            uint32_t hal;
            uint32_t div;
        } kTable[] = {
            {IWDG_PRESCALER_4, 4u},
            {IWDG_PRESCALER_8, 8u},
            {IWDG_PRESCALER_16, 16u},
            {IWDG_PRESCALER_32, 32u},
            {IWDG_PRESCALER_64, 64u},
            {IWDG_PRESCALER_128, 128u},
            {IWDG_PRESCALER_256, 256u},
        };

        for (const auto& e : kTable) {
            /* reload = timeout_ms * LSI / (div * 1000), округление вверх. */
            const uint64_t num = static_cast<uint64_t>(timeout_ms) * kLsiHz;
            const uint64_t den = static_cast<uint64_t>(e.div) * 1000u;
            uint64_t reload = (num + den - 1u) / den;
            if (reload == 0u) {
                reload = 1u;
            }
            if (reload <= kReloadMax) {
                outPrescaler = e.hal;
                outReload = static_cast<uint32_t>(reload);
                return true;
            }
        }
        return false;
    }

private:
    IWDG_HandleTypeDef _hiwdg{};
    bool _enabled = false;
    uint32_t _timeoutMs = 0u;
    uint32_t _prescaler = 0u;
    uint32_t _reload = 0u;
};

} // namespace PHL
