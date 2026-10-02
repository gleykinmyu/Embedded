#pragma once

/**
 * HMI-страница monitor — пустой фон 480×272. Сетка и кнопки рисует McUI (`MonitorView`).
 */

#include "UI/nexHmiConfig.hpp"
#include "nex.hpp"

namespace ui {

class Application;

struct MonitorPage : nex::Page<0> {
    HMI_PAGE_CFG(mon);

    explicit MonitorPage(nex::IAppUI& app) noexcept
        : Page<0>(app, "mon", PG::kPageId)
    {
    }

    void onExit() override;

private:
    [[nodiscard]] Application& ui() const noexcept;
};

} // namespace ui
