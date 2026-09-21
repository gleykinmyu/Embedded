#pragma once

/**
 * HMI-страница monitor — пустой фон 1024×600. Сетка и кнопки рисует McUI (`MonitorView`).
 */

#include "UI/nexHmiConfig.hpp"
#include "nex.hpp"

namespace ui {

class Application;

struct MonitorPage : nex::Page<0> {
    HMI_PAGE_CFG(monitor);

    explicit MonitorPage(nex::IAppUI& app) noexcept
        : Page<0>(app, "monitor", PG::kPageId)
    {
    }

    void onLoad() override;
    void onExit() override;

private:
    [[nodiscard]] Application& ui() const noexcept;
};

} // namespace ui
