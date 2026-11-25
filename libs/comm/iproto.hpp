/**
 * @file comm/iproto.hpp
 * @brief comm::IProto — абстрактный протокол (узел списка детей контейнера).
 *
 * Арбитр опрашивает исходящие и доставляет события. Один тип @a Frame на оба
 * направления (в SMCP — Packet). Память исходящего кадра — у нижнего слоя
 * (слот/mailbox); протокол только заполняет переданный Frame.
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
 * Лист: peekTx(Frame&) через ProtoBase; onTxDone(frame, ok) переопределяет сам.
 * Саморегистрация — comm::Proto (container.hpp) → attach в _list.
 */

#pragma once

#include <cstdint>

#include "mem/list.hpp"
#include "ms_timer.hpp"

namespace comm {

template <typename Payload>
struct IFrame {
    Payload frame;
    IFrame * next = nullptr;
};

template <typename Payload, typename Params>
struct InterfaceFrame {
    IFrame<Payload> frame;
    Params params;
};

template <typename BaseFrame>
class IProto {
public:
    using frame_type = BaseFrame;

    /**
     * Заполнить @a out следующим исходящим и записать владельца в @a owner.
     * @return false — нечего слать (@a out и @a owner не менять).
     */
    

    virtual void onTxEvent(const BaseFrame& frame, bool ok) noexcept
    {
        (void)frame;
        (void)ok;
    }

    virtual void onReceive(const BaseFrame& frame) noexcept = 0;

    virtual void abort() noexcept = 0;

protected:
    IProto() = default;
    ~IProto() = default;
    IProto(const IProto&) = delete;
    IProto& operator=(const IProto&) = delete;
};


} // namespace comm
