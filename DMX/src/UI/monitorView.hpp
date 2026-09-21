#pragma once

#include "UI/appColors.hpp"
#include "UI/cellMap.hpp"
#include "UI/layout.hpp"
#include "UI/radioGroup.hpp"
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
    void noteFullRedraw() noexcept { _repaintChrome = true; }

    [[nodiscard]] bool raiseOnPress() const noexcept override { return false; }

    void layout() noexcept override;
    void drawBackground(const nex::AppCanvas& cs) const override;
    void drawBackgroundRegion(const nex::AppCanvas& cs, nex::Region clip) const override;
    void onClick(nex::ovl::Object* target) noexcept override;
    void onTouch(const nex::msg::evTouchXY& e) noexcept override;

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

    struct Grid : nex::ovl::Object {
        MonitorView* host = nullptr;
        uint8_t hitCol = 0;
        uint8_t hitRow = 0;
        bool haveHit = false;

        void draw(const nex::AppCanvas& cs) const override;
        void presentDirty() noexcept;
        bool onTouchXY(const nex::msg::evTouchXY& e) noexcept override;
        [[nodiscard]] nex::Region cellRegion(uint8_t col, uint8_t row) const noexcept;

    private:
        mutable CellSnap _cache[kMaxRows][kCols]{};
        mutable char _txt[kMaxRows][kCols][4]{};
        mutable char _hdr[kCols][4]{};
        mutable uint8_t _cachePage = 0xFFu;
        mutable uint8_t _cacheRows = 0;
        mutable ViewMode _cacheView = ViewMode::Current;
        mutable bool _headersOk = false;

        mutable char _rowHdr[kMaxRows][5]{};

        void drawHeaders(const nex::AppCanvas& cs) const;
        void drawRowHeaders(const nex::AppCanvas& cs) const;
        void drawCell(const nex::AppCanvas& cs, uint8_t col, uint8_t row, const CellSnap& snap) const;
        [[nodiscard]] CellSnap snapOf(uint8_t col, uint8_t row) const noexcept;
        void resetCache(uint8_t page, uint8_t nRows, ViewMode view) const noexcept;
    };

    struct Label : nex::ovl::Object {
        char text[36]{};
        nex::Color fg{kCellFg};
        nex::Color bg{kBg};
        nex::HAlign align{nex::HAlign::Left};
        bool dirty = true;

        void setText(const char* src) noexcept;
        void setFg(nex::Color color) noexcept;
        void draw(const nex::AppCanvas& cs) const override;
    };

    void applyOemCaptions() noexcept;
    void addChrome() noexcept;
    void layoutChrome() noexcept;
    void syncChrome() noexcept;
    void present(nex::ovl::Object& obj) noexcept;
    void presentDirtyLabels() noexcept;
    void presentButtons() noexcept;
    void presentRadio(const RadioGroup::Paint& p) noexcept;
    void presentSelectedLabel() noexcept;
    void bindRadioGroups() noexcept;
    void onGridClick() noexcept;

    [[nodiscard]] uint8_t rows() const noexcept { return layout::rowsFit(); }
    [[nodiscard]] uint8_t pages() const noexcept { return pageCount(rows()); }

    Application& _app;
    Tester* _tester = nullptr;
    nex::ovl::Overlay* _overlay = nullptr;

    Grid _grid{};
    nex::ovl::Button _dir;
    nex::ovl::Button _graph;
    nex::ovl::Button _pageBtn[kMaxPages];
    char _pageLabel[kMaxPages][12]{};
    nex::ovl::Button _viewBtn[5];
    RadioGroup _pages{};
    RadioGroup _views{};
    nex::ovl::Button _zero;
    nex::ovl::Button _full;
    nex::ovl::Button _blk;
    Label _link{};
    Label _cycle{};
    Label _chVal{};
    bool _repaintChrome = false;
    bool _oemReady = false;
    char _oemCurrent[16]{};
    char _oemFast[16]{};
    char _oemLog[12]{};
    char _oemGraph[12]{};
};

} // namespace ui
