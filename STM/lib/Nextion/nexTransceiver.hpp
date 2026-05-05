#pragma once
#include <cstdint>
#include "core/nexProtocol.hpp"
#include "core/nexMessages.hpp"
#include "core/nexCommands.hpp"
#include "ibyte_stream.hpp"

namespace nex {
    /**
     * **Rx** **Framer** — потоковый разбор UART: накопление байт до полного кадра (заголовок Nextion + полезная нагрузка + `0xFF×3`).
     */
    class RxFramer {
        public:
            enum class State : uint8_t { WaitHeader, Collect, WaitTerm };

            /** Накопление кадра; при `appendByte(...) == true` — полный кадр, читать до следующего `appendByte`. */
            RxFrame frame{};

            void reset();
            /** Один байт с линии; `true` — в `frame` готовый кадр (до следующего вызова). */
            bool appendByte(uint8_t byte);

        private:
            State _state = State::WaitHeader;
            uint8_t _terms = 0;
        };

        /**
         * **Tx** **Framer** — неблокирующая отправка `TxFrame` в `IByteStream`: полезная нагрузка, затем `0xFF×3`.
         * В `Idle` заполняют `frame`; `tick` при `Idle && frame.length > 0` переходит в `Payload`.
         * `isIdle()` — `Idle` и пустой буфер (`length == 0`), чтобы не перезаписать кадр до конца передачи.
         */
        class TxFramer {
        public:
            TxFrame frame{};

            void reset() noexcept;
            /** `Idle` и `frame.length == 0` — можно снова сериализовать команду. */
            bool isIdle() const noexcept;
            /**
             * В `Idle` дописывает `0xFF×3` в хвост `frame` и шлёт буфер целиком (`Payload` до `reset`).
             * @return false — ошибка `getStatus` при `write==0` (`reset` и `purgeOutput`); иначе true
             * (нечего слать, или `write==0` при OK — отложить до следующего вызова).
             */
            bool tick(BIF::IByteStream& stream) noexcept;

        private:
            enum class State : uint8_t {Idle, Payload};

            uint16_t _pos = 0;
            State _state = State::Idle;
        };
    
    
        /**
         * **Transceiver** — приёмник+передатчик: UART с протоколом Nextion (`pushCommand` / `transmit` / `receive`).
         * Объединяет `RxFramer` и `TxFramer`; верхний уровень может вызывать периодический `update`.
         */
        class Transceiver {
        public:
            explicit Transceiver(BIF::IByteStream& s);

            /** Можно вызвать `pushCommand`: нет незавершённой передачи кадра на UART. */
            bool isTxIdle() const noexcept { return _txFramer.isIdle(); }

            /**
             * Отправить команду (сериализация в `TxFramer::frame`, затем первый `tick`).
             * @return false — занято, ошибка `serialize`, пустой кадр, или ошибка потока при первой записи.
             */
            bool pushCommand(const Command& cmd);

            /**
             * Отправить **преамбулу** команды с фазой transparent (NIS §1.16): первая строка + `0xFF×3`.
             * Далее вызывающий обязан передать `cmd.transparentPayloadBytes()` сырых байт по UART (тот же поток).
             * По сути вызывает `pushCommand`; тип `TransparentCommand` фиксирует контракт на этапе компиляции.
             */
            bool pushTransparentPreamble(const TransparentCommand& cmd);

            /** Один тик дозаписи кадра на линию (`TxFramer::tick`). @return false — ошибка `getStatus` у потока. */
            bool transmit() noexcept;

            /** Приём: доступные байты; при полном кадре — `TranslateMessage` в `out`. */
            bool receive(Message& out);

            /**
             * Запись **сырых** байт в тот же поток, что и кадры (NIS §1.16 — фаза данных после `pushTransparentPreamble`).
             * Не добавляет `0xFF×3`; вызывать только когда `Session` / вызывающий в фазе передачи payload.
             */
            size_t writeTransparentRaw(const uint8_t* data, size_t len) noexcept;

        private:
            BIF::IByteStream& _stream;
            RxFramer _rxFramer;
            TxFramer _txFramer;
        };
}