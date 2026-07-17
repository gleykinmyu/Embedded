#pragma once

#include <cstddef>
#include <cstdint>

#include "overlay/ovl.hpp"

namespace server {

/** Нижняя строка состояния McUI: Status слева, File по центру бара, Time справа. */
class StatusBar : public nex::ovl::Widget {
public:
    enum class Field : uint8_t {
        Status = 0,
        File,
        Time,
        Count,
    };

    static constexpr uint16_t kDefaultHeight = 48u;
    static constexpr nex::Coord kOriginY = 974;
    static constexpr uint16_t kStatusColumnWidth = 260u;
    static constexpr uint16_t kSideColumnWidth = 120u;
    static constexpr size_t kTextCap = 64u;

    explicit StatusBar(nex::Rect screen, uint16_t barHeight = kDefaultHeight) noexcept;

    void show(nex::ovl::Overlay& ovl) noexcept;
    void hide(nex::ovl::Overlay& ovl) noexcept;

    void setStatus(const char* text) noexcept { setField(Field::Status, text); }
    void setFile(const char* text) noexcept { setField(Field::File, text); }
    /** Имя файла; при @a edited дописывает `*` через append. */
    void setFile(const char* text, bool edited) noexcept;
    void setTime(const char* text) noexcept { setField(Field::Time, text); }

    /** Ширина боковой колонки (Status / Time); File всегда на всю ширину бара. */
    void setColumnWidth(Field field, uint16_t width) noexcept;

    void layout() noexcept override;
    void drawBackground(const nex::AppCanvas& cs) const override;
    [[nodiscard]] bool raiseOnPress() const noexcept override { return false; }

private:
    struct Column {
        char text[kTextCap]{};
        nex::Region region{};
        uint16_t width{0u};
        nex::HAlign align{nex::HAlign::Left};
    };

    void setField(Field field, const char* text) noexcept;
    void appendField(Field field, const char* text) noexcept;
    void layoutColumns() noexcept;
    void redrawIfShown() const noexcept;

    nex::Rect _screen;
    uint16_t _barHeight;
    Column _columns[static_cast<uint8_t>(Field::Count)]{};
    nex::ovl::Overlay* _overlay{nullptr};
};

} // namespace server
