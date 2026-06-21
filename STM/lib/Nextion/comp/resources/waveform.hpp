#pragma once

#include "../../core/nexDebug.hpp"
#include "../../core/nexTypes.hpp"
#include "../nexAttributes.hpp"
#include "../nexComponentBase.hpp"
#include "background.hpp"

namespace nex::resources {

namespace waveform_detail {

inline constexpr attr::Id pcoId(uint8_t channel) noexcept
{
    switch (channel) {
    case 0u: return attr::Id::Pco0;
    case 1u: return attr::Id::Pco1;
    case 2u: return attr::Id::Pco2;
    case 3u: return attr::Id::Pco3;
    default: return attr::Id::Pco0;
    }
}

} // namespace waveform_detail

/** Каналы Waveform (`ch` 1…4 в NIS): `ch[i].setColor` / `add` / `addt`.
 *
 * **`add` / streaming:** `awaiting_status = msg::kAwaitingNone` — сессия закрывается на `txIdle`,
 * очередь не блокируется ожиданием panel-status. Рекомендуемый **`bkcmd=OnFailure`**: панель шлёт
 * fail-байты (0x12 channel, 0x24 overflow) без Success ACK на каждый сэмпл. **`bkcmd=Always`**
 * даёт шум Success на потоке. При `kAwaitingNone` fail-status uncorrelated → `onError(0,0)` (маршрут
 * last-tx — NEX-R106f). См. example5 live probe.
 */
template<uint8_t ChannelCount>
struct WaveformChannels {
    static_assert(ChannelCount >= 1u && ChannelCount <= 4u,
        "nex::resources::WaveformChannels: ChannelCount must be 1..4");

    class Channel {
    public:
        void setColor(nex::Color v) noexcept
        {
            attr_detail::assignNumeric(_owner, waveform_detail::pcoId(_index), v);
        }

        /** Один сэмпл канала; NoAwaiting — для high-frequency streaming. */
        void add(uint8_t value) noexcept
        {
            _owner.page.app.enqueue(Transaction{
                cmd::WaveForm::add(_owner.id(), _index, value),
                _owner.page.ID, _owner.id(), 0u, Transaction::Kind::Command, msg::kAwaitingNone});
        }

        /** @experimental NEX-R215 — преамбула без TX payload; не для prod. */
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
        attr_detail::assignNumeric(owner, attr::Id::Ch, ChannelCount);
    }

    [[nodiscard]] Channel operator[](uint8_t index) noexcept
    {
        NEX_ASSERT(index < ChannelCount);
        return Channel{owner, index};
    }
};

/** Фон Waveform по `sta` (compile-time `S`); `Color` — с сеткой, остальные — как `Background`. */
template<BG S>
struct WfBackground;

template<>
struct WfBackground<BG::Color> {
    static constexpr BG kStyle = BG::Color;
    Component& owner;

    explicit WfBackground(Component& ownerIn) noexcept
        : owner{ownerIn}
    {}

    void setColor(nex::Color v) noexcept
    {
        attr_detail::assignNumeric(owner, bg_detail::colorId<0>(), v);
    }

    void setGridColor(uint16_t v) noexcept
    {
        attr_detail::assignNumeric(owner, attr::Id::Gdc, v);
    }

    void setGridWidth(uint16_t v) noexcept
    {
        attr_detail::assignNumeric(owner, attr::Id::Gdw, v);
    }

    void setGridHeight(uint16_t v) noexcept
    {
        attr_detail::assignNumeric(owner, attr::Id::Gdh, v);
    }
};

template<>
struct WfBackground<BG::Image> {
    static constexpr BG kStyle = BG::Image;
    Component& owner;

    explicit WfBackground(Component& ownerIn) noexcept
        : owner{ownerIn}
    {}

    void setImage(PicId v) noexcept
    {
        attr_detail::assignNumeric(owner, bg_detail::imageId<0>(), v);
    }
};

template<>
struct WfBackground<BG::CropImage> {
    static constexpr BG kStyle = BG::CropImage;
    Component& owner;

    explicit WfBackground(Component& ownerIn) noexcept
        : owner{ownerIn}
    {}

    void setCrop(PicId v) noexcept
    {
        attr_detail::assignNumeric(owner, bg_detail::cropId<0>(), v);
    }
};

template<>
struct WfBackground<BG::Transparent> {
    static constexpr BG kStyle = BG::Transparent;

    explicit WfBackground(Component&) noexcept {}
};

} // namespace nex::resources
