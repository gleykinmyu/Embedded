/**
 * @file demo_feed.hpp
 * @brief Синтетический вход DMX до появления UART/RS485.
 *
 * Пишет в Tester::live() и зовёт notifyFrame() — UI и link-badge оживают без железа.
 */
#pragma once

#include "tester.hpp"
#include "util.hpp"

#include <lv/lv.hpp>

namespace ui {

class DemoFeed {
public:
    /** periodMs — период обновления «кадра». */
    void start(Tester& tester, uint32_t periodMs = 150) noexcept
    {
        _tester = &tester;
        _timer = lv::Timer::create<&DemoFeed::on_tick>(periodMs, this);
    }

    void stop() noexcept
    {
        _timer = lv::Timer{};
        _tester = nullptr;
    }

private:
    void on_tick() noexcept
    {
        if (_tester == nullptr)
            return;
        const uint32_t now = nowMs();
        dmx::Frame& f = _tester->live();
        // Каналы 1…16 — медленный ramp; 17…40 — более быстрый step
        const uint8_t ramp = static_cast<uint8_t>((now / 40u) & 0xFFu);
        for (uint16_t ch = 1; ch <= 16; ++ch)
            f.set(ch, static_cast<uint8_t>((ramp + ch * 8u) & 0xFFu));
        const uint8_t step = static_cast<uint8_t>((now / 200u) % 256u);
        for (uint16_t ch = 17; ch <= 40; ++ch)
            f.set(ch, static_cast<uint8_t>((step + (ch - 17u) * 6u) & 0xFFu));
        _tester->notifyFrame(now);
    }

    Tester* _tester = nullptr;
    lv::Timer _timer{};
};

} // namespace ui
