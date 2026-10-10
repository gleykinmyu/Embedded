/**
 * @file ui.hpp
 * @brief Фасад приложения UI (singleton App).
 *
 * Экраны Monitor/Graph и DemoFeed живут в ui.cpp; отсюда только API
 * навигации, alert и доступа к Tester.
 */
#pragma once

namespace ui {

class Tester;

/** Точка входа C++ UI: init, переключение экранов, MsgBox. */
class App {
public:
    static App& instance() noexcept;

    /** Создаёт монитор, грузит его, стартует DemoFeed. Звать под esp_lv_adapter_lock. */
    void init() noexcept;

    void showMonitor() noexcept;
    void showGraph() noexcept; ///< Graph создаётся лениво при первом вызове
    [[nodiscard]] bool graphVisible() const noexcept { return _graphVisible; }

    void alert(const char* msg) noexcept;

    [[nodiscard]] Tester& tester() noexcept;

private:
    App() = default;
    bool _graphVisible = false;
};

} // namespace ui
