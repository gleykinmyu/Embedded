/**
 * @file touch_panel.hpp
 * @brief Параметры и команды с сенсорной панели (реализуйте ITouchPanel под ваш LCD).
 */

#pragma once

#include <cstdint>

namespace app {

/** Одноразовое действие с экрана (обрабатывается один раз в tick). */
enum class TouchAction : uint8_t {
    None,
    StopAll,
    RecallPreset,
    SavePreset,
    AwcStart,
    PowerToggle,
    QueryPosition,
};

/**
 * Настройки, которые оператор меняет на сенсорной панели.
 * Храните в RAM / EEPROM — по необходимости.
 */
struct PanelSettings {
    uint8_t pan_tilt_rate = 30;   ///< 1..49 — макс. скорость pan/tilt от джойстика
    uint8_t zoom_focus_rate = 20; ///< 1..49 — макс. скорость zoom/focus
    uint8_t preset = 1;           ///< 0..99 — выбранный пресет
    /** Значение OAW (цифра 0..9); для AW-HE130 = static_cast<uint8_t>(He130AwcMode). */
    uint8_t awc_mode = 0;

    bool pt_power_on = true;

    TouchAction pending = TouchAction::None;
};

class ITouchPanel {
public:
    virtual ~ITouchPanel() = default;

    /** Текущие настройки + необработанное действие (pending). */
    virtual PanelSettings poll() = 0;

    /** Сброс pending после обработки (опционально). */
    virtual void ackAction(TouchAction action) { (void)action; }
};

} // namespace app
