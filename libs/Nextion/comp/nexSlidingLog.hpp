#pragma once

/**
 * `SlidingText` + follow-tail лог (накачка `txt`, липкий хвост).
 *
 * HMI (Nextion Editor, у этого SLText):
 *   обязательно включить Touch Press Event и Touch Release Event
 *   («Send Component ID» / автоматическая посылка 0x65 на MCU).
 *   Follow смотрит только `Component::onTouch`; sendxy 0x67 не используется.
 */

#include <cstddef>
#include <cstdint>

#include "../app/nexApplication.hpp"
#include "nexComponents.hpp"
#include "../../Interfaces/ringbuffer.hpp"

namespace nex {
namespace comp {

template<BG S = BG::Color, std::size_t RingSize = 1024u>
class SlidingLog : public SlidingText<S, 0u> {
public:
    static constexpr Coord kFollowSlopPx = 48;
    static constexpr uint32_t kScrollSampleDelayMs = 120u;
    /** Pin хвоста: signed `val_y`, не 0xFFFF. */
    static constexpr Coord kFollowPinY = 0x7FFF;
    /** `txt_maxl` виджета на панели (HMI), не размер кольца MCU. */
    static constexpr std::size_t kPanelTxtMaxL = 1024u;
    static constexpr std::size_t kChunkCap = 48u;

    SlidingLog(IPage& owner, const Literal& name, uint8_t id = 0)
        : SlidingText<S, 0u>(owner, name, id)
    {}

    /** В кольцо (можно из UART callback). `\0` отбрасывается. */
    void write(const char* data, std::size_t len) noexcept
    {
        if (_pumping || data == nullptr || len == 0u)
            return;
        for (std::size_t i = 0u; i < len; ++i) {
            const char c = data[i];
            if (c == '\0')
                continue;
            (void)_ring.push(c);
        }
    }

    /** Слить кольцо на панель и сэмпл follow после Release. Звать из `Application::update`. */
    void tick() noexcept
    {
        if (!_pumping) {
            _pumping = true;
            flush();
            _pumping = false;
        }
        pumpScrollSample();
    }

    void onTouch(const msg::evTouch& e) override
    {
        if (e.state == TouchState::Press) {
            _followTail = false;
            _scrollSamplePending = false;
            return;
        }
        if (e.state == TouchState::Release) {
            _followTail = false;
            _scrollSamplePending = true;
            _scrollSampleAtMs = this->page.app.nowMs() + kScrollSampleDelayMs;
        }
    }

    void onResponse(const msg::getNumeric& response, uint8_t tag) override
    {
        SlidingText<S, 0u>::onResponse(response, tag);
        if (tag != static_cast<uint8_t>(attr::Id::MaxvalY))
            return;
        const Coord y = this->val_y;
        const Coord m = this->maxval_y;
        if (y < 0)
            _followTail = false;
        else
            _followTail = (m <= y) || ((m - y) <= kFollowSlopPx);
    }

private:
    void pumpScrollSample() noexcept
    {
        if (!_scrollSamplePending)
            return;
        const int32_t dt = static_cast<int32_t>(this->page.app.nowMs() - _scrollSampleAtMs);
        if (dt < 0)
            return;
        _scrollSamplePending = false;
        this->val_y.get();
        this->maxval_y.get();
    }

    /** Указатели в кольцо живы до `pumpUntilIdle`; `write` в это время отсекается `_pumping`. */
    void flush() noexcept
    {
        typename MISC::RingBuffer<char, RingSize>::Linear parts[2]{};
        const uint8_t nparts = _ring.peekLinear(parts);
        std::size_t toDrop = 0u;
        bool sent = false;
        for (uint8_t i = 0u; i < nparts; ++i) {
            char* p = parts[i].data;
            std::size_t left = parts[i].n;
            for (std::size_t k = 0u; k < left; ++k) {
                if (p[k] == '\n')
                    p[k] = '\r';
            }
            while (left > 0u) {
                const std::size_t n = (left < kChunkCap) ? left : kChunkCap;
                if ((_len + n) > kPanelTxtMaxL) {
                    this->setText("");
                    _len = 0u;
                    _followTail = true;
                }
                this->appendText(p, n);
                p += n;
                left -= n;
                toDrop += n;
                _len += n;
                sent = true;
            }
        }
        if (!sent)
            return;
        if (_followTail)
            this->val_y = kFollowPinY;
        (void)this->page.app.pumpUntilIdle();
        _ring.drop(toDrop);
    }

    MISC::RingBuffer<char, RingSize> _ring;
    std::size_t _len = 0u;
    bool _pumping = false;
    bool _followTail = true;
    bool _scrollSamplePending = false;
    uint32_t _scrollSampleAtMs = 0u;
};

} // namespace comp
} // namespace nex
