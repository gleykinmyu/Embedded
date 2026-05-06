#pragma once

#include <cstdint>
#include "../comp/nexComponentBase.hpp"
#include "../nexGateway.hpp"
#include "nexFifo.hpp"

namespace nex {

/**
 * Центр управления объектами HMI: поток байтов → `Gateway`, страницы и их компоненты.
 * Регистрируйте каждую `Page` через `registerPage`; входящие сообщения — `dispatchMessage` или `poll`.
 *
 * **Очередь команд:** `commands()` / `tryEnqueue` — исходящий `CommandFifo`; продвижение в `tick` / `poll`.
 * Команды `Get` блокируют очередь до ответа **0x70** / **0x71** (или `StatusResponse`) либо таймаута.
 * `TransparentCommand` — до полной отправки сырого блока через `writeTransparent` (см. `nexFifo.hpp`).
 *
 * `processCommandQueue` (внутренний) — конечный автомат по `_sendState`: из **Idle** забирает голову FIFO и
 * шлёт кадр; из **SendingHead** после `isTxIdle` либо ждёт ответ **Get**, либо переходит в **TransparentPayload**,
 * либо делает `pop` обычной команды. Цикл `for(;;)` нужен, чтобы за один вызов обработать сразу несколько
 * коротких команд подряд, пока UART успевает.
 *
 * **Пример использования**
 * @code
 * #include "nextion.hpp"
 *
 * // IByteStream к UART Nextion
 * static YourStream s;
 * static nex::ObjManager ui(s);
 *
 * static const nex::Literal kT0{"t0"};
 * static const nex::Literal kTxt{"txt"};
 * static const nex::cmd::TargetAttr t0_txt{kT0, kTxt};
 *
 * void ui_init() {
 *   ui.setGetReplyTimeoutMs(400);  // 0 = без таймаута, только пока не придёт кадр
 *   ui.registerPage(myPage);
 *   ui.tryEnqueue(nex::cmd::assign::Text{t0_txt, "Hello"});
 *   ui.tryEnqueue(nex::cmd::oper::Get{t0_txt});  // следующий enqueue выполнится после ответа get
 * }
 *
 * void ui_poll_ms(uint32_t now_ms) {
 *   ui.tick(now_ms);  // дозапись TX + движение очереди (нужно всегда, даже без RX)
 *   nex::Message msg;
 *   while (ui.poll(msg, now_ms)) {
 *     std::visit([](auto&& e) { (void)e; }, msg);  // touch, get-ответ, смена страницы, …
 *   }
 *   // NIS §1.16: если в очереди transparent-команда (WaveAddt, twfile, …), после преамбулы
 *   // дописывайте сырые байты; `writeTransparent` вернёт 0, когда фаза не активна или UART занят.
 *   (void)ui.writeTransparent(wave_chunk, chunk_len);
 * }
 * @endcode
 */
class ObjManager {
public:
    /** Совпадает с явной инстанциацией в `nexFifo.cpp`. */
    using CommandQueue = CommandFifo<4u, 128u>;

    explicit ObjManager(BIF::IByteStream& stream) noexcept;

    Gateway& gateway() noexcept { return _gateway; }
    const Gateway& gateway() const noexcept { return _gateway; }

    CommandQueue& commands() noexcept { return _cmdFifo; }
    const CommandQueue& commands() const noexcept { return _cmdFifo; }

    template <typename T>
    bool tryEnqueue(const T& cmd) noexcept {
        return _cmdFifo.tryPush(cmd);
    }

    /**
     * Таймаут ожидания ответа на `get` (мс). `0` — ждать ответ бесконечно (пока не придёт кадр).
     * Учитывается только при вызовах с ненулевым `nowMs`.
     */
    void setGetReplyTimeoutMs(uint32_t ms) noexcept { _getTimeoutMs = ms; }
    [[nodiscard]] uint32_t getReplyTimeoutMs() const noexcept { return _getTimeoutMs; }

    /** Очередь не берёт следующую команду: ждём ответ `get` или передаём transparent-payload. */
    [[nodiscard]] bool isCommandQueueBlocked() const noexcept {
        return _sendState == SendState::AwaitingGetReply || _sendState == SendState::TransparentPayload;
    }

    /**
     * Периодический вызов: дозапись TX и продвижение очереди.
     * @param nowMs монотонное время (мс); для проверки таймаута `get`. При `0` таймаут не проверяется.
     */
    void tick(uint32_t nowMs) noexcept;

    /**
     * Привязка страницы к индексу `page.ID` (как на панели Nextion).
     * @return false — слот уже занят другим экземпляром `Page`.
     */
    bool registerPage(Page& page) noexcept;

    [[nodiscard]] Page* page(uint8_t id) noexcept { return _pages[id]; }
    [[nodiscard]] const Page* page(uint8_t id) const noexcept { return _pages[id]; }

    /** Последний индекс из кадра 0x66; `0xFF`, пока события смены страницы не было. */
    uint8_t currentPageId() const noexcept { return _currentPageId; }

    /** Разбор `Message`: touch 0x65 → `Page::dispatchTouch`, page 0x66 → `currentPageId`. */
    void dispatchMessage(const Message& m) noexcept;

    /**
     * Один шаг приёма из `Gateway::receive` и маршрутизация при успешном кадре.
     * @param nowMs см. `tick`; при `0` таймаут ответа `get` не обрабатывается.
     * @return false — полного кадра ещё нет (как у `Gateway::receive`).
     */
    bool poll(Message& m, uint32_t nowMs) noexcept;

    /** Удобная перегрузка: без учёта таймаута `get` (только если `setGetReplyTimeoutMs(0)` или не критично). */
    bool poll(Message& m) noexcept { return poll(m, 0u); }

    /**
     * Фаза transparent после преамбулы: сырые байты в тот же UART (NIS §1.16).
     * @return число записанных байт; `0`, если сейчас не фаза payload или буфер занят.
     */
    size_t writeTransparent(const uint8_t* data, size_t len) noexcept;

private:
    /**
     * Состояние исходящей очереди относительно текущей команды в голове FIFO.
     * Следующая команда не стартует, пока не выйдем из `AwaitingGetReply` / `TransparentPayload`.
     */
    enum class SendState : uint8_t {
        Idle,                 ///< Нет активной «длинной» операции; можно брать голову очереди.
        SendingHead,          ///< Кадр команды ушёл в `TxFramer`; ждём `isTxIdle`, затем решаем Get / transparent / pop.
        AwaitingGetReply,     ///< После `get`: ждём 0x70/0x71 или status; объект в голове FIFO ещё жив.
        TransparentPayload,   ///< После преамбулы transparent — хост дописывает сырые байты через `writeTransparent`.
    };

    /** Один проход по состояниям: таймаут get, завершение TX, старт следующей команды. */
    void processCommandQueue(uint32_t nowMs) noexcept;

    /** Если ждём ответ на get — проверить, не завершает ли пришедший кадр ожидание. */
    void onReceiveWhileAwaitingGet(const Message& m) noexcept;

    Gateway _gateway;
    CommandQueue _cmdFifo{};       ///< Исходящие команды (placement new в слотах FIFO).
    Page* _pages[256]{};           ///< Индекс страницы Nextion → зарегистрированный `Page*`.
    uint8_t _currentPageId = 0xFF; ///< Последняя известная страница (0x66) или 0xFF до первого события.

    SendState _sendState = SendState::Idle;
    uint32_t _getTimeoutMs = 500u;  ///< 0 — без таймаута (см. `setGetReplyTimeoutMs`).
    uint32_t _getDeadlineMs = 0u;   ///< Абсолютный срок (`nowMs` + timeout); при `nowMs==0` может быть UINT32_MAX.
    uint32_t _transparentSent = 0u; ///< Уже отправлено байт второй фазы для текущей transparent-команды.
};

} // namespace nex
