#pragma once

#include "UI/appColors.hpp"
#include "UI/cellMap.hpp"
#include "UI/layout.hpp"
#include "UI/radioGroup.hpp"
#include "overlay/ovl.hpp"

namespace ui {

class Application;
class Tester;

/** График выбранных каналов (до 8). Нижний статус — как на мониторе. */
class GraphView : public nex::ovl::Widget {
public:
    explicit GraphView(Application& app) noexcept;

    void bind(Tester* tester) noexcept { _tester = tester; }

    void showOn(nex::ovl::Overlay& ovl) noexcept;
    void hideFrom(nex::ovl::Overlay& ovl) noexcept;
    void refresh() noexcept;

    [[nodiscard]] bool raiseOnPress() const noexcept override { return false; }

    void layout() noexcept override;
    void drawBackground(const nex::AppCanvas& cs) const override;
    void drawBackgroundRegion(const nex::AppCanvas& cs, nex::Region clip) const override;
    void onClick(nex::ovl::Object* target) noexcept override;

private:
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

    struct Plot : nex::ovl::Object {
        GraphView* host = nullptr;
        void draw(const nex::AppCanvas& cs) const override;
        void presentNew() noexcept;

    private:
        mutable uint16_t _gen = 0;
        mutable uint16_t _drawn = 0;
        mutable char _leg[kMaxSelect][8]{};
        mutable char _valLbl[5][4]{};
        mutable char _timeLbl[11][6]{};

        void drawAxes(const nex::AppCanvas& cs) const;
        void drawGrid(const nex::AppCanvas& cs) const;
        void drawLegend(const nex::AppCanvas& cs) const;
        void drawSeries(const nex::AppCanvas& cs, uint16_t from, uint16_t to) const;
        [[nodiscard]] nex::Region plotArea() const noexcept;
        [[nodiscard]] nex::Point samplePoint(uint8_t series, uint16_t i) const noexcept;
    };

    void applyOem() noexcept;
    void layoutChrome() noexcept;
    void syncChrome() noexcept;
    void syncSpanLabel() noexcept;
    void applySpan(int8_t delta) noexcept;
    void present(nex::ovl::Object& obj) noexcept;
    void presentSpan() noexcept;
    void presentDirtyLabels() noexcept;
    void goMonitor() noexcept;

    Application& _app;
    Tester* _tester = nullptr;
    nex::ovl::Overlay* _overlay = nullptr;

    nex::ovl::Button _back;
    nex::ovl::Button _spanPrev;
    nex::ovl::Button _spanNext;
    nex::ovl::Button _spanLabel;
    char _spanText[8]{};
    Plot _plot{};
    Label _link{};
    Label _cycle{};
    Label _chVal{};
    bool _oemReady = false;
    char _oemBack[12]{};
};

} // namespace ui
