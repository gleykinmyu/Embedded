#pragma once

#include <cstdint>
#include "comp/nexComponentBase.hpp"
#include "nexGateway.hpp"
#include "core/nexFifo.hpp"

namespace nex {

/**
 * Центр управления объектами HMI: поток байтов → `Gateway`, страницы и их компоненты.
 * Регистрируйте каждую `Page` через `registerPage`; входящие сообщения обрабатываются в `update`.
 *
 * **Очередь команд:** `commands()` / `enqueue` — исходящий `CommandFifo`; продвижение в `update`.
 * Команды `Get` блокируют очередь до ответа **0x70** / **0x71** (или `StatusResponse`) либо таймаута.
 * `TransparentCommand` — до полной отправки сырого блока через `writeTransparent` (см. `nexFifo.hpp`).
 *
 * `processCommandQueue` (внутренний) — конечный автомат по `_sendState`: из **Idle** забирает голову FIFO и
 * шлёт кадр; из **SendingHead** после `isTxIdle` либо ждёт ответ **Get**, либо (для transparent) ждёт 0xFE,
 * затем разрешает фазу payload и после передачи ждёт 0xFD, либо делает `pop` обычной команды. Цикл `for(;;)`
 * нужен, чтобы за один вызов обработать сразу несколько коротких команд подряд, пока UART успевает.
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
 *   ui.enqueue(nex::cmd::assign::Text{t0_txt, "Hello"});
 *   ui.enqueue(nex::cmd::Get{t0_txt});  // следующая команда пойдёт после ответа get
 * }
 *
 * void ui_poll_ms(uint32_t now_ms) {
 *   (void)ui.update(now_ms);  // один вызов в цикле: TX + очередь + обработка всех RX-кадров
 *   // NIS §1.16: если в очереди transparent-команда (WaveForm::addT, twfile, …), после преамбулы
 *   // дописывайте сырые байты; `writeTransparent` вернёт false, когда фаза не активна или UART занят.
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

    /**
     * Добавить команду в `CommandFifo` для последующей отправки в `update()`.
     * @param page_id идентификатор страницы-отправителя.
     * @param component_id идентификатор компонента-отправителя.
     * @param token пользовательский коррелятор запроса (возвращается в `msg::CommandResultEvent`).
     * @return false — очередь переполнена или команда не помещается в слот FIFO.
     */
    template <typename T>
    bool enqueue(const T& cmd, uint8_t page_id = 0u, uint8_t component_id = 0u, uint16_t token = 0u) noexcept {
        return _cmdFifo.tryPush(cmd, CommandMeta{page_id, component_id, token});
    }

    /**
     * Фаза transparent после преамбулы: сырые байты в тот же UART (NIS §1.16).
     * @return true, если весь переданный `len` принят в передачу; false, если не фаза payload, буфер занят
     *         или `len` превышает оставшийся объём transparent-фазы.
     */
     bool writeTransparent(const uint8_t* data, size_t len) noexcept;

    /**
     * Таймаут ожидания ответа на `get` (мс). `0` — ждать ответ бесконечно (пока не придёт кадр).
     * Учитывается только при вызовах с ненулевым `nowMs`.
     */
    void setGetReplyTimeoutMs(uint32_t ms) noexcept { _getTimeoutMs = ms; }
    [[nodiscard]] uint32_t getReplyTimeoutMs() const noexcept { return _getTimeoutMs; }

    /** Очередь не берёт следующую команду: ждём ответ/событие завершения текущей операции. */
    [[nodiscard]] bool isCommandQueueBlocked() const noexcept {
        return _sendState == SendState::AwaitingGetReply
            || _sendState == SendState::AwaitingTransparentReady
            || _sendState == SendState::TransparentPayload
            || _sendState == SendState::AwaitingTransparentComplete;
    }

    /**
     * Единый шаг цикла: дозапись TX, продвижение очереди и обработка всех доступных RX-кадров.
     * @return true, если за вызов был обработан хотя бы один входящий кадр.
     */
    bool update(uint32_t nowMs) noexcept;

    /**
     * Привязка страницы к индексу `page.ID` (как на панели Nextion).
     * @return false — слот уже занят другим экземпляром `Page`.
     */
    bool registerPage(Page& page) noexcept;

    [[nodiscard]] Page* page(uint8_t id) noexcept { return _pages[id]; }
    [[nodiscard]] const Page* page(uint8_t id) const noexcept { return _pages[id]; }

    /** Последний индекс из кадра 0x66; `0xFF`, пока события смены страницы не было. */
    uint8_t currentPageId() const noexcept { return _currentPageId; }



private:
    /**
     * Состояние исходящей очереди относительно текущей команды в голове FIFO.
     * Следующая команда не стартует, пока не выйдем из состояний ожидания ответа/события.
     */
    enum class SendState : uint8_t {
        Idle,                        ///< Нет активной «длинной» операции; можно брать голову очереди.
        SendingHead,                 ///< Кадр ушёл в `TxFramer`; ждём `isTxIdle`, затем решаем, как завершать команду.
        AwaitingGetReply,            ///< После `get`: ждём 0x70/0x71 или status; объект в голове FIFO ещё жив.
        AwaitingTransparentReady,    ///< После preamble transparent: ждём SystemEvent 0xFE.
        TransparentPayload,          ///< Разрешена 2-я фаза transparent — хост дописывает raw через `writeTransparent`.
        AwaitingTransparentComplete, ///< Все raw-байты отправлены; ждём SystemEvent 0xFD.
    };

    /** Один проход по состояниям: таймаут get, завершение TX, старт следующей команды. */
    void processCommandQueue(uint32_t nowMs) noexcept;

    /** Если команда в состоянии ожидания — проверить, не завершает ли пришедший кадр ожидание. */
    void onReceiveWhileAwaitingCommand(const Message& m) noexcept;
    /** Разбор `Message`: touch 0x65 → `Page::dispatchTouch`, page 0x66 → `currentPageId`. */
    void dispatchMessage(const Message& m) noexcept;
    /** Завершить текущую команду: отправить `CommandResultEvent`, удалить голову FIFO и перейти в Idle. */
    void completeCurrentCommand(msg::CommandResultEvent::Outcome outcome, uint8_t code = 0u) noexcept;

    Gateway _gateway;
    CommandQueue _cmdFifo{};       ///< Исходящие команды (placement new в слотах FIFO).
    Page* _pages[256]{};           ///< Индекс страницы Nextion → зарегистрированный `Page*`.
    uint8_t _currentPageId = 0xFF; ///< Последняя известная страница (0x66) или 0xFF до первого события.

    SendState _sendState = SendState::Idle;
    uint32_t _getTimeoutMs = 500u;  ///< 0 — без таймаута (см. `setGetReplyTimeoutMs`).
    uint32_t _getDeadlineMs = 0u;   ///< Абсолютный срок ожидания текущего ответа/события.
    uint32_t _transparentSent = 0u; ///< Уже отправлено байт второй фазы для текущей transparent-команды.
};

} // namespace nex
