/**
 * @file main.cpp
 * @brief Демо: пульт с джойстиком + сенсорная панель → PTZ по IByteStream.
 *
 * Сейчас — заглушки IJoystick / ITouchPanel и симуляция ввода.
 * Позже замените DemoJoystick / DemoTouchPanel на драйверы вашей панели.
 */

#include "app/joystick.hpp"
#include "app/ptz_operator.hpp"
#include "app/touch_panel.hpp"
#include "ccam.hpp"
#include "devices/devices.hpp"
#include "ibyte_stream.hpp"

#include <cstdint>

namespace {

uint32_t demoNowMs()
{
    static uint32_t ms = 0;
    return ms;
}

void demoDelayMs(uint32_t ms)
{
    (void)demoNowMs();
    static uint32_t demo_ms = 0;
    demo_ms += ms;
    (void)demo_ms;
}

/* --- UART/RS-422: замените на реальный BIF::IByteStream --- */
class StubByteStream : public BIF::IByteStream {
public:
    size_t write(const uint8_t* data, size_t size) override
    {
        (void)data;
        return open_ ? size : 0;
    }

    size_t read(uint8_t* buffer, size_t maxSize) override
    {
        (void)buffer;
        (void)maxSize;
        return 0;
    }

    size_t available() const override { return 0; }
    size_t availableForWrite() const override { return open_ ? 256 : 0; }
    void purge() override {}
    void purgeOutput() override {}
    void flush() override {}

    bool open(uint32_t baud) override
    {
        open_ = (baud != 0);
        return open_;
    }

    void close() override { open_ = false; }
    bool isOpen() override { return open_; }
    Status getStatus() override { return Status::OK; }
    void clearErrors() override {}

private:
    bool open_ = false;
};

/**
 * Демо-джойстик: круговое движение pan/tilt, периодический zoom.
 * Замените poll() на чтение ADC (X/Y) или UART с панели.
 */
class DemoJoystick : public app::IJoystick {
public:
    app::JoystickAxes poll() override
    {
        app::JoystickAxes joy{};

        ++tick_;
        const int16_t t = static_cast<int16_t>(tick_ % 360);

        joy.pan = static_cast<int16_t>((t < 180) ? (t * 5) : ((360 - t) * 5));
        joy.tilt = static_cast<int16_t>(((t + 90) % 180 - 90) * 4);

        if ((tick_ / 100) % 2 == 0) {
            joy.zoom = 600;
        } else {
            joy.zoom = -600;
        }

        joy.focus = 0;
        joy.button = false;

        return joy;
    }

private:
    uint32_t tick_ = 0;
};

/**
 * Демо-панель: по таймеру шлёт команды (пресет, AWC, стоп).
 */
class DemoTouchPanel : public app::ITouchPanel {
public:
    app::PanelSettings poll() override
    {
        app::PanelSettings s = settings_;
        ++tick_;

        if (tick_ % 250 == 0) {
            switch ((tick_ / 250) % 5) {
            case 0:
                s.pending = app::TouchAction::RecallPreset;
                s.preset = 1;
                break;
            case 1:
                s.pending = app::TouchAction::QueryPosition;
                break;
            case 2:
                s.pending = app::TouchAction::AwcStart;
                s.awc_mode = static_cast<uint8_t>(ccam::devices::He130AwcMode::Atw);
                break;
            case 3:
                s.pan_tilt_rate = 40;
                s.zoom_focus_rate = 25;
                break;
            case 4:
                s.pending = app::TouchAction::StopAll;
                break;
            default:
                break;
            }
        }

        settings_ = s;
        return s;
    }

private:
    app::PanelSettings settings_{};
    uint32_t tick_ = 0;
};

} // namespace

int main()
{
    StubByteStream uart;
    uart.open(ccam::kBaudRate);

    ccam::Rs485Transport transport(uart, &demoNowMs);

    /*
     * Выберите пару «камера + поворотное устройство» под вашу установку:
     *
     * PTZ (камера+pt в одном корпусе):
     *   He130Camera + He130Pt, He870Camera + He870Pt, Ue150Camera + Ue150Pt
     *
     * Студия + отдельная головка:
     *   E600Camera или E650Camera + Ph350Pt
     *
     * Головка + внешняя камера:
     *   GenericCamera + Ph360Pt / Ph650Pt / Ph405Pt
     */
    ccam::devices::He130Camera camera(transport);
    ccam::devices::He130Pt pt(transport);

    app::PtzOperator operator_panel(pt, camera);

    DemoJoystick joystick;
    DemoTouchPanel touch;

    app::PanelSettings panel{};
    panel.pt_power_on = true;
    panel.preset = 1;
    panel.pan_tilt_rate = 30;
    panel.zoom_focus_rate = 20;

    (void)pt.power(ccam::PtPowerMode::On);
    operator_panel.tick(joystick.poll(), panel);

    char model[48] = {};
    (void)camera.queryModel(model, sizeof(model));

    (void)camera.setAwcMode(ccam::devices::He130AwcMode::Atw);
    (void)pt.goHome();

    constexpr uint32_t kTickMs = 20;

    for (;;) {
        panel = touch.poll();
        const app::JoystickAxes joy = joystick.poll();

        operator_panel.tick(joy, panel);

        if (operator_panel.hasPosition()) {
            const ccam::PtPosition& pos = operator_panel.lastPosition();
            (void)pos;
        }

        demoDelayMs(kTickMs);
    }

    return 0;
}
