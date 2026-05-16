// nexApplicationAddons.cpp — вспомогательные команды и UI ошибок Application.

#include "nexApplication.hpp"
#include "../core/nexErrors.hpp"

namespace nex {

void Application::onError(const msg::Status& status, uint8_t page_id, uint8_t comp_id) noexcept {
    printStatusError(status, page_id, comp_id);
}

void Application::showErrorBox(const msg::Status& status, uint8_t page_id, uint8_t comp_id) noexcept {
    if (_screen.panel_width == 0u || _screen.panel_height == 0u)
        return;

    _touchBlocked = true;

    const uint16_t uw = _screen.userWidth();
    const uint16_t uh = _screen.userHeight();
    if (uw < 32u || uh < 32u) {
        _touchBlocked = false;
        return;
    }

    formatStatusMessage(status, page_id, comp_id, _error_box_text, kErrorBoxTextCap);

    const int16_t margin = static_cast<int16_t>((uw < uh ? uw : uh) / 16);
    int16_t boxW = static_cast<int16_t>((uw * 4u) / 5u);
    int16_t boxH = static_cast<int16_t>((uh * 2u) / 7u);
    if (boxW < 120)
        boxW = 120;
    if (boxH < 56)
        boxH = 56;
    const int16_t maxW = static_cast<int16_t>(uw) - 2 * margin;
    const int16_t maxH = static_cast<int16_t>(uh) - 2 * margin;
    if (boxW > maxW)
        boxW = maxW;
    if (boxH > maxH)
        boxH = maxH;

    const int16_t ux0 = (static_cast<int16_t>(uw) - boxW) / 2;
    const int16_t uy0 = (static_cast<int16_t>(uh) - boxH) / 2;
    const int16_t ux1 = ux0 + boxW - 1;
    const int16_t uy1 = uy0 + boxH - 1;

    static constexpr int16_t kBorder = 2;
    static constexpr int16_t kTextPad = 8;

    Point panelUl{};
    Point panelLr{};
    _screen.mapUserRectToPanel(ux0, uy0, ux1, uy1, panelUl, panelLr);

    const int16_t fillUx0 = ux0 + kBorder;
    const int16_t fillUy0 = uy0 + kBorder;
    const int16_t fillUx1 = ux1 - kBorder;
    const int16_t fillUy1 = uy1 - kBorder;
    Point fillUl{};
    Point fillLr{};
    _screen.mapUserRectToPanel(fillUx0, fillUy0, fillUx1, fillUy1, fillUl, fillLr);

    const int16_t textUx0 = ux0 + kTextPad;
    const int16_t textUy0 = uy0 + kTextPad;
    const uint32_t textW = static_cast<uint32_t>(boxW - 2 * kTextPad);
    const uint32_t textH = static_cast<uint32_t>(boxH - 2 * kTextPad);
    const Point textUl = _screen.mapUserToPanel(textUx0, textUy0);

    cs.rect_fill(fillUl, fillLr, Color::std::Navy);
    cs.rect_outline(panelUl, panelLr, Color::std::Red);
    cs.text_in_region(textUl, textW, textH, 2u, Color::std::White, Color::std::Navy, 1u, 1u, BGStyle::Color,
        _error_box_text);
}

void Application::calibrateTouch() noexcept {
    enqueue(Transaction{cmd::System::touchJ(), 0u, 0u});
}

void Application::restart() noexcept {
    enqueue(Transaction{cmd::System::restart(), 0u, 0u});
    _touchBlocked = false;
}

void Application::setRandGen(int32_t minVal, int32_t maxVal) noexcept {
    enqueue(Transaction{cmd::System::randset(minVal, maxVal), 0u, 0u});
}

void Application::play(uint32_t channel, uint32_t resourceId, uint32_t loop01) noexcept {
    enqueue(Transaction{cmd::Play(channel, resourceId, loop01), 0u, 0u});
}

} // namespace nex
