#pragma once

#include <cstddef>
#include <cstdint>

#include "overlay/ovl.hpp"

namespace PHL {
struct DateTime;
}

namespace server {

/** Нижняя строка состояния McUI: Status | File (середина, с усечением) | Time. */
class StatusBar : public nex::ovl::Widget {
public:
    enum Field : uint8_t {
        Status = 0,
        File,
        Time,
        Count,
    };

    static constexpr uint8_t kFieldCount = Count;
    static constexpr nex::Coord kDefaultHeight = 48;
    static constexpr nex::Coord kOriginY = 974;
    /** Хватает под `SD Connecting (NNNk)`; остаток ширины — имени файла. */
    static constexpr nex::Coord kStatusColumnWidth = 220;
    static constexpr nex::Coord kSideColumnWidth = 120;
    static constexpr size_t kTextCap = 64u;

    explicit StatusBar(nex::Rect screen, nex::Coord barHeight = kDefaultHeight,
                       nex::Coord originY = kOriginY) noexcept;

    void show(nex::ovl::Overlay& ovl) noexcept;
    void hide(nex::ovl::Overlay& ovl) noexcept;

    void setStatus(const char* text) noexcept { column(Status).setText(text); }
    /** Имя файла; при @a edited дописывает `*` и при необходимости усекает. */
    void setFile(const char* text, bool edited = false) noexcept;
    void setTime(const PHL::DateTime& dt) noexcept;

    /** Ширина боковой колонки (Status / Time); File занимает оставшуюся середину. */
    void setColumnWidth(Field field, nex::Coord width) noexcept;

    void layout() noexcept override;
    [[nodiscard]] bool raiseOnPress() const noexcept override { return false; }

private:
    struct Column : nex::ovl::Object {
        char text[kTextCap]{};
        nex::Coord width{0};
        nex::HAlign align{nex::HAlign::Left};
        bool fit{false};

        void setText(const char* src) noexcept;
        void append(const char* src) noexcept;
        void setWidth(nex::Coord w) noexcept;
        void draw(const nex::AppCanvas& cs) const override;
    };

    [[nodiscard]] Column& column(Field field) noexcept { return _columns[field]; }
    [[nodiscard]] const Column& column(Field field) const noexcept { return _columns[field]; }

    /** `Widget::draw` → фон полосы, затем `drawChildren` → Column::draw. */
    void drawBackground(const nex::AppCanvas& cs) const override;
    /** `redrawObject` / present одной колонки — подложка clip до Column::draw. */
    void drawBackgroundRegion(const nex::AppCanvas& cs, nex::Region clip) const override;

    void onColumnWidthChanged() noexcept;
    /** Одна колонка: `redrawObject` = drawBackgroundRegion + Column::draw. */
    void present(const nex::ovl::Object& obj) noexcept;
    /** Весь бар: `Widget::draw`. */
    void presentAll() noexcept;

    nex::Rect _screen;
    nex::Coord _barHeight;
    Column _columns[kFieldCount]{};
    nex::ovl::Overlay* _overlay{nullptr};
};

} // namespace server
