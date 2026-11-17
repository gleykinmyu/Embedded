/**
 * @file comm/ilink.hpp
 * @brief comm::ILink — транспорт кадра, без привязки к среде (CAN/TCP/…).
 *
 * Окно TX — у линка (mailbox / очередь среды). Один pollTx — один завершённый слот.
 * cookie необязателен: send(frame) ≡ send(frame, nullptr); pollTx возвращает тот же указатель.
 */

#pragma once

#include <cstddef>
#include <cstdint>

namespace comm {

template <typename Frame>
class ILink {
public:
    enum class State : uint8_t {
        Closed = 0,
        Open,
        Fault, ///< среда (BusOff и т.п.); sticky до clearFault()
    };

    enum class TxResult : uint8_t {
        Ok = 0,
        Failed,
    };

    virtual ~ILink() = default;

    [[nodiscard]] virtual State state() const noexcept = 0;
    [[nodiscard]] bool isOpen() const noexcept { return state() == State::Open; }

    /** Снять Fault → Open (если среда позволяет) или Closed. */
    virtual void clearFault() noexcept {}

    /** Свободных слотов TX. */
    [[nodiscard]] virtual std::size_t txSpace() const noexcept = 0;
    /** Слотов TX в полёте. */
    [[nodiscard]] virtual std::size_t txBusy() const noexcept = 0;

    /** send(frame, nullptr). */
    [[nodiscard]] bool send(const Frame& frame) noexcept { return send(frame, nullptr); }

    /**
     * Принять @a frame в слот среды вместе с @a cookie (может быть nullptr).
     * @return false — Closed / Fault / нет места.
     */
    [[nodiscard]] virtual bool send(const Frame& frame, void* cookie) noexcept = 0;

    /**
     * Один завершённый слот: кадр, cookie из send, исход.
     * Порядок — как отдаёт среда (не обязательно FIFO).
     * @return false — завершений нет.
     */
    [[nodiscard]] virtual bool pollTx(Frame& frame, void*& cookie,
                                      TxResult& result) noexcept = 0;

    /** Слоты в полёте завершатся через pollTx как Failed. */
    virtual void abortTx() noexcept = 0;

    /**
     * Один входящий кадр.
     * @return false — пусто / Closed / Fault.
     */
    [[nodiscard]] virtual bool receive(Frame& out) noexcept = 0;

protected:
    ILink() = default;
    ILink(const ILink&) = delete;
    ILink& operator=(const ILink&) = delete;
};

} // namespace comm
