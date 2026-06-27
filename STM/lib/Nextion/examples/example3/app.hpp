#pragma once

/**
 * Пример 3: одна страница, 10 кнопок (objname b0…b9), id на панели 1…10.
 *
 * Как example2, но режим Compiled: `comp` заданы в конструкторе, без опроса Discover.
 *
 * HMI (страница id=0): кнопки `b0` … `b9` с `.id` = 1 … 10.
 *
 * Сборка: pio run -e example3
 */

#include <cstdint>
#include <cstdio>

#include "nex.hpp"

namespace nex::examples {

using namespace nex::comp;

namespace detail {

inline const char* touch_state_cstr(TouchState s) noexcept
{
    return s == TouchState::Press ? "press" : "release";
}

class CountingButton : public Button<> {
public:
    CountingButton(uint32_t& counter, IPage& owner, const char* tag, const Literal& objname,
        uint8_t comp) noexcept
        : Button<>(owner, objname, comp)
        , hits(counter)
        , label(tag != nullptr ? tag : "?")
    {}

    void onTouch(const msg::evTouch& e) override
    {
        ++hits;
        NEX_DBG("[10btn-id] %s touch page=%u comp=%u state=%s hits=%lu id=%u\n", label,
            static_cast<unsigned>(e.route.page), static_cast<unsigned>(e.route.comp),
            touch_state_cstr(e.state), static_cast<unsigned long>(hits),
            static_cast<unsigned>(id()));
        Button<>::onTouch(e);
    }

private:
    uint32_t& hits;
    const char* label;
};

} // namespace detail

class TenButtonsCompiledApp : public AppUI<kDefaultMaxPages> {
public:
    static constexpr uint16_t kScreenWidth = 600;
    static constexpr uint16_t kScreenHeight = 1024;
    static constexpr uint8_t kPageId = 0u;
    static constexpr unsigned kButtonCount = 10u;
    explicit TenButtonsCompiledApp(BIF::IByteStream& stream, Application::ClockMsFn clockMs) noexcept
        : AppUI(stream, {kScreenWidth, kScreenHeight}, AppTiming{clockMs})
        , buttons_page(*this)
    {}

    uint32_t button_hits[kButtonCount]{};

    struct ButtonsPage : Page<10> {
        detail::CountingButton btn0;
        detail::CountingButton btn1;
        detail::CountingButton btn2;
        detail::CountingButton btn3;
        detail::CountingButton btn4;
        detail::CountingButton btn5;
        detail::CountingButton btn6;
        detail::CountingButton btn7;
        detail::CountingButton btn8;
        detail::CountingButton btn9;

        ButtonsPage(TenButtonsCompiledApp& a) noexcept
            : Page<10>(a, "page0", TenButtonsCompiledApp::kPageId)
            , btn0(a.button_hits[0], *this, "btn0", "b0", 1u)
            , btn1(a.button_hits[1], *this, "btn1", "b1", 2u)
            , btn2(a.button_hits[2], *this, "btn2", "b2", 3u)
            , btn3(a.button_hits[3], *this, "btn3", "b3", 4u)
            , btn4(a.button_hits[4], *this, "btn4", "b4", 5u)
            , btn5(a.button_hits[5], *this, "btn5", "b5", 6u)
            , btn6(a.button_hits[6], *this, "btn6", "b6", 7u)
            , btn7(a.button_hits[7], *this, "btn7", "b7", 8u)
            , btn8(a.button_hits[8], *this, "btn8", "b8", 9u)
            , btn9(a.button_hits[9], *this, "btn9", "b9", 10u)
        {}
    };

    ButtonsPage buttons_page;

    void onTouch(const msg::evTouch& e) override
    {
        NEX_DBG("[10btn-id] app onTouch page=%u comp=%u %s\n", static_cast<unsigned>(e.route.page),
            static_cast<unsigned>(e.route.comp), detail::touch_state_cstr(e.state));
    }
};

} // namespace nex::examples
