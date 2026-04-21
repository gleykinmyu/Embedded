#pragma once
#include <cstdint>
#include <cstring>
#include "nexProtocol.hpp"

namespace nex {

    /**
     * Базовая исходящая инструкция Nextion (полезная нагрузка кадра до 0xFF×3).
     * Наследники реализуют `serialize`; `Transceiver::send` пишет в `TxFramer::frame` и первый `transmit`.
     */
    class Command {
    public:
        virtual ~Command() = default;

        /** Записать инструкцию в `tx.payload`; при успехе `tx.length` — число байт полезной нагрузки (терминаторы не входят). */
        virtual bool serialize(TxFrame& tx) const noexcept = 0;

    protected:
        /** Десятичная ASCII в хвост `tx` (с `tx.length`), без ведущих нулей; не выходит за `TxFrame::MAX_PAYLOAD`. */
        static bool appendUint32(TxFrame& tx, uint32_t value) noexcept;
        static bool appendInt32(TxFrame& tx, int32_t value) noexcept;
    };

namespace cmd {

    /**
     * Сырая полезная нагрузка кадра (уже в формате байт Nextion без терминаторов 0xFF×3).
     * Аргументы конструктора: `text` — указатель на байты; `length` — сколько байт записать (без завершающего `\\0`).
     */
    class RawBytes final : public Command {
        const char* _text;
        uint16_t _length;

    public:
        RawBytes(const char* text, uint16_t length) noexcept : _text(text), _length(length) {}
        bool serialize(TxFrame& tx) const noexcept override;
    };

    /**
     * Строка как NTBS: в кадр уходит `strlen(z)` байт без нуля.
     * Аргумент конструктора: `z` — указатель на строку с нулём в конце; строка должна жить до `serialize`.
     */
    class CString final : public Command {
        const char* _z;

    public:
        explicit CString(const char* z) noexcept : _z(z) {}
        bool serialize(TxFrame& tx) const noexcept override;
    };

    /**
     * Присвоение целочисленного свойству: в UART уходит `objName.PropName=<десятичный ASCII целого>` (например `n0.val=42`).
     * Шаблон: `PropName` — указатель на NTBS имени атрибута (без точки), время жизни — до конца программы (`nex::prop::Prop::…` и т.п.).
     * Конструктор: `objName` — NTBS имени объекта Nextion; `value` — `int32_t` (в кадр до 11 символов со знаком — как у типичного целого в NIS).
     *
     * NIS: целые в протоколе без float; фактический допустимый знак и диапазон задаёт компонент и min/max в редакторе — не каждый атрибут принимает отрицательные значения и не везде есть смысл в величине > 65535 (см. комментарии у `nex::prop::Prop`).
     * Транспортно значение ограничено `int32_t`; для беззнаковых и узких диапазонов передавайте только допустимые числа.
     * Hex в строке `obj.attr=0x…` в NIS допустим, эта команда шлёт только десятичный формат.
     */
    template<const char* PropName>
    class AssinInt32 final : public Command {
        const char* _objName;
        int32_t _value;

    public:
        explicit AssinInt32(const char* objName, int32_t value) noexcept : _objName(objName), _value(value) {}

        bool serialize(TxFrame& tx) const noexcept override {
            if (_objName == nullptr || PropName == nullptr)
                return false;
            const size_t objLen = std::strlen(_objName);
            const size_t propLen = std::strlen(PropName);
            if (objLen == 0u || propLen == 0u)
                return false;
            if (objLen > static_cast<size_t>(UINT16_MAX) || propLen > static_cast<size_t>(UINT16_MAX))
                return false;
            const size_t left = objLen + 1u + propLen;
            if (left > static_cast<size_t>(UINT16_MAX))
                return false;
            const uint16_t prefix = static_cast<uint16_t>(left);
            // '=' + десятичное int32 (до 11 символов с минусом)
            if (static_cast<uint32_t>(prefix) + 1u + 11u > TxFrame::MAX_PAYLOAD)
                return false;
            if (!tx.pushBytes(_objName, static_cast<uint16_t>(objLen)))
                return false;
            const uint8_t dot = static_cast<uint8_t>('.');
            if (!tx.pushBytes(&dot, 1u))
                return false;
            if (!tx.pushBytes(PropName, static_cast<uint16_t>(propLen)))
                return false;
            const uint8_t eq = static_cast<uint8_t>('=');
            return tx.pushBytes(&eq, 1u) && appendInt32(tx, _value);
        }
    };
} // namespace cmd

} // namespace nex
