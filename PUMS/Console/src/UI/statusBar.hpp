#pragma once

#include <cstddef>
#include <cstdint>

#include "overlay/ovl.hpp"

namespace server {

/** Нижняя строка состояния McUI: Status | File | Time (слева направо). */
class StatusBar : public nex::ovl::Widget {
public:
    enum class Field : uint8_t {
        Status = 0,
        File,
        Time,
        Count,
    };

    static constexpr uint16_t kDefaultHeight = 32u;
    static constexpr size_t kTextCap = 48u;

    explicit StatusBar(nex::Rect screen, uint16_t barHeight = kDefaultHeight) noexcept;

    void show(nex::ovl::Overlay& ovl) noexcept;
    void hide(nex::ovl::Overlay& ovl) noexcept;

    void setStatus(const char* text) noexcept { setField(Field::Status, text); }
    void setFile(const char* text) noexcept { setField(Field::File, text); }
    void setTime(const char* text) noexcept { setField(Field::Time, text); }

    /** Ширина колонки в пикселях; `0` — доля поровну с остальными нулевыми. */
    void setColumnWidth(Field field, uint16_t width) noexcept;

    void layout() noexcept override;
    void drawBackground(const nex::AppCanvas& cs) const override;
    [[nodiscard]] bool raiseOnPress() const noexcept override { return false; }

private:
    struct Column {
        char text[kTextCap]{};
        nex::Region region{};
        uint16_t width{0u};
    };

    void setField(Field field, const char* text) noexcept;
    void layoutColumns() noexcept;
    void redrawIfShown() const noexcept;

    nex::Rect _screen;
    uint16_t _barHeight;
    Column _columns[static_cast<uint8_t>(Field::Count)]{};
    nex::ovl::Overlay* _overlay{nullptr};
};

} // namespace server
