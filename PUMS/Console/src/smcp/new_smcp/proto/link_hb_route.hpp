/**
 * @file new_smcp/proto/link_hb_route.hpp
 * @brief Первый Request от неизвестного peer: свободная сессия или Heartbeat Busy.
 *
 * Член smcp::SessionBank, в списке узла сразу после банка.
 * Банк забирает кадр открытой сессии и возвращает false, если сессии нет —
 * тогда узел доходит сюда. Повторный find не нужен.
 * Слот берём сами и зовём open(src, false): это ведомый.
 * Кадр тут же уходит в onPacket — LinkHb отвечает Ack.
 */

#pragma once

#include "smcp/new_smcp/session.hpp"

namespace smcp::proto {

//Протокол, который отвечает за маршрутизацию Heartbeat Request
//Должен быть зарегистрирован в SessionBank после всех сессий
//Отвечает за поиск свободной сессии для открытия нового сеанса
class LinkHbRoute : public comm::Proto<Frame> {
public:
    explicit LinkHbRoute(SessionSlots& bank) noexcept
        : Proto(bank.parent())
        , _bank(bank)
    {}

    [[nodiscard]] bool onPacket(const Frame& pkt, MISC::time_t now) noexcept override
    {
        SMCP_IF_MSG(Heartbeat) {
            if (body.msg == Heartbeat::Msg::Request && !pkt.dst.isBroadcast()) {
                Session* free = nullptr;
                _bank.forEach([&](Session& slot) -> bool {
                    if (!slot.isOpen()) {
                        free = &slot;
                        return true;
                    }
                    return false;
                });
                if (free != nullptr) {
                    free->CommSession::open(pkt.src);
                    return free->onPacket(pkt, now);
                }
                replyBusy(pkt.src);
                return true;
            }
        }
        return false;
    }

    void tick(MISC::time_t) noexcept override {}

    void abort() noexcept override
    {
        _busyDst = addr_t::kNone;
        _txReady = false;
    }

protected:
    [[nodiscard]] bool peekTx(Frame& out, MISC::time_t) noexcept override
    {
        if (_txReady) {
            out = {};
            out.dst = _busyDst;
            out.txn = 0;
            Heartbeat{Heartbeat::Msg::Busy}.pack(out);
            _txReady = false;
            return true;
        }
        return false;
    }

    void onTxDone(const Frame& pkt, bool ok) noexcept override
    {
        if (ok) {
            _busyDst = addr_t::kNone;
        } else {
            _txReady = true;
        }
    }

private:
    /** Слотов нет: в очередь один Heartbeat Busy этому peer. kNone — очереди нет. */
    void replyBusy(addr_t peer) noexcept { _busyDst = peer; _txReady = true; }

    SessionSlots& _bank;
    addr_t _busyDst{};
    bool _txReady = false;
};

} // namespace smcp::proto
