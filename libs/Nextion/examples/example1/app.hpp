#pragma once

/**
 * Пример 1: две страницы, touch, MsgBox, смена страниц.
 *
 * HMI: page id 0 и 1, компоненты с id 1 и 2 (см. константы класса).
 * page0 comp1 (release) — MsgBox ошибка; page0 comp2 — MsgBox обычный.
 */

#include <cstdint>
#include <cstdio>
#include <utility>

#include "nex.hpp"
#include "../../overlay/ovl.hpp"

namespace nex::examples {

using namespace nex::comp;

namespace detail {

inline const char* touch_state_cstr(TouchState s) noexcept
{
    return s == TouchState::Press ? "press" : "release";
}

template<class Base>
class CountingTouchWidget : public Base {
public:
    template<typename... Args>
    explicit CountingTouchWidget(uint32_t& hitCounter, const char* widget_tag, Args&&... args) noexcept
        : Base(std::forward<Args>(args)...)
        , hits(hitCounter)
        , tag(widget_tag != nullptr ? widget_tag : "?")
    {}

    void onTouch(const msg::evTouch& e) override
    {
        ++hits;
        NEX_DBG("[2page] %s onTouch page=%u comp=%u state=%s hits=%lu\n", tag,
            static_cast<unsigned>(e.route.page), static_cast<unsigned>(e.route.comp), touch_state_cstr(e.state),
            static_cast<unsigned long>(hits));
        Base::onTouch(e);
    }

    void onMsgBox(const msg::evMsgBox& e) override
    {
        NEX_DBG("[2page] %s onMsgBox action=%s tag=%u\n", tag,
            ovl::MsgBox::actionCstr(static_cast<ovl::MsgBox::Action>(e.action)), static_cast<unsigned>(e.tag));
        Base::onMsgBox(e);
    }

private:
    uint32_t& hits;
    const char* const tag;
};

} // namespace detail

class TwoPageTouchDemoApp : public AppUI<kDefaultMaxPages> {
public:
    static constexpr uint16_t kScreenWidth = 600;
    static constexpr uint16_t kScreenHeight = 1024;
    explicit TwoPageTouchDemoApp(BIF::IByteStream& stream, Application::ClockMsFn clockMs) noexcept
        : AppUI(stream, {kScreenWidth, kScreenHeight}, AppTiming{clockMs})
    {}

    static constexpr uint8_t kPage0Id = 0u;
    static constexpr uint8_t kPage1Id = 1u;
    static constexpr uint8_t kCompAId = 1u;
    static constexpr uint8_t kCompBId = 2u;

    uint32_t touch_btn_p0{};
    uint32_t touch_dsbtn_p0{};
    uint32_t touch_btn_p1{};
    uint32_t touch_txt_p1{};

    uint32_t app_touch_events{};
    uint32_t msgbox_demo_presses{};
    uint32_t app_page_changes{};
    uint32_t page0_loads{};
    uint32_t page0_exits{};
    uint32_t page1_loads{};
    uint32_t page1_exits{};

    uint8_t last_touch_page = 0xFF;
    uint8_t last_touch_comp = 0xFF;
    TouchState last_touch_state = TouchState::Release;
    uint8_t last_page_change_id = 0xFF;

    ovl::MsgBox msgBox{*this};

    struct Page0 : Page<2> {
        detail::CountingTouchWidget<Button<>> btn;
        detail::CountingTouchWidget<DualStateButton<>> dsbtn;

        Page0(TwoPageTouchDemoApp& a) noexcept
            : Page<2>(a, "page0", TwoPageTouchDemoApp::kPage0Id)
            , btn(a.touch_btn_p0, "btn_p0", *this, "b0", TwoPageTouchDemoApp::kCompAId)
            , dsbtn(a.touch_dsbtn_p0, "dsbtn_p0", *this, "ds0", TwoPageTouchDemoApp::kCompBId)
        {}
        void onLoad() override
        {
            auto& o = static_cast<TwoPageTouchDemoApp&>(app);
            ++o.page0_loads;
            NEX_DBG("[2page] page0 onLoad loads=%lu\n", static_cast<unsigned long>(o.page0_loads));
        }
        void onExit() override
        {
            auto& o = static_cast<TwoPageTouchDemoApp&>(app);
            ++o.page0_exits;
            NEX_DBG("[2page] page0 onExit exits=%lu\n", static_cast<unsigned long>(o.page0_exits));
        }
    };

    struct Page1 : Page<2> {
        detail::CountingTouchWidget<Button<>> btn;
        detail::CountingTouchWidget<Text<>> txt;

        Page1(TwoPageTouchDemoApp& a) noexcept
            : Page<2>(a, "page1", TwoPageTouchDemoApp::kPage1Id)
            , btn(a.touch_btn_p1, "btn_p1", *this, "b0", TwoPageTouchDemoApp::kCompAId)
            , txt(a.touch_txt_p1, "txt_p1", *this, "t0", TwoPageTouchDemoApp::kCompBId)
        {}
        void onLoad() override
        {
            auto& o = static_cast<TwoPageTouchDemoApp&>(app);
            ++o.page1_loads;
            NEX_DBG("[2page] page1 onLoad loads=%lu\n", static_cast<unsigned long>(o.page1_loads));
        }
        void onExit() override
        {
            auto& o = static_cast<TwoPageTouchDemoApp&>(app);
            ++o.page1_exits;
            NEX_DBG("[2page] page1 onExit exits=%lu\n", static_cast<unsigned long>(o.page1_exits));
        }
    };

    Page0 page0{*this};
    Page1 page1{*this};

    static constexpr uint32_t kDrawDemoPeriodMs = 1000u;

    void tickDrawDemo(uint32_t now_ms) noexcept
    {
        if (_draw_demo_last_ms != 0u && (now_ms - _draw_demo_last_ms) < kDrawDemoPeriodMs)
            return;
        _draw_demo_last_ms = now_ms;
        runDrawDemoStep(_draw_demo_step);
        ++_draw_demo_step;
    }

    void onTouch(const msg::evTouch& e) override
    {
        ++app_touch_events;
        last_touch_page = e.route.page;
        last_touch_comp = e.route.comp;
        last_touch_state = e.state;
        NEX_DBG("[2page] Application::onTouch page=%u comp=%u state=%s app_events=%lu\n",
            static_cast<unsigned>(e.route.page), static_cast<unsigned>(e.route.comp), detail::touch_state_cstr(e.state),
            static_cast<unsigned long>(app_touch_events));
        if (e.route.page == kPage0Id && e.state == TouchState::Release) {
            if (e.route.comp == kCompAId) {
                msg::Status st{};
                st.status = msg::Status::Code::Invalid_CompId;
                NEX_DBG("[2page] msgBox Status sim %s p%u c%u\n", cstr(st.status),
                    static_cast<unsigned>(kPage0Id), static_cast<unsigned>(kCompAId));
                onStatus(st, Route{kPage0Id, kCompAId});
                msgBox.show("NIS error", ovl::MsgBox::Preset::OK, kCompAId, ovl::MsgBox::Action::None, "%s", cstr(st.status));
                return;
            }
            if (e.route.comp == kCompBId) {
                ++msgbox_demo_presses;
                static constexpr ovl::MsgBox::Preset kPresets[] = {ovl::MsgBox::Preset::OK, ovl::MsgBox::Preset::OKCancel,
                    ovl::MsgBox::Preset::YesNo, ovl::MsgBox::Preset::YesNoCancel};
                const ovl::MsgBox::Preset preset = kPresets[(msgbox_demo_presses - 1u) % 4u];
                char title[24]{};
                char body[56]{};
                std::snprintf(title, sizeof(title), "Demo #%lu", static_cast<unsigned long>(msgbox_demo_presses));
                std::snprintf(body, sizeof(body), "%s", ovl::MsgBox::presetCstr(preset));
                NEX_DBG("[2page] msgBox demo press=%lu preset=%s\n",
                    static_cast<unsigned long>(msgbox_demo_presses), ovl::MsgBox::presetCstr(preset));
                msgBox.setRoute(Route{kPage0Id, kCompBId});
                msgBox.show(title, preset, kCompBId, ovl::MsgBox::Action::None, "%s", body);
            }
        }
        Application::onTouch(e);
    }

    void onPageChange(const msg::evPage& e) noexcept override
    {
        ++app_page_changes;
        last_page_change_id = e.page;
        NEX_DBG("[2page] onPageChange -> page=%u page_changes=%lu currentPage=%u\n",
            static_cast<unsigned>(e.page), static_cast<unsigned long>(app_page_changes),
            static_cast<unsigned>(currentPage()));
        Application::onPageChange(e);
    }

private:
    uint32_t _draw_demo_last_ms = 0u;
    uint8_t _draw_demo_step = 0u;
    char _draw_demo_text[32]{};

    void runDrawDemoStep(uint8_t step) noexcept
    {
        static constexpr Color::std kAccents[] = {Color::std::Red, Color::std::Green, Color::std::Blue,
            Color::std::Yellow, Color::std::Cyan, Color::std::Magenta};
        const Color black = Color::std::Black;
        const Color accent = kAccents[step % 6u];

        cs.clear_screen(black);

        const Coord x = static_cast<Coord>(20u + (step % 10u) * 14u);
        const Coord y = static_cast<Coord>(40u + (step % 8u) * 11u);

        if ((step & 1u) == 0u)
            cs.rect_fill(Point{x, y}, Point{static_cast<Coord>(x + 70u), static_cast<Coord>(y + 45u)}, accent);
        else
            cs.circle_filled(Point{static_cast<Coord>(x + 35u), static_cast<Coord>(y + 25u)}, 28u, accent);

        std::snprintf(_draw_demo_text, sizeof(_draw_demo_text), "demo %u", static_cast<unsigned>(step));
        cs.text_in_region(Region(Point{8u, 300u}, Rect{400u, 50u}), _draw_demo_text, 1u, Color::std::White,
            HAlign::Center, VAlign::Center, Color::std::Gray, BGStyle::Color);

        NEX_DBG("[2page] draw demo step=%u\n", static_cast<unsigned>(step));
    }
};

} // namespace nex::examples
