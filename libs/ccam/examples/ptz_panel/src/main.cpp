/**
 * @file main.cpp
 * @brief Демо: пульт с джойстиком + сенсорная панель → PTZ по RS485.
 *
 * Сейчас — заглушки IJoystick / ITouchPanel и симуляция ввода.
 * Позже замените DemoJoystick / DemoTouchPanel на драйверы вашей панели.
 */

#include "app/joystick.hpp"
#include "app/ptz_operator.hpp"
#include "app/touch_panel.hpp"
#include "ccam.hpp"
#include "devices/devices.hpp"

#include <cstdint>

namespace {

/* --- RS485: замените на UART + DE/RE (STM32 HAL) --- */
class StubRs485Hal : public ccam::IRs485Hal {
public:
    int send(const uint8_t* data, size_t len) override
    {
        (void)data;
        return static_cast<int>(len);
    }

    int receive(uint8_t* data, size_t len, uint32_t timeout_ms) override
    {
        (void)data;
        (void)len;
        (void)timeout_ms;
        return 0;
    }
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

        /* Синусоидальное «ведение» камеры */
        joy.pan = static_cast<int16_t>((t < 180) ? (t * 5) : ((360 - t) * 5));
        joy.tilt = static_cast<int16_t>(((t + 90) % 180 - 90) * 4);

        /* Zoom in/out каждые ~2 с (при tick каждые 20 ms) */
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
 * Замените poll() на парсинг событий touch LCD (LVGL, emWin, …).
 */
class DemoTouchPanel : public app::ITouchPanel {
public:
    app::PanelSettings poll() override
    {
        app::PanelSettings s = settings_;
        ++tick_;

        /* Каждые ~5 с — новое действие с «экрана» */
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

void demoDelayMs(uint32_t ms)
{
    static uint32_t demo_ms = 0;
    demo_ms += ms;
    (void)demo_ms;
}

} // namespace

int main()
{
    StubRs485Hal hal;
    ccam::Rs485Transport transport(hal);

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

    /* Пример прямого вызова методов устройства */
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
            /* TODO: вывести pan/tilt/zoom на LCD панели */
        }

        demoDelayMs(kTickMs);
    }

    return 0;
}
