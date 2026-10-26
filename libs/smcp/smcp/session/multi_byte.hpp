/**
 * @file multi_byte.hpp
 * @brief Multi-byte тело txn: RX/TX DataBegin → Data* (конец по size + CRC).
 *
 * Движок тела: seq/CRC/таймер/TX peek-commit. Только DataBegin → Data*.
 * Короткий ответ Get (один Data) — TaggedOps, сюда не приходит.
 * Владелец xfer — @c MultiByteClient (arm сохраняет указатель; NeedBuf / итог RX).
 * Mode — Idle/Rx/Tx (до reset). Phase — private. Error — причина Fail.
 * Result — внутренний итог onBegin/onData / TX Done.
 */

#pragma once

#include <cstdint>

#include "ms_timer.hpp"
#include "smcp/message.hpp"

namespace smcp {

/**
 * IBuf — buffer interface (non-owning: data + size).
 * TX: @c size = длина; RX: @c size = ёмкость. Tag — снаружи (Get/Set).
 */
struct IBuf {
    uint8_t* data = nullptr;
    uint16_t size = 0;
};

class MultiByteClient;
class Session;
struct TxSlot;

class MultiByte {
public:
    explicit MultiByte(Session& session) noexcept;

    // --- типы ---

    /** Направление / занятость. Done/Fail не меняют Mode — до @c resetXfer. */
    enum class Mode : uint8_t {
        Idle = 0,
        Rx,
        Tx,
    };

    /** Причина @c Result::Fail (и detail в @c msg::Nack::Error::Body). */
    enum class Error : uint8_t {
        None = 0,
        NotArmed,
        TagMismatch,
        Seq,
        Overflow,
        Crc,
        Unexpected,
        Abort,
        Timeout, /**< RX: ожидание DataBegin и межфреймовый gap. TX не сторожит. */
    };

    /** Итог внутренних onBegin/onData. */
    enum class Result : uint8_t {
        Continue = 0, /**< Ждём следующий кадр. */
        NeedBuf,      /**< Begin без буфера: клиент → onMbBuffer + attachRx. */
        Done,         /**< Успех (_error == None). */
        Fail,         /**< Смотри @c error(). */
    };

    [[nodiscard]] static const char* cstr(Error err) noexcept;

    // --- состояние (query) ---

    [[nodiscard]] Mode mode() const noexcept { return _mode; }
    [[nodiscard]] Error error() const noexcept { return _error; }
    [[nodiscard]] uint8_t tag() const noexcept { return _tag; }
    [[nodiscard]] uint8_t txn() const noexcept { return _txn; }
    /** View: data + принятые байты (onSet / onReply). */
    [[nodiscard]] IBuf bufReceived() const noexcept
    {
        return {_buf.data, _xfer.offset};
    }
    /** View: data + ёмкость, отданная в arm/attach. */
    [[nodiscard]] IBuf bufReserved() const noexcept { return _buf; }
    [[nodiscard]] bool isBusy() const noexcept { return _mode != Mode::Idle; }
    [[nodiscard]] bool isRx() const noexcept { return _mode == Mode::Rx; }
    [[nodiscard]] bool isTx() const noexcept { return _mode == Mode::Tx; }
    /** Фаза Data: DataBegin уже прошёл, куски тела принадлежат MB. */
    [[nodiscard]] bool isData() const noexcept { return _phase == Phase::Data; }
    /** Терминал (Done или Fail), Mode ещё Rx/Tx до reset. */
    [[nodiscard]] bool isFinished() const noexcept { return _finished; }

    // --- arm ---

    /**
     * RX, phase Begin, watchdog до DataBegin.
     * Пустой @a buf — буфер спросим в NeedBuf. Иначе ёмкость уже есть и
     * DataBegin сразу переводит в Data.
     */
    [[nodiscard]] bool armRx(uint8_t tag, uint8_t txn_id, uint32_t now_ms,
                             MultiByteClient* client, const IBuf& buf = {}) noexcept;

    /**
     * NeedBuf: привязать буфер (@a buf.size ≥ @a need). Фазу не меняет.
     */
    [[nodiscard]] bool attachRx(const IBuf& buf, uint16_t need) noexcept;

    /** TX (@a txn_id ≠ 0, @a buf.size ≥ 1). Phase Begin: следующий кадр — DataBegin. */
    [[nodiscard]] bool armTx(uint8_t tag, const IBuf& buf, uint8_t txn_id,
                             MultiByteClient* client) noexcept;

    // --- TX stream (Session::send из tick) ---

    [[nodiscard]] bool peekTx(Message& out) const noexcept;
    /**
     * TX-ok кадра тела: сдвиг phase/offset. Чужой / не pend / не Data* → false.
     * true → смотри @c isFinished / @c error.
     */
    [[nodiscard]] bool onTxResult(const TxSlot& slot) noexcept;

    // --- lifecycle ---

    /** Сброс xfer (Idle, txn=0, client=nullptr). */
    void resetXfer() noexcept;
    void abort() noexcept;

    // --- pump / RX feed ---

    /**
     * RX-watchdog → onMbResult(Timeout).
     * TX: peek → Session::send (один кадр в полёте).
     */
    void tick(uint32_t now_ms) noexcept;

    /**
     * RX DataBegin/Data своего txn. Чужой кадр, Data до DataBegin, не Rx, finished → false.
     * NeedBuf → client.onMbBuffer + attach → Data.
     * Done/Fail → client.onMbResult.
     */
    [[nodiscard]] bool onPacket(const Packet& pkt, uint32_t now_ms) noexcept;

private:
    /** Заголовок ещё не прошёл (Begin) или идут куски (Data). */
    enum class Phase : uint8_t {
        Begin = 0, /**< RX ждёт DataBegin. TX шлёт DataBegin. */
        Data,      /**< Куски тела. */
    };

    [[nodiscard]] Result onBegin(const msg::DataBegin& begin, uint32_t now_ms) noexcept;
    [[nodiscard]] Result onData(const msg::Data& chunk, uint32_t now_ms) noexcept;
    /** Буфер уже есть: принять заголовок и перейти в Data. */
    [[nodiscard]] Result enterData(const msg::DataBegin& begin, uint32_t now_ms) noexcept;
    [[nodiscard]] bool arm(uint8_t tag, const IBuf& buf, bool tx, uint8_t txn_id,
                           uint32_t now_ms, MultiByteClient* client) noexcept;
    void notifyResult() noexcept;
    void fail(Error err) noexcept;
    void succeed() noexcept;
    [[nodiscard]] Result finishRxCrc() noexcept;
    [[nodiscard]] Result finishedResult() const noexcept;
    [[nodiscard]] uint8_t txChunkLen() const noexcept;

    Session& _s;
    Mode _mode = Mode::Idle;
    Phase _phase = Phase::Begin;
    Error _error = Error::None;
    bool _finished = false;
    bool _tx_pend = false; /**< кадр тела в Session::_tx_ctrl */
    uint8_t _tag = 0;
    uint8_t _txn = 0;
    MultiByteClient* _client = nullptr;
    MISC::MsTimer _watch{};
    IBuf _buf{};

    struct {
        uint16_t size = 0;
        uint32_t crc = 0;
        uint8_t seq = 0;
        uint16_t offset = 0;
    } _xfer{};
};

/**
 * Владелец MultiByte xfer (TaggedOps, позже — Notify и т.п.).
 * Указатель задаётся в armTx/armRx; жить должен дольше xfer.
 */
class MultiByteClient {
public:
    virtual ~MultiByteClient() = default;

    /** NeedBuf: заполнить @a buf (ёмкость ≥ @a size). false — отказ. */
    [[nodiscard]] virtual bool onMbBuffer(uint8_t tag, uint8_t txn_id, uint16_t size,
                                          IBuf& buf) noexcept = 0;

    /** RX Done/Fail (в т.ч. Timeout из tick). Клиент сам reset владельца. */
    virtual void onMbResult(uint8_t tag, const IBuf& buf, MultiByte::Error err) noexcept = 0;

    /** TX тело дописано (последний Data). Клиент сам reset / AwaitAck. */
    virtual void onMbTxDone() noexcept = 0;
};

} // namespace smcp
