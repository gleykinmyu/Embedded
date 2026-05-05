#pragma once
#include <cstdint>
#include <cstring>
#include "nexProtocol.hpp"
#include "nexTypes.hpp"

/**
 * @file nexCommands.hpp
 *
 * **Список классов команд** (исходящие UART-инструкции NIS). Базовые типы: `nex::Command`, `nex::TransparentCommand`
 * (последний — команды с фазой transparent после первого кадра, §1.16).
 *
 * | Пространство имён   | Класс                  | База                   |
 * |---------------------|------------------------|------------------------|
 * | `nex`               | `Command`              | —                      |
 * | `nex`               | `TransparentCommand`   | `Command`              |
 * | `nex::cmd::assign`  | `Text`                 | `Command`              |
 * | `nex::cmd::assign`  | `TextSubtract`         | `Command`              |
 * | `nex::cmd::assign`  | `Numeric`              | `Command`              |
 * | `nex::cmd::oper`    | `BareOperCmd`          | `Command`              |
 * | `nex::cmd::oper`    | `Refresh`              | `Command`              |
 * | `nex::cmd::oper`    | `CompLiteralUintCmd`   | `Command`              |
 * | `nex::cmd::oper`    | `Setlayer`             | `Command`              |
 * | `nex::cmd::oper`    | `RefreshWaveform`      | `Command`              |
 * | `nex::cmd::oper`    | `Get`                  | `Command`              |
 * | `nex::cmd::oper`    | `ConvertEx`            | `Command`              |
 * | `nex::cmd::oper`    | `SubString`            | `Command`              |
 * | `nex::cmd::oper`    | `Strlen`               | `Command`              |
 * | `nex::cmd::oper`    | `SplitString`          | `Command`              |
 * | `nex::cmd::oper`    | `Randset`              | `Command`              |
 * | `nex::cmd::oper`    | `WaveAdd`              | `Command`              |
 * | `nex::cmd::oper`    | `WaveAddt`             | `TransparentCommand`   |
 * | `nex::cmd::oper`    | `WaveClear`            | `Command`              |
 * | `nex::cmd::oper`    | `EepromVariable`       | `Command`              |
 * | `nex::cmd::oper`    | `EepromTransparent`    | `TransparentCommand`   |
 * | `nex::cmd::oper`    | `Cfgpio`               | `Command`              |
 * | `nex::cmd::oper`    | `Move`                 | `Command`              |
 * | `nex::cmd::oper`    | `Play`                 | `Command`              |
 * | `nex::cmd::oper`    | `FsPathVerbCmd`        | `Command`              |
 * | `nex::cmd::oper`    | `FsRenameCmd`          | `Command`              |
 * | `nex::cmd::oper`    | `FsPathFindCmd`        | `Command`              |
 * | `nex::cmd::oper`    | `Rdfile`               | `Command`              |
 * | `nex::cmd::oper`    | `Newfile`              | `Command`              |
 * | `nex::cmd::oper`    | `Twfile`               | `TransparentCommand`   |
 * | `nex::cmd::gui`     | `ClearScreen`          | `Command`              |
 * | `nex::cmd::gui`     | `Picture`              | `Command`              |
 * | `nex::cmd::gui`     | `PictureCropInPlace`   | `Command`              |
 * | `nex::cmd::gui`     | `PictureCropDraw`      | `Command`              |
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
        virtual ~Command() = default;

        /** Записать инструкцию в `tx.payload`; при успехе `tx.length` — число байт полезной нагрузки (терминаторы не входят). */
        virtual bool serialize(TxFrame& tx) const noexcept = 0;

        /** После кадра с `0xFF×3` следует фаза transparent — сырые байты на UART (NIS §1.16). */
        virtual bool hasTransparentPhase() const noexcept { return false; }
        /** Число байт 2-й фазы; имеет смысл при `hasTransparentPhase() == true`. */
        virtual uint32_t transparentPayloadBytes() const noexcept { return 0; }
        /**
         * Для `Session`: после успешной отправки команды ожидается один кадр **0x70** или **0x71** с линии
         * (ответ NIS `get` на последовательный порт).
         */
        virtual bool awaitsGetAttributeReply() const noexcept { return false; }
    };

    /**
     * Исходящие команды с **transparent**-фазой: после первой строки инструкции хост передаёт блок байт
     * (`wept`/`rept`, `addt`, `twfile`, …). См. `Gateway::pushTransparentPreamble`.
     */
    class TransparentCommand : public Command {
    public:
        bool hasTransparentPhase() const noexcept final { return true; }
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
namespace oper {

    /**
     * Команды без параметров: `touch_j`, `rest`.
     */
    class BareOperCmd final : public Command {
    public:
        enum class Kind : uint8_t {
            TouchJ, /**< калибровка сенсора (NIS `touch_j`) */
            Restart, /**< перезагрузка дисплея (NIS `rest`) */
        };

    private:
        Kind _kind;

    public:
        explicit BareOperCmd(Kind k) noexcept : _kind(k) {}
        bool serialize(TxFrame& tx) const noexcept override;
    };

    //========== Работа с компонентами =========================================

    /**
     * **Ref**resh — принудительно перерисовать компонент (NIS `ref`), например после изменения данных без смены страницы.
     */
    class Refresh final : public Command {
        const Literal& _compName;

    public:
        explicit Refresh(const Literal& compName) noexcept : _compName(compName) {}
        bool serialize(TxFrame& tx) const noexcept override;
    };

    /**
     * **vis** / **tsw** / **click** — компонент и одно число 0/1 (NIS `vis`, `tsw`, `click`).
     */
    class CompLiteralUintCmd final : public Command {
    public:
        enum class Kind : uint8_t {
            Vis,   /**< показать/скрыть */
            Tsw,   /**< touch switch */
            Click, /**< программное нажатие/отпускание */
        };

    private:
        Kind _kind;
        const Literal& _compName;
        uint32_t _arg01;

        CompLiteralUintCmd(Kind k, const Literal& compName, uint32_t arg01) noexcept
            : _kind(k)
            , _compName(compName)
            , _arg01(arg01) {}

    public:
        static CompLiteralUintCmd vis(const Literal& compName, bool on) noexcept {
            return CompLiteralUintCmd(Kind::Vis, compName, on ? 1u : 0u);
        }
        static CompLiteralUintCmd tsw(const Literal& compName, bool enabled) noexcept {
            return CompLiteralUintCmd(Kind::Tsw, compName, enabled ? 1u : 0u);
        }
        static CompLiteralUintCmd click(const Literal& compName, uint8_t press1Release0) noexcept {
            return CompLiteralUintCmd(Kind::Click, compName, static_cast<uint32_t>(press1Release0));
        }

        bool serialize(TxFrame& tx) const noexcept override;
    };

    /**
     * **Set** **layer** — порядок отрисовки (Z-order): объект над другим или `kTopLayerLexeme` (`255`) в NIS (`setlayer`).
     */
     class Setlayer final : public Command {
        const Literal& _compName;
        const Literal& _aboveCompNameOr255;

    public:
        Setlayer(const Literal& compName, const Literal& aboveCompNameOr255) noexcept
            : _compName(compName)
            , _aboveCompNameOr255(aboveCompNameOr255) {}
        bool serialize(TxFrame& tx) const noexcept override;
    };

    /**
     * Лексема `255` для `setlayer` (NIS): компонент **над всеми** (верхний слой / top layer).
     * @see oper::Setlayer
     */
     inline constexpr Literal TopLayer{"255"};


    /**
     * **Ref**resh **stop** / **start** — остановить или возобновить автообновление waveform (NIS `ref_stop`, `ref_start`).
     */
    class RefreshWaveform final : public Command {
    public:
        enum class Op : uint8_t {
            Stop,
            Start,
        };

    protected:
        Op _op;

    public:
        explicit RefreshWaveform(Op op = Op::Stop) noexcept : _op(op) {}
        bool serialize(TxFrame& tx) const noexcept override;
    };

    //========== Работа с атрибутами ============================================

    /**
     * **Get** — запросить значение атрибута/переменной; дисплей отправит ответ по UART (NIS `get`).
     */
    class Get final : public Command {
        TargetAttr _operand;

    public:
        explicit Get(const TargetAttr& operand) noexcept : _operand(operand) {}

        bool serialize(TxFrame& tx) const noexcept override;
        bool awaitsGetAttributeReply() const noexcept override { return true; }
    };

    //========== Конвертация и строки ============================================

    /**
     * **Covx** (NIS, от *convert* + суффикс `x`) — преобразование число↔строка с параметрами длины и формата (`covx`).
     */
    class ConvertEx final : public Command {
        TargetAttr _src;
        TargetAttr _dst;
        int32_t _length;
        int32_t _format;

    public:
        ConvertEx(const TargetAttr& src, const TargetAttr& dst, int32_t length, int32_t format) noexcept
            : _src(src), _dst(dst), _length(length), _format(format) {}
        bool serialize(TxFrame& tx) const noexcept override;
    };

    /**
     * **Substr**ing — вырезать подстроку из текста в другой атрибут (NIS `substr`).
     */
    class SubString final : public Command {
        TargetAttr _fromTxt;
        TargetAttr _toTxt;
        uint32_t _start;
        uint32_t _count;

    public:
        SubString(const TargetAttr& fromTxt, const TargetAttr& toTxt, uint32_t start, uint32_t count) noexcept
            : _fromTxt(fromTxt), _toTxt(toTxt), _start(start), _count(count) {}
        bool serialize(TxFrame& tx) const noexcept override;
    };

    /**
     * Длина текстового атрибута в числовой: **символы** (NIS `strlen`) или **байты UTF-8** (NIS `btlen`).
     */
    class Strlen final : public Command {
    public:
        enum class Unit : uint8_t {
            Chars,     /**< `strlen` — длина в символах */
            Bytes, /**< `btlen` — длина в байтах */
        };

    private:
        Unit _unit;
        TargetAttr _txtAttr;
        TargetAttr _dstNumAttr;

    public:
        Strlen(const TargetAttr& txtAttr, const TargetAttr& dstNumAttr, Unit unit = Unit::Bytes) noexcept
            : _unit(unit), _txtAttr(txtAttr), _dstNumAttr(dstNumAttr) {}
        bool serialize(TxFrame& tx) const noexcept override;
    };

    /**
     * **Sp**lit **str**ing — взять поле по разделителю (NIS `spstr`, индекс фрагмента).
     */
    class SplitString final : public Command {
        TargetAttr _src;
        TargetAttr _dst;
        const char* _delimiterQuoted;
        uint32_t _index;

    public:
        SplitString(const TargetAttr& src, const TargetAttr& dst, const char* delimiterQuoted, uint32_t index) noexcept
            : _src(src), _dst(dst), _delimiterQuoted(delimiterQuoted), _index(index) {}
        bool serialize(TxFrame& tx) const noexcept override;
    };


    //========== Случайные числа ================================================
    /**
     * **Rand**om **set** — задать диапазон для `rand` / случайных значений в проекте (NIS `randset`).
     */
    class Randset final : public Command {
        int32_t _min;
        int32_t _max;

    public:
        Randset(int32_t minVal, int32_t maxVal) noexcept : _min(minVal), _max(maxVal) {}
        bool serialize(TxFrame& tx) const noexcept override;
    };

    //========== Waveform ========================================================
    class WaveAdd final : public Command {
        uint32_t _waveformId;
        uint32_t _channel;
        uint32_t _value0to255;

    public:
        WaveAdd(uint32_t waveformId, uint32_t channel, uint32_t value0to255) noexcept
            : _waveformId(waveformId), _channel(channel), _value0to255(value0to255) {}
        bool serialize(TxFrame& tx) const noexcept override;
    };

    /**
     * **Wave**form **add** **t**ransparent — режим прозрачной передачи блока точек (NIS `addt`).
     */
    class WaveAddt final : public TransparentCommand {
        uint32_t _waveformId;
        uint32_t _channel;
        uint32_t _byteCount;

    public:
        WaveAddt(uint32_t waveformId, uint32_t channel, uint32_t byteCount) noexcept
            : _waveformId(waveformId), _channel(channel), _byteCount(byteCount) {}
        bool serialize(TxFrame& tx) const noexcept override;
        uint32_t transparentPayloadBytes() const noexcept override { return _byteCount; }
    };

    /**
     * **Wave**form **cle**ar — очистить канал или все каналы осциллограммы (NIS `cle`).
     */
    class WaveClear final : public Command {
        uint32_t _waveformId;
        uint32_t _channelOr255All;

    public:
        WaveClear(uint32_t waveformId, uint32_t channelOr255All) noexcept
            : _waveformId(waveformId), _channelOr255All(channelOr255All) {}
        bool serialize(TxFrame& tx) const noexcept override;
    };

    //========== EEPROM ========================================================
    /**
     * EEPROM ↔ атрибут или переменная на дисплее (NIS `wepo` / `repo`), не transparent mode.
     */
    class EepromVariable final : public Command {
    public:
        enum class Op : uint8_t {
            Write, /**< `wepo` — значение переменной/константы в EEPROM */
            Read,  /**< `repo` — из EEPROM в переменную */
        };

    private:
        Op _op;
        TargetAttr _target;
        uint32_t _addr;

        EepromVariable(Op op, const TargetAttr& target, uint32_t addr) noexcept : _op(op), _target(target), _addr(addr) {}

    public:
        static EepromVariable write(const TargetAttr& variableOrConstant, uint32_t eepromStart) noexcept {
            return EepromVariable(Op::Write, variableOrConstant, eepromStart);
        }
        static EepromVariable read(const TargetAttr& destVariable, uint32_t eepromStart) noexcept {
            return EepromVariable(Op::Read, destVariable, eepromStart);
        }

        Op op() const noexcept { return _op; }

        bool serialize(TxFrame& tx) const noexcept override;
    };

    /**
     * EEPROM в **transparent mode** — сырой блок байт по UART (NIS `wept` / `rept`).
     */
    class EepromTransparent final : public TransparentCommand {
    public:
        enum class Op : uint8_t {
            Write, /**< `wept` */
            Read,  /**< `rept` */
        };

    private:
        Op _op;
        uint32_t _addr;
        uint32_t _byteCount;

        EepromTransparent(Op op, uint32_t addr, uint32_t byteCount) noexcept : _op(op), _addr(addr), _byteCount(byteCount) {}

    public:
        static EepromTransparent write(uint32_t eepromStart, uint32_t byteCount) noexcept {
            return EepromTransparent(Op::Write, eepromStart, byteCount);
        }
        static EepromTransparent read(uint32_t eepromStart, uint32_t byteCount) noexcept {
            return EepromTransparent(Op::Read, eepromStart, byteCount);
        }

        Op op() const noexcept { return _op; }

        bool serialize(TxFrame& tx) const noexcept override;
        uint32_t transparentPayloadBytes() const noexcept override { return _byteCount; }
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
     * **delfile** / **deldir** / **newdir** — одна строка пути в кавычках (NIS).
     */
    class FsPathVerbCmd final : public Command {
    public:
        enum class Verb : uint8_t {
            DelFile,
            DelDir,
            NewDir,
        };

    private:
        Verb _verb;
        const char* _pathQuoted;

    public:
        explicit FsPathVerbCmd(Verb v, const char* pathQuoted) noexcept : _verb(v), _pathQuoted(pathQuoted) {}
        bool serialize(TxFrame& tx) const noexcept override;
    };

    /**
     * **refile** / **redir** — два пути в кавычках через запятую (NIS).
     */
    class FsRenameCmd final : public Command {
    public:
        enum class Target : uint8_t {
            File,
            Dir,
        };

    private:
        Target _target;
        const char* _pathFromQuoted;
        const char* _pathToQuoted;

    public:
        FsRenameCmd(Target t, const char* pathFromQuoted, const char* pathToQuoted) noexcept
            : _target(t), _pathFromQuoted(pathFromQuoted), _pathToQuoted(pathToQuoted) {}
        bool serialize(TxFrame& tx) const noexcept override;
    };

    /**
     * **findfile** / **finddir** — путь и числовой атрибут результата (NIS).
     */
    class FsPathFindCmd final : public Command {
    public:
        enum class Target : uint8_t {
            File,
            Dir,
        };

    private:
        Target _target;
        const char* _pathQuoted;
        TargetAttr _dstNumAttr;

    public:
        FsPathFindCmd(Target t, const char* pathQuoted, const TargetAttr& dstNumAttr) noexcept
            : _target(t), _pathQuoted(pathQuoted), _dstNumAttr(dstNumAttr) {}
        bool serialize(TxFrame& tx) const noexcept override;
    };

    /**
     * **R**ea**d** **file** — прочитать фрагмент файла на UART (NIS `rdfile`, offset/count/CRC).
     */
    class Rdfile final : public Command {
        const char* _pathQuoted;
        uint32_t _offset;
        uint32_t _count;
        uint32_t _crcOption;

    public:
        Rdfile(const char* pathQuoted, uint32_t offset, uint32_t count, uint32_t crcOption) noexcept
            : _pathQuoted(pathQuoted), _offset(offset), _count(count), _crcOption(crcOption) {}
        bool serialize(TxFrame& tx) const noexcept override;
    };

    /**
     * **New** **file** — создать файл с зарезервированным размером (NIS `newfile`).
     */
    class Newfile final : public Command {
        const char* _pathQuoted;
        uint32_t _reservedSize;

    public:
        Newfile(const char* pathQuoted, uint32_t reservedSize) noexcept : _pathQuoted(pathQuoted), _reservedSize(reservedSize) {}
        bool serialize(TxFrame& tx) const noexcept override;
    };

    /**
     * **T**ransparent **w**rite **file** — подготовить приём файла по UART (размер, путь; NIS `twfile`).
     */
    class Twfile final : public TransparentCommand {
        const char* _pathQuoted;
        uint32_t _fileSize;

    public:
        Twfile(const char* pathQuoted, uint32_t fileSize) noexcept : _pathQuoted(pathQuoted), _fileSize(fileSize) {}
        bool serialize(TxFrame& tx) const noexcept override;
        uint32_t transparentPayloadBytes() const noexcept override { return _fileSize; }
    };

    
} // namespace oper

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

    /**
     * **Pic** **q** — заменить прямоугольник экрана фрагментом той же области из полноэкранного ресурса (NIS `picq`).
     */
    class PictureCropInPlace final : public Command {
        Point _upperLeft;
        uint32_t _w;
        uint32_t _h;
        PicId _pictureId;

    public:
        PictureCropInPlace(Point upperLeft, uint32_t w, uint32_t h, PicId pictureId) noexcept
            : _upperLeft(upperLeft), _w(w), _h(h), _pictureId(pictureId) {}
        bool serialize(TxFrame& tx) const noexcept override;
    };

    /**
     * **Xpic** — вырезать область из ресурса и нарисовать в `dst` (NIS `xpic`).
     */
    class PictureCropDraw final : public Command {
        Point _dst;
        uint32_t _w;
        uint32_t _h;
        Point _src;
        PicId _pictureId;

    public:
        PictureCropDraw(Point dst, uint32_t w, uint32_t h, Point src, PicId pictureId) noexcept
            : _dst(dst), _w(w), _h(h), _src(src), _pictureId(pictureId) {}
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
