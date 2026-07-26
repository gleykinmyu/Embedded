#pragma once

/**
 * PHL::Rtc — RTC STM32F4 на LSE (32.768 кГц).
 * Календарь сохраняется в backup-домене (метка RTC_BKP_DR0).
 * При первом запуске — дата/время сборки (__DATE__/__TIME__).
 */

#include <cstddef>
#include <cstdint>
#include <cstring>

#include <stm32f4xx_hal.h>

namespace PHL {

struct DateTime {
    uint16_t year = 2000u; ///< Полный год (2000…2099).
    uint8_t month = 1u;    ///< 1…12
    uint8_t day = 1u;      ///< 1…31
    uint8_t hour = 0u;     ///< 0…23
    uint8_t minute = 0u;   ///< 0…59
    uint8_t second = 0u;   ///< 0…59
};

class Rtc {
public:
    static constexpr uint32_t kBackupMagic = 0x32F2u;
    static constexpr uint32_t kBackupReg = RTC_BKP_DR0;

    /**
     * Включить LSE, RTC; при чистом backup — записать время сборки.
     * @return false, если LSE/HAL_RTC не поднялись.
     */
    [[nodiscard]] bool begin() noexcept
    {
        __HAL_RCC_PWR_CLK_ENABLE();
        HAL_PWR_EnableBkUpAccess();

        if (!enableLse()) {
            _ready = false;
            return false;
        }

        __HAL_RCC_RTC_CONFIG(RCC_RTCCLKSOURCE_LSE);
        __HAL_RCC_RTC_ENABLE();

        _hrtc.Instance = RTC;
        _hrtc.Init.HourFormat = RTC_HOURFORMAT_24;
        _hrtc.Init.AsynchPrediv = 127u;
        _hrtc.Init.SynchPrediv = 255u;
        _hrtc.Init.OutPut = RTC_OUTPUT_DISABLE;
        _hrtc.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;
        _hrtc.Init.OutPutType = RTC_OUTPUT_TYPE_OPENDRAIN;

        if (HAL_RTC_Init(&_hrtc) != HAL_OK) {
            _ready = false;
            return false;
        }

        const bool firstBoot = (HAL_RTCEx_BKUPRead(&_hrtc, kBackupReg) != kBackupMagic);
        if (firstBoot) {
            DateTime build{};
            if (!parseBuildDateTime(build) || !set(build)) {
                _ready = false;
                return false;
            }
            HAL_RTCEx_BKUPWrite(&_hrtc, kBackupReg, kBackupMagic);
        }

        _ready = true;
        return true;
    }

    [[nodiscard]] bool isReady() const noexcept { return _ready; }

    [[nodiscard]] bool get(DateTime& out) const noexcept
    {
        if (!_ready) {
            return false;
        }

        RTC_TimeTypeDef t{};
        RTC_DateTypeDef d{};
        if (HAL_RTC_GetTime(&_hrtc, &t, RTC_FORMAT_BIN) != HAL_OK) {
            return false;
        }
        if (HAL_RTC_GetDate(&_hrtc, &d, RTC_FORMAT_BIN) != HAL_OK) {
            return false;
        }

        out.year = static_cast<uint16_t>(2000u + d.Year);
        out.month = d.Month;
        out.day = d.Date;
        out.hour = t.Hours;
        out.minute = t.Minutes;
        out.second = t.Seconds;
        return true;
    }

    [[nodiscard]] bool set(const DateTime& in) noexcept
    {
        if (_hrtc.Instance == nullptr) {
            return false;
        }
        if (in.year < 2000u || in.year > 2099u ||
            in.month < 1u || in.month > 12u ||
            in.day < 1u || in.day > 31u ||
            in.hour > 23u || in.minute > 59u || in.second > 59u) {
            return false;
        }

        RTC_TimeTypeDef t{};
        t.Hours = in.hour;
        t.Minutes = in.minute;
        t.Seconds = in.second;
        t.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
        t.StoreOperation = RTC_STOREOPERATION_RESET;

        RTC_DateTypeDef d{};
        d.Year = static_cast<uint8_t>(in.year - 2000u);
        d.Month = in.month;
        d.Date = in.day;
        d.WeekDay = weekdayStm(in.year, in.month, in.day);

        if (HAL_RTC_SetTime(&_hrtc, &t, RTC_FORMAT_BIN) != HAL_OK) {
            return false;
        }
        if (HAL_RTC_SetDate(&_hrtc, &d, RTC_FORMAT_BIN) != HAL_OK) {
            return false;
        }
        return true;
    }

    /**
     * Время в формате FatFs (get_fattime).
     * @return 0, если RTC не готов.
     */
    [[nodiscard]] uint32_t fatTime() const noexcept
    {
        DateTime dt{};
        if (!get(dt)) {
            return 0u;
        }
        if (dt.year < 1980u || dt.year > 2107u) {
            return 0u;
        }

        const uint32_t y = static_cast<uint32_t>(dt.year - 1980u);
        return (y << 25) |
               (static_cast<uint32_t>(dt.month) << 21) |
               (static_cast<uint32_t>(dt.day) << 16) |
               (static_cast<uint32_t>(dt.hour) << 11) |
               (static_cast<uint32_t>(dt.minute) << 5) |
               (static_cast<uint32_t>(dt.second) >> 1);
    }

    [[nodiscard]] RTC_HandleTypeDef& handle() noexcept { return _hrtc; }
    [[nodiscard]] const RTC_HandleTypeDef& handle() const noexcept { return _hrtc; }

private:
    mutable RTC_HandleTypeDef _hrtc{};
    bool _ready = false;

    [[nodiscard]] static bool enableLse() noexcept
    {
        RCC_OscInitTypeDef osc{};
        osc.OscillatorType = RCC_OSCILLATORTYPE_LSE;
        osc.LSEState = RCC_LSE_ON;
        osc.PLL.PLLState = RCC_PLL_NONE;
        return HAL_RCC_OscConfig(&osc) == HAL_OK;
    }

    /** STM32 WeekDay: 1=Mon … 7=Sun. */
    [[nodiscard]] static uint8_t weekdayStm(uint16_t year, uint8_t month, uint8_t day) noexcept
    {
        static constexpr uint8_t kT[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
        int y = static_cast<int>(year);
        const int m = static_cast<int>(month);
        const int d = static_cast<int>(day);
        y -= (m < 3);
        const int sak = (y + y / 4 - y / 100 + y / 400 + kT[static_cast<std::size_t>(m - 1)] + d) % 7;
        return static_cast<uint8_t>((sak == 0) ? 7 : sak);
    }

    [[nodiscard]] static bool parseBuildDateTime(DateTime& out) noexcept
    {
        // __DATE__ "Mmm dd yyyy" / "Mmm  d yyyy"; __TIME__ "hh:mm:ss"
        static constexpr char kMonths[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
        const char* const date = __DATE__;
        const char* const time = __TIME__;

        uint8_t month = 0u;
        for (uint8_t i = 0u; i < 12u; ++i) {
            if (std::memcmp(date, kMonths + static_cast<std::size_t>(i) * 3u, 3u) == 0) {
                month = static_cast<uint8_t>(i + 1u);
                break;
            }
        }
        if (month == 0u) {
            return false;
        }

        unsigned day = 0u;
        unsigned year = 0u;
        unsigned hour = 0u;
        unsigned minute = 0u;
        unsigned second = 0u;

        // day: один или два символа перед пробелом+годом
        const char* p = date + 4;
        if (*p == ' ') {
            ++p;
        }
        while (*p >= '0' && *p <= '9') {
            day = day * 10u + static_cast<unsigned>(*p - '0');
            ++p;
        }
        while (*p == ' ') {
            ++p;
        }
        while (*p >= '0' && *p <= '9') {
            year = year * 10u + static_cast<unsigned>(*p - '0');
            ++p;
        }

        if (time[0] < '0' || time[0] > '9') {
            return false;
        }
        hour = static_cast<unsigned>((time[0] - '0') * 10 + (time[1] - '0'));
        minute = static_cast<unsigned>((time[3] - '0') * 10 + (time[4] - '0'));
        second = static_cast<unsigned>((time[6] - '0') * 10 + (time[7] - '0'));

        if (year < 2000u || year > 2099u || day < 1u || day > 31u) {
            return false;
        }

        out.year = static_cast<uint16_t>(year);
        out.month = month;
        out.day = static_cast<uint8_t>(day);
        out.hour = static_cast<uint8_t>(hour);
        out.minute = static_cast<uint8_t>(minute);
        out.second = static_cast<uint8_t>(second);
        return true;
    }
};

} // namespace PHL
