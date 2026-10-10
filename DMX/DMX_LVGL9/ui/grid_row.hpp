/**
 * @file grid_row.hpp
 * @brief Один ряд сетки монитора: подпись десятков слева + 10 клеток.
 *
 * Клик кодируется в user_data как pack(col,row)+1 — иначе клетка (0,0)
 * даёт nullptr и LVGL теряет обработчик. Пустые клетки (хвост 512) скрыты.
 */
#pragma once

#include "cell_map.hpp"
#include "chrome.hpp"
#include "colors.hpp"
#include "layout.hpp"
#include "tester.hpp"

#include <cstdio>
#include <cstring>

#include <lv/lv.hpp>

namespace ui {

/**
 * Снимок клетки для dirty-check перед перерисовкой.
 * flags: bit0 empty, bit1 selected, bit2 fast-changed.
 */
struct CellSnap {
    uint8_t value = 0;
    uint8_t flags = 0;

    [[nodiscard]] constexpr bool empty() const noexcept { return (flags & 1u) != 0u; }
    [[nodiscard]] constexpr bool selected() const noexcept { return (flags & 2u) != 0u; }
    [[nodiscard]] constexpr bool fast() const noexcept { return (flags & 4u) != 0u; }
    [[nodiscard]] constexpr bool operator==(const CellSnap& o) const noexcept
    {
        return value == o.value && flags == o.flags;
    }

    static constexpr CellSnap vacant() noexcept { return CellSnap{0, 1u}; }
    static constexpr CellSnap make(uint8_t v, bool sel, bool changed) noexcept
    {
        return CellSnap{v, static_cast<uint8_t>((sel ? 2u : 0u) | (changed ? 4u : 0u))};
    }
};

/** Ряд сетки: header «десятков» + kCols клеток с метками значений. */
class GridRow {
public:
    /**
     * Создаёт ряд row на parent; MemFn — обработчик клика клетки (обычно MonitorScreen::on_cell).
     */
    template<auto MemFn, typename Host>
    void create(lv::ObjectView parent, uint8_t row, Host* host) noexcept
    {
        _row = row;
        const int32_t cw = layout::cellW();
        const int32_t ch = layout::cellH();
        const int32_t gx = layout::gridOriginX();
        const int32_t gy = layout::gridOriginY();
        const int32_t cellW = cw - 2 * layout::kCellInset;
        const int32_t cellH = ch - 2 * layout::kCellInset;
        const int32_t y = gy + static_cast<int32_t>(row) * ch;

        _hdr.create(parent, layout::kRowHdrW, ch, "").align(LV_ALIGN_TOP_LEFT, 0, y);

        for (uint8_t col = 0; col < kCols; ++col) {
            auto cell = lv::Box::create(parent);
            lv_obj_remove_style_all(cell.get());
            lv_obj_set_style_anim_duration(cell.get(), 0, 0);
            styleCenterSlot(cell.get());
            cell.size(cellW, cellH)
                .align(LV_ALIGN_TOP_LEFT, gx + col * cw, y)
                .radius(0)
                .bg_color(colors::cell_bg())
                .bg_opa(LV_OPA_COVER)
                .border_width(1)
                .border_color(colors::border())
                .padding(0)
                .user_data(reinterpret_cast<void*>(pack(col)))
                .template on_click<MemFn>(host);
            lv_obj_set_clickable(cell.get(), true);

            auto lbl = lv::Label::create(cell);
            lv_obj_remove_style_all(lbl.get());
            lbl.text("").text_color(colors::text());
            lv_obj_set_style_pad_all(lbl.get(), 0, 0);
            lv_label_set_long_mode(lbl.get(), LV_LABEL_LONG_CLIP);
            lv_obj_set_event_bubble(lbl.get(), true); // клик по тексту → клетка

            _cellLbl[col] = lbl;
            _cells[col] = cell;
            _snaps[col] = CellSnap::vacant();
            _txt[col][0] = '\0';
        }
    }

    /** Подпись слева = номер «десятки» (каналы 1–10 → «0», 11–20 → «1», …). */
    void syncHeader(Tester& tester) noexcept
    {
        char buf[8]{};
        const uint16_t chn = channelOf(tester.page(), tester.rows(), 0u, _row);
        // col0 пуст → весь ряд за пределами 512 — подпись не рисуем
        if (chn == 0u) {
            _hdr.set_text("");
            lv_obj_set_hidden(_hdr.slot().get(), true);
            return;
        }
        lv_obj_set_hidden(_hdr.slot().get(), false);
        std::snprintf(buf, sizeof(buf), "%u", static_cast<unsigned>((chn - 1u) / 10u));
        _hdr.set_text(buf);
    }

    /** @return true если клетка реально перерисована (для бюджета syncCells). */
    [[nodiscard]] bool syncOne(Tester& tester, uint8_t col, bool force) noexcept
    {
        if (col >= kCols)
            return false;

        CellSnap next = snapOf(tester, col);
        // В режиме Fast помечаем изменение значения относительно прошлого снимка
        if (!next.empty() && !_snaps[col].empty() && tester.view() == ViewMode::Fast
            && next.value != _snaps[col].value) {
            next = CellSnap::make(next.value, next.selected(), true);
        }

        char nextTxt[4]{};
        if (!next.empty())
            tester.formatValue(next.value, nextTxt, sizeof(nextTxt));

        const bool styleChanged = force || (next.flags != _snaps[col].flags);
        const bool textChanged = force || (std::strcmp(_txt[col], nextTxt) != 0);
        if (!styleChanged && !textChanged)
            return false;

        _snaps[col] = next;
        if (textChanged)
            std::memcpy(_txt[col], nextTxt, sizeof(nextTxt));
        paint(col, next, styleChanged, textChanged);
        return true;
    }

    void syncAll(Tester& tester, bool force) noexcept
    {
        for (uint8_t col = 0; col < kCols; ++col)
            (void)syncOne(tester, col, force);
    }

    [[nodiscard]] const CellSnap& snap(uint8_t col) const noexcept { return _snaps[col]; }
    [[nodiscard]] const char* text(uint8_t col) const noexcept { return _txt[col]; }
    [[nodiscard]] uint8_t row() const noexcept { return _row; }

    /** Распаковка user_data клетки → col/row. */
    static void unpack(uintptr_t v, uint8_t& col, uint8_t& row) noexcept
    {
        // +1 в pack: (0,0) иначе даёт nullptr и клик ломается
        const uintptr_t raw = v - 1u;
        col = static_cast<uint8_t>(raw & 0xFFu);
        row = static_cast<uint8_t>((raw >> 8) & 0xFFu);
    }

private:
    [[nodiscard]] uintptr_t pack(uint8_t col) const noexcept
    {
        return ((static_cast<uintptr_t>(_row) << 8) | col) + 1u;
    }

    [[nodiscard]] CellSnap snapOf(Tester& tester, uint8_t col) const noexcept
    {
        const uint16_t chan = channelOf(tester.page(), tester.rows(), col, _row);
        if (chan == 0u)
            return CellSnap::vacant();
        return CellSnap::make(tester.cellValue(col, _row), tester.isSelected(chan), false);
    }

    void paint(uint8_t col, const CellSnap& snap, bool styleChanged, bool textChanged) noexcept
    {
        if (snap.empty()) {
            lv_obj_set_hidden(_cells[col].get(), true);
            if (textChanged)
                lv_label_set_text(_cellLbl[col].get(), "");
            return;
        }

        lv_obj_set_hidden(_cells[col].get(), false);
        if (styleChanged) {
            lv_color_t bg = colors::cell_bg();
            lv_color_t fg = colors::text();
            if (snap.fast()) {
                bg = colors::changed();
                fg = colors::text_light();
            } else if (snap.selected()) {
                bg = colors::accent();
                fg = colors::text_light();
            }
            _cells[col].bg_color(bg);
            _cellLbl[col].text_color(fg);
        }
        if (textChanged)
            lv_label_set_text(_cellLbl[col].get(), _txt[col]);
    }

    uint8_t _row = 0;
    ChromeLabel _hdr{};
    lv::Box _cells[kCols]{};
    lv::Label _cellLbl[kCols]{};
    CellSnap _snaps[kCols]{};
    char _txt[kCols][4]{};
};

} // namespace ui
