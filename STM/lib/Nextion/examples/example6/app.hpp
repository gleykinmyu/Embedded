#pragma once

/**
 * Пример 6: latency bench — одна команда, ждём Success, затем следующая.
 *
 * `bkcmd=Always` — ACK на каждую команду.
 * `[ex6-tx]` / `[ex6-rx]` — отправка и статусы с панели (без правок библиотеки).
 *
 * Сборка: pio run -e example6
 */

#include "latency_bench.hpp"
#include "examples/example5/demo_controls.hpp"
#include "nex.hpp"

namespace nex::examples {

using namespace nex::comp;

class LatencyBenchApp;

namespace ex6 {
void runLatencyBench(LatencyBenchApp& app, LatencyRecorder& rec, ex5::PageAWidgets& a, ex5::PageBWidgets& b,
    ex5::PageCWidgets& c, ex5::PageDWidgets& d) noexcept;
}

class LatencyBenchApp : public Application {
public:
    static constexpr uint16_t kScreenWidth = 800;
    static constexpr uint16_t kScreenHeight = 480;

    /** Таймаут ожидания `Success` после одной enqueue. */
    static constexpr uint32_t kWaitSuccessTimeoutMs = 2000u;

    /** Дренаж «хвоста» ACK после idle перед следующей командой. */
    static constexpr uint32_t kAckDrainMs = 50u;

    explicit LatencyBenchApp(BIF::IByteStream& stream, Application::ClockMsFn clockMs) noexcept
        : Application(stream, {kScreenWidth, kScreenHeight}, clockMs)
        , page_a(*this)
        , page_b(*this)
        , page_c(*this)
        , page_d(*this)
    {}

    struct PageA : PageImpl<8> {
        Timer timer;
        NumericVar nvar;
        StringVar<64> svar;
        Hotspot hotspot;
        QRCode qrcode;
        Picture picture;
        CropPicture crop_picture;
        Waveform<BG::Color, 4> waveform;

        PageA(LatencyBenchApp& app) noexcept
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
    };

    struct PageB : PageImpl<6> {
        ProgressBar<BG::Color> pbar_color;
        ProgressBar<BG::Image> pbar_image;
        Slider<> slider;
        Gauge<> gauge;
        ComboBox<> combo;
        TextSelect<> text_select;

        PageB(LatencyBenchApp& app) noexcept
            : PageImpl<6>(app, "ex5b", 1u)
            , pbar_color(*this, "pbar0")
            , pbar_image(*this, "pbari0")
            , slider(*this, "slid0")
            , gauge(*this, "gauge0")
            , combo(*this, "combo0")
            , text_select(*this, "tsel0")
        {}
    };

    struct PageC : PageImpl<5> {
        Text<> text;
        SlidingText<> sltext;
        ScrollText<> scroll_text;
        Number<> number;
        XFloat<> xfloat;

        PageC(LatencyBenchApp& app) noexcept
            : PageImpl<5>(app, "ex5c", 2u)
            , text(*this, "text0")
            , sltext(*this, "slt0")
            , scroll_text(*this, "stext0")
            , number(*this, "num0")
            , xfloat(*this, "xf0")
        {}
    };

    struct PageD : PageImpl<5> {
        Button<> button;
        DualStateButton<> dual_button;
        Checkbox checkbox;
        Radio radio;
        ToggleSwitch toggle;

        PageD(LatencyBenchApp& app) noexcept
            : PageImpl<5>(app, "ex5d", 3u)
            , button(*this, "btn0")
            , dual_button(*this, "dual0")
            , checkbox(*this, "chk0")
            , radio(*this, "rad0")
            , toggle(*this, "tsw0")
        {}
    };

    PageA page_a;
    PageB page_b;
    PageC page_c;
    PageD page_d;

    void enableBkcmdAlways() noexcept
    {
        bkcmd = BkCmd::Always;
        NEX_DBG("[ex6] bkcmd=Always (enqueue)\n");
    }

    /** Дождаться `Success` на `bkcmd=Always` (маршрут p255 c255). */
    void waitBkcmdApplied() noexcept
    {
        _cmd_label = "bkcmd";
        _cmd_page = Route::kSysVarPageId;
        _cmd_comp = Route::kSysVarCompId;
        if (waitForPanelSuccess(kWaitSuccessTimeoutMs))
            NEX_DBG("[ex6] bkcmd=Always applied (Success p255 c255)\n");
        else
            NEX_DBG("[ex6] bkcmd: timeout / no Success (%lu ms)\n",
                static_cast<unsigned long>(kWaitSuccessTimeoutMs));
        _cmd_label = nullptr;
        drainPanelAcks();
    }

    /** `pumpUntilIdle` + крутим RX, чтобы сбросить осиротевшие ACK. */
    void drainPanelAcks() noexcept
    {
        if (!pumpUntilIdle())
            NEX_DBG("[ex6] pumpUntilIdle: timeout (session not idle)\n");
        const uint32_t t0 = nowMs();
        while ((nowMs() - t0) < kAckDrainMs)
            update();
    }

    /** Крутит `update()` пока не придёт panel-статус или истечёт `timeout_ms`. */
    bool waitForPanelSuccess(uint32_t timeout_ms) noexcept
    {
        _await_panel = true;
        _got_success = false;
        _panel_done = false;

        const uint32_t t0 = nowMs();
        while (!_panel_done && (nowMs() - t0) < timeout_ms)
            update();

        _await_panel = false;
        return _got_success;
    }

    template<typename Fn>
    void measureOne(ex6::LatencyRecorder& rec, const char* label, uint8_t page_id, uint8_t comp_id,
        uint8_t expect_page, uint8_t expect_comp, Fn&& fn) noexcept
    {
        drainPanelAcks();
        clearErrors();

        _cmd_label = label;
        _cmd_page = expect_page;
        _cmd_comp = expect_comp;
        _last_rx_status = 0u;

        NEX_DBG("[ex6-tx] t=%010lu p%u c%u  %s (expect p%u c%u)\n", static_cast<unsigned long>(nowMs()),
            static_cast<unsigned>(page_id), static_cast<unsigned>(comp_id), label,
            static_cast<unsigned>(expect_page), static_cast<unsigned>(expect_comp));

        const uint32_t t0 = nowMs();
        fn();
        const bool ok = waitForPanelSuccess(kWaitSuccessTimeoutMs);
        const uint32_t dt = ok ? (_response_tick - t0) : (nowMs() - t0);
        const uint8_t rx_status = ok ? static_cast<uint8_t>(msg::Status::Code::Success) : _last_rx_status;

        rec.add(label, page_id, comp_id, dt, ok ? 1u : 0u, rx_status);

        NEX_DBG("[ex6] %4lu ms  p%u c%u  %s  %s\n", static_cast<unsigned long>(dt),
            static_cast<unsigned>(page_id), static_cast<unsigned>(comp_id), label, ok ? "Success" : "TIMEOUT");

        if (!ok)
            NEX_DBG("[ex6]   (no matched Success within %lu ms, last rx 0x%02X)\n",
                static_cast<unsigned long>(kWaitSuccessTimeoutMs), static_cast<unsigned>(_last_rx_status));

        _cmd_label = nullptr;
        drainPanelAcks();
    }

    template<typename Fn>
    void measureOne(ex6::LatencyRecorder& rec, const char* label, uint8_t page_id, uint8_t comp_id,
        Fn&& fn) noexcept
    {
        measureOne(rec, label, page_id, comp_id, page_id, comp_id, static_cast<Fn&&>(fn));
    }

    void measureSwitchPage(ex6::LatencyRecorder& rec, uint8_t page_id) noexcept
    {
        /** `switchPage` enqueue: маршрут tx p0 c0; ACK панели — тоже p0 c0. */
        measureOne(rec, "switchPage", page_id, 0u, 0u, 0u, [&]() noexcept { switchPage(page_id); });
    }

    void onError(const msg::Status& status, uint8_t page_id, uint8_t comp_id) noexcept override
    {
        Application::onError(status, page_id, comp_id);

        if (status.isAppError()) {
            const AppError reporter = appErrorReporter(status);
            NEX_DBG("[ex6-rx] t=%010lu AppError %s %s p%u c%u\n", static_cast<unsigned long>(nowMs()),
                cstr(reporter), appErrorDetailCstr(reporter, appErrorDetail(status)),
                static_cast<unsigned>(page_id), static_cast<unsigned>(comp_id));
        } else {
            NEX_DBG("[ex6-rx] t=%010lu %s (0x%02X) p%u c%u", static_cast<unsigned long>(nowMs()),
                cstr(status.status), static_cast<unsigned>(static_cast<uint8_t>(status.status)),
                static_cast<unsigned>(page_id), static_cast<unsigned>(comp_id));
        }

        if (_cmd_label != nullptr) {
            NEX_DBG("         after tx \"%s\" expect p%u c%u\n", _cmd_label, static_cast<unsigned>(_cmd_page),
                static_cast<unsigned>(_cmd_comp));
        } else {
            NEX_DBG("\n");
        }

        if (!_await_panel)
            return;

        if (status.isAppError())
            return;

        if (!responseMatchesExpected(page_id, comp_id))
            return;

        if (status.status != msg::Status::Code::Success)
            return;

        _panel_done = true;
        _response_tick = nowMs();
        _last_rx_status = static_cast<uint8_t>(status.status);
        if (status.status == msg::Status::Code::Success)
            _got_success = true;
    }

    [[nodiscard]] bool responseMatchesExpected(uint8_t page_id, uint8_t comp_id) const noexcept
    {
        return page_id == _cmd_page && comp_id == _cmd_comp;
    }

    void runLatencyBenchOnce() noexcept
    {
        if (_bench_done)
            return;
        _bench_done = true;

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
        ex6::runLatencyBench(*this, _recorder, wa, wb, wc, wd);
    }

    void printBenchSummary() const noexcept
    {
        _recorder.printSummary();
    }

private:
    bool _bench_done = false;
    ex6::LatencyRecorder _recorder{};
    bool _await_panel = false;
    bool _panel_done = false;
    bool _got_success = false;
    uint32_t _response_tick = 0u;
    const char* _cmd_label = nullptr;
    uint8_t _cmd_page = 0u;
    uint8_t _cmd_comp = 0u;
    uint8_t _last_rx_status = 0u;
};

namespace ex6 {
namespace detail {

using ex5::detail::kAccent;
using ex5::detail::kMark;
using ex5::detail::kPic;

inline void benchPageA(LatencyBenchApp& app, LatencyRecorder& rec, ex5::PageAWidgets& w) noexcept
{
    const uint8_t p = ex5::kPageAId;
    app.measureSwitchPage(rec, p);

    app.measureOne(rec, "timer.tim", p, w.timer.id(), [&]() noexcept { w.timer.setPeriod(500u); });
    app.measureOne(rec, "timer.en", p, w.timer.id(), [&]() noexcept { w.timer.enable(); });
    app.measureOne(rec, "nvar.val", p, w.nvar.id(), [&]() noexcept { w.nvar.val = 12345; });
    app.measureOne(rec, "svar.txt", p, w.svar.id(), [&]() noexcept { w.svar.txt.set("MCU"); });
    app.measureOne(rec, "hotspot.tsw", p, w.hotspot.id(), [&]() noexcept { w.hotspot.touchSwitch(true); });
    app.measureOne(rec, "qrcode.txt", p, w.qrcode.id(), [&]() noexcept { w.qrcode.setText("nextion.tech"); });
    app.measureOne(rec, "qrcode.pco", p, w.qrcode.id(), [&]() noexcept { w.qrcode.setPenColor(kMark); });
    app.measureOne(rec, "picture.pic", p, w.picture.id(), [&]() noexcept { w.picture.setImage(kPic); });
    app.measureOne(rec, "crop.pic", p, w.crop_picture.id(), [&]() noexcept { w.crop_picture.setCrop(kPic); });
    app.measureOne(rec, "waveform.dis", p, w.waveform.id(), [&]() noexcept { w.waveform.setDataScale(100u); });
    app.measureOne(rec, "waveform.ch0.pco", p, w.waveform.id(), [&]() noexcept {
        w.waveform.ch[0].setColor(Color::std::Red);
    });
}

inline void benchPageB(LatencyBenchApp& app, LatencyRecorder& rec, ex5::PageBWidgets& w) noexcept
{
    const uint8_t p = ex5::kPageBId;
    app.measureSwitchPage(rec, p);

    app.measureOne(rec, "pbar_image.val", p, w.pbar_image.id(), [&]() noexcept { w.pbar_image.value = 25u; });
    app.measureOne(rec, "pbar_image.pic", p, w.pbar_image.id(), [&]() noexcept { w.pbar_image.bg.setImage(kPic); });
    app.measureOne(rec, "slider.val", p, w.slider.id(), [&]() noexcept { w.slider.value = 40u; });
    app.measureOne(rec, "slider.cursor.pco", p, w.slider.id(), [&]() noexcept {
        w.slider.cursor.setColor(kAccent);
    });
    app.measureOne(rec, "gauge.val", p, w.gauge.id(), [&]() noexcept { w.gauge.setAngle(90); });
    app.measureOne(rec, "gauge.cen.diam", p, w.gauge.id(), [&]() noexcept { w.gauge.center.setDiameter(50u); });
    app.measureOne(rec, "combo.path", p, w.combo.id(), [&]() noexcept { w.combo.path.set("sd0:/list.txt"); });
    app.measureOne(rec, "combo.val", p, w.combo.id(), [&]() noexcept { w.combo.val = 1; });
    app.measureOne(rec, "combo.font", p, w.combo.id(), [&]() noexcept { w.combo.font.setId(1u); });
    app.measureOne(rec, "tsel.path", p, w.text_select.id(), [&]() noexcept {
        w.text_select.path.set("sd0:/list.txt");
    });
    app.measureOne(rec, "tsel.val", p, w.text_select.id(), [&]() noexcept { w.text_select.val = 1; });
    app.measureOne(rec, "tsel.pco", p, w.text_select.id(), [&]() noexcept {
        w.text_select.setSelColor(Color::std::Yellow);
    });
}

inline void benchPageC(LatencyBenchApp& app, LatencyRecorder& rec, ex5::PageCWidgets& w) noexcept
{
    const uint8_t p = ex5::kPageCId;
    app.measureSwitchPage(rec, p);

    app.measureOne(rec, "text.txt", p, w.text.id(), [&]() noexcept { w.text.setText("Hello"); });
    app.measureOne(rec, "sltext.txt", p, w.sltext.id(), [&]() noexcept { w.sltext.setText("Slide"); });
    app.measureOne(rec, "sltext.val_y", p, w.sltext.id(), [&]() noexcept { w.sltext.val_y = 0u; });
    app.measureOne(rec, "scroll.txt", p, w.scroll_text.id(), [&]() noexcept { w.scroll_text.setText("Scroll"); });
    app.measureOne(rec, "scroll.en", p, w.scroll_text.id(), [&]() noexcept { w.scroll_text.enable(); });
    app.measureOne(rec, "number.val", p, w.number.id(), [&]() noexcept { w.number.val = 42; });
    app.measureOne(rec, "number.lenth", p, w.number.id(), [&]() noexcept { w.number.setDigitCount(4u); });
    app.measureOne(rec, "xfloat.val", p, w.xfloat.id(), [&]() noexcept { w.xfloat.val = 100; });
    app.measureOne(rec, "xfloat.format", p, w.xfloat.id(), [&]() noexcept { w.xfloat.setFormat(3u, 2u); });
}

inline void benchPageD(LatencyBenchApp& app, LatencyRecorder& rec, ex5::PageDWidgets& w) noexcept
{
    const uint8_t p = ex5::kPageDId;
    app.measureSwitchPage(rec, p);

    app.measureOne(rec, "button.txt", p, w.button.id(), [&]() noexcept { w.button.setText("Press"); });
    app.measureOne(rec, "button.layer", p, w.button.id(), [&]() noexcept { w.button.placeAbove(w.button); });
    app.measureOne(rec, "button.move", p, w.button.id(), [&]() noexcept {
        w.button.move(Point{0, 0}, Point{4, 4}, 0u, 0u);
    });
    app.measureOne(rec, "dual.val", p, w.dual_button.id(), [&]() noexcept { w.dual_button.val = 1u; });
    app.measureOne(rec, "dual.pco", p, w.dual_button.id(), [&]() noexcept {
        w.dual_button.pressed.bg.setColor(Color::std::Maroon);
    });
    app.measureOne(rec, "chk.val", p, w.checkbox.id(), [&]() noexcept { w.checkbox.val = true; });
    app.measureOne(rec, "rad.val", p, w.radio.id(), [&]() noexcept { w.radio.val = true; });
    app.measureOne(rec, "tsw.val", p, w.toggle.id(), [&]() noexcept { w.toggle.val = true; });
    app.measureOne(rec, "tsw.txt", p, w.toggle.id(), [&]() noexcept { w.toggle.txt.set("ON"); });
}

} // namespace detail

inline void runLatencyBench(LatencyBenchApp& app, LatencyRecorder& rec, ex5::PageAWidgets& a,
    ex5::PageBWidgets& b, ex5::PageCWidgets& c, ex5::PageDWidgets& d) noexcept
{
    NEX_DBG("[ex6] === latency bench start (1 cmd → matched Success → next, bkcmd=Always) ===\n");

    detail::benchPageA(app, rec, a);
    detail::benchPageB(app, rec, b);
    detail::benchPageC(app, rec, c);
    detail::benchPageD(app, rec, d);

    if (!app.pumpUntilIdle())
        NEX_DBG("[ex6] pumpUntilIdle: timeout after bench\n");
}

} // namespace ex6

} // namespace nex::examples
