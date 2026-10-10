/**
 * @file chrome.hpp
 * @brief Chrome-виджеты поверх lv:: для футеров Monitor/Graph.
 *
 * Вёрстка рядов — через lv::hbox (makeChromeBar / makeChromeFootBar),
 * без ручного placeRightOf: LVGL сам раскладывает детей по flex.
 */
#pragma once

#include "colors.hpp"
#include "layout.hpp"
#include "tester.hpp"
#include "util.hpp"

#include <cstring>

#include <lv/lv.hpp>

namespace ui {

/** Прозрачный слот: flex row + center по обеим осям. */
inline void styleCenterSlot(lv_obj_t* obj) noexcept
{
    if (!obj)
        return;
    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_set_style_radius(obj, 0, 0);
    lv_obj_set_style_margin_all(obj, 0, 0);
    lv_obj_set_scrollable(obj, false);
    lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_flex_flow(obj, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(obj, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
}

/** Корневой контейнер экрана (тёмный фон, без pad). */
inline lv::Box makeScreenRoot(lv::ObjectView parent) noexcept
{
    return lv::Box::create(parent)
        .fill()
        .bg_color(colors::bg())
        .bg_opa(LV_OPA_COVER)
        .padding(0)
        .border_width(0)
        .radius(0);
}

/**
 * Горизонтальный flex-ряд для chrome (футер, группы кнопок).
 * По умолчанию: высота kBtnH, SIZE_CONTENT по ширине, gap между детьми.
 */
inline lv::Flex makeChromeBar(lv::ObjectView parent, int32_t gap = layout::kGap) noexcept
{
    auto bar = lv::hbox(parent)
                   .height(layout::kBtnH)
                   .column_gap(gap)
                   .align(LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
    lv_obj_set_style_bg_opa(bar.get(), LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(bar.get(), 0, 0);
    lv_obj_set_style_pad_all(bar.get(), 0, 0);
    lv_obj_set_style_radius(bar.get(), 0, 0);
    return bar;
}

/** Полноширинная полоса футера на экране. */
inline lv::Flex makeChromeFootBar(lv::ObjectView parent, int32_t y, int32_t gap = layout::kGap) noexcept
{
    auto bar = makeChromeBar(parent, gap)
                   .width(layout::kScreenW)
                   .align(LV_ALIGN_TOP_LEFT, 0, y);
    lv_obj_set_style_pad_hor(bar.get(), layout::kPad, 0);
    return bar;
}

/**
 * Подпись в фиксированном слоте.
 * По умолчанию текст по центру; framed()/text_left() — для списка каналов.
 */
class ChromeLabel {
public:
    ChromeLabel& create(lv::ObjectView parent, int32_t w, int32_t h, const char* txt) noexcept
    {
        _slot = lv::Box::create(parent).size(w, h).bg_opa(LV_OPA_TRANSP).border_width(0).padding(0).radius(0);
        styleCenterSlot(_slot.get());
        lv_obj_set_clickable(_slot.get(), false);
        _lbl = lv::Label::create(_slot).text(txt ? txt : "").text_color(colors::text());
        lv_obj_set_style_text_align(_lbl.get(), LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_pad_all(_lbl.get(), 0, 0);
        lv_obj_set_clickable(_lbl.get(), false);
        return *this;
    }

    ChromeLabel& align(lv_align_t a, int32_t x, int32_t y) noexcept
    {
        _slot.align(a, x, y);
        return *this;
    }

    ChromeLabel& set_text(const char* txt) noexcept
    {
        _lbl.text(txt ? txt : "");
        return *this;
    }

    ChromeLabel& text_color(lv_color_t c) noexcept
    {
        _lbl.text_color(c);
        return *this;
    }

    /** Рамка как у клетки монитора. */
    ChromeLabel& framed() noexcept
    {
        _slot.bg_color(colors::cell_bg())
            .bg_opa(LV_OPA_COVER)
            .border_width(1)
            .border_color(colors::border())
            .radius(0);
        return *this;
    }

    /** Текст слева, с небольшим pad (для списка каналов). */
    ChromeLabel& text_left(int32_t padHor = layout::kSelPad) noexcept
    {
        lv_obj_set_style_text_align(_lbl.get(), LV_TEXT_ALIGN_LEFT, 0);
        lv_obj_set_flex_align(_slot.get(), LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_hor(_slot.get(), padHor, 0);
        return *this;
    }

    template<auto MemFn, typename T>
    ChromeLabel& on_click(T* host) noexcept
    {
        lv_obj_set_clickable(_lbl.get(), true);
        _lbl.template on_click<MemFn>(host);
        return *this;
    }

    [[nodiscard]] lv::Label& label() noexcept { return _lbl; }
    [[nodiscard]] lv::Box& slot() noexcept { return _slot; }
    [[nodiscard]] explicit operator bool() const noexcept { return static_cast<bool>(_slot); }

private:
    lv::Box _slot{};
    lv::Label _lbl{};
};

/**
 * Кнопка футера: без press-анимации темы, подпись по центру (flex).
 * style(lit, accent) — подсветка Clear / Fast / % и т.п.
 */
class ChromeBtn {
public:
    ChromeBtn& create(lv::ObjectView parent, int32_t w, int32_t h, const char* txt) noexcept
    {
        _btn = lv::Button::create(parent);
        lv_obj_remove_style_all(_btn.get());
        lv_obj_set_style_anim_duration(_btn.get(), 0, 0);
        lv_obj_set_clickable(_btn.get(), true);
        styleCenterSlot(_btn.get());
        _btn.size(w, h)
            .radius(layout::kBtnRadius)
            .bg_color(colors::btn())
            .bg_color(colors::btn(), LV_STATE_PRESSED)
            .bg_opa(LV_OPA_COVER)
            .bg_opa(LV_OPA_COVER, LV_STATE_PRESSED)
            .border_width(1)
            .border_color(colors::border())
            .padding(0)
            .text(txt)
            .text_color(colors::text())
            .text_color(colors::text(), LV_STATE_PRESSED)
            .transform_width(0, LV_STATE_PRESSED)
            .transform_height(0, LV_STATE_PRESSED);
        return *this;
    }

    ChromeBtn& style(bool lit, bool accent = false) noexcept
    {
        if (!_btn)
            return *this;
        lv_color_t bg = colors::btn();
        lv_color_t fg = colors::text();
        lv_color_t bd = colors::border();
        if (accent) {
            bg = colors::changed();
            fg = colors::text_light();
            bd = colors::text_light();
        } else if (lit) {
            bg = colors::accent();
            fg = colors::text_light();
            bd = colors::accent();
        }
        _btn.bg_color(bg)
            .bg_color(bg, LV_STATE_PRESSED)
            .text_color(fg)
            .text_color(fg, LV_STATE_PRESSED)
            .border_color(bd);
        return *this;
    }

    ChromeBtn& align(lv_align_t a, int32_t x, int32_t y) noexcept
    {
        _btn.align(a, x, y);
        return *this;
    }

    ChromeBtn& set_text(const char* txt) noexcept
    {
        _btn.set_text(txt);
        return *this;
    }

    template<auto MemFn, typename T>
    ChromeBtn& on_click(T* host) noexcept
    {
        _btn.template on_click<MemFn>(host);
        return *this;
    }

    [[nodiscard]] lv::Button& btn() noexcept { return _btn; }
    [[nodiscard]] explicit operator bool() const noexcept { return static_cast<bool>(_btn); }

private:
    lv::Button _btn{};
};

/**
 * Индикатор link: кружок (pip) + подпись Idle/DMX/Error.
 * sync() читает Tester::linkSnap(); invalidate() сбрасывает кеш подписи.
 */
class LinkBadge {
public:
    void create(lv::ObjectView parent, int32_t linkW) noexcept
    {
        const int32_t w = layout::kPipW + layout::kGap + linkW;
        _root = lv::Box::create(parent)
                    .size(w, layout::kBtnH)
                    .bg_opa(LV_OPA_TRANSP)
                    .border_width(0)
                    .padding(0)
                    .radius(0);
        styleCenterSlot(_root.get());
        lv_obj_set_flex_align(_root.get(), LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(_root.get(), layout::kGap, 0);

        _pip = lv::Box::create(_root)
                   .size(layout::kPipSize, layout::kPipSize)
                   .radius(LV_RADIUS_CIRCLE)
                   .bg_color(colors::border())
                   .bg_opa(LV_OPA_COVER)
                   .border_width(0);

        _link = lv::Label::create(_root).text("No signal").text_color(colors::text());
        lv_obj_set_style_pad_all(_link.get(), 0, 0);
        lv_obj_set_clickable(_link.get(), true);
        _valid = false;
        _pipOn = false;
    }

    void sync(Tester& tester, bool force = false) noexcept
    {
        tester.pollLink(nowMs());
        const LinkSnap snap = tester.linkSnap();
        if (force || !_valid || snap.state != _state) {
            _state = snap.state;
            const char* cap = linkCaptionUtf8(snap.state);
            if (force || !_valid || std::strcmp(_caption, cap) != 0) {
                std::strncpy(_caption, cap, sizeof(_caption) - 1);
                _caption[sizeof(_caption) - 1] = '\0';
                _link.text(_caption);
            }
            _link.text_color(snap.state == LinkUi::Live ? colors::text() : colors::changed());
        }
        if (force || !_valid || snap.pipOn != _pipOn || snap.state == LinkUi::Error) {
            _pipOn = snap.pipOn;
            _pip.bg_color(snap.state == LinkUi::Error
                    ? colors::changed()
                    : (snap.pipOn ? colors::accent() : colors::border()));
        }
        _valid = true;
    }

    void invalidate() noexcept { _valid = false; }

    template<auto MemFn, typename T>
    void on_click(T* host) noexcept
    {
        _link.template on_click<MemFn>(host);
    }

private:
    lv::Box _root{};
    lv::Box _pip{};
    lv::Label _link{};
    char _caption[16]{};
    LinkUi _state = LinkUi::Idle;
    bool _pipOn = false;
    bool _valid = false;
};

/**
 * Группа «< значение >» (страница, span, Y-band).
 * Сама является вложенным flex-рядом; bind<> вешает клики на стрелки.
 */
class ArrowNav {
public:
    void create(lv::ObjectView parent, int32_t labelW, const char* initial) noexcept
    {
        auto bar = makeChromeBar(parent, layout::kArrowGap);
        _prev.create(bar, layout::kNavW, layout::kBtnH, "<");
        _mid.create(bar, labelW, layout::kBtnH, initial).framed();
        _next.create(bar, layout::kNavW, layout::kBtnH, ">");
    }

    template<auto PrevFn, auto NextFn, typename T>
    void bind(T* host) noexcept
    {
        _prev.on_click<PrevFn>(host);
        _next.on_click<NextFn>(host);
    }

    void setText(const char* txt) noexcept { _mid.set_text(txt); }

private:
    ChromeBtn _prev{};
    ChromeBtn _next{};
    ChromeLabel _mid{};
};

} // namespace ui
