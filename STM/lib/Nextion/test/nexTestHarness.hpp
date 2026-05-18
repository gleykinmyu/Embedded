#pragma once

/**
 * @file nexTestHarness.hpp
 *
 * Демо-приложение Nextion для **реального UART** (`BIF::IByteStream`): две страницы, по два компонента,
 * счётчики `onTouch` / `onPageChange` / `onLoad` / `onExit` — удобно проверить на железе (лог, отладчик).
 *
 * На стороне HMI задайте те же `page` id (0, 1) и `id` компонентов (1, 2), что и константы класса.
 * page0 comp1 (release) — MsgBox ошибка (`showErrorBox`, симуляция `Status`);
 * page0 comp2 (release) — MsgBox обычный (цикл preset ×4).
 *
 * @code
 *   nex::test::TwoPageTouchDemoApp nextion_app(board.serial1);
 *   while (1) {
 *       const uint32_t now = board.GetTick();
 *       nextion_app.update(now);
 *       nextion_app.tickDrawDemo(now);
 *   }
 * @endcode
 */

#include <cstdint>
#include <cstdio>
#include <utility>

#include "../nex.hpp"

namespace nex::test {

namespace detail {

inline const char* touch_state_cstr(TouchState s) noexcept {
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

    void onTouch(const msg::evTouch& e) override {
        ++hits;
        std::printf("[Nextion demo] %s onTouch page=%u comp=%u state=%s hits=%lu\n", tag,
            static_cast<unsigned>(e.page_id), static_cast<unsigned>(e.component_id), touch_state_cstr(e.state),
            static_cast<unsigned long>(hits));
        Base::onTouch(e);
    }

private:
    uint32_t& hits;
    const char* const tag;
};

} // namespace detail

class TwoPageTouchDemoApp : public nex::Application {
public:
    static constexpr uint16_t kScreenWidth = 600;
    static constexpr uint16_t kScreenHeight = 1024;

    explicit TwoPageTouchDemoApp(BIF::IByteStream& stream) noexcept
        : Application(stream, kScreenWidth, kScreenHeight)
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
    uint8_t last_touch_component = 0xFF;
    TouchState last_touch_state = TouchState::Release;
    uint8_t last_page_change_id = 0xFF;

    struct Page0 : nex::PageImpl<4> {
        detail::CountingTouchWidget<nex::Button<>> btn;
        detail::CountingTouchWidget<nex::DualStateButton<>> dsbtn;

        Page0(TwoPageTouchDemoApp& a) noexcept
            : nex::PageImpl<4>(a, "page0", TwoPageTouchDemoApp::kPage0Id)
            , btn(a.touch_btn_p0, "btn_p0", *this, "b0", TwoPageTouchDemoApp::kCompAId)
            , dsbtn(a.touch_dsbtn_p0, "dsbtn_p0", *this, "ds0", TwoPageTouchDemoApp::kCompBId)
        {}
        void onLoad() override {
            auto& o = static_cast<TwoPageTouchDemoApp&>(app);
            ++o.page0_loads;
            std::printf("[Nextion demo] page0 onLoad loads=%lu\n", static_cast<unsigned long>(o.page0_loads));
        }
        void onExit() override {
            auto& o = static_cast<TwoPageTouchDemoApp&>(app);
            ++o.page0_exits;
            std::printf("[Nextion demo] page0 onExit exits=%lu\n", static_cast<unsigned long>(o.page0_exits));
        }
    };

    struct Page1 : nex::PageImpl<4> {
        detail::CountingTouchWidget<nex::Button<>> btn;
        detail::CountingTouchWidget<nex::Text<>> txt;

        Page1(TwoPageTouchDemoApp& a) noexcept
            : nex::PageImpl<4>(a, "page1", TwoPageTouchDemoApp::kPage1Id)
            , btn(a.touch_btn_p1, "btn_p1", *this, "b0", TwoPageTouchDemoApp::kCompAId)
            , txt(a.touch_txt_p1, "txt_p1", *this, "t0", TwoPageTouchDemoApp::kCompBId)
        {}
        void onLoad() override {
            auto& o = static_cast<TwoPageTouchDemoApp&>(app);
            ++o.page1_loads;
            std::printf("[Nextion demo] page1 onLoad loads=%lu\n", static_cast<unsigned long>(o.page1_loads));
        }
        void onExit() override {
            auto& o = static_cast<TwoPageTouchDemoApp&>(app);
            ++o.page1_exits;
            std::printf("[Nextion demo] page1 onExit exits=%lu\n", static_cast<unsigned long>(o.page1_exits));
        }
    };

    Page0 page0{*this};
    Page1 page1{*this};

    static constexpr uint32_t kDrawDemoPeriodMs = 1000u;

    void tickDrawDemo(uint32_t now_ms) noexcept {
        if (_draw_demo_last_ms != 0u && (now_ms - _draw_demo_last_ms) < kDrawDemoPeriodMs)
            return;
        _draw_demo_last_ms = now_ms;
        runDrawDemoStep(_draw_demo_step);
        ++_draw_demo_step;
    }

    void onTouch(const msg::evTouch& e) override {
        ++app_touch_events;
        last_touch_page = e.page_id;
        last_touch_component = e.component_id;
        last_touch_state = e.state;
        std::printf("[Nextion demo] Application::onTouch page=%u comp=%u state=%s app_events=%lu\n",
            static_cast<unsigned>(e.page_id), static_cast<unsigned>(e.component_id), detail::touch_state_cstr(e.state),
            static_cast<unsigned long>(app_touch_events));
        if (e.page_id == kPage0Id && e.state == TouchState::Release) {
            if (e.component_id == kCompAId) {
                msg::Status st{};
                st.status = msg::Status::Code::Invalid_CompId;
                std::printf("[Nextion demo] msgBox Status sim %s p%u c%u\n", statusCodeCstr(st.status),
                    static_cast<unsigned>(kPage0Id), static_cast<unsigned>(kCompAId));
                showErrorBox(st, kPage0Id, kCompAId);
                return;
            }
            if (e.component_id == kCompBId) {
                ++msgbox_demo_presses;
                static constexpr MsgBox::Preset kPresets[] = {MsgBox::Preset::OK, MsgBox::Preset::OKCancel,
                    MsgBox::Preset::YesNo, MsgBox::Preset::YesNoCancel};
                const MsgBox::Preset preset = kPresets[(msgbox_demo_presses - 1u) % 4u];
                char title[24]{};
                char body[56]{};
                std::snprintf(title, sizeof(title), "Demo #%lu", static_cast<unsigned long>(msgbox_demo_presses));
                std::snprintf(body, sizeof(body), "%s", MsgBox::presetCstr(preset));
                std::printf("[Nextion demo] msgBox demo press=%lu preset=%s\n",
                    static_cast<unsigned long>(msgbox_demo_presses), MsgBox::presetCstr(preset));
                msgBox.show(title, body, preset);
            }
        }
    }

    void onMsgBox(const MsgBox::Event& e) noexcept override {
        std::printf("[Nextion demo] onMsgBox action=%s preset=%s error=%u\n", MsgBox::actionCstr(e.action),
            MsgBox::presetCstr(msgBox.btnPreset()), e.isError ? 1u : 0u);
    }

    void onPageChange(uint8_t page_id) override {
        ++app_page_changes;
        last_page_change_id = page_id;
        std::printf("[Nextion demo] onPageChange -> page=%u page_changes=%lu currentPageId=%u\n",
            static_cast<unsigned>(page_id), static_cast<unsigned long>(app_page_changes),
            static_cast<unsigned>(currentPageId()));
        nex::Application::onPageChange(page_id);
    }

private:
    uint32_t _draw_demo_last_ms = 0u;
    uint8_t _draw_demo_step = 0u;
    char _draw_demo_text[32]{};

    void runDrawDemoStep(uint8_t step) noexcept {
        static constexpr Color::std kAccents[] = {Color::std::Red, Color::std::Green, Color::std::Blue,
            Color::std::Yellow, Color::std::Cyan, Color::std::Magenta};
        const Color black = Color::std::Black;
        const Color accent = kAccents[step % 6u];

        cs.clear_screen(black);

        const uint16_t x = static_cast<uint16_t>(20u + (step % 10u) * 14u);
        const uint16_t y = static_cast<uint16_t>(40u + (step % 8u) * 11u);

        if ((step & 1u) == 0u)
            cs.rect_fill(Point{x, y}, Point{static_cast<uint16_t>(x + 70u), static_cast<uint16_t>(y + 45u)}, accent);
        else
            cs.circle_filled(Point{static_cast<uint16_t>(x + 35u), static_cast<uint16_t>(y + 25u)}, 28u, accent);

        std::snprintf(_draw_demo_text, sizeof(_draw_demo_text), "demo %u", static_cast<unsigned>(step));
        cs.text_in_region(Region(Point{8u, 300u}, Rect{400u, 50u}), _draw_demo_text, 1u, Color::std::White,
            HAlign::Center, VAlign::Center, Color::std::Gray, BGStyle::Color);

        std::printf("[Nextion demo] draw demo step=%u\n", static_cast<unsigned>(step));
    }
};

} // namespace nex::test
