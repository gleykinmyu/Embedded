#pragma once

/**
 * Пример 5: все листья nexComponents.hpp (без ExComponents) на 4 страницах HMI.
 *
 * Страницы: ex5a (0), ex5b (1), ex5c (2), ex5d (3) — objname в README.md.
 * Touch: Send Component ID (0x65). `bkcmd=Always` — ACK на каждую команду.
 * Сборка: pio run -e example5
 */

#include <cstdint>
#include <cstdio>

#include "demo_controls.hpp"
#include "nex.hpp"

namespace nex::examples {

using namespace nex::comp;

class AllComponentsDemoApp : public Application {
public:
    static constexpr uint16_t kScreenWidth = 800;
    static constexpr uint16_t kScreenHeight = 480;
    static constexpr unsigned kPageCount = 4u;

    explicit AllComponentsDemoApp(BIF::IByteStream& stream, Application::ClockMsFn clockMs) noexcept
        : Application(stream, {kScreenWidth, kScreenHeight}, clockMs)
        , page_a(*this)
        , page_b(*this)
        , page_c(*this)
        , page_d(*this)
    {}

    /** Страница 0 — переменные, hotspot, картинки, waveform (8 виджетов). */
    struct PageA : PageImpl<8> {
        Timer timer;
        NumericVar nvar;
        StringVar<64> svar;
        Hotspot hotspot;
        QRCode qrcode;
        Picture picture;
        CropPicture crop_picture;
        Waveform<BG::Color, 4> waveform;

        PageA(AllComponentsDemoApp& app) noexcept
            : PageImpl<8>(app, "ex5a", 0u)
            , timer(*this, "timer0")
            , nvar(*this, "nvar0")
            , svar(*this, "svar0")
            , hotspot(*this, "hot0")
            , qrcode(*this, "qr0")
            , picture(*this, "pic0")
            , crop_picture(*this, "crop0")
            , waveform(*this, "wave0")
        {}

        void onLoad() override { NEX_DBG("[ex5a] page onLoad\n"); }

        void onExit() override { NEX_DBG("[ex5a] page onExit\n"); }
    };

    /** Страница 1 — progress, slider, gauge, списки, data record (7 виджетов). */
    struct PageB : PageImpl<7> {
        ProgressBar<BG::Color> pbar_color;
        ProgressBar<BG::Image> pbar_image;
        Slider<> slider;
        Gauge<> gauge;
        ComboBox<> combo;
        TextSelect<> text_select;
        DataRecord<> data_record;

        PageB(AllComponentsDemoApp& app) noexcept
            : PageImpl<7>(app, "ex5b", 1u)
            , pbar_color(*this, "pbar0")
            , pbar_image(*this, "pbari0")
            , slider(*this, "slid0")
            , gauge(*this, "gauge0")
            , combo(*this, "combo0")
            , text_select(*this, "tsel0")
            , data_record(*this, "drec0")
        {}

        void onLoad() override { NEX_DBG("[ex5b] page onLoad\n"); }
        void onExit() override { NEX_DBG("[ex5b] page onExit\n"); }
    };

    /** Страница 2 — текст и числовой ввод (5 виджетов). */
    struct PageC : PageImpl<5> {
        Text<> text;
        SlidingText<> sltext;
        ScrollText<> scroll_text;
        Number<> number;
        XFloat<> xfloat;

        PageC(AllComponentsDemoApp& app) noexcept
            : PageImpl<5>(app, "ex5c", 2u)
            , text(*this, "text0")
            , sltext(*this, "slt0")
            , scroll_text(*this, "stext0")
            , number(*this, "num0")
            , xfloat(*this, "xf0")
        {}

        void onLoad() override { NEX_DBG("[ex5c] page onLoad\n"); }
        void onExit() override { NEX_DBG("[ex5c] page onExit\n"); }
    };

    /** Страница 3 — кнопки и selection (5 виджетов). */
    struct PageD : PageImpl<5> {
        Button<> button;
        DualStateButton<> dual_button;
        Checkbox checkbox;
        Radio radio;
        ToggleSwitch toggle;

        PageD(AllComponentsDemoApp& app) noexcept
            : PageImpl<5>(app, "ex5d", 3u)
            , button(*this, "btn0")
            , dual_button(*this, "dual0")
            , checkbox(*this, "chk0")
            , radio(*this, "rad0")
            , toggle(*this, "tsw0")
        {}

        void onLoad() override { NEX_DBG("[ex5d] page onLoad\n"); }
        void onExit() override { NEX_DBG("[ex5d] page onExit\n"); }
    };

    PageA page_a;
    PageB page_b;
    PageC page_c;
    PageD page_d;

    /** NIS `bkcmd=3`: статус UART после каждой команды — ловим ошибки атрибутов. */
    void enableBkcmdAlways() noexcept
    {
        bkcmd = BkCmd::Always;
        NEX_DBG("[ex5] bkcmd=Always\n");
    }

    /** Прогон demo_controls (один раз); вызывать из main после `enableBkcmdAlways()`. */
    void runAttributeDemoOnce() noexcept
    {
        if (_demo_done)
            return;
        _demo_done = true;

        ex5::PageAWidgets wa{
            page_a.timer,
            page_a.nvar,
            page_a.svar,
            page_a.hotspot,
            page_a.qrcode,
            page_a.picture,
            page_a.crop_picture,
            page_a.waveform,
        };
        ex5::PageBWidgets wb{
            page_b.pbar_color,
            page_b.pbar_image,
            page_b.slider,
            page_b.gauge,
            page_b.combo,
            page_b.text_select,
            page_b.data_record,
        };
        ex5::PageCWidgets wc{
            page_c.text,
            page_c.sltext,
            page_c.scroll_text,
            page_c.number,
            page_c.xfloat,
        };
        ex5::PageDWidgets wd{
            page_d.button,
            page_d.dual_button,
            page_d.checkbox,
            page_d.radio,
            page_d.toggle,
        };
        ex5::runAllDemos(*this, wa, wb, wc, wd);
        bkcmd = BkCmd::OnFailure;
        NEX_DBG("[ex5] bkcmd=OnFailure (live demo)\n");
        ex5::logLivePagePrompt(_live.live_page);
    }

    /** Enter на debug UART — следующая live-страница (A→B→C→A). */
    void advanceLiveDemoPage() noexcept
    {
        if (!_demo_done)
            return;
        ex5::advanceLivePage(*this, _live);
    }

    /** Периодическое обновление Waveform, ProgressBar, Gauge и др. — из main loop. */
    void tickLiveDemos(uint32_t now_ms) noexcept
    {
        if (!_demo_done)
            return;

        ex5::PageAWidgets wa{
            page_a.timer,
            page_a.nvar,
            page_a.svar,
            page_a.hotspot,
            page_a.qrcode,
            page_a.picture,
            page_a.crop_picture,
            page_a.waveform,
        };
        ex5::PageBWidgets wb{
            page_b.pbar_color,
            page_b.pbar_image,
            page_b.slider,
            page_b.gauge,
            page_b.combo,
            page_b.text_select,
            page_b.data_record,
        };
        ex5::PageCWidgets wc{
            page_c.text,
            page_c.sltext,
            page_c.scroll_text,
            page_c.number,
            page_c.xfloat,
        };
        ex5::tickLiveDemos(*this, _live, now_ms, wa, wb, wc);
    }

    void onPageChange(uint8_t page_id) noexcept override
    {
        Application::onPageChange(page_id);
        NEX_DBG("[ex5] onPageChange -> %u\n", static_cast<unsigned>(page_id));
    }

private:
    bool _demo_done = false;
    ex5::LiveDemoState _live{};
};

} // namespace nex::examples
