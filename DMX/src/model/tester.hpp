/**
 * @file tester.hpp
 * @brief Кадр, лог min/max, страница сетки 16×N, режим просмотра.
 */
#pragma once

#include "UI/cellMap.hpp"
#include "UI/layout.hpp"
#include "idmx.hpp"

#include <cstdio>
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

    void setChannel(uint16_t ch, uint8_t v) noexcept { _live.set(ch, v); }

    void setSelectedValues(uint8_t v) noexcept
    {
        for (uint8_t i = 0; i < _selN; ++i)
            _live.set(_sel[i], v);
    }

    [[nodiscard]] uint16_t selected() const noexcept { return (_selN == 0u) ? 0u : _sel[_selN - 1u]; }
    [[nodiscard]] uint8_t selectedCount() const noexcept { return _selN; }
    [[nodiscard]] uint16_t selectedAt(uint8_t i) const noexcept
    {
        return (i < _selN) ? _sel[i] : 0u;
    }
    [[nodiscard]] bool isSelected(uint16_t ch) const noexcept
    {
        for (uint8_t i = 0; i < _selN; ++i) {
            if (_sel[i] == ch)
                return true;
        }
        return false;
    }

    enum class SelectResult : uint8_t { Added, Removed, Full, Invalid };

    SelectResult toggleSelect(uint16_t ch) noexcept
    {
        if (ch < 1u || ch > dmx::kMaxChannels)
            return SelectResult::Invalid;
        for (uint8_t i = 0; i < _selN; ++i) {
            if (_sel[i] != ch)
                continue;
            for (uint8_t j = i; j + 1u < _selN; ++j)
                _sel[j] = _sel[j + 1u];
            --_selN;
            clearTrace();
            return SelectResult::Removed;
        }
        if (_selN >= kMaxSelect)
            return SelectResult::Full;
        _sel[_selN++] = ch;
        clearTrace();
        return SelectResult::Added;
    }

    void formatSelection(char* dst, std::size_t cap) const noexcept
    {
        if (dst == nullptr || cap == 0u)
            return;
        if (_selN == 0u) {
            dst[0] = '-';
            dst[1] = '\0';
            return;
        }
        if (_selN == 1u) {
            std::snprintf(dst, cap, "ch %u = %u", static_cast<unsigned>(_sel[0]),
                static_cast<unsigned>(_live.get(_sel[0])));
            return;
        }
        int n = std::snprintf(dst, cap, "%u:", static_cast<unsigned>(_selN));
        if (n < 0 || static_cast<std::size_t>(n) >= cap)
            return;
        for (uint8_t i = 0; i < _selN; ++i) {
            const int add = std::snprintf(dst + n, cap - static_cast<std::size_t>(n), "%s%u",
                (i == 0u) ? " " : ",", static_cast<unsigned>(_sel[i]));
            if (add < 0)
                return;
            n += add;
            if (static_cast<std::size_t>(n) >= cap)
                return;
        }
    }

    void blackout() noexcept { _live.fill(0); }
    void full() noexcept { _live.fill(255); }

    void poll() noexcept { _port.poll(); }

    void tick() noexcept
    {
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

    void sampleTrace() noexcept
    {
        if (_selN == 0u)
            return;
        if (_traceN >= kTraceLen) {
            _traceN = 0;
            ++_traceGen;
        }
        for (uint8_t i = 0; i < kMaxSelect; ++i)
            _trace[i][_traceN] = (i < _selN) ? _live.get(_sel[i]) : 0;
        ++_traceN;
    }

    [[nodiscard]] uint8_t traceCount() const noexcept { return _selN; }
    [[nodiscard]] uint16_t traceLen() const noexcept { return _traceN; }
    [[nodiscard]] uint16_t traceGen() const noexcept { return _traceGen; }
    [[nodiscard]] uint8_t traceAt(uint8_t series, uint16_t i) const noexcept
    {
        if (series >= _selN || i >= _traceN)
            return 0;
        return _trace[series][i];
    }

    void clearTrace() noexcept
    {
        _traceN = 0;
        ++_traceGen;
    }

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
    uint16_t _sel[kMaxSelect]{1};
    uint8_t _selN = 1;
    uint8_t _trace[kMaxSelect][kTraceLen]{};
    uint16_t _traceN = 0;
    uint16_t _traceGen = 1;
    uint8_t _rows = layout::rowsFit();
    uint8_t _page = 0;
    ViewMode _view = ViewMode::Current;
    bool _logging = false;
};

} // namespace ui
