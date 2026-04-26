#pragma once
#include <cstdint>
#include <cstring>
#include "nexProtocol.hpp"

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

    protected:
        /** Десятичная ASCII в хвост `tx` (с `tx.length`), без ведущих нулей; не выходит за `TxFrame::MAX_PAYLOAD`. */
        static bool appendUint32(TxFrame& tx, uint32_t value) noexcept;
        static bool appendInt32(TxFrame& tx, int32_t value) noexcept;
        static bool appendComma(TxFrame& tx) noexcept;
    };

namespace cmd {



    /**
     * **Target** + **Component** — адрес объекта на странице **без имени атрибута** (для `ref`, `vis`, `click`, …).
     * `page` — префикс страницы в NIS (часто `p0`); `name` — `objname` на текущей странице, если `page == nullptr`.
     */
    struct TargetComponent {
        const char* page = nullptr;
        const char* name = nullptr;
        TargetComponent() noexcept : page(nullptr), name(nullptr) {}
        TargetComponent(const char* pageName, const char* compName) noexcept : page(pageName), name(compName) {}
        TargetComponent(const char* compName) noexcept : page(nullptr), name(compName) {}
        TargetComponent(const TargetComponent& other) noexcept : page(other.page), name(other.name) {}
    };

    /**
     * **Target** + **Attribute** — полный адрес значения в NIS: компонент (`TargetComponent`) и имя атрибута (`txt`, `val`, …)
     * или одна строка-лексема целиком (`sys0`, `p0.t0.val`).
     */
    struct TargetAttr {
        TargetComponent comp = {};
        const char* attr   = nullptr;

        TargetAttr() noexcept {}
        TargetAttr(const TargetComponent& component, const char* attr) noexcept : comp(component), attr(attr) {}
        TargetAttr(const char* pageName, const char* compName, const char* attr) noexcept : comp(pageName, compName), attr(attr) {}
        TargetAttr(const char* compName, const char* attr) noexcept : comp(nullptr, compName), attr(attr) {}
        /** Вся левая часть одной строкой: `sys0`, `t0.txt`, `p0.t0.val` (поля `comp` пустые). */
        TargetAttr(const char* fullLhsLexeme) noexcept : comp(), attr(fullLhsLexeme) {}
        TargetAttr(const TargetAttr& other) noexcept : comp(other.comp), attr(other.attr) {}
    };

/**
 * Собрать в `tx` лексему **атрибута**: `attr` | `name.attr` | `page.name.attr` (`TargetComponent` + имя атрибута).
 */
bool appendTargetLexeme(TxFrame& tx, const TargetAttr& t) noexcept;

/**
 * Собрать в `tx` лексему **компонента** (без `.attr`): `name` | `page.name` — для `ref`, `vis`, `click`, `move`, `setlayer`, `cfgpio`.
 */
bool appendTargetComponentLexeme(TxFrame& tx, const TargetComponent& c) noexcept;

/**
 * NIS §2 — присваивания по UART (`target=…`, без слова set).
 * Порядок в файле: сначала **текстовые** атрибуты (`.txt` и совместимые), затем **числовые**.
 */
namespace assign {

    // -------------------------------------------------------------------------
    // Текстовые присваивания (NIS §2.1–2.3: =, +=, -= для строковых атрибутов)
    // -------------------------------------------------------------------------

    /**
     * **Text** assign — присвоение строки (NIS §2.1): `…="<text>"` к строковому атрибуту (`txt` и т.д.).
     * Экранирование `\r`, `\"`, `\\` — §1.11 (сериализация позже).
     */
    class Text final : public Command {
        TargetAttr _target;
        const char* text;

    public:
        Text(const TargetAttr& target, const char* text) noexcept : _target(target), text(text) {}
        bool serialize(TxFrame& tx) const noexcept override;
    };

    /** **Text** **append** — дописать строку к атрибуту (NIS §2.2): `…+="<text>"`. */
    class TextAppend final : public Command {
        TargetAttr _target;
        const char* text;

    public:
        TextAppend(const TargetAttr& target, const char* text) noexcept : _target(target), text(text) {}
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

/**
 * NIS §3 — operational commands (`команда параметры`, без `{}` / if/while/for по UART).
 * Имена классов — глагол NIS; параметры и диапазоны см. [instruction-set](https://nextion.tech/instruction-set/).
 */
namespace oper {

    // --- Страница, обновление, запрос значения на UART ---
    /** **Page** — переключить активную страницу по индексу (NIS `page`). */
    class Page final : public Command {
        uint8_t _pageID;

    public:
        explicit Page(uint8_t pageID) noexcept : _pageID(pageID) {}
        bool serialize(TxFrame& tx) const noexcept override;
    };

    /**
     * **Send** **me** — запросить у дисплея номер текущей страницы по UART (NIS `sendme`); ответ — кадр с id страницы.
     */
    class SendMe final : public Command {
    public:
        bool serialize(TxFrame& tx) const noexcept override;
    };
    
    /**
     * **Ref**resh — принудительно перерисовать компонент (NIS `ref`), например после изменения данных без смены страницы.
     */
    class Refresh final : public Command {
        TargetComponent _component;

    public:
        explicit Refresh(const TargetComponent& component) noexcept : _component(component) {}
        bool serialize(TxFrame& tx) const noexcept override;
    };

    /**
     * **Ref**resh **stop** — остановить автообновление waveform (NIS `ref_stop`).
     */
    class RefreshStop final : public Command {
    public:
        bool serialize(TxFrame& tx) const noexcept override;
    };

    /**
     * **Ref**resh **start** — возобновить автообновление waveform (NIS `ref_start`).
     */
    class RefreshStart final : public Command {
    public:
        bool serialize(TxFrame& tx) const noexcept override;
    };

    /**
     * **Get** — запросить значение атрибута/переменной; дисплей отправит ответ по UART (NIS `get`).
     */
    class Get final : public Command {
        TargetAttr _operand;

    public:
        explicit Get(const TargetAttr& operand) noexcept : _operand(operand) {}
        explicit Get(const char* globalOrFullPath) noexcept : _operand(globalOrFullPath) {}
        explicit Get(const char* objName, const char* attr) noexcept : _operand(objName, attr) {}
        Get(const char* pageName, const char* objName, const char* attr) noexcept : _operand(pageName, objName, attr) {}

        bool serialize(TxFrame& tx) const noexcept override;
    };

    // --- Конвертация и строки ---
    /**
     * **Cov**ert — преобразование число↔строка с фиксированной длиной поля (NIS `cov`, без формата).
     */
    class Convert final : public Command {
        TargetAttr _src;
        TargetAttr _dst;
        int32_t _length;

    public:
        Convert(const TargetAttr& src, const TargetAttr& dst, int32_t length) noexcept : _src(src), _dst(dst), _length(length) {}
        bool serialize(TxFrame& tx) const noexcept override;
    };

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
    class Substr final : public Command {
        TargetAttr _fromTxt;
        TargetAttr _toTxt;
        uint32_t _start;
        uint32_t _count;

    public:
        Substr(const TargetAttr& fromTxt, const TargetAttr& toTxt, uint32_t start, uint32_t count) noexcept
            : _fromTxt(fromTxt), _toTxt(toTxt), _start(start), _count(count) {}
        bool serialize(TxFrame& tx) const noexcept override;
    };

    /**
     * **Str**ing **len**gth — записать в числовой атрибут длину строки в символах (NIS `strlen`).
     */
    class Strlen final : public Command {
        TargetAttr _txtAttr;
        TargetAttr _dstNumAttr;

    public:
        Strlen(const TargetAttr& txtAttr, const TargetAttr& dstNumAttr) noexcept : _txtAttr(txtAttr), _dstNumAttr(dstNumAttr) {}
        bool serialize(TxFrame& tx) const noexcept override;
    };

    /**
     * **B**ytes **len**gth — длина строки в байтах UTF-8 (NIS `btlen`).
     */
    class Btlen final : public Command {
        TargetAttr _txtAttr;
        TargetAttr _dstNumAttr;

    public:
        Btlen(const TargetAttr& txtAttr, const TargetAttr& dstNumAttr) noexcept : _txtAttr(txtAttr), _dstNumAttr(dstNumAttr) {}
        bool serialize(TxFrame& tx) const noexcept override;
    };

    /**
     * **Sp**lit **str**ing — взять поле по разделителю (NIS `spstr`, индекс фрагмента).
     */
    class Spstr final : public Command {
        TargetAttr _src;
        TargetAttr _dst;
        const char* _delimiterQuoted;
        uint32_t _index;

    public:
        Spstr(const TargetAttr& src, const TargetAttr& dst, const char* delimiterQuoted, uint32_t index) noexcept
            : _src(src), _dst(dst), _delimiterQuoted(delimiterQuoted), _index(index) {}
        bool serialize(TxFrame& tx) const noexcept override;
    };

    // --- Касание, видимость, симуляция press/release ---
    /**
     * **Touch** **j** — калибровка сенсора (NIS `touch_j`); после команды дисплей ждёт касаний по углам.
     */
    class TouchJ final : public Command {
    public:
        bool serialize(TxFrame& tx) const noexcept override;
    };

    /**
     * **Vis**ibility — показать/скрыть компонент (NIS `vis`, 0/1).
     */
    class Vis final : public Command {
        TargetComponent _component;
        uint8_t _state;

    public:
        Vis(const TargetComponent& component, uint8_t state) noexcept : _component(component), _state(state) {}
        bool serialize(TxFrame& tx) const noexcept override;
    };

    /**
     * **T**ouch **sw**itch — включить/выключить реакцию компонента на касания (NIS `tsw`).
     */
    class Tsw final : public Command {
        TargetComponent _component;
        uint8_t _enabled;

    public:
        Tsw(const TargetComponent& component, uint8_t enabled01) noexcept : _component(component), _enabled(enabled01) {}
        bool serialize(TxFrame& tx) const noexcept override;
    };

    /**
     * **Click** — программно сгенерировать нажатие/отпускание (NIS `click`, press/release).
     */
    class Click final : public Command {
        TargetComponent _component;
        uint8_t _press1Release0;

    public:
        Click(const TargetComponent& component, uint8_t press1Release0) noexcept : _component(component), _press1Release0(press1Release0) {}
        bool serialize(TxFrame& tx) const noexcept override;
    };

    // --- Случайные числа ---
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

    // --- Waveform ---
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
    class WaveAddt final : public Command {
        uint32_t _waveformId;
        uint32_t _channel;
        uint32_t _byteCount;

    public:
        WaveAddt(uint32_t waveformId, uint32_t channel, uint32_t byteCount) noexcept
            : _waveformId(waveformId), _channel(channel), _byteCount(byteCount) {}
        bool serialize(TxFrame& tx) const noexcept override;
    };

    /**
     * **Wave**form **cle**ar — очистить канал или все каналы осциллограммы (NIS `cle`).
     */
    class WaveCle final : public Command {
        uint32_t _waveformId;
        uint32_t _channelOr255All;

    public:
        WaveCle(uint32_t waveformId, uint32_t channelOr255All) noexcept
            : _waveformId(waveformId), _channelOr255All(channelOr255All) {}
        bool serialize(TxFrame& tx) const noexcept override;
    };

    // --- Устройство и цикл событий ---
    /**
     * **Rest**art — перезагрузка дисплея Nextion (NIS `rest`).
     */
    class Rest final : public Command {
    public:
        bool serialize(TxFrame& tx) const noexcept override;
    };

    /**
     * **Do** **events** — отдать управление внутреннему циклу дисплея (обработка очереди, NIS `doevents`).
     */
    class DoEvents final : public Command {
    public:
        bool serialize(TxFrame& tx) const noexcept override;
    };

    // --- EEPROM ---
    /**
     * **W**rite **ep**rom **o**peration — записать значение переменной/константы во встроенную EEPROM (NIS `wepo`).
     */
    class Wepo final : public Command {
        TargetAttr _source;
        uint32_t _addr;

    public:
        Wepo(const TargetAttr& variableOrConstant, uint32_t eepromStart) noexcept : _source(variableOrConstant), _addr(eepromStart) {}
        bool serialize(TxFrame& tx) const noexcept override;
    };

    /**
     * **R**ead **ep**rom **o**peration — прочитать из EEPROM в переменную (NIS `repo`).
     */
    class Repo final : public Command {
        TargetAttr _dest;
        uint32_t _addr;

    public:
        Repo(const TargetAttr& destVariable, uint32_t eepromStart) noexcept : _dest(destVariable), _addr(eepromStart) {}
        bool serialize(TxFrame& tx) const noexcept override;
    };

    /**
     * **W**rite **ep**rom **t**ransparent — запись сырого блока байт в EEPROM (NIS `wept`).
     */
    class Wept final : public Command {
        uint32_t _addr;
        uint32_t _byteCount;

    public:
        Wept(uint32_t eepromStart, uint32_t byteCount) noexcept : _addr(eepromStart), _byteCount(byteCount) {}
        bool serialize(TxFrame& tx) const noexcept override;
    };

    /**
     * **R**ead **ep**rom **t**ransparent — чтение блока из EEPROM по UART (NIS `rept`).
     */
    class Rept final : public Command {
        uint32_t _addr;
        uint32_t _byteCount;

    public:
        Rept(uint32_t eepromStart, uint32_t byteCount) noexcept : _addr(eepromStart), _byteCount(byteCount) {}
        bool serialize(TxFrame& tx) const noexcept override;
    };

    // --- GPIO, анимация, звук ---
    /**
     * **Cfg**igure **gpio** — настроить вывод MCU, видимый дисплею, и привязку к компоненту (NIS `cfgpio`).
     */
    class Cfgpio final : public Command {
        uint32_t _pin;
        uint32_t _mode;
        TargetComponent _bindComponentOrZero;

    public:
        Cfgpio(uint32_t pin, uint32_t mode, const TargetComponent& bindComponentOrZero) noexcept
            : _pin(pin), _mode(mode), _bindComponentOrZero(bindComponentOrZero) {}
        bool serialize(TxFrame& tx) const noexcept override;
    };

    /**
     * **Move** — анимация перемещения компонента между двумя точками за время `timeMs` (NIS `move`).
     */
    class Move final : public Command {
        TargetComponent _component;
        int32_t _x0;
        int32_t _y0;
        int32_t _x1;
        int32_t _y1;
        uint32_t _priority;
        uint32_t _timeMs;

    public:
        Move(const TargetComponent& component, int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t priority, uint32_t timeMs) noexcept
            : _component(component), _x0(x0), _y0(y0), _x1(x1), _y1(y1), _priority(priority), _timeMs(timeMs) {}
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

    // --- Файлы и каталоги (Intelligent / расширенные серии) ---
    /**
     * **T**ransparent **w**rite **file** — подготовить приём файла по UART (размер, путь; NIS `twfile`).
     */
    class Twfile final : public Command {
        const char* _pathQuoted;
        uint32_t _fileSize;

    public:
        Twfile(const char* pathQuoted, uint32_t fileSize) noexcept : _pathQuoted(pathQuoted), _fileSize(fileSize) {}
        bool serialize(TxFrame& tx) const noexcept override;
    };

    /** **Del**ete **file** — удалить файл на SD/файловой системе дисплея (NIS `delfile`). */
    class Delfile final : public Command {
        const char* _pathQuoted;

    public:
        explicit Delfile(const char* pathQuoted) noexcept : _pathQuoted(pathQuoted) {}
        bool serialize(TxFrame& tx) const noexcept override;
    };

    /**
     * **Re**name **file** — переименовать/переместить файл (NIS `refile`).
     */
    class Refile final : public Command {
        const char* _pathFromQuoted;
        const char* _pathToQuoted;

    public:
        Refile(const char* pathFromQuoted, const char* pathToQuoted) noexcept
            : _pathFromQuoted(pathFromQuoted), _pathToQuoted(pathToQuoted) {}
        bool serialize(TxFrame& tx) const noexcept override;
    };

    /**
     * **Find** **file** — проверить существование файла, результат в числовой атрибут (NIS `findfile`).
     */
    class Findfile final : public Command {
        const char* _pathQuoted;
        TargetAttr _dstNumAttr;

    public:
        Findfile(const char* pathQuoted, const TargetAttr& dstNumAttr) noexcept : _pathQuoted(pathQuoted), _dstNumAttr(dstNumAttr) {}
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
     * **Set** **layer** — порядок отрисовки (Z-order): объект над другим или «255» как в NIS (`setlayer`).
     */
    class Setlayer final : public Command {
        TargetComponent _component;
        TargetComponent _aboveOr255;

    public:
        Setlayer(const TargetComponent& component, const TargetComponent& aboveComponentOr255) noexcept
            : _component(component), _aboveOr255(aboveComponentOr255) {}
        bool serialize(TxFrame& tx) const noexcept override;
    };

    /** **New** **dir**ectory — создать каталог (NIS `newdir`, путь со слэшем в конце). */
    class Newdir final : public Command {
        const char* _pathQuoted;

    public:
        explicit Newdir(const char* pathQuotedTrailingSlash) noexcept : _pathQuoted(pathQuotedTrailingSlash) {}
        bool serialize(TxFrame& tx) const noexcept override;
    };

    /** **Del**ete **dir**ectory — удалить каталог (NIS `deldir`). */
    class Deldir final : public Command {
        const char* _pathQuoted;

    public:
        explicit Deldir(const char* pathQuotedTrailingSlash) noexcept : _pathQuoted(pathQuotedTrailingSlash) {}
        bool serialize(TxFrame& tx) const noexcept override;
    };

    /**
     * **Re**name **dir**ectory — переименовать каталог (NIS `redir`).
     */
    class Redir final : public Command {
        const char* _pathFromQuoted;
        const char* _pathToQuoted;

    public:
        Redir(const char* pathFromQuoted, const char* pathToQuoted) noexcept
            : _pathFromQuoted(pathFromQuoted), _pathToQuoted(pathToQuoted) {}
        bool serialize(TxFrame& tx) const noexcept override;
    };

    /**
     * **Find** **dir**ectory — проверить каталог, результат в числовой атрибут (NIS `finddir`).
     */
    class Finddir final : public Command {
        const char* _pathQuoted;
        TargetAttr _dstNumAttr;

    public:
        Finddir(const char* pathQuotedTrailingSlash, const TargetAttr& dstNumAttr) noexcept
            : _pathQuoted(pathQuotedTrailingSlash), _dstNumAttr(dstNumAttr) {}
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

    
} // namespace oper

} // namespace cmd

} // namespace nex
