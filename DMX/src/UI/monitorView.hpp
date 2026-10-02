#pragma once

#include "UI/appColors.hpp"
#include "UI/cellMap.hpp"
#include "UI/layout.hpp"
#include "UI/radioGroup.hpp"
#include "overlay/ovl.hpp"

namespace ui {

class Application;
class Tester;

/** McUI главной страницы: сетка 10×4, шапка, страницы диапазона, режимы, TX. */
class MonitorView : public nex::ovl::Widget {
public:
    explicit MonitorView(Application& app) noexcept;

    void bind(Tester* tester) noexcept { _tester = tester; }
    [[nodiscard]] Tester* tester() const noexcept { return _tester; }

    void showOn(nex::ovl::Overlay& ovl) noexcept;
    void hideFrom(nex::ovl::Overlay& ovl) noexcept;

    void refresh() noexcept;
    void noteFullRedraw() noexcept;

    [[nodiscard]] bool raiseOnPress() const noexcept override { return false; }

    void layout() noexcept override;
    void drawBackground(const nex::AppCanvas& cs) const override;
    void drawBackgroundRegion(const nex::AppCanvas& cs, nex::Region clip) const override;
    void onClick(nex::ovl::Object* target) noexcept override;

private:
    struct CellSnap {
        uint8_t value = 0;
        uint8_t flags = 0; ///< bit0 empty, bit1 selected, bit2 fast

        [[nodiscard]] constexpr bool empty() const noexcept { return (flags & 1u) != 0u; }
        [[nodiscard]] constexpr bool selected() const noexcept { return (flags & 2u) != 0u; }
        [[nodiscard]] constexpr bool fast() const noexcept { return (flags & 4u) != 0u; }
        [[nodiscard]] constexpr bool operator==(const CellSnap& o) const noexcept
        {
            return value == o.value && flags == o.flags;
        }
        [[nodiscard]] constexpr bool operator!=(const CellSnap& o) const noexcept { return !(*this == o); }

        static constexpr CellSnap vacant() noexcept { return CellSnap{0, 1u}; }
        static constexpr CellSnap make(uint8_t v, bool sel, bool changed) noexcept
        {
            return CellSnap{v, static_cast<uint8_t>((sel ? 2u : 0u) | (changed ? 4u : 0u))};
        }
    };

    struct Cell : nex::ovl::Object {
        MonitorView* host = nullptr;
        uint8_t col = 0;
        uint8_t row = 0;
        CellSnap snap = CellSnap::vacant();
        char txt[4]{};

        [[nodiscard]] bool sync() noexcept;
        void draw(const nex::AppCanvas& cs) const override;
        bool onTouchXY(const nex::msg::evTouchXY& e) noexcept override;
    };

    struct Head : nex::ovl::Object {
        char text[5]{};
        bool dirty = true;

        void setText(const char* src) noexcept;
        void draw(const nex::AppCanvas& cs) const override;
    };

    struct Label : nex::ovl::Object {
        char text[36]{};
        nex::Color fg{kText};
        nex::Color bg{kChrome};
        nex::HAlign align{nex::HAlign::Left};
        bool dirty = true;

        void setText(const char* src) noexcept;
        void setFg(nex::Color color) noexcept;
        void draw(const nex::AppCanvas& cs) const override;
        bool onTouchXY(const nex::msg::evTouchXY& e) noexcept override;
    };

    struct Pip : nex::ovl::Object {
        nex::Color fill{kBorder};
        bool dirty = true;

        void setFill(nex::Color color) noexcept;
        void draw(const nex::AppCanvas& cs) const override;
        bool onTouchXY(const nex::msg::evTouchXY& e) noexcept override;
    };

    struct SelFrame : nex::ovl::Object {
        enum Side : uint8_t { Left = 1u, Right = 2u, Bottom = 4u, Top = 8u };
        uint8_t sides = static_cast<uint8_t>(Left | Right | Bottom);

        void draw(const nex::AppCanvas& cs) const override;
    };

    void applyOemCaptions() noexcept;
    void addChrome() noexcept;
    void layoutChrome() noexcept;
    void layoutGrid() noexcept;
    void syncChrome() noexcept;
    void syncHeaders() noexcept;
    bool present(nex::ovl::Object& obj) noexcept;
    bool enqueueDraw(nex::ovl::Object& obj) noexcept;
    [[nodiscard]] bool isGridCell(const nex::ovl::Object& obj) const noexcept;
    void presentDirtyLabels() noexcept;
    void presentDirtyCells() noexcept;
    void presentButtons() noexcept;
    void styleClear(bool lit) noexcept;
    void syncViewLabel() noexcept;
    void syncScaleLabel() noexcept;
    void applyScale() noexcept;
    void onCellClick(Cell& cell) noexcept;
    void applyPage(int8_t delta) noexcept;
    void syncPageLabel() noexcept;
    [[nodiscard]] CellSnap snapOf(uint8_t col, uint8_t row) const noexcept;

    [[nodiscard]] uint8_t rows() const noexcept { return layout::rowsFit(); }
    [[nodiscard]] uint8_t pages() const noexcept { return pageCount(rows()); }

    Application& _app;
    Tester* _tester = nullptr;
    nex::ovl::Overlay* _overlay = nullptr;

    Cell _cells[kMaxRows][kCols]{};
    Head _colH[kCols]{};
    Head _rowH[kMaxRows]{};
    nex::ovl::Button _graph;
    nex::ovl::Button _clear;
    nex::ovl::Button _pagePrev;
    nex::ovl::Button _pageNext;
    Label _pageRange{};
    char _pageLabel[16]{};
    nex::ovl::Button _view;
    nex::ovl::Button _scale;
    SelFrame _pageBox{};
    SelFrame _selBox{};
    Label _link{};
    Pip _pip{};
    Label _chVal{};
    bool _repaintChrome = false;
    bool _forceCells = false;
    bool _clearLit = false;
    ViewMode _viewShown = ViewMode::Current;
    ValueScale _scaleShown = ValueScale::Dmx;
    uint8_t _hdrPage = 0xFFu;
    bool _oemReady = false;
    char _oemFast[16]{};
    char _oemGraph[12]{};
    char _oemClear[12]{};
};

} // namespace ui
