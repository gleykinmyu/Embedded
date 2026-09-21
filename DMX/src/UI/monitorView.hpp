#pragma once

#include "UI/appColors.hpp"
#include "UI/cellMap.hpp"
#include "UI/layout.hpp"
#include "overlay/ovl.hpp"

namespace ui {

class Application;
class Tester;

/** McUI главной страницы: сетка 16×N, шапка, страницы диапазона, режимы, TX. */
class MonitorView : public nex::ovl::Widget {
public:
    explicit MonitorView(Application& app) noexcept;

    void bind(Tester* tester) noexcept { _tester = tester; }
    [[nodiscard]] Tester* tester() const noexcept { return _tester; }

    void showOn(nex::ovl::Overlay& ovl) noexcept;
    void hideFrom(nex::ovl::Overlay& ovl) noexcept;

    void refresh() noexcept;

    [[nodiscard]] bool raiseOnPress() const noexcept override { return false; }

    void layout() noexcept override;
    void drawBackground(const nex::AppCanvas& cs) const override;
    void drawBackgroundRegion(const nex::AppCanvas& cs, nex::Region clip) const override;
    void onClick(nex::ovl::Object* target) noexcept override;

private:
    struct Grid : nex::ovl::Object {
        MonitorView* host = nullptr;
        uint8_t hitCol = 0;
        uint8_t hitRow = 0;
        bool haveHit = false;

        void draw(const nex::AppCanvas& cs) const override;
        bool onTouchXY(const nex::msg::evTouchXY& e) noexcept override;
        [[nodiscard]] nex::Region cellRegion(uint8_t col, uint8_t row) const noexcept;
    };

    struct Label : nex::ovl::Object {
        char text[20]{};
        nex::Color fg{kCellFg};
        nex::Color bg{kBg};
        nex::HAlign align{nex::HAlign::Left};

        void setText(const char* src) noexcept;
        void draw(const nex::AppCanvas& cs) const override;
    };

    void addChrome() noexcept;
    void layoutChrome() noexcept;
    void syncChrome() noexcept;
    void present(nex::ovl::Object& obj) noexcept;
    void applyButtonStyle(nex::ovl::Button& btn, bool selected) noexcept;
    void onGridClick() noexcept;
    void goNet() noexcept;

    [[nodiscard]] uint8_t rows() const noexcept { return layout::rowsFit(); }
    [[nodiscard]] uint8_t pages() const noexcept { return pageCount(rows()); }

    Application& _app;
    Tester* _tester = nullptr;
    nex::ovl::Overlay* _overlay = nullptr;

    Grid _grid{};
    nex::ovl::Button _dir;
    nex::ovl::Button _rs485;
    nex::ovl::Button _art;
    nex::ovl::Button _sacn;
    nex::ovl::Button _net;
    Label _univ{};
    nex::ovl::Button _pageBtn[kMaxPages];
    char _pageLabel[kMaxPages][12]{};
    nex::ovl::Button _viewBtn[5];
    nex::ovl::Button _zero;
    nex::ovl::Button _full;
    nex::ovl::Button _blk;
    Label _link{};
    Label _cycle{};
    Label _chVal{};
};

} // namespace ui
