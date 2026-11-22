/**
 * @file comm/old/iproto.hpp
 * @brief comm::IProto — абстрактный протокол (узел списка детей контейнера).
 *
 * Арбитр опрашивает исходящие и доставляет события. Один тип @a Packet на оба
 * направления. Память исходящего пакета — у нижнего слоя
 * (слот/mailbox); протокол только заполняет переданный Packet.
 *
 * Стек имён:
 *   peekTx → onTxDone   заполнить слот / исход отправки
 *   onPacket            входящий (true — съел)
 *   tick / abort / ok   жизнь
 *
 * Контракт peekTx(owner, out, now):
 *   - true → @a out заполнен, @a owner — конечный протокол (лист: this;
 *     контейнер пробрасывает owner ребёнка), кадр «в полёте» до onTxDone;
 *   - пока onTxDone не вызван, повторный peekTx у того же протокола → false
 *     (один outstanding на протокол);
 *   - false → @a out и @a owner не трогать.
 *
 * Контракт onTxDone:
 *   - слот хранит owner из peekTx и зовёт owner->onTxDone(frame, ok);
 *   - who не нужен; возврат void.
 *
 * Контейнер держит детей в одном списке и не маршрутизирует onTxDone.
 * Лист: peekTx(Packet&) через ProtoBase; onTxDone(frame, ok) переопределяет сам.
 * Саморегистрация — comm::Proto (container.hpp) → attach в _list.
 */

#pragma once

#include <cstdint>

#include "mem/list.hpp"
#include "ms_timer.hpp"

namespace comm {

template <typename Packet>
class IProto : public mem::ListNode {
public:
    using packet_type = Packet;

    /**
     * Заполнить @a out следующим исходящим и записать владельца в @a owner.
     * @return false — нечего слать (@a out и @a owner не менять).
     */
    [[nodiscard]] virtual bool peekTx(IProto*& owner, Packet& out,
                                      MISC::time_t now) noexcept = 0;
    /** Исход отправки (слот зовёт владельца из peekTx). */
    virtual void onTxDone(const Packet& frame, bool ok) noexcept
    {
        (void)frame;
        (void)ok;
    }
    /** Входящий кадр: true — съел (дальше не предлагать). */
    [[nodiscard]] virtual bool onPacket(const Packet& frame, MISC::time_t now) noexcept = 0;
    /** Таймауты / повторы / доставка итогов. */
    virtual void tick(MISC::time_t now) noexcept = 0;
    /** Сброс работы (закрытие сессии / узла). */
    virtual void abort() noexcept = 0;
    /** Дети / инварианты в порядке (рекурсивно у контейнеров). */
    [[nodiscard]] virtual bool ok() const noexcept { return true; }

protected:
    IProto() = default;
    ~IProto() = default;
    IProto(const IProto&) = delete;
    IProto& operator=(const IProto&) = delete;
};

/**
 * Лист: владелец TX — всегда this.
 *
 * peekTx(owner, …) закрыт (private final). Наследник пишет peekTx(Packet&, now)
 * и при необходимости onTxDone(frame, ok).
 */
template <typename Packet>
class ProtoBase : public IProto<Packet> {
protected:
    ProtoBase() = default;
    ~ProtoBase() = default;

    /** Заполнить кадр. true — TX взят (обёртка запишет owner = this). */
    [[nodiscard]] virtual bool peekTx(Packet& out, MISC::time_t now) noexcept = 0;

private:
    [[nodiscard]] bool peekTx(IProto<Packet>*& owner, Packet& out,
                              MISC::time_t now) noexcept final
    {
        if (!peekTx(out, now)) {
            return false;
        }
        owner = this;
        return true;
    }
};

} // namespace comm
