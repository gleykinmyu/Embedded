#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>
#include "nexProtocol.hpp"
#include "nexTypes.hpp"

/**
 * @file nexCommands.hpp
 *
 * **Список классов команд** (исходящие UART-инструкции NIS). Базовый тип: `nex::Command`.
 * Ожидаемый ответ UART и маршрутизация — в `Transaction`; обработка — в `Application` (`core/nexSession.hpp`).
 *
 * | Пространство имён   | Класс                  | База                   |
 * |---------------------|------------------------|------------------------|
 * | `nex`               | `Command`              | —                      |
 * | `nex::cmd::assign`  | `Text`                 | `Command`              |
 * | `nex::cmd::assign`  | `TextSubtract`         | `Command`              |
 * | `nex::cmd::assign`  | `Numeric`              | `Command`              |
 * | `nex::cmd`          | `System`               | `Command`              |
 * | `nex::cmd`          | `Page`                 | `Command`              |
 * | `nex::cmd`          | `Cfgpio`               | `Command`              |
 * | `nex::cmd`          | `Get`                  | `Command`              |
 * | `nex::cmd`          | `String`               | `Command`              |
 * | `nex::cmd`          | `Component`            | `Command`              |
     * | `nex::cmd`          | `FileStream`           | `Command`              |
     * | `nex::cmd`          | `Move`                 | `Command`              |
 * | `nex::cmd`          | `WaveForm`             | `Command`              |
 * | `nex::cmd`          | `Eeprom`               | `Command`              |
 * | `nex::cmd`          | `Play`                 | `Command`              |
 * | `nex::cmd`          | `File`                 | `Command`              |
 * | `nex::cmd`          | `Directory`            | `Command`              |
 * | `nex::cmd::gui`     | `ClearScreen`          | `Command`              |
 * | `nex::cmd::gui`     | `Picture`              | `Command`              |
 * | `nex::cmd::gui`     | `PictureCrop`          | `Command`              |
 * | `nex::cmd::gui`     | `TextInRegion`         | `Command`              |
 * | `nex::cmd::gui`     | `Rect`                 | `Command`              |
 * | `nex::cmd::gui`     | `Line`                 | `Command`              |
 * | `nex::cmd::gui`     | `Circle`               | `Command`              |
 */

namespace nex {

/**
 * Пустая лексема компонента для `AttrRef`: при `comp.len == 0` в кадр идёт только `attr`
 * (`sys0`, `p0.t0.val`, …).
 */
inline constexpr Literal kEmptyCompLexeme{""};

/**
 * Ссылка на атрибут NIS: `comp` (объект / путь без точки до атрибута) и `attr` (`txt`, `val`, …).
 * Если `comp.len == 0`, в кадр выводится только `attr` как вся левая часть.
 * Объекты `Literal` для `comp` и `attr` должны жить дольше, чем хранящий `AttrRef`.
 */
struct AttrRef {
    const Literal& comp;
    const Literal& attr;
};

    /**
     * Базовый класс **исходящей UART-инструкции** к дисплею Nextion (NIS — Nextion Instruction Set).
     * Полезная нагрузка до `TxFrame::MAX_PAYLOAD` байт; терминатор `0xFF×3` добавляет `TxFramer`.
     */
    class Command {
    public:
        enum class Status : uint8_t {
            OK = 0,
            TxFrameOverflow,
            NullPointer,
            EmptyLiteral,
            EmptyAttribute,
            InvalidColor,
            InvalidGeometry,
            InvalidFields,
            UnknownKind,
            SlotTooSmall,
            SlotAlignMismatch,
        };

        /** Операция числового присваивания NIS (§2.4–2.9): `=`, `+=`, … */
        enum class Op : uint8_t {
            Assign,
            Add,
            Sub,
            Mul,
            Div,
            Mod,
        };

        Command() noexcept = default;
        virtual ~Command() = default;

        /** Записать инструкцию в `tx.payload`; при успехе `tx.length` — число байт полезной нагрузки (терминаторы не входят).
         * При `false` смотрите `getStatus()`.
         */
        virtual bool serialize(TxFrame& tx) const noexcept = 0;

        [[nodiscard]] Status getStatus() const noexcept { return _status; }
        void clearErrors() const noexcept { _status = Status::OK; }

        /** Копия в слот очереди; при неудаче — `false`, смотрите `getStatus()`. */
        virtual bool emplaceIn(void* storage, std::size_t maxBytes, std::size_t maxAlign) const noexcept = 0;
        virtual void destroyIn(void* storage) const noexcept = 0;

    protected:
        mutable Status _status = Status::OK;

        /** `_status = s; return false` — удобно в `if` без `{}`. */
        [[nodiscard]] bool fail(Status s) const noexcept {
            _status = s;
            return false;
        }

        bool pushBytes(TxFrame& tx, const void* src, uint16_t n) const noexcept;
        bool printLiteral(TxFrame& tx, const Literal& lit) const noexcept;
        bool printNisToken(TxFrame& tx, const char* token) const noexcept;
        bool printSpace(TxFrame& tx) const noexcept;
        bool printDot(TxFrame& tx) const noexcept;
        bool printComma(TxFrame& tx) const noexcept;
        bool printQuotedString(TxFrame& tx, const char* text) const noexcept;
        bool printUint32(TxFrame& tx, uint32_t value) const noexcept;
        bool printInt32(TxFrame& tx, int32_t value) const noexcept;
        bool printOperation(TxFrame& tx, Op op) const noexcept;
        bool printAttrLexeme(TxFrame& tx, const AttrRef& t) const noexcept;
        bool printColorConst(TxFrame& tx, Color::std c) const noexcept;
    };

inline const char* cstr(Command::Status s) noexcept {
    switch (s) {
    case Command::Status::OK: return "OK";
    case Command::Status::NullPointer: return "NullPointer";
    case Command::Status::EmptyLiteral: return "EmptyLiteral";
    case Command::Status::EmptyAttribute: return "EmptyAttribute";
    case Command::Status::TxFrameOverflow: return "TxFrameOverflow";
    case Command::Status::InvalidColor: return "InvalidColor";
    case Command::Status::InvalidGeometry: return "InvalidGeometry";
    case Command::Status::InvalidFields: return "InvalidFields";
    case Command::Status::UnknownKind: return "UnknownKind";
    case Command::Status::SlotTooSmall: return "SlotTooSmall";
    case Command::Status::SlotAlignMismatch: return "SlotAlignMismatch";
    default: return "?";
    }
}

/** Реализация слота очереди в производном классе. */
#define NEX_COMMAND_SLOT(ClassName) \
    bool emplaceIn(void* storage, std::size_t maxBytes, std::size_t maxAlign) const noexcept override { \
        if (sizeof(ClassName) > maxBytes) \
            return fail(Status::SlotTooSmall); \
        if (alignof(ClassName) > maxAlign) \
            return fail(Status::SlotAlignMismatch); \
        new (storage) ClassName(static_cast<const ClassName&>(*this)); \
        return true; \
    } \
    void destroyIn(void* storage) const noexcept override { static_cast<ClassName*>(storage)->~ClassName(); }

namespace misc {
/** Отладка TX: печать полезной нагрузки `serialize` (`-DNEX_TRACE_TX` или `-DNEX_DEBUG`). */
void printTxPayloadLine(const char* label, const TxFrame& tx) noexcept;
}

namespace cmd {
/**
 * NIS §2 — присваивания по UART (`target=…`, без слова set).
 * Порядок в файле: сначала **текстовые** атрибуты (`.txt` и совместимые), затем **числовые**.
 */
namespace assign {

    // -------------------------------------------------------------------------
    // Текстовые присваивания (NIS §2.1–2.3: =, +=, -= для строковых атрибутов)
    // -------------------------------------------------------------------------

    /**
     * **Text** assign / append — строковое присваивание к атрибуту (NIS §2.1–2.2): `…="<text>"` или `…+="<text>"`.
     * Экранирование `\r`, `\"`, `\\` — §1.11.
     */
    class Text final : public Command {
    public:
        /** **Op**eration — `=` или `+=` для строковых атрибутов. */
        enum class Op : uint8_t {
            Assign,
            Append, /**< += */
        };

        Text(const AttrRef& target, const char* text, Op op) noexcept
            : _target(target)
            , _text(text)
            , _op(op) {}

        bool serialize(TxFrame& tx) const noexcept override;
        NEX_COMMAND_SLOT(Text)

    private:
        AttrRef _target;
        const char* _text;
        Op _op;
    };

    /**
     * **TextSubtract** — укоротить строковый атрибут на `n` символов с конца (NIS §2.3).
     */
    class TextSubtract final : public Command {
    public:
        TextSubtract(const AttrRef& target, uint32_t n) noexcept : _target(target), _n(n) {}
        bool serialize(TxFrame& tx) const noexcept override;
        NEX_COMMAND_SLOT(TextSubtract)

    private:
        AttrRef _target;
        uint32_t _n;
    };

    // -------------------------------------------------------------------------
    // Числовые присваивания (NIS §2.4–2.9)
    // -------------------------------------------------------------------------

    /**
     * **Numeric** assign / modify — числовое присваивание к атрибуту (NIS §2.4–2.9).
     */
    class Numeric final : public Command {
    public:
        Numeric(const AttrRef& target, int32_t value, Op op = Op::Assign) noexcept
            : _target(target)
            , _value(value)
            , _op(op) {}

        bool serialize(TxFrame& tx) const noexcept override;
        NEX_COMMAND_SLOT(Numeric)

    private:
        AttrRef _target;
        int32_t _value;
        Op _op;
    };

} // namespace assign

    //========== Системные команды =============================================

    /**
     * Системные инструкции NIS: `rest`, `touch_j`, …
     */
    class System final : public Command {
    public:
        enum class Kind : uint8_t {
            Restart,
            TouchJ,
        };

        static System restart() noexcept { return System(Kind::Restart); }
        static System touchJ() noexcept { return System(Kind::TouchJ); }

        bool serialize(TxFrame& tx) const noexcept override;
        NEX_COMMAND_SLOT(System)

    private:
        Kind _kind;

        explicit System(Kind kind) noexcept : _kind(kind) {}
    };

    /**
     * Навигация по страницам NIS: `page`, `ref`, `sendme`.
     */
    class Page final : public Command {
    public:
        enum class Kind : uint8_t {
            Switch,
            Refresh,
            SendMe,
        };

        /** Переключить страницу по числовому id (`page 0`, …). */
        static Page switchTo(uint32_t pageId) noexcept { return Page(Kind::Switch, pageId); }
        /** Перерисовать страницу 0 (`ref 0`). */
        static Page refresh() noexcept { return Page(Kind::Refresh, 0u); }
        /** Запросить id текущей страницы; ответ — `0x66` (`msg::evPage`). */
        static Page sendMe() noexcept { return Page(Kind::SendMe, 0u); }

        bool serialize(TxFrame& tx) const noexcept override;
        NEX_COMMAND_SLOT(Page)

    private:
        Kind _kind;
        uint32_t _pageId;

        Page(Kind kind, uint32_t pageId) noexcept : _kind(kind), _pageId(pageId) {}
    };

    /**
     * **Cfgpio** — привязка GPIO к компоненту (NIS `cfgpio`).
     */
    class Cfgpio final : public Command {
    public:
        Cfgpio(uint32_t pin, uint32_t mode, const Literal& bindCompNameOrZero) noexcept
            : _pin(pin)
            , _mode(mode)
            , _bindCompNameOrZero(bindCompNameOrZero) {}
        bool serialize(TxFrame& tx) const noexcept override;
        NEX_COMMAND_SLOT(Cfgpio)

    private:
        uint32_t _pin;
        uint32_t _mode;
        const Literal& _bindCompNameOrZero;
    };


    //========== Работа с атрибутами ============================================

    /**
     * **Get** — запросить значение атрибута/переменной; дисплей отправит ответ по UART (NIS `get`).
     * Тип ожидаемого ответа задаётся в `TransactionState` (`Application::request*AttributeGet`).
     */
    class Get final : public Command {
    public:
        static Get numeric(const AttrRef& operand) noexcept { return Get(operand); }
        static Get string(const AttrRef& operand) noexcept { return Get(operand); }

        bool serialize(TxFrame& tx) const noexcept override;
        NEX_COMMAND_SLOT(Get)

    private:
        AttrRef _operand;

        explicit Get(const AttrRef& operand) noexcept : _operand(operand) {}
    };

    /**
     * Строковые операции NIS: `covx`, `substr`, `strlen`/`btlen`, `spstr`.
     */
    class String final : public Command {
    public:
        enum class Kind : uint8_t {
            Covx,
            Substr,
            Strlen,
            Btlen,
            Spstr,
        };

        static String covx(const AttrRef& src, const AttrRef& dst, int32_t length, int32_t format) noexcept {
            return String(Kind::Covx, src, dst, length, format, nullptr);
        }

        static String substr(const AttrRef& fromTxt, const AttrRef& toTxt, uint32_t start, uint32_t count) noexcept {
            return String(Kind::Substr, fromTxt, toTxt, static_cast<int32_t>(start), static_cast<int32_t>(count), nullptr);
        }

        static String strlen(const AttrRef& txtAttr, const AttrRef& dstNumAttr) noexcept {
            return String(Kind::Strlen, txtAttr, dstNumAttr, 0, 0, nullptr);
        }

        static String btlen(const AttrRef& txtAttr, const AttrRef& dstNumAttr) noexcept {
            return String(Kind::Btlen, txtAttr, dstNumAttr, 0, 0, nullptr);
        }

        static String spstr(const AttrRef& src, const AttrRef& dst, const char* delimiterQuoted, uint32_t index) noexcept {
            return String(Kind::Spstr, src, dst, static_cast<int32_t>(index), 0, delimiterQuoted);
        }

        bool serialize(TxFrame& tx) const noexcept override;
        NEX_COMMAND_SLOT(String)

    private:
        Kind _kind;
        AttrRef _a1;
        AttrRef _a2;
        int32_t _i1;
        int32_t _i2;
        const char* _delimiterQuoted;

        String(Kind kind, const AttrRef& a1, const AttrRef& a2, int32_t i1, int32_t i2,
                 const char* delimiterQuoted) noexcept
            : _kind(kind)
            , _a1(a1)
            , _a2(a2)
            , _i1(i1)
            , _i2(i2)
            , _delimiterQuoted(delimiterQuoted) {}
    };


    //========== Работа с компонентами =========================================

    /**
     * Операции с компонентом: `ref`, `vis`, `tsw`, `click`, `setlayer` (NIS).
     */
    class Component final : public Command {
    public:
        enum class Kind : uint8_t {
            Refresh,   /**< принудительная перерисовка компонента */
            Visible,   /**< показать/скрыть */
            TouchSwitch,   /**< touch switch */
            Click, /**< программное нажатие/отпускание */
            Setlayer, /**< порядок отрисовки (Z-order) */
        };

        static Component refresh(const Literal& compName) noexcept {
            return Component(Kind::Refresh, compName, 0u);
        }
        static Component visible(const Literal& compName, bool on) noexcept {
            return Component(Kind::Visible, compName, on ? 1u : 0u);
        }
        static Component tsw(const Literal& compName, bool enabled) noexcept {
            return Component(Kind::TouchSwitch, compName, enabled ? 1u : 0u);
        }
        static Component click(const Literal& compName, TouchState state) noexcept {
            return Component(Kind::Click, compName, static_cast<uint32_t>(state));
        }
        static Component setlayer(const Literal& compName, const Literal& aboveCompNameOr255) noexcept {
            return Component(Kind::Setlayer, compName, aboveCompNameOr255);
        }

        bool serialize(TxFrame& tx) const noexcept override;
        NEX_COMMAND_SLOT(Component)

    private:
        Kind _kind;
        const Literal& _compName;
        union Arg {
            uint32_t arg01;
            const Literal* aboveCompNameOr255;

            constexpr Arg() noexcept : arg01(0u) {}
            constexpr explicit Arg(uint32_t value) noexcept : arg01(value) {}
            constexpr explicit Arg(const Literal& compName) noexcept : aboveCompNameOr255(&compName) {}
        } _arg;

        Component(Kind k, const Literal& compName, uint32_t arg01) noexcept
            : _kind(k)
            , _compName(compName)
            , _arg(arg01) {}

        Component(Kind k, const Literal& compName, const Literal& aboveCompNameOr255) noexcept
            : _kind(k)
            , _compName(compName)
            , _arg(aboveCompNameOr255) {}
    };

    /**
     * Лексема `255` для `setlayer` (NIS): компонент **над всеми** (верхний слой / top layer).
     * @see cmd::Component::setlayer
     */
     inline constexpr Literal TopLayer{"255"};

    /**
     * **Move** — анимация перемещения компонента между двумя точками за время `timeMs` (NIS `move`).
     */
    class Move final : public Command {
    public:
        Move(const Literal& compName, Point from, Point to, uint32_t priority, uint32_t timeMs) noexcept
            : _compName(compName)
            , _from(from)
            , _to(to)
            , _priority(priority)
            , _timeMs(timeMs) {}
        bool serialize(TxFrame& tx) const noexcept override;
        NEX_COMMAND_SLOT(Move)

    private:
        const Literal& _compName;
        Point _from;
        Point _to;
        uint32_t _priority;
        uint32_t _timeMs;
    };

    //========== Waveform ========================================================
    class WaveForm final : public Command {
    public:
        enum class Kind : uint8_t {
            Add,
            Clear,
            AddT,
            RefreshStop,
            RefreshStart,
        };

        static WaveForm add(uint32_t waveformId, uint32_t channel, uint32_t value0to255) noexcept {
            return WaveForm(Kind::Add, waveformId, channel, value0to255);
        }

        static WaveForm clear(uint32_t waveformId, uint32_t channelOr255All) noexcept {
            return WaveForm(Kind::Clear, waveformId, channelOr255All, 0u);
        }

        static WaveForm addT(uint32_t waveformId, uint32_t channel, uint32_t byteCount) noexcept {
            return WaveForm(Kind::AddT, waveformId, channel, byteCount);
        }
        static WaveForm refreshStop() noexcept { return WaveForm(Kind::RefreshStop, 0u, 0u, 0u); }
        static WaveForm refreshStart() noexcept { return WaveForm(Kind::RefreshStart, 0u, 0u, 0u); }

        bool serialize(TxFrame& tx) const noexcept override;
        NEX_COMMAND_SLOT(WaveForm)

    private:
        Kind _kind;
        uint32_t _waveformId;
        uint32_t _arg1;
        uint32_t _arg2;

        WaveForm(Kind kind, uint32_t waveformId, uint32_t arg1, uint32_t arg2) noexcept
            : _kind(kind)
            , _waveformId(waveformId)
            , _arg1(arg1)
            , _arg2(arg2) {}
    };

    //========== EEPROM ========================================================
    /**
     * EEPROM-команды: `wepo`/`repo` (через переменную) и `wept`/`rept` (transparent).
     */
    class Eeprom final : public Command {
    public:
        enum class Op : uint8_t {
            Write, /**< `wepo` / `wept` */
            Read,  /**< `repo` / `rept` */
        };

        static Eeprom write(const AttrRef& variableOrConstant, uint32_t eepromStart) noexcept {
            return Eeprom(Op::Write, &variableOrConstant, eepromStart, 0u);
        }
        static Eeprom read(const AttrRef& destVariable, uint32_t eepromStart) noexcept {
            return Eeprom(Op::Read, &destVariable, eepromStart, 0u);
        }

        static Eeprom writeT(uint32_t eepromStart, uint32_t byteCount) noexcept {
            return Eeprom(Op::Write, nullptr, eepromStart, byteCount);
        }
        static Eeprom readT(uint32_t eepromStart, uint32_t byteCount) noexcept {
            return Eeprom(Op::Read, nullptr, eepromStart, byteCount);
        }

        bool serialize(TxFrame& tx) const noexcept override;
        NEX_COMMAND_SLOT(Eeprom)

    private:
        Op _op;
        const AttrRef* _target;
        uint32_t _addr;
        uint32_t _byteCount;

        Eeprom(Op op, const AttrRef* target, uint32_t addr, uint32_t byteCount) noexcept
            : _op(op)
            , _target(target)
            , _addr(addr)
            , _byteCount(byteCount) {}
    };

    /**
     * **Play** — воспроизведение звука/ресурса по каналу (NIS `play`, loop 0/1).
     */
    class Play final : public Command {
    public:
        Play(uint8_t channel, uint32_t resourceId, uint32_t loop01) noexcept
            : _channel(channel), _resourceId(resourceId), _loop01(loop01) {}
        bool serialize(TxFrame& tx) const noexcept override;
        NEX_COMMAND_SLOT(Play)

    private:
        uint8_t _channel;
        uint32_t _resourceId;
        uint32_t _loop01;
    };

    //========== Файлы и каталоги ==============================================
    /**
     * Файловые команды: `delfile`, `refile`, `findfile`, `newfile`, `rdfile`, `twfile` (NIS).
     */
    class File final : public Command {
    public:
        enum class Op : uint8_t {
            Delete,
            Rename,
            Find,
            Create,
            Read,
            WriteT,
        };

        static File remove(const char* pathQuoted) noexcept { return File(Op::Delete, pathQuoted, nullptr, nullptr, 0u, 0u, 0u, 0u); }
        static File rename(const char* pathFromQuoted, const char* pathToQuoted) noexcept {
            return File(Op::Rename, pathFromQuoted, pathToQuoted, nullptr, 0u, 0u, 0u, 0u);
        }
        static File find(const char* pathQuoted, const AttrRef& dstNumAttr) noexcept {
            return File(Op::Find, pathQuoted, nullptr, &dstNumAttr, 0u, 0u, 0u, 0u);
        }
        static File create(const char* pathQuoted, uint32_t reservedSize) noexcept {
            return File(Op::Create, pathQuoted, nullptr, nullptr, reservedSize, 0u, 0u, 0u);
        }
        static File read(const char* pathQuoted, uint32_t offset, uint32_t byteCount, uint32_t crcOption) noexcept {
            return File(Op::Read, pathQuoted, nullptr, nullptr, 0u, offset, byteCount, crcOption);
        }
        static File writeT(const char* pathQuoted, uint32_t byteCount) noexcept {
            return File(Op::WriteT, pathQuoted, nullptr, nullptr, byteCount, 0u, 0u, 0u);
        }

        bool serialize(TxFrame& tx) const noexcept override;
        NEX_COMMAND_SLOT(File)

    private:
        Op _op;
        const char* _pathQuoted;
        const char* _pathToQuoted;
        const AttrRef* _dstNumAttr;
        uint32_t _reservedSize;
        uint32_t _offset;
        uint32_t _count;
        uint32_t _crcOption;

        File(Op op, const char* pathQuoted, const char* pathToQuoted, const AttrRef* dstNumAttr, uint32_t reservedSize, uint32_t offset,
             uint32_t count, uint32_t crcOption) noexcept
            : _op(op)
            , _pathQuoted(pathQuoted)
            , _pathToQuoted(pathToQuoted)
            , _dstNumAttr(dstNumAttr)
            , _reservedSize(reservedSize)
            , _offset(offset)
            , _count(count)
            , _crcOption(crcOption) {}
    };


    /**
     * Команды каталогов: `deldir`, `newdir`, `redir`, `finddir` (NIS).
     */
    class Directory final : public Command {
    public:
        enum class Op : uint8_t {
            Delete,
            Create,
            Rename,
            Find,
        };

        static Directory remove(const char* pathQuoted) noexcept {
            return Directory(Op::Delete, pathQuoted, nullptr, nullptr);
        }
        static Directory create(const char* pathQuoted) noexcept {
            return Directory(Op::Create, pathQuoted, nullptr, nullptr);
        }
        static Directory rename(const char* pathFromQuoted, const char* pathToQuoted) noexcept {
            return Directory(Op::Rename, pathFromQuoted, pathToQuoted, nullptr);
        }
        static Directory find(const char* pathQuoted, const AttrRef& dstNumAttr) noexcept {
            return Directory(Op::Find, pathQuoted, nullptr, &dstNumAttr);
        }

        bool serialize(TxFrame& tx) const noexcept override;
        NEX_COMMAND_SLOT(Directory)

    private:
        Op _op;
        const char* _pathQuoted;
        const char* _pathToQuoted;
        const AttrRef* _dstNumAttr;

        Directory(Op op, const char* pathQuoted, const char* pathToQuoted, const AttrRef* dstNumAttr) noexcept
            : _op(op)
            , _pathQuoted(pathQuoted)
            , _pathToQuoted(pathToQuoted)
            , _dstNumAttr(dstNumAttr) {}
    };

    /**
     * Операции **FileStream**-компонента: `open`, `read`, `write`, `close`, `find` (NIS).
     */
    class FileStream final : public Command {
    public:
        enum class Kind : uint8_t {
            Open,
            Read,
            Write,
            Close,
            Find,
        };

        static FileStream open(const Literal& compName, const char* path) noexcept {
            return FileStream(Kind::Open, compName, path, 0u, 0u);
        }
        static FileStream read(const Literal& compName, uint32_t offset, uint32_t byteCount) noexcept {
            return FileStream(Kind::Read, compName, nullptr, offset, byteCount);
        }
        static FileStream write(const Literal& compName, uint32_t byteCount) noexcept {
            return FileStream(Kind::Write, compName, nullptr, byteCount, 0u);
        }
        static FileStream close(const Literal& compName) noexcept {
            return FileStream(Kind::Close, compName, nullptr, 0u, 0u);
        }
        static FileStream find(const Literal& compName, const char* path) noexcept {
            return FileStream(Kind::Find, compName, path, 0u, 0u);
        }

        bool serialize(TxFrame& tx) const noexcept override;
        NEX_COMMAND_SLOT(FileStream)

    private:
        Kind _kind;
        const Literal& _compName;
        const char* _path;
        uint32_t _arg1;
        uint32_t _arg2;

        FileStream(Kind kind, const Literal& compName, const char* path, uint32_t arg1, uint32_t arg2) noexcept
            : _kind(kind)
            , _compName(compName)
            , _path(path)
            , _arg1(arg1)
            , _arg2(arg2)
        {}
    };

} // namespace cmd

namespace cmd {
namespace gui {

    class ClearScreen final : public Command {
    public:
        explicit ClearScreen(Color color) noexcept : _color(color) {}
        bool serialize(TxFrame& tx) const noexcept override;
        NEX_COMMAND_SLOT(ClearScreen)

    private:
        Color _color;
    };

    class Picture final : public Command {
    public:
        Picture(Point at, PicId pictureId) noexcept : _at(at), _pictureId(pictureId) {}
        bool serialize(TxFrame& tx) const noexcept override;
        NEX_COMMAND_SLOT(Picture)

    private:
        Point _at;
        PicId _pictureId;
    };

    /** NIS: `picq` — crop in place; `xpic` — crop with offset in picture resource. */
    class PictureCrop final : public Command {
    public:
        enum class Mode : uint8_t { InPlace, Draw };

        /** `picq x,y,w,h,picid` — `region` on screen. */
        static PictureCrop inPlace(Region region, PicId pictureId) noexcept {
            return PictureCrop(Mode::InPlace, region, pictureId);
        }
        static PictureCrop inPlace(Point upperLeft, uint32_t w, uint32_t h, PicId pictureId) noexcept {
            return inPlace(Region(upperLeft, Rect(static_cast<uint16_t>(w), static_cast<uint16_t>(h))), pictureId);
        }

        /** `xpic dstX,dstY,w,h,srcX,srcY,picid` — `dst` upper-left on screen; `src` crop in picture (w×h from `src`). */
        static PictureCrop draw(Point dst, Region src, PicId pictureId) noexcept {
            return PictureCrop(Mode::Draw, dst, src, pictureId);
        }

        bool serialize(TxFrame& tx) const noexcept override;
        NEX_COMMAND_SLOT(PictureCrop)

    private:
        Mode _mode;
        /** InPlace: on screen; Draw: crop in picture resource. */
        Region _region;
        Point _dst;
        PicId _pictureId;

        PictureCrop(Mode mode, Region region, PicId pictureId) noexcept
            : _mode(mode)
            , _region(region)
            , _pictureId(pictureId) {}

        PictureCrop(Mode mode, Point dst, Region src, PicId pictureId) noexcept
            : _mode(mode)
            , _region(src)
            , _dst(dst)
            , _pictureId(pictureId) {}
    };

    /** `xstr`: `pco`/`bco` — значение 565 (`Color::raw`); текст — в кавычках. */
    class TextInRegion final : public Command {
    public:
        TextInRegion(Region region, FontId fontId, Color fg, Color bg, HAlign hAlign, VAlign vAlign,
            BGStyle fill, const char* contentToken) noexcept
            : _region(region)
            , _fontId(fontId)
            , _fg(fg)
            , _bg(bg)
            , _hAlign(hAlign)
            , _vAlign(vAlign)
            , _fill(fill)
            , _contentToken(contentToken) {}

        bool serialize(TxFrame& tx) const noexcept override;
        NEX_COMMAND_SLOT(TextInRegion)

    private:
        Region _region;
        FontId _fontId;
        Color _fg;
        Color _bg;
        HAlign _hAlign;
        VAlign _vAlign;
        BGStyle _fill;
        const char* _contentToken;
    };

    class Rect final : public Command {
    public:
        enum class Mode : uint8_t { Outline, Fill };

        Rect(Mode mode, Region region, Color color) noexcept
            : _mode(mode)
            , _region(region)
            , _color(color) {}

        bool serialize(TxFrame& tx) const noexcept override;
        NEX_COMMAND_SLOT(Rect)

    private:
        Mode _mode;
        Region _region;
        Color _color;
    };

    class Line final : public Command {
    public:
        Line(Point from, Point to, Color color) noexcept : _from(from), _to(to), _color(color) {}
        bool serialize(TxFrame& tx) const noexcept override;
        NEX_COMMAND_SLOT(Line)

    private:
        Point _from;
        Point _to;
        Color _color;
    };

    class Circle final : public Command {
    public:
        enum class Kind : uint8_t { Outline, Filled };

        Circle(Kind kind, Point center, uint16_t radius, Color color) noexcept
            : _kind(kind)
            , _center(center)
            , _radius(radius)
            , _color(color) {}

        bool serialize(TxFrame& tx) const noexcept override;
        NEX_COMMAND_SLOT(Circle)

    private:
        Kind _kind;
        Point _center;
        uint16_t _radius;
        Color _color;
    };

} // namespace gui
} // namespace cmd

} // namespace nex
