#pragma once

#include <cstddef>
#include <cstdint>

#include "../nexTransceiver.hpp"

namespace nex {

    class Command;
    class TransparentCommand;

    /** Фаза исходящей передачи transparent (NIS §1.16): преамбула отправлена → ожидание **0xFE** → данные → **0xFD**. */
    enum class SessionTransparentPhase : uint8_t {
        Idle = 0,
        /** Преамбула ушла, ждём `TransparentReadyToReceive` с панели. */
        AwaitingReady,
        /** Панель готова; можно вызывать `writeTransparentPayload`. */
        PumpingPayload,
        /** Все байты payload записаны в поток; ждём `TransparentBlockComplete`. */
        AwaitingBlockAck,
    };

    /**
     * Как трактовать `Message` из `Session::receive` (тот же `Message`, что и у `Transceiver`, без отдельных вариантов).
     */
    enum class SessionRoute : uint8_t {
        /** Обычная доставка: касание, страница, пр. */
        App = 0,
        /** Ответ на последний `get` — в `msg` лежит `msg::NumericResponse` или `msg::StringResponse`. */
        GetReply,
        /** Кадр статуса после команды при включённом `setReportCommandAcks(true)` — в `msg` только `msg::StatusResponse`. */
        CommandAck,
        /** **0xFE** / **0xFD** в цепочке transparent TX — в `msg` только `msg::SystemEvent` с соответствующим кодом. */
        Transparent,
    };

    /** Один принятый кадр и способ его маршрутизации уровнем сессии. */
    struct SessionIncoming {
        SessionRoute route = SessionRoute::App;
        Message msg;
    };

    /**
     * **Session** — уровень над `Transceiver`: ожидание ответа на `get`, разбор **исходящей** transparent-фазы
     * (преамбула → **0xFE** → `writeTransparentPayload` → **0xFD**), при `setReportCommandAcks(true)` — отдельная
     * метка маршрута для статусов после команд (согласовать с `bkcmd` на панели). Приём **rept** / сырого блока от панели
     * с байтами **0xFF** внутри данных по-прежнему требует обхода кадрового приёмника — здесь не реализовано.
     */
    class Session {
    public:
        explicit Session(Transceiver& xc) noexcept : _xc(xc) {}

        Transceiver& transceiver() noexcept { return _xc; }
        const Transceiver& transceiver() const noexcept { return _xc; }

        /** Согласовать с системным `bkcmd` на дисплее: если true, статус после команды помечается `SessionRoute::CommandAck`. */
        void setReportCommandAcks(bool on) noexcept { _reportCmdAck = on; }
        bool reportCommandAcks() const noexcept { return _reportCmdAck; }

        bool isTxIdle() const noexcept { return _xc.isTxIdle(); }
        SessionTransparentPhase transparentPhase() const noexcept { return _tp; }
        /** Сколько байт payload ещё нужно записать (`writeTransparentPayload`) в фазе `PumpingPayload`. */
        uint32_t transparentPayloadRemaining() const noexcept { return _transparentRemaining; }

        /**
         * Отправить команду без transparent-фазы. Не вызывать, пока активна transparent-передача (`transparentPhase() != Idle`).
         */
        bool pushCommand(const Command& cmd);

        /** Преамбула transparent-команды; далее — `receive` до **0xFE**, затем `writeTransparentPayload`, затем `receive` до **0xFD**. */
        bool pushTransparentPreamble(const TransparentCommand& cmd);

        /**
         * Дозаписать payload в UART (тот же поток, что и кадры). Имеет смысл при `transparentPhase() == PumpingPayload`.
         * @return число записанных байт (может быть меньше len при неполной записи драйвера).
         */
        size_t writeTransparentPayload(const uint8_t* data, size_t len) noexcept;

        /** Один принятый кадр: заполняет `out.route` и `out.msg` (без дополнительных `std::variant` поверх `Message`). */
        bool receive(SessionIncoming& out);

        /** Сброс ожиданий и фазы transparent (после ошибки канала или сброса дисплея). */
        void resetConversationState() noexcept;

    private:
        Transceiver& _xc;
        bool _pendingGet = false;
        bool _reportCmdAck = false;
        bool _awaitStatusAfterCmd = false;
        SessionTransparentPhase _tp = SessionTransparentPhase::Idle;
        uint32_t _transparentRemaining = 0;

        bool dispatchMessage(Message& raw, SessionIncoming& out) noexcept;
    };

} // namespace nex
