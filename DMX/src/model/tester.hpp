/**
 * @file tester.hpp
 * @brief Кадр, лог min/max, страница сетки 16×N, режим просмотра.
 */
#pragma once

#include "UI/cellMap.hpp"
#include "UI/layout.hpp"
#include "idmx.hpp"

#include <cstring>

namespace ui {

class Tester {
public:
    explicit Tester(dmx::IDmx& port) noexcept
        : _port(port)
    {
        resetLog();
    }

    [[nodiscard]] dmx::IDmx& port() noexcept { return _port; }
    [[nodiscard]] const dmx::Frame& live() const noexcept { return _live; }

    void setRows(uint8_t rows) noexcept
    {
        _rows = (rows == 0u) ? 1u : rows;
        const uint8_t n = pageCount(_rows);
        if (_page >= n)
            _page = static_cast<uint8_t>(n - 1u);
    }

    [[nodiscard]] uint8_t rows() const noexcept { return _rows; }
    [[nodiscard]] uint8_t page() const noexcept { return _page; }

    void setPage(uint8_t page) noexcept
    {
        const uint8_t n = pageCount(_rows);
        if (page < n)
            _page = page;
    }

    [[nodiscard]] ViewMode view() const noexcept { return _view; }
    void setView(ViewMode v) noexcept { _view = v; }

    [[nodiscard]] bool logging() const noexcept { return _logging; }

    void startLog() noexcept
    {
        resetLog();
        _logging = true;
    }

    void stopLog() noexcept { _logging = false; }

    void toggleLog() noexcept
    {
        if (_logging)
            stopLog();
        else
            startLog();
    }

    [[nodiscard]] uint8_t cellValue(uint8_t col, uint8_t row) const noexcept
    {
        const uint16_t ch = channelOf(_page, _rows, col, row);
        if (ch == 0u)
            return 0;
        switch (_view) {
        case ViewMode::Min:
            return (_max[ch - 1u] == 0u) ? 0u : _min[ch - 1u];
        case ViewMode::Max:
            return _max[ch - 1u];
        default:
            return _live.get(ch);
        }
    }

    [[nodiscard]] bool pageHasSignal(uint8_t page) const noexcept
    {
        const uint16_t a = pageFirst(page, _rows);
        const uint16_t b = pageLast(page, _rows);
        if (a == 0u)
            return false;
        for (uint16_t ch = a; ch <= b; ++ch) {
            if (_live.get(ch) != 0u)
                return true;
        }
        return false;
    }

    [[nodiscard]] bool cellChanged(uint8_t col, uint8_t row) const noexcept
    {
        const uint16_t ch = channelOf(_page, _rows, col, row);
        return ch != 0u && _changed[ch - 1u] != 0u;
    }

    void setChannel(uint16_t ch, uint8_t v) noexcept
    {
        _live.set(ch, v);
        _selected = ch;
    }

    [[nodiscard]] uint16_t selected() const noexcept { return _selected; }

    void select(uint16_t ch) noexcept
    {
        if (ch >= 1u && ch <= dmx::kMaxChannels)
            _selected = ch;
    }

    void blackout() noexcept { _live.fill(0); }
    void full() noexcept { _live.fill(255); }

    void tick() noexcept
    {
        _port.poll();
        std::memset(_changed, 0, sizeof(_changed));

        if (_port.direction() != dmx::Direction::Receive)
            return;

        dmx::Frame next{};
        if (!_port.recv(next))
            return;

        for (uint16_t i = 0; i < dmx::kMaxChannels; ++i) {
            if (next.slots[i] != _live.slots[i])
                _changed[i] = 1;
        }
        _live = next;

        if (!_logging)
            return;

        const uint16_t n = _live.count == 0u ? static_cast<uint16_t>(dmx::kMaxChannels) : _live.count;
        for (uint16_t i = 0; i < n; ++i) {
            const uint8_t v = _live.slots[i];
            if (v == 0u)
                continue;
            if (v < _min[i])
                _min[i] = v;
            if (v > _max[i])
                _max[i] = v;
        }
    }

    bool sendLive() noexcept { return _port.send(_live); }

private:
    void resetLog() noexcept
    {
        for (std::size_t i = 0; i < dmx::kMaxChannels; ++i) {
            _min[i] = 255;
            _max[i] = 0;
        }
        std::memset(_changed, 0, sizeof(_changed));
    }

    dmx::IDmx& _port;
    dmx::Frame _live{};
    uint8_t _min[dmx::kMaxChannels]{};
    uint8_t _max[dmx::kMaxChannels]{};
    uint8_t _changed[dmx::kMaxChannels]{};
    uint16_t _selected = 1;
    uint8_t _rows = layout::rowsFit();
    uint8_t _page = 0;
    ViewMode _view = ViewMode::Current;
    bool _logging = false;
};

} // namespace ui
