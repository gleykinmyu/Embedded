#pragma once

#include "UI/appColors.hpp"
#include "overlay/ovl.hpp"

namespace ui {

/** Эксклюзивный выбор среди `ovl::Button`. Не виджет: не рисует полосу целиком. */
class RadioGroup {
public:
    static constexpr uint8_t kNone = 0xFFu;
    static constexpr uint8_t kCap = 8u;

    struct Paint {
        bool hit = false;
        nex::ovl::Button* a = nullptr;
        nex::ovl::Button* b = nullptr;
    };

    void bind(nex::ovl::Button& btn) noexcept;

    [[nodiscard]] uint8_t size() const noexcept { return _n; }
    [[nodiscard]] uint8_t selected() const noexcept { return _sel; }
    [[nodiscard]] int8_t indexOf(const nex::ovl::Object* obj) const noexcept;

    /** Стили по индексу, без отрисовки. Повтор с тем же индексом — no-op. */
    void sync(uint8_t index) noexcept;

    Paint select(uint8_t index) noexcept;
    Paint select(const nex::ovl::Object* target) noexcept;

    static void style(nex::ovl::Button& btn, bool on) noexcept;
    static void setLabel(nex::ovl::Button& btn, const char* label) noexcept;

private:
    nex::ovl::Button* _btn[kCap]{};
    uint8_t _n = 0;
    uint8_t _sel = kNone;
};

} // namespace ui
