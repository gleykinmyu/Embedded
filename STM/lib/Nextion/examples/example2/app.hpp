#pragma once

/**
 * Пример 2: две страницы, на каждой 10 кнопок (objname b0…b9).
 *
 * HMI: page id 0 и page id 1; на обеих страницах кнопки `b0` … `b9`.
 * Touch Press/Release: **Send Component ID** (кадры 0x65 на MCU).
 * Старт: CID Discover по обеим страницам, затем touch.
 *
 * Сборка: pio run -e example2
 */

#include <cstdint>
#include <cstdio>

#include "nex.hpp"

namespace nex::examples {

namespace detail {

inline const char* touch_state_cstr(TouchState s) noexcept
{
    return s == TouchState::Press ? "press" : "release";
}

class CountingButton : public Button<> {
public:
    CountingButton(uint32_t& counter, Page& owner, const char* tag, const Literal& objname) noexcept
        : Button<>(owner, objname, 0u)
        , hits(counter)
        , label(tag != nullptr ? tag : "?")
    {}

    void onTouch(const msg::evTouch& e) override
    {
        ++hits;
        std::printf("[ex2] %s touch page=%u comp=%u state=%s hits=%lu id=%u\n", label,
            static_cast<unsigned>(e.page_id), static_cast<unsigned>(e.component_id),
            touch_state_cstr(e.state), static_cast<unsigned long>(hits),
            static_cast<unsigned>(id()));
        Button<>::onTouch(e);
    }

private:
    uint32_t& hits;
    const char* label;
};

} // namespace detail

class TenButtonsApp : public Application {
public:
    static constexpr uint16_t kScreenWidth = 600;
    static constexpr uint16_t kScreenHeight = 1024;
    static constexpr uint8_t kPage0Id = 0u;
    static constexpr uint8_t kPage1Id = 1u;
    static constexpr unsigned kPageCount = 2u;
    static constexpr unsigned kButtonsPerPage = 10u;
    static constexpr uint16_t kCidRecordCount = 20u;

    CidTableStorage<kCidRecordCount> cid_registration;

    explicit TenButtonsApp(BIF::IByteStream& stream) noexcept
        : Application(stream, kScreenWidth, kScreenHeight, cid_registration.table)
        , page0(*this)
        , page1(*this)
    {}

    uint32_t button_hits[kPageCount][kButtonsPerPage]{};

    struct Page0 : PageImpl<10> {
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

        Page0(TenButtonsApp& a) noexcept
            : PageImpl<10>(a, "page0", TenButtonsApp::kPage0Id)
            , btn0(a.button_hits[0][0], *this, "p0/b0", "b0")
            , btn1(a.button_hits[0][1], *this, "p0/b1", "b1")
            , btn2(a.button_hits[0][2], *this, "p0/b2", "b2")
            , btn3(a.button_hits[0][3], *this, "p0/b3", "b3")
            , btn4(a.button_hits[0][4], *this, "p0/b4", "b4")
            , btn5(a.button_hits[0][5], *this, "p0/b5", "b5")
            , btn6(a.button_hits[0][6], *this, "p0/b6", "b6")
            , btn7(a.button_hits[0][7], *this, "p0/b7", "b7")
            , btn8(a.button_hits[0][8], *this, "p0/b8", "b8")
            , btn9(a.button_hits[0][9], *this, "p0/b9", "b9")
        {}
        void onLoad() override { std::printf("[ex2] page0 onLoad\n"); }
        void onExit() override { std::printf("[ex2] page0 onExit\n"); }
    };

    struct Page1 : PageImpl<10> {
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

        Page1(TenButtonsApp& a) noexcept
            : PageImpl<10>(a, "page1", TenButtonsApp::kPage1Id)
            , btn0(a.button_hits[1][0], *this, "p1/b0", "b0")
            , btn1(a.button_hits[1][1], *this, "p1/b1", "b1")
            , btn2(a.button_hits[1][2], *this, "p1/b2", "b2")
            , btn3(a.button_hits[1][3], *this, "p1/b3", "b3")
            , btn4(a.button_hits[1][4], *this, "p1/b4", "b4")
            , btn5(a.button_hits[1][5], *this, "p1/b5", "b5")
            , btn6(a.button_hits[1][6], *this, "p1/b6", "b6")
            , btn7(a.button_hits[1][7], *this, "p1/b7", "b7")
            , btn8(a.button_hits[1][8], *this, "p1/b8", "b8")
            , btn9(a.button_hits[1][9], *this, "p1/b9", "b9")
        {}
        void onLoad() override { std::printf("[ex2] page1 onLoad\n"); }
        void onExit() override { std::printf("[ex2] page1 onExit\n"); }
    };

    Page0 page0;
    Page1 page1;

    bool cid_poll_done = false;
    bool cid_poll_ok = false;

    void runCidDiscover(uint32_t now_ms) noexcept
    {
        setMode(CidMode::Discover);
        cid_poll_done = false;
        cid_poll_ok = false;
        printf("[ex2] CID Discover started (%u pages x %u buttons)\n", static_cast<unsigned>(kPageCount),
            static_cast<unsigned>(kButtonsPerPage));
        (void)now_ms;
    }

    bool tickCidDiscover(uint32_t now_ms) noexcept
    {
        if (cid_poll_done)
            return false;

        update(now_ms);

        return !cid_poll_done;
    }

    void onCidPollComplete(bool success) noexcept override
    {
        cid_poll_done = true;
        cid_poll_ok = success;
        if (!success) {
            printf("[ex2] CID poll FAILED\n");
            return;
        }

        const CidTable& table = cid_registration.table;
        printf("[ex2] CID poll OK, records=%u (expect %u)\n", static_cast<unsigned>(table.count),
            static_cast<unsigned>(kCidRecordCount));
        for (uint16_t i = 0u; i < table.count; ++i) {
            const CidRecord& r = table.records[i];
            char name[32]{};
            const uint8_t n = (r.name_len < 31u) ? r.name_len : 31u;
            for (uint8_t j = 0u; j < n; ++j)
                name[j] = r.name[j];
            name[n] = '\0';
            printf("  page=%u name=%s panel_id=%u\n", static_cast<unsigned>(r.page_id), name,
                static_cast<unsigned>(r.panel_id));
        }
    }

    void onPageChange(uint8_t page_id) noexcept override
    {
        std::printf("[ex2] onPageChange -> page=%u\n", static_cast<unsigned>(page_id));
    }

    void onTouch(const msg::evTouch& e) override
    {
        std::printf("[ex2] app onTouch page=%u comp=%u %s\n", static_cast<unsigned>(e.page_id),
            static_cast<unsigned>(e.component_id), detail::touch_state_cstr(e.state));
    }
};

} // namespace nex::examples
