/**
 * @file themed_alert.hpp
 * @brief lv_msgbox в палитре тестера (фон/accent/кнопка OK).
 *
 * Вызывается из App::alert() — например при переполнении выбора (max 8).
 */
#pragma once

#include "colors.hpp"
#include "layout.hpp"

#include <lv/lv.hpp>

namespace ui {

class ThemedAlert {
public:
    static void show(const char* title, const char* msg) noexcept
    {
        auto box = lv::Msgbox::create(nullptr);
        box.size(420, 200)
            .bg_color(colors::bg())
            .bg_opa(LV_OPA_COVER)
            .border_width(2)
            .border_color(colors::border())
            .radius(layout::kBtnRadius + 2)
            .text_color(colors::text())
            .padding(0);

        // Затемнение родителя (backdrop msgbox)
        if (lv_obj_t* parent = lv_obj_get_parent(box.get())) {
            lv_obj_set_style_bg_color(parent, colors::bg(), 0);
            lv_obj_set_style_bg_opa(parent, LV_OPA_70, 0);
        }

        stylePart(box.header().get(), colors::btn(), colors::accent());
        stylePart(box.content().get(), colors::bg(), colors::text());
        stylePart(box.footer().get(), colors::bg(), colors::text());

        auto t = box.add_title(title ? title : "DMX");
        if (t) {
            lv_obj_set_style_text_color(t.get(), colors::accent(), 0);
            lv_obj_set_style_pad_ver(t.get(), 4, 0);
        }

        auto body = box.add_text(msg ? msg : "");
        if (body) {
            lv_obj_set_style_text_color(body.get(), colors::text(), 0);
            lv_obj_set_style_pad_ver(body.get(), 8, 0);
            lv_obj_set_style_text_align(body.get(), LV_TEXT_ALIGN_CENTER, 0);
        }

        auto ok = box.add_footer_button("OK");
        if (ok) {
            styleBtn(ok.get());
            lv_obj_add_event_cb(ok.get(), &ThemedAlert::on_close, LV_EVENT_CLICKED, nullptr);
        }

        auto close = box.add_close_button();
        if (close) {
            lv_obj_set_style_bg_color(close.get(), colors::btn(), 0);
            lv_obj_set_style_bg_opa(close.get(), LV_OPA_COVER, 0);
            lv_obj_set_style_text_color(close.get(), colors::text(), 0);
            lv_obj_set_style_border_width(close.get(), 0, 0);
            lv_obj_set_style_anim_duration(close.get(), 0, 0);
        }

        if (auto foot = box.footer()) {
            lv_obj_set_style_pad_bottom(foot.get(), 16, 0);
            lv_obj_set_style_pad_top(foot.get(), 8, 0);
            lv_obj_set_style_min_height(foot.get(), 56, 0);
            lv_obj_set_flex_align(foot.get(), LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        }
    }

private:
    static void stylePart(lv_obj_t* obj, lv_color_t bg, lv_color_t fg) noexcept
    {
        if (!obj)
            return;
        lv_obj_set_style_bg_color(obj, bg, 0);
        lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
        lv_obj_set_style_text_color(obj, fg, 0);
        lv_obj_set_style_border_width(obj, 0, 0);
        lv_obj_set_style_pad_all(obj, 12, 0);
    }

    static void styleBtn(lv_obj_t* btn) noexcept
    {
        if (!btn)
            return;
        lv_obj_set_style_anim_duration(btn, 0, 0);
        lv_obj_set_style_bg_color(btn, colors::accent(), 0);
        lv_obj_set_style_bg_color(btn, colors::accent(), LV_STATE_PRESSED);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_STATE_PRESSED);
        lv_obj_set_style_text_color(btn, colors::text_light(), 0);
        lv_obj_set_style_text_color(btn, colors::text_light(), LV_STATE_PRESSED);
        lv_obj_set_style_border_width(btn, 1, 0);
        lv_obj_set_style_border_color(btn, colors::accent(), 0);
        lv_obj_set_style_radius(btn, layout::kBtnRadius, 0);
        lv_obj_set_style_pad_hor(btn, 20, 0);
        lv_obj_set_style_pad_ver(btn, 10, 0);
        lv_obj_set_style_transform_width(btn, 0, LV_STATE_PRESSED);
        lv_obj_set_style_transform_height(btn, 0, LV_STATE_PRESSED);
        lv_obj_set_height(btn, 40);
    }

    /** Закрыть msgbox: OK лежит во footer → поднимаемся до класса lv_msgbox. */
    static void on_close(lv_event_t* e) noexcept
    {
        lv_obj_t* btn = lv_event_get_target_obj(e);
        lv_obj_t* mbox = lv_obj_get_parent(btn);
        while (mbox && !lv_obj_check_type(mbox, &lv_msgbox_class))
            mbox = lv_obj_get_parent(mbox);
        if (mbox)
            lv_msgbox_close(mbox);
    }
};

} // namespace ui
