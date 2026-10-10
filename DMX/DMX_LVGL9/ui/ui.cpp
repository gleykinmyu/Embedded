/**
 * @file ui.cpp
 * @brief Реализация App + C-обёртка ui_init().
 *
 * Владеет глобальными Tester / MonitorScreen / GraphScreen / DemoFeed.
 * Graph монтируется только при первом showGraph() — быстрее холодный старт.
 */

#include "ui.hpp"

#include "ui.h"

#include "colors.hpp"
#include "demo_feed.hpp"
#include "graph_screen.hpp"
#include "monitor_screen.hpp"
#include "tester.hpp"
#include "themed_alert.hpp"

#include "esp_log.h"

#include <lv/lv.hpp>

namespace {

const char* TAG = "dmx_ui";

ui::Tester g_tester;
ui::MonitorScreen g_monitor{g_tester};
ui::GraphScreen g_graph{g_tester};
ui::DemoFeed g_demo;

lv_obj_t* g_monScreen = nullptr;
lv_obj_t* g_graphScreen = nullptr;
bool g_graphBuilt = false;

/** Тёмный фон экрана до/вместо темы LVGL — меньше белой вспышки. */
void paintScreenBg(lv_obj_t* scr) noexcept
{
    if (!scr)
        return;
    lv_obj_set_style_bg_color(scr, ui::colors::bg(), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
}

/** Ленивая сборка экрана графика (один раз). */
void ensureGraph() noexcept
{
    if (g_graphBuilt)
        return;
    auto graph = lv::screen_create();
    paintScreenBg(graph.get());
    g_graphScreen = graph.get();
    g_graph.mount(graph);
    g_graphBuilt = true;
}

} // namespace

namespace ui {

App& App::instance() noexcept
{
    static App app;
    return app;
}

Tester& App::tester() noexcept
{
    return g_tester;
}

void App::init() noexcept
{
    ESP_LOGI(TAG, "UI init (DemoFeed until RS485)");

    paintScreenBg(lv_screen_active());

    auto mon = lv::screen_create();
    paintScreenBg(mon.get());
    g_monScreen = mon.get();
    g_monitor.mount(mon);

    lv::screen_load(mon);
    g_demo.start(g_tester);
    _graphVisible = false;
}

void App::showMonitor() noexcept
{
    if (g_graphBuilt)
        g_graph.pause();
    _graphVisible = false;
    if (g_monScreen) {
        lv_screen_load(g_monScreen);
        g_monitor.resume();
    }
}

void App::showGraph() noexcept
{
    g_monitor.pause();
    ensureGraph();
    _graphVisible = true;
    if (g_graphScreen) {
        lv_screen_load(g_graphScreen);
        g_graph.on_show();
    }
}

void App::alert(const char* msg) noexcept
{
    ThemedAlert::show("DMX", msg);
}

} // namespace ui

extern "C" void ui_init(void)
{
    ui::App::instance().init();
}
