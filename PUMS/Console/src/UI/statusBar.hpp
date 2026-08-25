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
    static constexpr uint16_t kDefaultHeight = 48u;
    static constexpr nex::Coord kOriginY = 974;
    /** Хватает под `min NNNk`; остаток ширины — имени файла. */
    static constexpr uint16_t kStatusColumnWidth = 140u;
    static constexpr uint16_t kSideColumnWidth = 120u;
    static constexpr size_t kTextCap = 64u;

    explicit StatusBar(nex::Rect screen, uint16_t barHeight = kDefaultHeight) noexcept;

    void show(nex::ovl::Overlay& ovl) noexcept;
    void hide(nex::ovl::Overlay& ovl) noexcept;

    void setStatus(const char* text) noexcept { column(Status).setText(text); }
    /** Имя файла; при @a edited дописывает `*` и при необходимости усекает. */
    void setFile(const char* text, bool edited = false) noexcept;
    void setTime(const PHL::DateTime& dt) noexcept;

    /** Ширина боковой колонки (Status / Time); File занимает оставшуюся середину. */
    void setColumnWidth(Field field, uint16_t width) noexcept;

    void layout() noexcept override;
    [[nodiscard]] bool raiseOnPress() const noexcept override { return false; }

private:
    struct Column : nex::ovl::Object {
        char text[kTextCap]{};
        uint16_t width{0u};
        nex::HAlign align{nex::HAlign::Left};
        bool fit{false};

        void setText(const char* src) noexcept;
        void append(const char* src) noexcept;
        void setWidth(uint16_t w) noexcept;
        void draw(const nex::AppCanvas& cs) const override;
    };

    [[nodiscard]] Column& column(Field field) noexcept { return _columns[field]; }
    [[nodiscard]] const Column& column(Field field) const noexcept { return _columns[field]; }

    void drawBackground(const nex::AppCanvas& cs) const override;
    void drawBackgroundRegion(const nex::AppCanvas& cs, nex::Region clip) const override;

    void onColumnWidthChanged() noexcept;
    void present(const nex::ovl::Object& obj) noexcept;
    void presentAll() noexcept;

    nex::Rect _screen;
    uint16_t _barHeight;
    Column _columns[kFieldCount]{};
    nex::ovl::Overlay* _overlay{nullptr};
};

} // namespace server
