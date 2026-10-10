/**
 * @file tester.hpp
 * @brief Модель монитора: кадр DMX, страница сетки, выбор, трасса, link.
 *
 * Без STM32-wire: транспорт пишет в live() и зовёт notifyFrame().
 * На ESP пока DemoFeed; позже — UART RS485.
 */
#pragma once

#include "cell_map.hpp"
#include "dmx_data.hpp"
#include "layout.hpp"

#include <cstdio>

namespace ui {

/** Состояние link-badge в UI. */
enum class LinkUi : uint8_t { Idle, Live, Error };

struct LinkSnap {
    LinkUi state = LinkUi::Idle;
    bool pipOn = false; ///< Мигание pip при Live
};

inline constexpr uint32_t kLinkHoldMs = 1000u; ///< Нет кадра дольше → Idle
inline constexpr uint32_t kLinkPipMs = 500u;   ///< Период мигания pip

[[nodiscard]] inline const char* linkCaptionUtf8(const LinkUi s) noexcept
{
    switch (s) {
    case LinkUi::Live:
        return "DMX";
    case LinkUi::Error:
        return "Error";
    default:
        return "No signal";
    }
}

class Tester {
public:
    Tester() noexcept
    {
        setRows(layout::rowsFit());
    }

    [[nodiscard]] dmx::Frame& live() noexcept { return _live; }
    [[nodiscard]] const dmx::Frame& live() const noexcept { return _live; }

    void notifyFrame(uint32_t nowMs) noexcept
    {
        ++_framesSeen;
        _lastFrameMs = nowMs;
        _hadFrame = true;
        _linkNowMs = nowMs;
    }

    void pollLink(uint32_t nowMs) noexcept { _linkNowMs = nowMs; }

    void setStatus(dmx::Status st) noexcept { _status = st; }
    void clearErrors() noexcept { _status = dmx::Status::OK; }
    [[nodiscard]] dmx::Status status() const noexcept { return _status; }
    [[nodiscard]] uint32_t frameCount() const noexcept { return _framesSeen; }

    [[nodiscard]] LinkSnap linkSnap() const noexcept
    {
        if (_status == dmx::Status::OverFlowRX || _status == dmx::Status::DataError)
            return LinkSnap{LinkUi::Error, false};
        const bool live = _hadFrame && ((_linkNowMs - _lastFrameMs) < kLinkHoldMs);
        if (!live)
            return LinkSnap{LinkUi::Idle, false};
        return LinkSnap{LinkUi::Live, ((_linkNowMs / kLinkPipMs) & 1u) != 0u};
    }

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

    void stepPage(int8_t delta) noexcept
    {
        const int16_t n = static_cast<int16_t>(pageCount(_rows));
        if (n <= 0)
            return;
        int16_t next = static_cast<int16_t>(static_cast<int16_t>(_page) + delta) % n;
        if (next < 0)
            next = static_cast<int16_t>(next + n);
        _page = static_cast<uint8_t>(next);
    }

    [[nodiscard]] ViewMode view() const noexcept { return _view; }
    void setView(ViewMode v) noexcept { _view = v; }

    [[nodiscard]] ValueScale scale() const noexcept { return _scale; }
    void setScale(ValueScale s) noexcept { _scale = s; }
    void toggleScale() noexcept
    {
        _scale = (_scale == ValueScale::Dmx) ? ValueScale::Percent : ValueScale::Dmx;
    }

    [[nodiscard]] YBand yBand() const noexcept { return _yBand; }
    void stepYBand(int8_t delta) noexcept
    {
        const int16_t n = static_cast<int16_t>(kYBandN);
        int16_t next = (static_cast<int16_t>(_yBand) + delta) % n;
        if (next < 0)
            next = static_cast<int16_t>(next + n);
        _yBand = static_cast<YBand>(next);
    }
    [[nodiscard]] uint8_t yLo() const noexcept { return yBandLo(_yBand); }
    [[nodiscard]] uint8_t yHi() const noexcept { return yBandHi(_yBand); }

    [[nodiscard]] uint8_t displayValue(uint8_t raw) const noexcept
    {
        return (_scale == ValueScale::Percent) ? toPercent(raw) : raw;
    }

    void formatValue(uint8_t raw, char* dst, std::size_t cap) const noexcept
    {
        if (dst == nullptr || cap == 0u)
            return;
        std::snprintf(dst, cap, "%u", static_cast<unsigned>(displayValue(raw)));
    }

    void formatYBand(char* dst, std::size_t cap) const noexcept
    {
        if (dst == nullptr || cap == 0u)
            return;
        const unsigned lo = displayValue(yLo());
        const unsigned hi = displayValue(yHi());
        if (_scale == ValueScale::Percent)
            std::snprintf(dst, cap, "%u-%u%%", lo, hi);
        else
            std::snprintf(dst, cap, "%u-%u", lo, hi);
    }

    [[nodiscard]] uint8_t cellValue(uint8_t col, uint8_t row) const noexcept
    {
        const uint16_t ch = channelOf(_page, _rows, col, row);
        return (ch == 0u) ? 0u : _live.get(ch);
    }

    [[nodiscard]] uint8_t traceSpanS() const noexcept { return _spanS; }
    [[nodiscard]] uint32_t tracePeriodMs() const noexcept
    {
        const uint16_t div = (kTraceLen > 1u) ? static_cast<uint16_t>(kTraceLen - 1u) : 1u;
        return (static_cast<uint32_t>(_spanS) * 1000u) / div;
    }

    void stepTraceSpan(int8_t delta) noexcept
    {
        const int16_t step = kTraceSpanStepS;
        const int16_t lo = kTraceSpanMinS;
        const int16_t hi = kTraceSpanMaxS;
        const int16_t n = static_cast<int16_t>(((hi - lo) / step) + 1);
        int16_t i = static_cast<int16_t>((static_cast<int16_t>(_spanS) - lo) / step);
        i = (i + delta) % n;
        if (i < 0)
            i = static_cast<int16_t>(i + n);
        const uint8_t next = static_cast<uint8_t>(lo + i * step);
        if (next == _spanS)
            return;
        _spanS = next;
        clearTrace();
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

    void clearSelection() noexcept
    {
        if (_selN == 0u)
            return;
        _selN = 0;
        clearTrace();
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
            if (_scale == ValueScale::Percent) {
                std::snprintf(dst, cap, "ch %u = %u%%", static_cast<unsigned>(_sel[0]),
                    static_cast<unsigned>(displayValue(_live.get(_sel[0]))));
            } else {
                std::snprintf(dst, cap, "ch %u = %u", static_cast<unsigned>(_sel[0]),
                    static_cast<unsigned>(_live.get(_sel[0])));
            }
            return;
        }
        uint16_t lo = _sel[0];
        uint16_t hi = _sel[0];
        for (uint8_t i = 1u; i < _selN; ++i) {
            if (_sel[i] < lo)
                lo = _sel[i];
            if (_sel[i] > hi)
                hi = _sel[i];
        }
        if (static_cast<uint16_t>(hi - lo + 1u) == _selN) {
            std::snprintf(dst, cap, "ch %u-%u", static_cast<unsigned>(lo), static_cast<unsigned>(hi));
            return;
        }
        int n = 0;
        for (uint8_t i = 0; i < _selN; ++i) {
            const int add = std::snprintf(dst + n, cap - static_cast<std::size_t>(n), "%s%u",
                (i == 0u) ? "" : ",", static_cast<unsigned>(_sel[i]));
            if (add < 0)
                return;
            n += add;
            if (static_cast<std::size_t>(n) >= cap)
                return;
        }
    }

    /** Записать точки трассы по периоду tracePeriodMs() (кольцо kTraceLen). */
    void sampleTrace(uint32_t nowMs) noexcept
    {
        if (_selN == 0u)
            return;
        const uint32_t period = tracePeriodMs();
        if (period == 0u)
            return;
        if (_traceN == 0u) {
            storeSample();
            _traceMs = nowMs;
            return;
        }
        const uint32_t elapsed = nowMs - _traceMs;
        if (elapsed < period)
            return;
        const uint32_t steps = elapsed / period;
        if (steps >= kTraceLen) {
            _traceMs = nowMs;
            return;
        }
        for (uint32_t i = 0; i < steps; ++i) {
            if (_traceN >= kTraceLen) {
                _traceN = 0;
                ++_traceGen;
            }
            storeSample();
            _traceMs += period;
        }
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
    void storeSample() noexcept
    {
        for (uint8_t i = 0; i < kMaxSelect; ++i)
            _trace[i][_traceN] = (i < _selN) ? _live.get(_sel[i]) : 0;
        ++_traceN;
    }

    dmx::Frame _live{};
    dmx::Status _status = dmx::Status::OK;
    uint32_t _traceMs = 0;
    uint16_t _sel[kMaxSelect]{1};
    uint8_t _selN = 1;
    uint8_t _trace[kMaxSelect][kTraceLen]{};
    uint16_t _traceN = 0;
    uint16_t _traceGen = 1;
    uint8_t _rows = layout::rowsFit();
    uint8_t _page = 0;
    ViewMode _view = ViewMode::Current;
    ValueScale _scale = ValueScale::Dmx;
    YBand _yBand = YBand::Full;
    uint8_t _spanS = 20;
    uint32_t _framesSeen = 0;
    uint32_t _lastFrameMs = 0;
    uint32_t _linkNowMs = 0;
    bool _hadFrame = false;
};

} // namespace ui
