#pragma once
#include <cstdint>
#include <cstring>
#include "nexProtocol.hpp"
#include "nexTypes.hpp"

/**
 * @file nexCommands.hpp
 *
 * **Список классов команд** (исходящие UART-инструкции NIS). Базовый тип: `nex::Command` (в т.ч. команды с фазой
 * transparent после первого кадра — флаг `Command::Flag::TransparentReadyToReceive`, §1.16).
 *
 * | Пространство имён   | Класс                  | База                   |
 * |---------------------|------------------------|------------------------|
 * | `nex`               | `Command`              | —                      |
 * | `nex::cmd::assign`  | `Text`                 | `Command`              |
 * | `nex::cmd::assign`  | `TextSubtract`         | `Command`              |
 * | `nex::cmd::assign`  | `Numeric`              | `Command`              |
 * | `nex::cmd`          | `System`               | `Command`              |
 * | `nex::cmd`          | `Cfgpio`               | `Command`              |
 * | `nex::cmd`          | `Get`                  | `Command`              |
 * | `nex::cmd`          | `String`               | `Command`              |
 * | `nex::cmd`          | `Component`            | `Command`              |
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
     * Базовый класс **исходящей UART-инструкции** к дисплею Nextion (NIS — Nextion Instruction Set).
     * Полезная нагрузка до `TxFrame::MAX_PAYLOAD` байт; терминатор `0xFF×3` добавляет `TxFramer`.
     */
    class Command {
    public:
        enum class Flag : uint32_t {
            NumericResponce = (1u << 0),
            StringResponse = (1u << 1),
            TransparentReadyToReceive = (1u << 2),
            RawDataReceive = (1u << 3),
        };

    protected:
        uint32_t _flags;

        static constexpr uint32_t flagMask(Flag f) noexcept { return static_cast<uint32_t>(f); }
        constexpr void setFlag(Flag f, bool enabled = true) noexcept {
            if (enabled)
                _flags |= flagMask(f);
            else
                _flags &= ~flagMask(f);
        }

    public:
        explicit Command(uint32_t flags = 0u) noexcept : _flags(flags) {}
        virtual ~Command() = default;
        constexpr bool hasFlag(Flag f) const noexcept { return (_flags & flagMask(f)) != 0u; }

        /** Записать инструкцию в `tx.payload`; при успехе `tx.length` — число байт полезной нагрузки (терминаторы не входят). */
        virtual bool serialize(TxFrame& tx) const noexcept = 0;

        /**
         * Для команд с `Flag::TransparentReadyToReceive` (MCU → панель): сколько байт сырой полезной нагрузки
         * после события `0xFE` (NIS §1.16). Иначе `0`.
         */
        virtual uint32_t transparentPayloadBytes() const noexcept { return 0u; }
    };

namespace cmd {
    /**
     * Пустая лексема компонента для `TargetAttr`: при `comp.len == 0` в кадр идёт только `attr`
     * (`sys0`, `p0.t0.val`, …).
     */
    inline constexpr Literal kEmptyCompLexeme{""};

    /**
     * **Target Attribute** — адрес значения в NIS: `comp` (объект / путь без точки до атрибута) и `attr`
     * (`txt`, `val`, …). Если `comp.len == 0`, в кадр выводится только `attr` как вся левая часть.
     * Объекты `Literal` для `comp` и `attr` должны жить дольше, чем хранящий `TargetAttr` объект.
     */
    struct TargetAttr {
        const Literal& comp;
        const Literal& attr;
    };

} // namespace cmd

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

    protected:
        TargetAttr _target;
        const char* _text;
        Op _op;

    public:
        Text(const TargetAttr& target, const char* text, Op op = Op::Assign) noexcept : _target(target), _text(text), _op(op) {}
        bool serialize(TxFrame& tx) const noexcept override;
    };

    /**
     * **Text** **subtract** — укоротить строку с конца (NIS §2.3): `…-=<n>` (`n` — десятичное ASCII в кадре).
     */
    class TextSubtract final : public Command {
        TargetAttr _target;
        uint32_t _charsCount;

    public:
        TextSubtract(const TargetAttr& target, uint32_t charsCount) noexcept : _target(target), _charsCount(charsCount) {}
        bool serialize(TxFrame& tx) const noexcept override;
    };

    // -------------------------------------------------------------------------
    // Числовые присваивания (NIS §2.7–2.12, §1.18 hex; цели — `.val` и др.)
    // -------------------------------------------------------------------------

    /**
     * **Numeric** assign — целочисленное присваивание к атрибуту или системной переменной (NIS §2.7, §6):
     * `…=<int32>` или составные операторы §2.8–2.12 (`+=`, `-=`, …).
     */
    class Numeric final : public Command {
    public:
        /** **Op**eration — вид составного присваивания для числовых атрибутов. */
        enum class Op : uint8_t {
            Assign,
            Add, /**< += */
            Sub, /**< -= */
            Mul, /**< *= */
            Div, /**< /= */
            Mod, /**< %= */
        };

    protected:
        TargetAttr _target;
        int32_t _value;
        Op _op;

    public:
        Numeric(const TargetAttr& target, int32_t value, Op op = Op::Assign) noexcept : _target(target), _value(value), _op(op) {}
        bool serialize(TxFrame& tx) const noexcept override;
    };

} // namespace assign

} // namespace cmd

namespace cmd {
/**
 * NIS §3 — operational commands (`команда параметры`)
 * Имена классов — глагол NIS; параметры и диапазоны см. [instruction-set](https://nextion.tech/instruction-set/).
 */

    /**
     * Системные команды: `touch_j`, `rest`, `randset`.
     */
    class System final : public Command {
    public:
        enum class Kind : uint8_t {
            TouchJ, /**< калибровка сенсора (NIS `touch_j`) */
            Restart, /**< перезагрузка дисплея (NIS `rest`) */
            Randset, /**< диапазон случайного числа (NIS `randset`) */
        };

    private:
        Kind _kind;
        int32_t _min;
        int32_t _max;

        explicit System(Kind k) noexcept : _kind(k), _min(0), _max(0) {}
        System(int32_t minVal, int32_t maxVal) noexcept : _kind(Kind::Randset), _min(minVal), _max(maxVal) {}

    public:
        static System touchJ() noexcept { return System(Kind::TouchJ); }
        static System restart() noexcept { return System(Kind::Restart); }
        static System randset(int32_t minVal, int32_t maxVal) noexcept { return System(minVal, maxVal); }
        bool serialize(TxFrame& tx) const noexcept override;
    };

    // --- GPIO, анимация, звук ---
    /**
     * **Cfg**igure **gpio** — настроить вывод MCU, видимый дисплею, и привязку к компоненту (NIS `cfgpio`).
     */
    class Cfgpio final : public Command {
        uint32_t _pin;
        uint32_t _mode;
        const Literal& _bindCompNameOrZero;

    public:
        Cfgpio(uint32_t pin, uint32_t mode, const Literal& bindCompNameOrZero) noexcept
            : _pin(pin), _mode(mode), _bindCompNameOrZero(bindCompNameOrZero) {}
        bool serialize(TxFrame& tx) const noexcept override;
    };


    //========== Работа с атрибутами ============================================

    /**
     * **Get** — запросить значение атрибута/переменной; дисплей отправит ответ по UART (NIS `get`).
     */
    class Get final : public Command {
        TargetAttr _operand;

        Get(const TargetAttr& operand, uint32_t flags) noexcept
            : Command(flags)
            , _operand(operand) {}

    public:
        static Get numeric(const TargetAttr& operand) noexcept {
            return Get(operand, flagMask(Flag::NumericResponce));
        }

        static Get string(const TargetAttr& operand) noexcept {
            return Get(operand, flagMask(Flag::StringResponse));
        }

        bool serialize(TxFrame& tx) const noexcept override;
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

    private:
        Kind _kind;
        TargetAttr _a1;
        TargetAttr _a2;
        int32_t _i1;
        int32_t _i2;
        const char* _delimiterQuoted;

        String(Kind kind, const TargetAttr& a1, const TargetAttr& a2, int32_t i1, int32_t i2,
                 const char* delimiterQuoted) noexcept
            : _kind(kind)
            , _a1(a1)
            , _a2(a2)
            , _i1(i1)
            , _i2(i2)
            , _delimiterQuoted(delimiterQuoted) {}

    public:
        static String covx(const TargetAttr& src, const TargetAttr& dst, int32_t length, int32_t format) noexcept {
            return String(Kind::Covx, src, dst, length, format, nullptr);
        }

        static String substr(const TargetAttr& fromTxt, const TargetAttr& toTxt, uint32_t start, uint32_t count) noexcept {
            return String(Kind::Substr, fromTxt, toTxt, static_cast<int32_t>(start), static_cast<int32_t>(count), nullptr);
        }

        static String strlen(const TargetAttr& txtAttr, const TargetAttr& dstNumAttr) noexcept {
            return String(Kind::Strlen, txtAttr, dstNumAttr, 0, 0, nullptr);
        }

        static String btlen(const TargetAttr& txtAttr, const TargetAttr& dstNumAttr) noexcept {
            return String(Kind::Btlen, txtAttr, dstNumAttr, 0, 0, nullptr);
        }

        static String spstr(const TargetAttr& src, const TargetAttr& dst, const char* delimiterQuoted, uint32_t index) noexcept {
            return String(Kind::Spstr, src, dst, static_cast<int32_t>(index), 0, delimiterQuoted);
        }

        bool serialize(TxFrame& tx) const noexcept override;
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

    public:
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
        const Literal& _compName;
        Point _from;
        Point _to;
        uint32_t _priority;
        uint32_t _timeMs;

    public:
        Move(const Literal& compName, Point from, Point to, uint32_t priority, uint32_t timeMs) noexcept
            : _compName(compName)
            , _from(from)
            , _to(to)
            , _priority(priority)
            , _timeMs(timeMs) {}
        bool serialize(TxFrame& tx) const noexcept override;
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

    private:
        Kind _kind;
        uint32_t _waveformId;
        uint32_t _arg1;
        uint32_t _arg2;

        WaveForm(Kind kind, uint32_t waveformId, uint32_t arg1, uint32_t arg2, uint32_t flags = 0u) noexcept
            : Command(flags)
            , _kind(kind)
            , _waveformId(waveformId)
            , _arg1(arg1)
            , _arg2(arg2) {}

    public:
        static WaveForm add(uint32_t waveformId, uint32_t channel, uint32_t value0to255) noexcept {
            return WaveForm(Kind::Add, waveformId, channel, value0to255);
        }

        static WaveForm clear(uint32_t waveformId, uint32_t channelOr255All) noexcept {
            return WaveForm(Kind::Clear, waveformId, channelOr255All, 0u);
        }

        static WaveForm addT(uint32_t waveformId, uint32_t channel, uint32_t byteCount) noexcept {
            return WaveForm(Kind::AddT, waveformId, channel, byteCount, flagMask(Flag::TransparentReadyToReceive));
        }
        static WaveForm refreshStop() noexcept { return WaveForm(Kind::RefreshStop, 0u, 0u, 0u); }
        static WaveForm refreshStart() noexcept { return WaveForm(Kind::RefreshStart, 0u, 0u, 0u); }

        bool serialize(TxFrame& tx) const noexcept override;
        uint32_t transparentPayloadBytes() const noexcept override {
            return (_kind == Kind::AddT) ? _arg2 : 0u;
        }
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

    private:
        Op _op;
        const TargetAttr* _target;
        uint32_t _addr;
        uint32_t _byteCount;

        Eeprom(Op op, const TargetAttr* target, uint32_t addr, uint32_t byteCount, uint32_t flags) noexcept
            : Command(flags)
            , _op(op)
            , _target(target)
            , _addr(addr)
            , _byteCount(byteCount) {}

    public:
        static Eeprom write(const TargetAttr& variableOrConstant, uint32_t eepromStart) noexcept {
            return Eeprom(Op::Write, &variableOrConstant, eepromStart, 0u, 0u);
        }
        static Eeprom read(const TargetAttr& destVariable, uint32_t eepromStart) noexcept {
            return Eeprom(Op::Read, &destVariable, eepromStart, 0u, 0u);
        }

        static Eeprom writeT(uint32_t eepromStart, uint32_t byteCount) noexcept {
            return Eeprom(Op::Write, nullptr, eepromStart, byteCount, flagMask(Flag::TransparentReadyToReceive));
        }
        static Eeprom readT(uint32_t eepromStart, uint32_t byteCount) noexcept {
            return Eeprom(Op::Read, nullptr, eepromStart, byteCount, flagMask(Flag::RawDataReceive));
        }

        bool serialize(TxFrame& tx) const noexcept override;
        uint32_t transparentPayloadBytes() const noexcept override {
            return (_op == Op::Write && hasFlag(Flag::TransparentReadyToReceive)) ? _byteCount : 0u;
        }
    };

    /**
     * **Play** — воспроизведение звука/ресурса по каналу (NIS `play`, loop 0/1).
     */
    class Play final : public Command {
        uint32_t _channel;
        uint32_t _resourceId;
        uint32_t _loop01;

    public:
        Play(uint32_t channel, uint32_t resourceId, uint32_t loop01) noexcept
            : _channel(channel), _resourceId(resourceId), _loop01(loop01) {}
        bool serialize(TxFrame& tx) const noexcept override;
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

    private:
        Op _op;
        const char* _pathQuoted;
        const char* _pathToQuoted;
        const TargetAttr* _dstNumAttr;
        uint32_t _reservedSize;
        uint32_t _offset;
        uint32_t _count;
        uint32_t _crcOption;

        File(Op op, const char* pathQuoted, const char* pathToQuoted, const TargetAttr* dstNumAttr, uint32_t reservedSize, uint32_t offset,
             uint32_t count, uint32_t crcOption, uint32_t flags = 0u) noexcept
            : Command(flags)
            , _op(op)
            , _pathQuoted(pathQuoted)
            , _pathToQuoted(pathToQuoted)
            , _dstNumAttr(dstNumAttr)
            , _reservedSize(reservedSize)
            , _offset(offset)
            , _count(count)
            , _crcOption(crcOption) {}

    public:
        static File remove(const char* pathQuoted) noexcept { return File(Op::Delete, pathQuoted, nullptr, nullptr, 0u, 0u, 0u, 0u); }
        static File rename(const char* pathFromQuoted, const char* pathToQuoted) noexcept {
            return File(Op::Rename, pathFromQuoted, pathToQuoted, nullptr, 0u, 0u, 0u, 0u);
        }
        static File find(const char* pathQuoted, const TargetAttr& dstNumAttr) noexcept {
            return File(Op::Find, pathQuoted, nullptr, &dstNumAttr, 0u, 0u, 0u, 0u);
        }
        static File create(const char* pathQuoted, uint32_t reservedSize) noexcept {
            return File(Op::Create, pathQuoted, nullptr, nullptr, reservedSize, 0u, 0u, 0u);
        }
        static File read(const char* pathQuoted, uint32_t offset, uint32_t byteCount, uint32_t crcOption) noexcept {
            return File(Op::Read, pathQuoted, nullptr, nullptr, 0u, offset, byteCount, crcOption, flagMask(Flag::RawDataReceive));
        }
        static File writeT(const char* pathQuoted, uint32_t byteCount) noexcept {
            return File(Op::WriteT, pathQuoted, nullptr, nullptr, byteCount, 0u, 0u, 0u, flagMask(Flag::TransparentReadyToReceive));
        }

        bool serialize(TxFrame& tx) const noexcept override;
        uint32_t transparentPayloadBytes() const noexcept override {
            return (_op == Op::WriteT) ? _reservedSize : 0u;
        }
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

    private:
        Op _op;
        const char* _pathQuoted;
        const char* _pathToQuoted;
        const TargetAttr* _dstNumAttr;

        Directory(Op op, const char* pathQuoted, const char* pathToQuoted, const TargetAttr* dstNumAttr) noexcept
            : _op(op)
            , _pathQuoted(pathQuoted)
            , _pathToQuoted(pathToQuoted)
            , _dstNumAttr(dstNumAttr) {}

    public:
        static Directory remove(const char* pathQuoted) noexcept { return Directory(Op::Delete, pathQuoted, nullptr, nullptr); }
        static Directory create(const char* pathQuoted) noexcept { return Directory(Op::Create, pathQuoted, nullptr, nullptr); }
        static Directory rename(const char* pathFromQuoted, const char* pathToQuoted) noexcept {
            return Directory(Op::Rename, pathFromQuoted, pathToQuoted, nullptr);
        }
        static Directory find(const char* pathQuoted, const TargetAttr& dstNumAttr) noexcept {
            return Directory(Op::Find, pathQuoted, nullptr, &dstNumAttr);
        }

        bool serialize(TxFrame& tx) const noexcept override;
    };
    


/**
 * NIS §4 — GUI Designing Commands (рисование на активной странице; до загрузки страницы в Program.s недопустимы).
 * Параметры и примеры: [instruction-set §4](https://nextion.tech/instruction-set/).
 */
namespace gui {

    /** **Cls** — залить весь экран цветом (NIS `cls`); в кадр — десятичное `color.raw` (RGB565). */
    class ClearScreen final : public Command {
        Color _color;

    public:
        explicit ClearScreen(Color color) noexcept : _color(color) {}
        bool serialize(TxFrame& tx) const noexcept override;
    };

    /** **Pic** — вывести ресурс-картинку в точке `at` (NIS `pic`). */
    class Picture final : public Command {
        Point _at;
        PicId _pictureId;

    public:
        Picture(Point at, PicId pictureId) noexcept : _at(at), _pictureId(pictureId) {}
        bool serialize(TxFrame& tx) const noexcept override;
    };

    /** Обрезка/рисование картинки: `picq` (in place) или `xpic` (draw). */
    class PictureCrop final : public Command {
    public:
        enum class Kind : uint8_t {
            InPlace, /**< NIS `picq` */
            Draw,    /**< NIS `xpic` */
        };

    private:
        Kind _kind;
        Point _a;
        uint32_t _w;
        uint32_t _h;
        Point _b;
        PicId _pictureId;

        PictureCrop(Kind kind, Point a, uint32_t w, uint32_t h, Point b, PicId pictureId) noexcept
            : _kind(kind), _a(a), _w(w), _h(h), _b(b), _pictureId(pictureId) {}

    public:
        /** `picq`: заменить область экрана фрагментом той же области полноэкранной картинки. */
        static PictureCrop inPlace(Point upperLeft, uint32_t w, uint32_t h, PicId pictureId) noexcept {
            return PictureCrop(Kind::InPlace, upperLeft, w, h, Point{}, pictureId);
        }

        /** `xpic`: вырезать область из ресурса (`src`) и нарисовать в `dst`. */
        static PictureCrop draw(Point dst, uint32_t w, uint32_t h, Point src, PicId pictureId) noexcept {
            return PictureCrop(Kind::Draw, dst, w, h, src, pictureId);
        }

        bool serialize(TxFrame& tx) const noexcept override;
    };

    /**
     * **Xstr** — текст в прямоугольнике (NIS `xstr`).
     * `fg` / `bg` — в кадр как десятичное `raw` (RGB565); при `BGStyle::CropImage` / `BGStyle::Image` у `bg` — `picid` (то же число в NIS).
     * `fill` — режим фона (`sta` в NIS для xstr), см. `BGStyle`.
     * `contentToken` — хвост лексемы (`va0.txt`, `"Hello"`, …).
     */
    class TextInRegion final : public Command {
        Point _upperLeft;
        uint32_t _w;
        uint32_t _h;
        FontId _fontId;
        Color _fg;
        Color _bg;
        uint32_t _hAlign;
        uint32_t _vAlign;
        BGStyle _fill;
        const char* _contentToken;

    public:
        TextInRegion(Point upperLeft, uint32_t w, uint32_t h, FontId fontId, Color fg, Color bg, uint32_t hAlign, uint32_t vAlign,
            BGStyle fill, const char* contentToken) noexcept
            : _upperLeft(upperLeft)
            , _w(w)
            , _h(h)
            , _fontId(fontId)
            , _fg(fg)
            , _bg(bg)
            , _hAlign(hAlign)
            , _vAlign(vAlign)
            , _fill(fill)
            , _contentToken(contentToken) {}
        bool serialize(TxFrame& tx) const noexcept override;
    };

    /**
     * Прямоугольник на экране: заливка (`fill`) или контур (`draw`).
     * Две точки — верхний левый и **включительный** нижний правый угол; `Mode` задаёт команду NIS.
     */
    class Rect final : public Command {
    public:
        enum class Mode : uint8_t {
            Fill,    /**< `fill` — в UART уходят x,y и вычисленные w,h по углам */
            Outline, /**< `draw` */
        };

    private:
        Mode _mode;
        Point _upperLeft;
        /** Включительный нижний правый угол. */
        Point _lowerRight;
        Color _color;

    public:
        Rect(Mode mode, Point upperLeft, Point lowerRight, Color color) noexcept
            : _mode(mode), _upperLeft(upperLeft), _lowerRight(lowerRight), _color(color) {}

        bool serialize(TxFrame& tx) const noexcept override;
    };

    /** **Line** — отрезок (NIS `line`). */
    class Line final : public Command {
        Point _from;
        Point _to;
        Color _color;

    public:
        Line(Point from, Point to, Color color) noexcept : _from(from), _to(to), _color(color) {}
        bool serialize(TxFrame& tx) const noexcept override;
    };

    /**
     * **Cir** / **Cirs** — окружность: только контур или заливка (NIS `cir` / `cirs`).
     */
    class Circle final : public Command {
    public:
        enum class Kind : uint8_t {
            Outline, /**< `cir` */
            Filled,  /**< `cirs` */
        };

    private:
        Kind _kind;
        Point _center;
        uint32_t _radius;
        Color _color;

    public:
        Circle(Kind kind, Point center, uint32_t radius, Color color) noexcept
            : _kind(kind), _center(center), _radius(radius), _color(color) {}

        bool serialize(TxFrame& tx) const noexcept override;
    };

} // namespace gui

} // namespace cmd

} // namespace nex
