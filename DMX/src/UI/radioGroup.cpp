#include "UI/radioGroup.hpp"

namespace ui {

void RadioGroup::bind(nex::ovl::Button& btn) noexcept
{
    if (_n >= kCap)
        return;
    _btn[_n++] = &btn;
}

int8_t RadioGroup::indexOf(const nex::ovl::Object* const obj) const noexcept
{
    if (obj == nullptr)
        return -1;
    for (uint8_t i = 0; i < _n; ++i) {
        if (_btn[i] == obj)
            return static_cast<int8_t>(i);
    }
    return -1;
}

void RadioGroup::style(nex::ovl::Button& btn, const bool on) noexcept
{
    const nex::Region keep = btn.region();
    btn.setStyle(on ? kBtnOn : kBtnIdle);
    btn.setRegion(keep);
}

void RadioGroup::setLabel(nex::ovl::Button& btn, const char* const label) noexcept
{
    const nex::Region keep = btn.region();
    btn.setLabel(label);
    btn.setRegion(keep);
}

void RadioGroup::sync(const uint8_t index) noexcept
{
    const uint8_t next = (index < _n) ? index : kNone;
    if (next == _sel)
        return;
    for (uint8_t i = 0; i < _n; ++i)
        style(*_btn[i], i == next);
    _sel = next;
}

RadioGroup::Paint RadioGroup::select(const uint8_t index) noexcept
{
    Paint p{};
    if (index >= _n || _btn[index] == nullptr)
        return p;

    p.hit = true;
    p.b = _btn[index];
    if (_sel < _n && _sel != index)
        p.a = _btn[_sel];
    if (p.a == nullptr)
        p.a = p.b;

    if (_sel < _n && _sel != index)
        style(*_btn[_sel], false);
    style(*_btn[index], true);
    _sel = index;
    return p;
}

RadioGroup::Paint RadioGroup::select(const nex::ovl::Object* const target) noexcept
{
    const int8_t i = indexOf(target);
    if (i < 0)
        return {};
    return select(static_cast<uint8_t>(i));
}

} // namespace ui
