#pragma once

/**
 * Пример 5: одна страница со всеми конечными виджетами nex::comp и прогоном MCU-API.
 *
 * HMI: страница id=0, objname — см. README.md в этой папке.
 * Touch: Send Component ID (0x65). Сборка: pio run -e example5
 */

#include <cstdint>
#include <cstdio>

#include "demo_controls.hpp"
#include "nex.hpp"

namespace nex::examples {
namespace ex5 {

using namespace nex::comp;

/** Все виджеты страницы ex5 (для runAllDemos). */
struct Ex5Widgets {
    Timer& timer;
    NumericVariable& nvar;
    StringVariable<64>& svar;
    Hotspot& hotspot;
    QRCode& qrcode;
    Picture& picture;
    CropPicture& crop_picture;
    Waveform<>& waveform;
    ProgressBar<BGStyle::Color>& pbar_color;
    ProgressBar<BGStyle::Image>& pbar_image;
    Slider<>& slider;
    Gauge<>& gauge;
    ComboBox<>& combo;
    Text<>& text;
    ScrollText<>& scroll_text;
    Button<>& button;
    DualStateButton<>& dual_button;
    Number<>& number;
    XFloat<>& xfloat;
    Checkbox<>& checkbox;
    Radio<>& radio;
    ToggleSwitch<>& toggle;
};

inline void runAllDemos(Ex5Widgets& w) noexcept
{
    NEX_DBG("[ex5] === attribute demo start ===\n");

    demoTimer(w.timer);
    demoNumericVariable(w.nvar);
    demoStringVariable(w.svar);
    demoHotspot(w.hotspot);
    demoQRCode(w.qrcode);
    demoPicture(w.picture);
    demoCropPicture(w.crop_picture);
    demoWaveform(w.waveform);
    demoProgressBarColor(w.pbar_color);
    demoProgressBarImage(w.pbar_image);
    demoSlider(w.slider);
    demoGauge(w.gauge);
    demoComboBox(w.combo);
    demoText(w.text);
    demoScrollText(w.scroll_text);
    demoButton(w.button, w.text);
    demoDualStateButton(w.dual_button);
    demoNumber(w.number);
    demoXFloat(w.xfloat);
    demoCheckbox(w.checkbox);
    demoRadio(w.radio);
    demoToggleSwitch(w.toggle);

    NEX_DBG("[ex5] === attribute demo enqueued ===\n");
}

} // namespace ex5

using namespace nex::comp;

class AllComponentsDemoApp : public Application {
public:
    static constexpr uint16_t kScreenWidth = 800;
    static constexpr uint16_t kScreenHeight = 480;
    static constexpr uint8_t kPageId = 0u;
    static constexpr unsigned kWidgetCount = 22u;

    explicit AllComponentsDemoApp(BIF::IByteStream& stream) noexcept
        : Application(stream, kScreenWidth, kScreenHeight)
        , page(*this)
    {}

    struct Ex5Page : PageImpl<kWidgetCount> {
        Timer timer;
        NumericVariable nvar;
        StringVariable<64> svar;
        Hotspot hotspot;
        QRCode qrcode;
        Picture picture;
        CropPicture crop_picture;
        Waveform<> waveform;
        ProgressBar<BGStyle::Color> pbar_color;
        ProgressBar<BGStyle::Image> pbar_image;
        Slider<> slider;
        Gauge<> gauge;
        ComboBox<> combo;
        Text<> text;
        ScrollText<> scroll_text;
        Button<> button;
        DualStateButton<> dual_button;
        Number<> number;
        XFloat<> xfloat;
        Checkbox<> checkbox;
        Radio<> radio;
        ToggleSwitch<> toggle;

        bool demo_done = false;

        Ex5Page(AllComponentsDemoApp& app) noexcept
            : PageImpl<kWidgetCount>(app, "ex5", AllComponentsDemoApp::kPageId)
            , timer(*this, "timer0")
            , nvar(*this, "nvar0")
            , svar(*this, "svar0")
            , hotspot(*this, "hot0")
            , qrcode(*this, "qr0")
            , picture(*this, "pic0")
            , crop_picture(*this, "crop0")
            , waveform(*this, "wave0")
            , pbar_color(*this, "pbar0")
            , pbar_image(*this, "pbari0")
            , slider(*this, "slid0")
            , gauge(*this, "gauge0")
            , combo(*this, "combo0")
            , text(*this, "text0")
            , scroll_text(*this, "stext0")
            , button(*this, "btn0")
            , dual_button(*this, "dual0")
            , number(*this, "num0")
            , xfloat(*this, "xf0")
            , checkbox(*this, "chk0")
            , radio(*this, "rad0")
            , toggle(*this, "tsw0")
        {}

        void onLoad() override
        {
            NEX_DBG("[ex5] page onLoad\n");
            static_cast<AllComponentsDemoApp&>(app).runAttributeDemoOnce();
        }

        void onExit() override { NEX_DBG("[ex5] page onExit\n"); }
    };

    Ex5Page page;

    /** Прогон demo_controls (один раз); вызывать из main, если onLoad ещё не сработал. */
    void runAttributeDemoOnce() noexcept
    {
        if (page.demo_done)
            return;
        page.demo_done = true;

        ex5::Ex5Widgets w{
            page.timer,
            page.nvar,
            page.svar,
            page.hotspot,
            page.qrcode,
            page.picture,
            page.crop_picture,
            page.waveform,
            page.pbar_color,
            page.pbar_image,
            page.slider,
            page.gauge,
            page.combo,
            page.text,
            page.scroll_text,
            page.button,
            page.dual_button,
            page.number,
            page.xfloat,
            page.checkbox,
            page.radio,
            page.toggle,
        };
        ex5::runAllDemos(w);
    }

    void onPageChange(uint8_t page_id) noexcept override
    {
        Application::onPageChange(page_id);
        NEX_DBG("[ex5] onPageChange -> %u\n", static_cast<unsigned>(page_id));
    }
};

} // namespace nex::examples
