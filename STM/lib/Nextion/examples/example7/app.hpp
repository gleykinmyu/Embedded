#pragma once

/**
 * Пример 7: McUI overlay — z-order, перекрытие виджетов, modal.
 *
 * Панели A/B/C накладываются со смещением; верхний shown-виджет получает touch.
 * Non-modal: промах по региону виджета — `continue` ниже; тап в тело — `onTouch` + raise.
 * Док внизу всегда в стеке: +A/+B/+C, Pop (hide top), Log, Modal.
 * Под ними — backdrop (чёрный фон + hint); без него Pop не затирает «дырки».
 */

#include <cstdio>

#include "nex.hpp"
#include "../../overlay/ovl.hpp"
#include "../../app/nexOvlApp.hpp"

namespace nex::examples {

class OverlayZOrderDemoApp : public OvlApp {
public:
    static constexpr uint16_t kScreenWidth = 600;
    static constexpr uint16_t kScreenHeight = 1024;

    explicit OverlayZOrderDemoApp(BIF::IByteStream& stream, ClockMsFn clockMs) noexcept
        : OvlApp(stream, {kScreenWidth, kScreenHeight}, AppTiming{clockMs})
        , dock_(*this)
        , backdrop_(*this)
        , panelA_(*this, "Panel A", Color::std::Red, Point{30u, 130u})
        , panelB_(*this, "Panel B", Color::std::Green, Point{100u, 200u})
        , panelC_(*this, "Panel C", Color::std::Blue, Point{170u, 270u})
        , msgBox_(*this)
    {}

    void boot() noexcept;
    void onMsgBox(const msg::evMsgBox& e) noexcept override;

    void hideTopWidget() noexcept;
    void showModal() noexcept;
    void logStack(const char* why) const noexcept;

private:
    enum class DockCmd : uint8_t { ShowA, ShowB, ShowC, Pop, Log, Modal };

    class ZPanel;

    /** Нижний слой стека: заливка экрана + hint (перерисовывается при Pop). */
    class Backdrop : public ovl::Widget {
    public:
        explicit Backdrop(OverlayZOrderDemoApp& app) noexcept;

        void drawBackground(const AppCanvas& cs) const override;
        [[nodiscard]] bool raiseOnPress() const noexcept override { return false; }

    private:
        OverlayZOrderDemoApp& app_;
    };

    class ControlDock : public ovl::Widget {
    public:
        explicit ControlDock(OverlayZOrderDemoApp& app) noexcept;

        void layout() noexcept override;
        void drawBackground(const AppCanvas& cs) const override;
        void drawBackgroundRegion(const AppCanvas& cs, Region clip) const override;
        void onClick(ovl::Object* target) noexcept override;
        [[nodiscard]] bool raiseOnPress() const noexcept override { return false; }

    private:
        struct Item {
            ovl::Button btn;
            DockCmd cmd;
        };

        OverlayZOrderDemoApp& app_;
        Item items_[6]{};
    };

    class ZPanel : public ovl::Widget {
    public:
        ZPanel(OverlayZOrderDemoApp& app, const char* title, Color bg, Point origin) noexcept;

        void layout() noexcept override;
        void drawBackground(const AppCanvas& cs) const override;
        void drawBackgroundRegion(const AppCanvas& cs, Region clip) const override;
        void onClick(ovl::Object* target) noexcept override;

    private:
        OverlayZOrderDemoApp& app_;
        const char* title_;
        Color bg_;
        ovl::Button pingBtn_;
        ovl::Button frontBtn_;
    };

    [[nodiscard]] const char* widgetName(const ovl::Widget& w) const noexcept {
        if (&w == &backdrop_)
            return "Backdrop";
        if (&w == &dock_)
            return "Dock";
        if (&w == &panelA_)
            return "Panel A";
        if (&w == &panelB_)
            return "Panel B";
        if (&w == &panelC_)
            return "Panel C";
        if (&w == &msgBox_)
            return "MsgBox";
        return "?";
    }

    void drawHint() noexcept {
        cs.text_in_region(Region(Point{8u, 8u}, Rect{584u, 96u}),
            "ex7 z-order: +A/+B/+C stack overlapping panels. Tap overlap — top wins. "
            "Pop removes top widget. Modal blocks touches to panels under dialog.",
            1u, Color::std::White, HAlign::Left, VAlign::Top, Color::std::Black, BGStyle::Color);
    }

    void showPanel(ZPanel& panel) noexcept;
    void onDock(DockCmd cmd) noexcept;

    ControlDock dock_;
    Backdrop backdrop_;
    ZPanel panelA_;
    ZPanel panelB_;
    ZPanel panelC_;
    ovl::MsgBox msgBox_;
};

// --- Backdrop ---

inline OverlayZOrderDemoApp::Backdrop::Backdrop(OverlayZOrderDemoApp& app) noexcept : app_(app) {
    setRegion(Region(Point{0u, 0u}, Rect{kScreenWidth, kScreenHeight}));
}

inline void OverlayZOrderDemoApp::Backdrop::drawBackground(const AppCanvas& cs) const {
    cs.rect_fill(screenRegion(), Color::std::Black);
    app_.drawHint();
}

// --- ControlDock ---

inline OverlayZOrderDemoApp::ControlDock::ControlDock(OverlayZOrderDemoApp& app) noexcept : app_(app) {
    static constexpr struct {
        const char* label;
        DockCmd cmd;
    } kDefs[] = {
        {"+A", DockCmd::ShowA},
        {"+B", DockCmd::ShowB},
        {"+C", DockCmd::ShowC},
        {"Pop", DockCmd::Pop},
        {"Log", DockCmd::Log},
        {"Mdl", DockCmd::Modal},
    };

    setRegion(Region(Point{0u, 944u}, Rect{600u, 80u}));
    for (std::size_t i = 0u; i < sizeof(kDefs) / sizeof(kDefs[0]); ++i) {
        items_[i].cmd = kDefs[i].cmd;
        items_[i].btn = ovl::Button{kDefs[i].label, Rect{84u, 48u},
            ovl::ButtonStyle{
                ovl::TextBoxStyle{Color(0x528Au), Color::std::White, 2u, 1u, 24u, Color::std::White},
                ovl::TextBoxStyle{Color::std::Green, Color::std::White, 2u, 1u, 24u, Color::std::White},
            }};
        addChildTop(items_[i].btn);
    }
    layout();
}

inline void OverlayZOrderDemoApp::ControlDock::layout() noexcept {
    Coord x = 8;
    constexpr Coord y = 16;
    constexpr uint16_t gap = 8u;
    for (Item& item : items_) {
        item.btn.setRegion(Region(Point{x, y}, item.btn.region().size));
        x = static_cast<Coord>(x + item.btn.region().size.w + gap);
    }
    Widget::layout();
}

inline void OverlayZOrderDemoApp::ControlDock::drawBackground(const AppCanvas& cs) const {
    const Region r = screenRegion();
    cs.rect_fill(r, Color(0x2104u));
    cs.rect_bordered(r, Color(0x2104u), Color::std::Gray, 2u);
}

inline void OverlayZOrderDemoApp::ControlDock::drawBackgroundRegion(const AppCanvas& cs, const Region clip) const {
    cs.rect_fill(clip, Color(0x2104u));
}

inline void OverlayZOrderDemoApp::ControlDock::onClick(ovl::Object* const target) noexcept {
    for (const Item& item : items_) {
        if (target == &item.btn) {
            app_.onDock(item.cmd);
            return;
        }
    }
}

// --- ZPanel ---

inline OverlayZOrderDemoApp::ZPanel::ZPanel(OverlayZOrderDemoApp& app, const char* const title, const Color bg,
    const Point origin) noexcept
    : app_(app)
    , title_(title != nullptr ? title : "?")
    , bg_(bg)
    , pingBtn_{"Ping", Rect{88u, 44u},
          ovl::ButtonStyle{
              ovl::TextBoxStyle{Color(0x528Au), Color::std::White, 2u, 1u, 24u, Color::std::White},
              ovl::TextBoxStyle{Color::std::Yellow, Color::std::White, 2u, 1u, 24u, Color::std::Black},
          }}
    , frontBtn_{"Front", Rect{88u, 44u},
          ovl::ButtonStyle{
              ovl::TextBoxStyle{Color(0x528Au), Color::std::White, 2u, 1u, 24u, Color::std::White},
              ovl::TextBoxStyle{Color::std::Cyan, Color::std::White, 2u, 1u, 24u, Color::std::Black},
          }}
{
    setRegion(Region(origin, Rect{220u, 180u}));
    addChildTop(pingBtn_);
    addChildTop(frontBtn_);
    layout();
}

inline void OverlayZOrderDemoApp::ZPanel::layout() noexcept {
    pingBtn_.setRegion(Region(Point{16, 110}, Rect{88u, 44u}));
    frontBtn_.setRegion(Region(Point{116, 110}, Rect{88u, 44u}));
    Widget::layout();
}

inline void OverlayZOrderDemoApp::ZPanel::drawBackground(const AppCanvas& cs) const {
    const Region frame = screenRegion();
    static constexpr uint16_t kBorder = 3u;
    static constexpr uint16_t kTitleH = 36u;

    cs.rect_bordered(frame, bg_, Color::std::White, kBorder);
    cs.text_in_region(Region(frame.ul, Rect{frame.size.w, static_cast<uint16_t>(kTitleH + 2u * kBorder)}), kBorder,
        title_, 1u, Color::std::White, HAlign::Center, VAlign::Center, bg_, BGStyle::Color);
}

inline void OverlayZOrderDemoApp::ZPanel::drawBackgroundRegion(const AppCanvas& cs, const Region clip) const {
    cs.rect_fill(clip, bg_);
}

inline void OverlayZOrderDemoApp::ZPanel::onClick(ovl::Object* const target) noexcept {
    if (target == &pingBtn_) {
        NEX_DBG("[ex7] %s Ping (top=%s)\n", title_, app_.widgetName(*app_.overlay.topWidget()));
        return;
    }
    if (target == &frontBtn_) {
        show(app_.overlay);
        (void)app_.pumpUntilIdle();
        app_.logStack("front");
        NEX_DBG("[ex7] %s -> bring to front\n", title_);
    }
}

inline void OverlayZOrderDemoApp::boot() noexcept {
    backdrop_.show(overlay);
    dock_.show(overlay);
    (void)pumpUntilIdle();
    logStack("boot");
}

inline void OverlayZOrderDemoApp::onMsgBox(const msg::evMsgBox& e) noexcept {
    NEX_DBG("[ex7] MsgBox action=%s tag=%u (modal off, touch HMI restored)\n",
        ovl::MsgBox::actionCstr(static_cast<ovl::MsgBox::Action>(e.action)), static_cast<unsigned>(e.tag));
    OvlApp::onMsgBox(e);
}

inline void OverlayZOrderDemoApp::hideTopWidget() noexcept {
    ovl::Widget* const top = overlay.topWidget();
    if (top == nullptr || top == &dock_ || top == &backdrop_) {
        NEX_DBG("[ex7] Pop: nothing above dock\n");
        return;
    }
    top->hide(overlay);
    (void)pumpUntilIdle();
    logStack("pop");
}

inline void OverlayZOrderDemoApp::showModal() noexcept {
    msgBox_.show("Z-order", ovl::MsgBox::Preset::OKCancel, 0u, ovl::MsgBox::Action::Cancel,
        "Modal: touch below is blocked. OK/Cancel to dismiss.");
    (void)pumpUntilIdle();
    logStack("modal");
}

inline void OverlayZOrderDemoApp::logStack(const char* const why) const noexcept {
    const ovl::Widget* bottom = overlay.topWidget();
    if (bottom == nullptr) {
        NEX_DBG("[ex7] stack (%s): empty\n", why != nullptr ? why : "?");
        return;
    }
    while (const ovl::Widget* const below = ovl::Overlay::nextBelow(bottom))
        bottom = below;

    NEX_DBG("[ex7] stack (%s) bottom->top:\n", why != nullptr ? why : "?");
    for (const ovl::Widget* w = bottom; w != nullptr; w = ovl::Overlay::nextAbove(w))
        NEX_DBG("  - %s\n", widgetName(*w));
}

inline void OverlayZOrderDemoApp::showPanel(ZPanel& panel) noexcept {
    panel.show(overlay);
    (void)pumpUntilIdle();
    logStack("show");
}

inline void OverlayZOrderDemoApp::onDock(const DockCmd cmd) noexcept {
    switch (cmd) {
    case DockCmd::ShowA:
        showPanel(panelA_);
        return;
    case DockCmd::ShowB:
        showPanel(panelB_);
        return;
    case DockCmd::ShowC:
        showPanel(panelC_);
        return;
    case DockCmd::Pop:
        hideTopWidget();
        return;
    case DockCmd::Log:
        logStack("dock");
        return;
    case DockCmd::Modal:
        showModal();
        return;
    }
}

} // namespace nex::examples
