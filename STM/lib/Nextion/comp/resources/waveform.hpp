#pragma once

#include "../../core/nexDebug.hpp"
#include "../../core/nexTypes.hpp"
#include "../nexAttributes.hpp"
#include "../nexComponentBase.hpp"
#include "background.hpp"

namespace nex::resources {

namespace waveform_detail {

inline constexpr Literal pcoName(uint8_t channel) noexcept
{
    switch (channel) {
    case 0u: return Literal{"pco0"};
    case 1u: return Literal{"pco1"};
    case 2u: return Literal{"pco2"};
    case 3u: return Literal{"pco3"};
    default: return Literal{"pco0"};
    }
}

} // namespace waveform_detail

/** Каналы Waveform (`ch` 1…4 в NIS): `ch[i].setColor` / `add` / `addt`. */
template<uint8_t ChannelCount>
struct WaveformChannels {
    static_assert(ChannelCount >= 1u && ChannelCount <= 4u,
        "nex::resources::WaveformChannels: ChannelCount must be 1..4");

    enum Tag : uint8_t {
        Count = 192u,
        Pco0,
        Pco1,
        Pco2,
        Pco3,
    };

    class Channel {
    public:
        void setColor(nex::Color v) noexcept
        {
            attr_detail::assignNumeric(_owner, waveform_detail::pcoName(_index),
                static_cast<uint8_t>(Tag::Pco0 + _index), v);
        }

        void add(uint8_t value) noexcept
        {
            _owner.page.app.enqueue(Transaction{
                cmd::WaveForm::add(_owner.id(), _index, value),
                _owner.page.ID, _owner.id()});
        }

        //TODO добавить сохранение буфера передачи
        void addt(uint32_t byteCount) noexcept
        {
            _owner.page.app.enqueue(Transaction{
                cmd::WaveForm::addT(_owner.id(), _index, byteCount),
                _owner.page.ID, _owner.id()});
        }

    private:
        friend struct WaveformChannels;

        Component& _owner;
        uint8_t _index;

        Channel(Component& owner, uint8_t index) noexcept
            : _owner(owner)
            , _index(index)
        {}
    };

    Component& owner;

    explicit WaveformChannels(Component& ownerIn) noexcept
        : owner{ownerIn}
    {
        attr_detail::assignNumeric(owner, Literal{"ch"}, Tag::Count, ChannelCount);
    }

    [[nodiscard]] Channel operator[](uint8_t index) noexcept
    {
        NEX_ASSERT(index < ChannelCount);
        return Channel{owner, index};
    }
};

/** Фон Waveform по `sta` (compile-time `S`); `Color` — с сеткой, остальные — как `Background`. */
template<BGStyle S>
struct WfBackground;

template<>
struct WfBackground<BGStyle::Color> {
    enum Tag : uint8_t {
        Color = bg_detail::tag<32u, 0>(),
        Gdc,
        Gdw,
        Gdh,
    };

    static constexpr BGStyle kStyle = BGStyle::Color;
    Component& owner;

    explicit WfBackground(Component& ownerIn) noexcept
        : owner{ownerIn}
    {}

    void setColor(nex::Color v) noexcept
    {
        attr_detail::assignNumeric(owner, bg_detail::colorName<0>(), Tag::Color, v);
    }

    void setGridColor(uint16_t v) noexcept
    {
        attr_detail::assignNumeric(owner, Literal{"gdc"}, Tag::Gdc, v);
    }

    void setGridWidth(uint16_t v) noexcept
    {
        attr_detail::assignNumeric(owner, Literal{"gdw"}, Tag::Gdw, v);
    }

    void setGridHeight(uint16_t v) noexcept
    {
        attr_detail::assignNumeric(owner, Literal{"gdh"}, Tag::Gdh, v);
    }
};

template<>
struct WfBackground<BGStyle::Image> {
    enum Tag : uint8_t {
        Image = bg_detail::tag<33u, 0>(),
    };

    static constexpr BGStyle kStyle = BGStyle::Image;
    Component& owner;

    explicit WfBackground(Component& ownerIn) noexcept
        : owner{ownerIn}
    {}

    void setImage(PicId v) noexcept
    {
        attr_detail::assignNumeric(owner, bg_detail::imageName<0>(), Tag::Image, v);
    }
};

template<>
struct WfBackground<BGStyle::CropImage> {
    enum Tag : uint8_t {
        Crop = bg_detail::tag<34u, 0>(),
    };

    static constexpr BGStyle kStyle = BGStyle::CropImage;
    Component& owner;

    explicit WfBackground(Component& ownerIn) noexcept
        : owner{ownerIn}
    {}

    void setCrop(PicId v) noexcept
    {
        attr_detail::assignNumeric(owner, bg_detail::cropName<0>(), Tag::Crop, v);
    }
};

template<>
struct WfBackground<BGStyle::Transparent> {
    static constexpr BGStyle kStyle = BGStyle::Transparent;

    explicit WfBackground(Component&) noexcept {}
};

} // namespace nex::resources
